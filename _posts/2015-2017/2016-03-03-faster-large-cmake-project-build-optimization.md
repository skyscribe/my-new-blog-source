---
layout: post
title: Faster build optimization for large CMake based project
categories: [build, tools]
tags: [programming, build, tools, cmake, cpp]
---
Building time is always a big concern for large scale C/C++ based software projects, there've been a lot of outstanding tools invented to relief the pain, CMake is an emering de-facto standard for big projects, however there're lots of misuse that may slow down project building dramatically. This post would cover a real life case on how to correct those gotchas to improve build time and enable delta build to boost R&D efficiency.
<!--more-->
## Background
In daily development work, engineers are frequently frustrated by the slow build system

- Even without no single line of code change, re-triggering the execution of test cases takes up to **3 minutes** before testing results are given back
- When build servers are busy (indicates more people are compiling), the feedback time would be longer
- This would make TDD near to impossible and programmers just throw their changes to CI (Jenkins) jobs and let CI give them feedback.

### Initiatives from CI and the delima
Around 2015, solution development office already innovated a lot of smart ideas to shorten the CI efficiency, including

- Introduced **scache** to share the intermediate object files to reduce unnecessary re-generating object files
- Deployed multiple cloud instances and dispatched CI jobs to multiple cloud instances so jobs can run in parallel
- Transformed to Git and Gerrit to alleviate the infamous check/export issues of subversion
- Take the power of Gerrit and link CI jobs with every patch set on Gerrit

Above ideas are awesome from CI side, unfortunately daily TDD cycle is quite different with CI. For **majority of the time, developers just want to change a small sub-set of the source code**, and they hardly need to rebuild the whole system. Every piece of efforts were invested on CI even makes programmers think their only rescue is to ask CI to do the build/testing. When they make some changes, they just ~~create a patch and throw it to Gerrit~~, do something else for a while and get back to check if Jenkins jobs are done with positive (or negative) feedbacks. Things do work well with their own downside:

- **Jenkins/CI becomes more busy** due to more and more engineers push their local changes (not verified locally) to CI
- There is **less and less room to improve from CI** side unless more budget can be assigned (allocate more cloud instances)
- Programmers are still quite distracted due to **task switches**(waiting for building/testing results), everybody knows it's bad, and lots of them get depressed and think nothing can change

### Untouched dark side
Looking at the Jenkins jobs of TDD CPRI, it is sad that every time the job is triggered, it will discard previous build space, and **build everything from scratch**. Thanks to the **clever scache**, most of the object files won't be rebuilt, however each source file's checksum has to be re-calculated and compared to ensure cache is still valid. This is quite non-trivial considering the fact that probably **tens of (even hundreds of) engineers may work on the same Linux box**, let it alone strace is implemented in bash and the **compare part relies on the time-consuming strace**. This also contribute to high system load and makes build servers slower and slower - some times we even see the shell is out of service due to memory swapping.

The motivation sounds like pretty simple and intuitive - we need the **incremental build**, so only changed part are really checked and rebuilt. If the computer (make system) has the **correct and reliable** knowledge of what needs to be rebuilt, CPU resources can be saved, and feedback cycle would be significantly shortened.

The challenges are also quite outstanding:

1. Increment build is hard though possible - too many factors may make it broken. 
2. Keeping increment build stable and reliable is even harder - definitely true when your project demands a lot of 3rd party libraries/headers For

Fortunately things would be easier and the target is to reduce programmers cycle time, based on below facts

1. Programmers typically stick to a fixed set to external resources for daily work, for most of his/her time
2. When external references change, we can still **fall back to scache**, this only happens when people needs to merge/rebase code, and it is much safer to do clean build under such circumstances

### Domain specific build/testing
Another difference between CI and daily development cycle is: developers typically work in a narrow scope of the source tree, so they're confident that their changes won't break much. He/she may want to verify if small changes breaks legacy system or not, or if newly added code/tests works as expected or not.

It is **too over kill to build the whole system and run all the tests for such relatively trivial tasks**. Things would be perfect if we can **do building/testing selectively, on a folder** (is generally called an internal domain). By narrowing down the scope, feedback cycle will be naturally shorter.

Whenever a developer changes a few places and want to verify the impacts by unit tests (we like the idea of TDD, as long as it can be more practical), he/she only to follow below steps:

1. Identify the domain/folder under testing
1. Trigger a single build step that only build/test impacted parts:

    1. Target source library can be successfully built and linked
    2. Legacy test cases can be rebuilt and rerun
    3. Newly added test cases (if any) can be automatically built and checked

1. Building/test results can be given back shortly (**by seconds would be optimal**)

## The Solution

Ideas being simple and straightforward, implementations/optimizations seldom are. Walking through the building system of TDD CPRI, below shortcoming has to be coped with:

- Mixed use of GNU make and CMake and the glue layer is complicated
- 3rd party libraries are stored on SVN as external references, while main codebase is managed by Git/Gerrit
- A wrapper python script was written to generate hierarchical make files brings more complexity
- CMake binary was wrapped by an external project and provisioned as _crosscmake_

### Separate Developer Commands From CI Commands

CI jobs take use of below commands:

- `make fsmf_target` to generate package for entity testing
- `make fsmf_test` to generate testable for UT/MT
- `make fsmf_test_run` to run previously generated test cases
- `make fsmf_clean` to cleanup whole build workspace

Almost all above commands would invoke **slow svn commands** to do sanity checking on external links, and trigger external CMake system. **This is not needed for daily usage** and can be skipped.

For daily work, developer may need to verify below typical scenarios:

- If test build pass, check if UT/MT works
- Verify changes for a given domain can pass compile for UT/MT binaries
- Verify if source changes can still make a valid knife/package

Introducing extra command line options can alleviate the work so Make system can detect what would be done.

- If user passes `domain=bbswitch`, only bbswitch domain specific targets would be rebuilt.
- If user passes `use_gcov=1`, coverage flag would be turned on - not a typical scenario for daily development jobs

In case people want to verify multiple domains, a list of domains can be supported, this facility further reduce the requirement to invoke building everything.

### Keep CMake Cache As Reliable

Legacy make scripts (the top wrapper) manages the CMake sub-system, and translate user commands into internal CMake sub-system. For some reason, the cached files are re-configured and generated each time people want to make something. This brings considerable overhead, **CMake is not designed to work like this**, being a **Meta-build system**, it's better to respect CMake and let itself to manage its build system's integrity.

The solution is simple once external factors that may invalidate CMake's cache system are identified:

- Things that impact compilation flags shall be controlled by CMake variables
- Things that shall be decided by run time (like which domain to run tests) shall be passed as runtime parameters, than CMake variables - note each time a variable is changed, **CMake has to be unnecessarily reconfigured**, and we want to reduce cache rebuilding as possible!
- Give explicit target than relying on the **default** make target - previously almost every sensible targets (libraries/binaries/custom\_targets) were specified to be built by default; this makes a simple `make ` command takes minutes to return
- A few bugs were identified like test binaries were removed each time cmake refresh its cache, while linking is quite time consuming, and the old binaries should have been reused instead

Another subtle bug introduced by `crosscmake` was also fixed due to the fact that `cmake -H` and `cmake -D` options are not compatible. CMake system relies on `-H` option to rebuild its make files as necessary, while crosscmake makes this impossible. It was suspected this would be one reason why global team choose to regenerate makefiles every time.

## Benchmark Result

Exciting results were perceived. 

Previously, no code change (simply invoking `fsmf_target`) took 2~3 minutes to complete on a decent free Linux server. After those enhancements/optimizations, only **7~8 seconds** were consumed to do the make stuff. Note in this scenario, no real code changes were made.

When one or two files are changed, typically 1~2 binaries needs to be re-built besides the object file generation. The net time for make system checking can still be saved, extra gain comes from less targets scanned/linked. It typically takes **10~30 seconds** to complete, while in the past, we need to wait **4~6 minutes**.

When large amount of code changes are made, the benefit might be less obvious since the C++ compilation/linkage time dominates the overall time slices.This is right the place where **scache** are designed for.


## Next steps

This is of course not the end of our story - we don't touch the incremental build part of CI yet and it's full of potential. The characteristics of CI is quite different with daily development, however below ideas would be interesting:
- Take use of better file system to boost compiling speed, like using memory mapped file system - a lot of GCC's runtime are spent on IO
- Saving previous workspace (or tagged workspace) and not cleaning everything
- Using distcc/ninja for better C++ building performance


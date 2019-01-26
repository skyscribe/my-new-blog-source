---
layout: post
title: "A software design and test guideline for C++ project"
comments: true
categories: [design, engineering]
tags: [design, programming, test, strategy, guideline]
---

To make large scale C++ software project in good shape is not an easy job, especially when you have **a pretty big development team that may form multiple sub-teams**. One of the key actions is to have **common understanding on software design and testing strategies, rules and guidelines**, which are far more important than simply coding guidelines.

This article is my personal understanding and suggestion on such a critical (yet controversial) topic.

<!--more-->

# Design/Test Strategy
Here's the general rules regarding design and testing strategy.

## Testing Strategy

- Software testing strategy would be **UT + SCT** during development.
- Obsolete MT(module test) cases in all domains in the long run, consider adding SCT to cover specific scenarios while counting whole binary as SUT
- Consider UT and SCT as different level of tests, so **SCT coverage wouldn't replace UT coverage**, vice versa 
- Remove legacy MT case when it's considered to be too heavy to maintain
- **DONOT** test ~~against internal implementation of an entity~~ (either a class or binary), test its interface (public methods or exposed component interfaces) intensively
- Design complicated scenarios into different layers, so unit tests can be written **separately in different levels**
- Avoid writing tests that treats different layers of implementations as SUT, write testing against lowest layer directly as possible

## Class Design For Testability
- Program your class to interface than implementation
- Keep class as **small with single responsibility** as possible
- Design your class so it can easily be tested, with small interface, clear responsibility and explicit collaboration with other classes
- Also take SCT testability into account in design
- Wrap utility functions within nested namespaces, and test them individually. DONOT place them in a class with static qualifier.
- DONOT add code to a legacy big smelly class, consider **extract the changes into smaller classes** and use **dependency injection** to make changes testable

# Technical Detailed Guidelines
A few detailed guidelines are listed here for reference.

## Class Design
Class is the basic element of object oriented software, the majority of the unit tests shall be taken a single class as SUT. Here lists some general guidelines:

- A class should not provide too many public interfaces, generally 3~7 public methods shall be preferred
- A class should not leak its internal states to external users by public interfaces
- A class should initialize its collaborators through **dependency injection** than directly construct by instance
- A class should not work on different abstract levels
- High level classes (working towards the center of business logic interaction) should not work directly with low level details - design lower level classes to finish the low level work
- A class should not collaborate with too many external classes, no specific number is suggested here, while working with **tens of external classes would be definitely problematic** in most cases

### Boundary Class Design
A boundary class is designed to be the interface to external entities (module or higher level entities), the design should apply below guidelines
- It should be an abstract class with visible interfaces, main intention is the **separation of concerns**
- It should never reveal internal design details
- It should not bring unnecessary dependencies (like introducing template meta-programming elements)
- It should not handle processing details rather act as the bridge to other entities

Above guidelines also holds for general class design, while special care shall be taken when you're designing a boundary class, since violation of them might turn overall software architecture into *a big ball of mud*.
>A Big Ball of Mud is a haphazardly structured, sprawling, sloppy, duct-tape-and-baling-wire, spaghetti-code jungle. **These systems show unmistakable signs of unregulated growth, and repeated, expedient repair**. Information is shared promiscuously among distant elements of the system, often to the point where **nearly all the important information becomes global or duplicated**. The overall structure of the system may never have been well defined. If it was, it may have eroded beyond recognition. Programmers with a shred of architectural sensibility shun these quagmires. Only those who are unconcerned about architecture, and, perhaps, are comfortable with the inertia of the day-to-day chore of patching the holes in these failing dikes, are content to work on such systems.

> — Brian Foote and Joseph Yoder, Big Ball of Mud. Fourth Conference on Patterns ? Languages of Programs (PLoP '97/EuroPLoP '97) Monticello, Illinois, September 1997


Also be careful not to introduce too many boundary classes.

### Application-logic Class Design
Application logic specific classes are those who are created to fulfill certain specific business logic. It shall comply with below guidelines
- It should be kept as low in coupling and high in cohesion
- It should have single responsibility, have **good balance between SOLID principles**, and not violate the law of Demeter
> Each unit should have only limited knowledge about other units : only units "closely" related to the current unit.
>
> Each unit should only talk to its friends; don't talk to strangers.
>
> Only talk to your immediate friends.

- It should never work on different abstraction levels, like a **manager/controller class handles low level platform APIs should be discouraged**
- It should not contain too many data members, which is typically sever violation of single responsibility
- They may further by abstracted into different levels, if this is the case, keep **dependency inversion principle** followed such that abstractions (higher level classes) shall not depend on implementation details (lower level classes)
> A. High-level modules should not depend on low-level modules. Both should depend on abstractions.
>
> B. Abstractions should not depend on details. Details should depend on abstractions.

### TMP Usage
Template-meta-programming are widely adopted by modern programmers, unfortunately it's quite often misused/overused. When it's overtaken, compiling dependence might be a serious problem, and compiler diagnostics messages might kill your time. It's not a problem of generics itself, but rather a limitation of compilers and c++ language.Here's some general ideas
- DONOT bring TMP to public interfaces unless you're designing low level utilities
- Balance OOD and TMP, hide TMP into implementation details would be a good idea
- DONOT reinvent the wheels, make good use of standard libraries

## Unit Test Design
This chapter would **not** cover basic howtos about unit testing, although some important guidelines are listed. Walk through [unit test guide](cpri_handler_unit_test_guide.md) for that purpose.

### General Rules
Below general rules shall be applied always as possible
- Each non-trivial classes shall be tested
- DONOT test against factory method or classes since they're designed to bring up other objects - it's still valuable to test against complicated startup procedures
- Keep **test design and class design as synchronized** - whenever class design is changed, test design shall be refined accordingly

### Test Case Intention And Focus
- A test case/suite shall **test against a sinle class in most of the time**, testing against multiple classes without abstraction generally makes tests fragile and hard to maintain; be careful when you want to bring multiple classes into SUT
- A test case shall **test against the public (exposed) interface** only, and consider the SUT (specific class) as black box as possible
- A test case shall focus on the behavior (business intention) of its SUT than internal implementation, which are more subject to change
- Different test cases shall be added to **cover both normal scenarios and exceptional scenarios based on intention**
- A test case shall be as specific as possible, and shall have **clear expectation and strict validation**
- DONOT try to cover more than one scenario in one test case, feel free to add more cases for exceptional scenarios

### Testing Interaction With Mocks/Stubs

- Be careful on heavy mocks, and **add strong checks on matchers and set desired actions** if you want to validate the output (interaction) in customized mocks
- Prefer grouping mocks in different test suites than organizing them in common functions, the latter is harder to maintain
- Combine stubs with mocks wisely
- DONOT create threads before careful reasoning - introducing threads to unit tests makes test cases hared to maintain and track
- DONOT introduce real timers to test cases - advance a mocked timer to simulate the timeout behavior makes tests more stable and predictable
- **Never sleep nor wait** in test cases
- Make unit tests run fast as possible - generally one unit test shall not take over **300 milliseconds** to finish

### Test Cases Grouping/Suites
- **Generalize common operations and reuse them** as test fixtures that can be shared by multiple test cases
- Prefer split big/complicated tests into smaller ones and group them according to logical abstractions - big tests typically indicates design smelly in SUT
- DONOT create very large fixtures, consider ways to re-organize setups/fixtures by abstraction
- Keep one test group (based on a common fixture typically) in one or more source files, **DONOT** place irreverent tests in one source file

# References
- [Class Responsibility Collaboration(CRC) models: An Agile Introduction](http://agilemodeling.com/artifacts/crcModel.htm)
- [Wikipedia:CRC Cards](https://en.wikipedia.org/wiki/Class-responsibility-collaboration_card)
- [Dependency Inversion Principle](https://en.wikipedia.org/wiki/Dependency_inversion_principle)
- [Big Ball of Mud](http://www.laputan.org/mud/)

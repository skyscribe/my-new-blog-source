---
layout: post
title: CppCon2015 and Cpp Core Guidelines
categories: [cpp, programming, design, engineering]
tags: [programming, design, cpp, guideline]
---
C++社区的第二届编程语言社区活动[CppCon2015](https://cppcon.org/2015program/)落下了帷幕；作为C++语言的发明人和灵魂人物，
Stroustroup和Herb Sutter一起宣布他们正工作与一个基于现代C++核心语言的编程规范（**Core Guidelines**）并发布
在[GitHub](https://github.com/isocpp/cppcoreguidelines)上，并立即引起了轰动。

<!--more-->

CppCon大会所有的演进材料和培训资料都放在[Github](https://github.com/CppCon/CppCon2015)上；今年的重头戏无疑属于Core Guidelines。

## 核心编程规范

该规范目前主要的编辑依然是Herb Sutter和Stroustroup；其目的是为了提供一份相对**官方的编程指南**；帮助社区程序员用好C++语言，避免误用，提高现代C++
的使用率，减少不合理的误用、滥用，因为C++语言实在是太复杂了；很多是仅仅是因为语言提供了某些机制，就像到处使用该机制制造出难以理解和维护的代码；
毕竟**可以这样做并不意味值就必须要这样做**。

另外一个诱因是，现代的C++语言引入了很多新的语言特性导致很多传统的C++程序员有点疑惑这是否是一门新的语言了。
> "Within C++ is a smaller, simpler, safer language struggling to get out." -- Bjarne Stroustrup

文档提供了一份在线版本，可以从[项目的GitHub Pages页面](http://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)来访问和浏览非常方便。

### 顶层目标

该编程规范的目的明显的不同于传统商业公司或者项目团队的编程规范；因为它不会侧重于解释某一些语言特性或者特定的问题；而是更多**从设计的角度**给出建议，包括
- 接口设计和使用
- 模板元编程的取舍，什么场景下应该使用，什么情况下算误用
- 设计约束和限制，例外情况
- 设计/编码哲学，如何取舍不同方案等
- 工程实践推荐，使用工具，如何使用
- 错误处理机制，异常，返回值等

### 基本组成
规范采用统一格式的条目组成，这些条目按照主题被分成为不同的章节，包括
- 概要介绍：规范的目标，非目标，文档结构和一些主要的章节
- 设计哲学：如何表述编码意图，采纳标准语言设施，表述业务意图，采用强类型，优先采纳编译器类型检查机制，采用RAII防止资源泄露，使用合适的工具或库等
- 接口设计：遵循良好的业界**基于接口编程**的实践，避免全局变量，采用强类型的接口；明确表述前置条件/后置条件等
- 其它按照语言特性分开描述的章节

### 单独条目组织方式
每个具体的条目规则采用示例代码加描述的方式，便于搜索和参考。大部分条目由5个部分组成。

Reason部分描述为什么需要遵守给定的规则；会简单的描述一下特定规则背后的动机和原因。

Example会给出具体的例子来阐述规则的内容。大部分情况下，会给出好的示例和坏的示例。比较复杂的条目，可能给出多个例子便于参考。
语言机制有所演进的时候，不同的语言版本下的好的或者坏的例子都会给出来。

Note部分会给出一些额外需要注意的地方；或者参照应用链接等。

Enforcement会列出对应规则的附加限制条件。

Exception对适应的规则会给出一些例外情况，因为大部分规则通常都有例外情况。有例子的话也会一并给出。

### 项目支持工具

Microsoft开源了一个相关的规范支持工具 - [GSL](https://github.com/Microsoft/GSL)，这是一个仅有头文件组成的库，应该很容易集成到支持C++14的编程环境中。
该项目提供了一系列测试用例，可以在GCC/VC/LLVM上编译。

希望该编程规范可以被更多的C++项目团队所采纳。

## 其它印象深刻的材料

除了上述编程规范，还有一些演讲让人眼前一亮

### Ranges Library
来自于Eric Niebler的Ranges库([演讲材料](https://github.com/CppCon/CppCon2015/blob/master/Keynotes/Ranges%20for%20the%20Standard%20Library/Ranges%20for%20the%20Standard%20Library%20-%20Eric%20Niebler%20-%20CppCon%202015.pptx)) 
 将管道的思想和函数式编程的类Monad操作发挥到了一个令人瞠目结舌的境界，可以用简洁的代码更清晰的描述业务逻辑。
示例代码是一个优雅地打印一个字符界面的日历的例子，可以简洁地写为

```cpp
std::copy(
    dates_in_year(2015)                         //0. Raw range data (Date objects)
        | by_month()                            //1. Group the dates by month
        | layout_months()                       //2. format the months as a range of strings 
        | chunk(3)                              //3. Group the months as chunks of 3 side by side
        | transpose_months()                    //4. Transpose the rows and columns of the side-by-side months
        | view::join                            //5. Ungroup the months 
        | join_months(),                        //6. Join the string of transposed months
    std::ostream_iterator<>(std::cout, "\n")
);
```
函数式编程的可组合、高度可重用、不需要循环而仅仅关注意图的编程风格带来更容易维护、更简洁清晰的代码。 
声明式编程风格对C++而言不再遥远。期望它能早日进入新的语言标准库中造福广大程序员。

更多细节和Proposal详见[N4128](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4128.html)和[N4382](http://www.open-std.org/JTC1/SC22/WG21/docs/papers/2015/n4382.pd)。
基于C++11实现的库代码开源在[Github](http://www.github.com/ericniebler/range-v3
)。
项目文档在该项目的[GitHub Pages](https://ericniebler.github.io/range-v3/index.html)页面上；包含有几个基本部分的文档
- 基本安装和使用：基于头文件的轻量级的库，仅仅需要放置在合适的安装路径中，包含头文件即可；本身也提供了一个`<range/v3/all.hpp>`的头文件方便使用。
- 可组合的Views类，采用函数式风格的组合模式。
- Actions类用于对range做一些修改性操作，譬如join/erase/insert/drop等。

#### C++中的Atomics
[C++11, 14, 17 Atomics - the Deep Dive](https://github.com/CppCon/CppCon2015/blob/master/Presentations/C++11,%2014,%2017%20Atomics%20-%20the%20Deep%20Dive/C++11,%2014,%2017%20Atomics%20-%20the%20Deep%20Dive%20-%20Michael%20Wong%20-%20CppCon%202015.pdf) 来自于前OpenMP项目创始人的华人C++大拿的深入探讨新的C++语言中的原子性操作支持和并发编程；读来让人深思。

Michael Wong回顾了传统C++编程中的性能、并发、一致性的挑战，以及语言层面的逐步进化：从早期C++98/03标准的未置一词，到C++11首次引入的内存模型，原子类型等基本概念的支持，
进而是C++14的无所算法增强和无锁算法支持和`atomic_signal_fence`，再到未来的C++17中需要定义的内存访问次序强一致性保证和原子性视图，
有一些追赶Java并发支持的味道，当然C++有自己的挑战需要解决；看来是个长期的任务。

#### 并行STL
[Parralellizing the C++ STL](https://github.com/CppCon/CppCon2015/blob/master/Presentations/Parallelizing%20the%20C++%20STL/Parallelizing%20the%20C++%20STL%20-%20Grant%20Mercer%20and%20Daniel%20Bourgeois%20-%20CppCon%202015.pdf)介绍了新的C++中，容器算法的并行化做法，基本上一些耦合不紧易于分解的数据算法，可以很简单的调度到多核CPU上


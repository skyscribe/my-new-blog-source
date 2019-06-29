---
layout: post
title: C++20 - 下一个大版本功能确定
comments: true
categories: [programming, cpp, language, programming]
tags: [programming,engineering, language, cpp]
---

C++20的功能特性已经于3月份冻结，显然**这次终于来了一波大的改进**，而不再是像之前C++14/C++17那般小打小闹的做小步快跑，尤其是三个讨论很久的大feature终于被合入主干；并且这些feature终将会极大地影响后续C++代码的书写方式。

<!--more-->

## 核心语言特性终于有了大变化
新的版本之所以被认为是下一个大的版本，主要原因还是来自于核心语言特性的扩充和简化。看起来好像两个目标有些互相矛盾，但是内在的逻辑其实还是统一的：
- 扩充新的特性可以弥补之前一些遗留已久的功能限制，**方便提高程序员的生产力**，减少社区中长期存在的奇技淫巧侵蚀程序员宝贵的心智空间；
- 简化的方向主要是出于“照顾”新手程序员，帮助他们**更快地上手产生生产力**而不是匍匐在陡峭的学习曲线上靠长期的实践积累来摸索，从而培养下一代的新鲜血液，否则语言就会因为失去活力而慢慢消亡；这显然不是标准委员会愿意看到的。

从这两个角度看，也许片面地评价标准委员会的资深专家们为“学院派”或者“老学究”，总归是有些不合适的；因为C++一开始在90年代上半段的风靡完全是因为它是一门实用的程序语言。
只是随着时间的推进，很多**早期做出的设计决策多多少少被整个产业界的各色各样的业务需求催生的奇技淫巧所侵蚀**；

尤其是**模板元编程的流行和语言特性本身的滞后**带来的矛盾一直没有得到合适的处理，背后的原因正是标准委员会需要照顾已有的软件代码的兼容性（当然背后也有很多大公司的利益考量），妥协再妥协；最终演变成不得不变的地步。

## Concepts
像concept这样可以**明显提升程序员生活质量**的特性(想象一下用错了一个容器的成员函数之后GCC打印出来的"成吨"的编译错误，很多程序员形容是恨不得捏着鼻子绕着走)，愣是从C++03定稿之后就被提出出来，却活生生被推迟了一次又一次，甚至GCC的版本库上的concepts分支都经历了加上来又移除掉的曲折过程 - **速度和质量**始终是一对很难权衡的矛盾。

幸好，经历了**十几年的再三讨论**，concept这一模板元大杀器终于被投票送进了C++20标准的正式列表里。

关于Concepts最好的介绍当然是Bjarne自己的这篇[Concepts: The Future of Generic Programming](http://www.stroustrup.com/good_concepts.pdf)的文章，
另外一个比较好的描述来自于[cppreference](https://en.cppreference.com/w/cpp/concepts)；简单来说，它完成的事情就是用来描述泛型定义中，关于**类型参数的约束和校验**；
出于零成本的考虑，我们需要做到这个校验可以
- 在**编译期间完成**检查，对生成的实际代码没有影响（就像手写的代码一样）
- 具备**定义良好的接口**形式，可以方便地进行组合
- 尽量地保持通用性

### 使用Concepts

通过使用concepts，传统的模板元编程方面关于编译错误的痛点可以得到极大改善，编译器可以给出更加符合人类直觉的错误提示。
比如标准库中的[std::find](https://en.cppreference.com/w/cpp/algorithm/find)算法的声明如下：
```cpp
template< class InputIt, class T >
InputIt find( InputIt first, InputIt last, const T& value );
```
这里的**两个模板参数其实有更多额外的要求**用传统的语法是没法表达的，第一个类型参数`Input`我们期望它是一个可遍历的迭代器类型，并且其中的元素类型需要和`T`类型匹配，并且该类型能够用来做相等比较。
这些**约束条件在现有的语言标准中都是隐性**的，一旦用错，编译器就会拿海量的错误信息来招呼你，因为编译器背后会使用SFINAE这样的语言特性来比较各种重载并给出一个常常的candidate列表，然后告诉你任何一个尝试都没有成功，所以失败了。

Concepts相当于将这些要求用一种显而易见的方式给出来，比如我们想表述一个在序列容器上查找的类似算法，可以用concepts来描述为

```cpp
template <typename S, typename T>
requires Sequence<S> && Equality_comparable<Value_type<S>, T>
Iterator_of<S> find(S& seq, const T& value);

//using alias
template<typename X> using Value_type<X> = X::value_type;
template<typename X> using Iterator_of<X> = X::iterator;
```

这时候如果使用不满足条件的输入参数，编译器会直观地告诉我们错误的具体原因
```cpp
vector<string> vs;
list<double> list;
auto p0 = find(vs, "waldo"); //okay
auto p1 = find(vs, 0.11); //error! - can't compare string and double
auto p2 = find(list, 0.5); //okay
auto p3 = find(list, "waldo"); //error! - can't compare double and string
```

显然这里的例子有点啰嗦，出于**节约键盘敲击次数**的考虑（Java太啰嗦了？原来的模板元函数的写法也已经够啰嗦的了！），可以进一步简化这个写法，将简单的concepts约束直接嵌入到声明的地方：
```cpp
template <Sequence S, typename T> 
    requires Equality_comparable<Value_type<S>, T>
Iterator_of<S> find(S& seq, const T& value);
```

### 自定义concepts
对于上面的简单的concepts，标准库已经提供了一个开箱可用的封装，不过**出于学习目的自己动手做一个轮子**也很简单。比如用上面的比较为例，可以写作
```cpp
template <typename T>
concept bool Equality_comparable = requires(T a, T b) {
    { a == b} => bool; //compar with ==, and should return a bool
    { a != b} => bool; //compare with !=, and should return a bool
}
```
语法上和定义一个模板元函数很想象，所不同的地方是
- 这里我们定义的对象是一个关于类型的检查约束
- 这里的`requires`部分引申出具体的检查约束，必须同时实现操作符相等和不相等，两个操作符都需要返回bool类型
- 整个concept本身可以用在逻辑表达式中

### 简化concept的格式负担
如果能将简单的事情变得更简单，为什么不更进一步呢？这个设计哲学是C++的核心设计思想之一（参见Bjarne的D&E CPP），考虑下面的例子
```cpp
template <typename Seq>
    requires Sortable<Seq>
void sort(Seq& seq);
```
这里的`Sortable`表示某个可以被排序的容器类型；因为concept也是用于限制类型，而函数的参数也是用来限定类型，一个自然的想法就是逐步简化它
```cpp
//应用上述的简化方式，concept描述放在模板参数声明中
template <Sortable Seq>
void sort(Seq& seq);
```
进一步地，去掉`template`部分的声明会更加简单，**就像是一个普通的函数声明**，只不过参数类型是一个编译器可以检查的泛型类型
```cpp
void sort(Sortable& s);
```
这样一来，就和其它语言中的接口很类似了，没错就和Java的JDK中的泛型接口很相似了；只是底层的实现技术是完全不一样的，Java由于根深蒂固的OO设计而不得不借助于类型擦除术；当然这个扯的稍微远了一点。

### auto类型的参数
其实C++14里面已经允许了`auto`作为函数参数的类型这一用法，显然它和`concept`的简化写法完全不矛盾。
```cpp
void func(auto x); // x实际上可以是任意类型!
void func1(auto x, auto y); //x和y可以是任意的类型，可以不相同
```

在多个参数的情况下， 如果我们想限定两个参数的类型必须总是一样，有一种很简单的机巧做到
```cpp
constexpr concept bool Any = true; //任何类型都是Any
void func(Any x, Any y); //x和y的类型必须相同，尽管他们可以是任意类型
```

### 标准库中的预定义concepts
C++20的标准库中预备了很多开箱即用的concepts，通过库的方式提供，用户只需要包含`<concepts>`库即可。
详细的列表可以参考[concepts header](https://en.cppreference.com/w/cpp/header/concepts);从大的分来来看，包括
1. 核心语言相关的concepts，比如判断类型是否相同，是否是由继承关系，是否可以赋值/拷贝/构造/析构/转换等。
2. 比较相关的concepts，如`Boolean`用来判断是否可以用在逻辑判断上下文中，`EquallyComparable`和`EquallyComparableWith`声明了`==`运算符是否表述等价关系；`StrictTotallyOrdered`/`StrictTotallyOrderedWith`声明了比较运算符是否产生一个完全有序的结果
3. 关于对象的concepts，包括`Movable`，`Copyable`，分别声明可移动和可拷贝（包含了可以swap），而`Semiregular`声明对象是否可以被移动/拷贝/交换/默认构造；`Regular`则等价于`Semiregular`加上`EquallyComparable`
4. 关于函数调用的concepts，包含`Invocable`(声明对应的类型可以想函数一样调用并用给定的一系列参数来作为输入参数)，`Predicate`指定可调用的对象符合断言约束并返回bool，`Relation`指定对应的可调用函数是一个二元函数；`StrictWeakOrdering`则表明对应的函数满足[弱排序](https://en.cppreference.com/w/cpp/concepts/StrictWeakOrder)(具体细节需要一些逻辑数学知识)
5. 用于range库的对象交换要求

应该可以预期后续的版本将会加入更多的支持。

### 编译器的支持情况
GCC目前仍然是通过TS的方式来支持，编译时候需要加上`-fconcepts`开关；
Clang的全功能支持已经在将近一年前[在redit上宣布完工](https://www.reddit.com/r/cpp/comments/958sj9/clang_concepts_is_now_featurecomplete/)，只是其[官方的列表](https://clang.llvm.org/cxx_status.html)上依然没有更新。
MSVC则于更早一点宣布了完整的concept支持，只是我们需要Visual Studio 2017 15.7版本。
> The MSVC compiler toolset in Visual Studio version 15.7 conforms with the C++ Standard!
> <br/> https://devblogs.microsoft.com/cppblog/announcing-msvc-conforms-to-the-c-standard/

总体上看来，GCC的开发进度有些迟缓，clange的也不算很透明，只有MSVC比较领先。

## 模块化支持
模块支持被写入新的语言核心，这一新的封装方式甚至可以认为是**C++诞生35年以来最大**的一个新功能；
也是语言标准化以来，第一次通过修改核心语法允许程序员用一种**全新的方式来描述带命名的信息封装边界**。

### 信息封装手法的更新
传统的封装手段基本上都是采用如下的方式：
- 将用户自定义的结构或者类取一个名字
- 将相关联的细节都隐藏在这个名字的后面

不管是变量声明，函数定义，自定义的类，结构体，无一例外都满足这个模式。即使是模板元编程方法，其实也是通过类型绑定的方法间接地使用上述的封装手段。

### 头文件的不完美封装
除了上述的基本信息封装单元，C++中屈指可数的封装办法**就剩下了从古老的C语言继承下来的头文件包含**的方式了。
在软件规模还局限在数万行代码一下的时代，使用头文件的方式一股脑将需要的东西都大包大揽在一个编译单元中，然后使用诸如唯一定义规则的方法让链接器在生成最终可执行代码或者库的时候做冲突检测是一个**简单而优雅**的方案。
因为对于它想解决的问题规模来说，这样的解决方案就足够了。

然而随着行业中软件项目的复杂性与日俱增，越来越多的商业项目**需要数百甚至上千的头文件**被包含在一个编译单元中，这个时候既有的方式就越来越捉襟见肘
1. **编译时间长**成为了一个突出的痛点，以至于实际项目中出于减少编译时间的考虑，聪明的工程师发明了如IWYU的**头文件检测工具来缓解**这一问题
2. 像pimp惯用法这样的技术手段可以减少放在头文件中的依赖，但是程序员却不得不**承担额外内存申请**的开销（可能没有这么个指针很多对象本身就可以在栈上完成快速构造和析构），即使有了`unique_ptr`来环境内存泄露的隐忧
3. **模板方式提供的抽象代码无法声明，必须放置在头文件中内联实现**，否则对应的cpp文件在编译单元进行代码生成的时候会因为找不到定义而无法通过编译；之前提出的一些export声明也因为种种缺陷和编译器支持不力而被废弃

简单来看，**现代的编程语言都或多或少带有模块化系统**；缺乏现代的模块化支持成为了C++语言的一种硬伤，严重制约了C++开发大项目的能力。

### 模块化系统需要的核心功能

模块化是一个很自然的逻辑信息隐藏手段，一个良好的模块化系统应该允许
1. **良好的信息隔离**，可以方便的指定哪些可以被外部访问，哪些应该不允许被外部访问
2. 支持**嵌套的隔离机制**，即可以在模块中嵌套子模块
3. **细粒度的访问权限**控制，尤其是能提供模块内部/模块外部/子模块不同的访问权限则更好
4. 和**操作系统的文件访问系统保持一致的视图**，比如期望子文件夹可以对应子模块是一个非常符合直觉的方法
5. 支持**类似命名空间的隔离和访问**，比如支持重新导出一些模块中公开的可访问部分到另外一个空间中或者嵌套的命名、重命名模块等
6. 和**构建系统、打包系统**具有清晰易懂的交互接口；支持语言本身和IDE、语法检查等生态系统工具设施的无缝融合

要同时实现这些目标，并没有想象中的容易；其它一些流行的编程语言其实都小心仔细地对这些可能“讨好”程序员的目标做取舍，并在定义中详细地描述好限制。
比如Java一开始用Jar打包的方式来模拟模块，但是却由于不支持嵌套子模块中复杂的访问控制而遭到很多用户的不满；而Go语言中的模块和文件系统中文件名的纠葛同样也是Go语言中一个晦涩的知识点。
NodeJS通过NPM机制来提供模块化支持，然而其嵌套的打包方式和让人窒息的依赖树结构导致打包的时候需要依赖其它的第三方工具才能避免中招。

### 后向兼容的艰难挑战
C++的模块机制是奔着替换旧有的头文件包含机制的目标来的，同时又因为需要照顾庞大的既有代码库不被破坏而**不得不同时兼容头文件包含机制**。
和已有的其它语言特性一样，这种向后兼容带来的额外复杂性是否是必要的还又不小不同的声音，不过主流的声音还是决定走兼容的道路。

### 基本语法
如果我们希望声明一个模块，可以用如下的语法
```cpp
export module example; //声明一个模块名字为example
export int add(int first, int second) { //可以导出的函数
    return first + second;
}
``` 
因为我们丢弃了头文件的方法，可以将该模块定义保存在`example.cppm`的文件中。这里的`cppm`后缀用于告诉编译器这是一个模块定义文件。

假设我们希望使用该模块，则用如下的代码
```cpp
import example; //导入上述定义的模块
int main() {
    add(1, 2); //调用example模块中的函数
}
```

#### 分离模块接口和实现
如果我们想**分离模块的声明和实现，将他们放在不同的文件**中，这样更符合传统的接口定义和实现分离的编写代码方法（其实可以看作是C++比Java更干净的一个地方），我们可以对上面的`example.cppm`做如下的修改
```cpp
export module example;
extern int add(int first, int second);
```
然后创建一个源代码文件，放置模块函数的实现
```cpp
module example; //当前模块是example
int add(int first, int second) {
    return first + second;
}
```

出于灵活起见，C++20支持将一个模块中声明的**函数放在多个模块实现单元中分别实现**，这样更容易实现干净的代码，并提高编译速度。

#### 隔离权限指定
模块访问权是通过`export`声明来指定的，**没有声明的类或者函数等默认是不能被外部代码访问**的；基于声明的语法也决定了如果分离声明和实现，可见性在实现单元中其实是忽略的。

为了避免代码变得过于啰嗦，语法层面上也支持通过括号作用于一次性声明多个导出函数或者类，比如
```cpp
export module example;
export {
    void doSth();
    int doAnother(auto x, auto y);
}
void internalImpl(); //外部不可访问
```

#### 模块和namespace是正交的语言设施

旧的C++标准早就支持通过namespace来实现信息封装和隔离，而新的module机制可以和namespace结合使用，提供清晰的隔离结构，比如
```cpp
export module example;
export namespace name {
    void doSth();
    int doAnother(auto x, auto y);
}
```

语言机制上提供了灵活的手段，但是程序员却要自己**做好权衡，保持模块的粒度适中**，匹配实际的应用场景。

#### 模块重新导出
实际应用中，复杂的软件项目可能有很多形形色色的模块，它们可能处于不同的抽象单元；和应用代码比较近的上层模块可能**需要将某些它自己可见的模块开放给上层代码直接使用**，提供重新导出的功能可以极大地提高信息封装的能力，提高模块的内聚度减少不必要的耦合。

一个简单的方法就是将import的部分重新放在export块中，即下面的代码例子
```cpp
export module mid;
export {
    import low_module1;
    import low_module2;
    void myFunc(auto x);
}
```

### 标准库中的模块
标准库中提供的工具函数和类显然应该被模块化，只需要使用`import std.xxx`即可导入。
现代的WG21委员会的工作方式是有很多并行开发但是还没有进入主干库的"准标准库"，编译器可以选择实现，等到对应的规范成熟的时候，它们才会被正式地移入标准库中。

Visual C++的封装方式如下
1. `std.filesystem`提供文件系统的库，相当于`<experimental/filesystem>`
2. `std.memory` 提供智能指针的库，相当于`<memory>`
3. `std.regex`提供对正则表达式库的封装，相当于`<regex>`
4. `std.threading`提供对线程库的支持（已经于C++11中正式支持），相当于`<atomic>/<future>/<mutex>/<thread>/<shared_mutex>`
5. `std.core`包含了其它所有的标准库设施

### 潜在的争议？

作为一门有着30多年历史的语言，模块化机制的一个设计难点就是**保持和古老的include机制（本质上是代码的复制）兼容该如何实现**。
好在WG21经过漫长的讨论终于实现了起码在理论上完美的兼容 - 用户可以自由混用两者，只要不产生重复和链接问题即可。
Redit的cpp频道里面有人发起了一个[是否提供一种机制让用户强制在某一个模块中清理旧有的include模式的讨论](https://www.reddit.com/r/cpp/comments/agcw7d/c_modules_a_chance_to_clean_up_the_language/)，采用的思路正式类似Rust语言的版本指定的思路。

这个想法其实有很重要的现实意义，因此有很多自身CPP用户发表了自己的看法，大概标准定义成现在这个样子应该主要是两个方面的原因
1. 委员会中的代表有很多来自于大公司，这些公司有海量的遗留代码；因此出于**自身利益的考虑**他们不会选择一种不兼容的方案和自己过不去。
2. **社区分裂的风险**，这个其实从C++03诞生依赖就有人开始质疑，乃至于早期的D语言就是冲着这一点不满才决定选择单干，可惜有评论认为现在的D语言的分裂情况和C++比较起来完全是不遑多让的。

**这些问题其实都是很现实的问题**，个人觉得WG21选择向后兼容的思路并没有什么问题，因为从新发明轮子的时候都是简单的，真正**复杂的是如何长期稳定地维护和更新**。
C++的使用领域一直在缩小（或者有人说它是退回到了适合的领域）是个不争的事实，然而在适合的领域，它的优势不光在于语言本身还依赖于这些遗留系统的支撑。

## 协程支持
协程并不是一个新鲜的概念，甚至在现代编程语言出现之前很久就被提出出来，并在其它一些编程语言中被实现了很长时间了；它的基本思想是要求一个函数或者过程可以在执行过程中被操作系统或者调度器临时中止，然后在适合的时机（获取CPU计算资源等）再被恢复执行。详细的描述可以参考[这里](https://en.wikipedia.org/wiki/Coroutine)。

### 为什么需要协程
协程最明显的一个好处是允许我们书写**看起来顺序执行但是其实背后却异步执行的代码**，这样技能协调人大脑擅长顺序逻辑和计算机处理擅长异步执行的矛盾，兼顾效率和心智负担。
同时协程还可以支持惰性赋值和初始化的逻辑，进一步提高程序的运行效率（仅仅在需要的时候做运算）但是又不对程序员的大脑产生太多的额外负担。

协程是一个比进程和线程更轻量级一点的概念，具体实现上来说可以分为有栈协程和无栈协程；技术上来说前者可以通过第三方库实现就可以，但是性能开销比较大也容易出问题；而**无栈协程更加轻量级但是需要语言特性上做出改动**。

C++20引入的协程属于无栈协程。

### 基本语法定义
C++中的协程首先要是一个函数，它满足如下特性
1. 可以被中止，保存状态然后在合适的时机恢复执行 - 这也是协程的基本要求
2. 本身是不需要额外的栈的，即stackless
3. 当被暂时中止的时候，执行权被交回了调用者
4. 它的定义语法满足下述的特征

#### 协程函数语法和关键字
协程函数定义可以又如下一些特征：

##### `co_await`操作符等待另外一个协程的完成
比如如下的从网络读取数据并写回对方的echo代码，从逻辑上看循环内部的两行代码是顺序执行的，但是`co_awit`关键字却标明了逻辑上它是**通过”等待“另外一个协程完成**才继续往下执行的。
```cpp
task<> tcp_echo_server() {
  char data[1024];
  for (;;) {
    size_t n = co_await socket.async_read_some(buffer(data));
    co_await async_write(socket, buffer(data, n));
  }
}
```

##### `co_yield` 可以直接挂起当前的协程执行并返回一个值
比如下面的循环中，每次到`yield`操作的时候，当前的协程便被暂时中止执行并返回一个整数
```cpp
generator<int> iota(int n = 0) {
  while(true)
    co_yield n++;
}
```
这种用法在其它语言中也叫generator函数或者生成器。

##### `co_return`用于直接返回
```cpp
lazy<int> f() {
  co_return 7;
}
```

##### 返回类型要求
因为协程的返回值并不是普通的值而是一个可以和另外一个协程相互协作的对象，因此C++标准对协程的返回值有如下要求：
- 不能使用可变参数
- 不能使用普通的`return`语句
- 不能返回自动推导的类型，如`auto`或者`concept`类型等

同时如下的函数也不能是协程
- `constexpr`函数因为需要在编译器运算，不能是协程
- 构造函数和析构函数用于普通对象的构造，也不能被延迟执行进而不能是协程
- 主函数不能是协程，否则操作系统无从启动程序

### 协程的执行

任何一个协程其实由如下这些要素构成
1. **`promise`对象需要充当一个桥梁**，由协程内部改变其状态，将值提交给另外一方等待该promise的协程
2. **外部协程操控另外一个协作协程的句柄**，外部协程需要借助该句柄来决定是否挂起对方的协程或者将其协程帧销毁
3. 隐式的**协程状态对象**，该对象需要能够
> - 保存内嵌的promise对象
> - 用值拷贝方法传递的参数值对象 - 显然出于内存安全的考虑不能由引用或者指针
> - 当前执行到哪个阶段的状态标识，从而外部协程知道下一步是否应该迁移状态还是需要销毁帧数据
> - 其它一些生存期超越当前挂起点的局部变量

#### 协程执行的流程
当一个协程执行的时候，它的实际运行顺序如下
1. 使用`operator new`来分配上述的状态对象
2. **拷贝所有的函数参数**到这个对象中（因为协程本身也是个函数），值类型直接拷贝，如果由**引用或者指针类型，其安全性需要程序员自己保证有效性**，因为在协程中他们同样使引用和指针
3. 构造promise对象，这里先查找它**是否支持对应协程所有传入参数为参数签名的构造函数**并调用，如果没有则调用默认构造函数来构造
4. 调用`promise.get_return_object`函数，将结果放在一个局部变量中；这样**第一次协程被挂起的时候，该局部变量就可以被返回给调用者**。如果在第一次执行到挂起之前发生了异常，对应的结果都不会放置在promise中，而是通过该局部变量返回
5. 调用`promise.initial_suspsed`函数，并使用`co_await`等待它的结果。通常情况下，promise类型要么返回`suspend_always`,要么返回`suspend_never`;前者对应的使延迟启动的协程，后者则对应提前启动的协程
6. 当`co_await promise.initial_suspend`恢复的时候，协程的函数体才开始别执行

当该协程函数执行到一个挂起点，返回对象将会通过必要的类型转换返回给外部协程的等待方。

##### 返回
如果协程函数执行到一个`co_return`语句，则执行如下的操作
1. 如果返回类型是如下的几种，则调用`promise.return_void`
> - `co_return;`
> - `co_return expr;` 但是`expr`的类型其实是void
> - 直接跳过了可能的`co_return`语句而执行到了函数的结果；如果promise对象恰好定义了`Promise::return_void()`函数，那么**行为就是未定义**的，需要格外留意！
2. 否则将调用`promise.return_value(expr)`返回需要的类型
3. 销毁协程开始阶段创建的所有的局部自由变量，销毁的顺序和构造顺序相反
4. 调用`promise.final_suspend`函数，并等待其`co_awit`结果

##### 协程异常处理
如果协程中抛出了**未捕获异常**，它的行为如下
1. 捕获异常，并调用`promise.unhandled_exception`函数
2. 调用`promise.final_suspend`函数，并等待其`co_awit`结果

##### 状态对象的销毁
当协程状态对象因为`co_return`或者异常情况需要销毁的时候，其执行过程如下
1. 调用promise对象本身的析构函数
2. 调用协程参数对象的析构函数
3. 调用`operator delete`来完成对状态对象本身的销毁
4. 将执行权交回外部调用者

### 堆内存分配
协程的状态**必须要通过**`operator new`来分配，因为标准要求这里必须是无栈协程。分配过程遵循如下两条规则
1. 如果`Promise`类型定义了一个类级别的`operator new`，则**优先选择此分配方法**，否则会调用全局的`operator new`来完成内存分配
2. 如果`Promise`类型定义了自定义的`operator new`，并且**其函数签名和协程的函数参数签名一致，这些参数将会被一并传递**给该函数，这样的目的是为了和leading allocator convention的行为保持一致，即签名一致的分配器有优先权

#### 可能的分配优化
如果有办法事先确认协程状态对象的生存周期一定比调用方的生存周期短，并且该协程的帧大小在调用的时候可以明确得到。
该优化即使对用户自定义的内存分配器也可以使用。
这种情况下，**被调用的协程的栈帧其实是内嵌在了调用方的函数栈帧**中，就像一个迷你的内联函数调用一样。

### `Promise`类型
实际的promise类型则由**编译器根据实际协程声明中的签名类型**结合`std::corountine_traits`模板推到得出。

比如当一个协程的类型被定义为 `task<float> foo(std::string x, bool flag)`，那么编译器推导出来的类型为`std::coroutine_traits<task<float>, std::string, bool>::promise_type`。

如果协程被定义为非static的成员函数，比如`task<void> my_class::method1(int x) const`,对应的推导出来的Promise类型为`std::coroutine_traits<task<void>, const my_class&, int>::promise_type`,同时对象类型会被放置在第一个参数模板了行中。

### 编译器支持情况

Visual Studio是这个提案的主推者之一，所以早在2013年MSVC就提供了自己的协程实现；并且在[VS2017中正式将关键字向标准提案靠拢](https://devblogs.microsoft.com/cppblog/yield-keyword-to-become-co_yield-in-vs-2017/)。
Clang也提供了支持，尽管[其C++ Status页面](https://clang.llvm.org/cxx_status.html)显示的还是partial支持。
遗憾的是GCC的corountine支持还处于比较早期的阶段，目前仍然在一个[分支上开发](https://gcc.gnu.org/wiki/cxx-coroutines)。

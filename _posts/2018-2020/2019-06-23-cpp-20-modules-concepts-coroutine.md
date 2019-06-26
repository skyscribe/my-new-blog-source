---
layout: post
title: C++20 - 下一个大版本功能确定
comments: true
categories: [programming, cpp, language, programming]
tags: [programming,engineering, language, cpp]
---

C++20的功能特性已经于3月份冻结，显然**这次终于来了一波大的改进**，而不再是像之前C++14/C++17那般小打小闹的做小步快跑，尤其是三个讨论很久的大feature终于被合入主干；并且这些feature终将会极大地影响后续C++代码的书写方式。

<!--more-->

## 核心语言特性
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
**TBD**

## 协程支持
**TBD**

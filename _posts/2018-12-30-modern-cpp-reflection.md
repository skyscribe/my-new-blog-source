---
layout: post
title: 现代C++语言是否走上了歧路?
comments: true
categories: [programming, language, study, notes, cpp]
tags: [design, programming, language, cpp]
---
C++标准的演进和推出速度过慢一直被内外社区批评，甚至当第一个21世纪的语言标准终于于2011年发布的时候，标准委员会的人都自嘲说我们是一门计算机语言，所以0x需要用十六进制数表示。
进入了第二个十年之后，社区的老学究们终于决定引入时髦的＂发布火车＂的模型，滚动地推出新的功能，之后新的语言标准总算按照一个大版本一个小版本间杂而出的方式推到了程序员的面前。
只是社区中对于新的语言的发展方向一直有很多不同的声音，最近的一次大规模声讨则是起源于Aras P在[这篇名为哀悼现代C++](https://aras-p.info/blog/2018/12/28/Modern-C-Lamentations/)的博客文章。

<!--more-->

作者是一位来自Unity的游戏开发程序员，而一般意义上认为C++仍然具有顽强生命力的领域中，游戏开发是很大的一块地盘；因此这篇来自游戏社区的自己人的反思更加惹人注目；
难道是死忠粉都不愿意继续坚持自己的语言宗教信仰而要改换门庭另投明主了吗？仔细阅读了作者的文字，又发现并不是完全这样。

## 作者为什么要挑起这个话题

其实作者也是有感于最近的C++20语言标准正式纳入了ranges这个有名的第三方库而发（其实我个人第一次看到这个库的时候也是眼前一亮，可惜这么多年还在语言标准的门外徘徊）。
Ranges库的作者在自己的[博客中](http://ericniebler.com/2018/12/05/standard-ranges/)兴奋地告诉大家它的库重要要成为新的2020标准的一部分了，
并且**信心满满地给大家演示了一个具体的例子**来宣传这个新的特性是如何的激动人心。不料社区众人并不买账，
很多游戏开发圈子的人给出的确是大大的不喜欢，甚至Aras直接引述了他自己写的一篇twitter消息**将不满发泄到了整个现代的语言标准进化方向**上(
不知道Bjarne老爷子会不会气的跳起来，毕竟他在负责领导标准委员会中的语言演进方向工作小组)，
并直言不讳地说，负责制定语言标准的大佬们实在是走错了方向，没有真正关心一线开发人员的诉求；于是一石激起千层浪，
整个社区都充满了对语言标准委员会掌舵的演进方向的不满。

撇开很是主观的争论不说，技术方面看，Aras是否太偏激了？

## 作者的痛点

整篇文章写的比较长，也许是预料到很多人没有耐心仔细读完，作者在开头就简单概括了他所认为的现代语言标准的三宗罪
1. **编译时间**是个非常重要的话题，可惜标准委员会视若不见
1. 非优化模式即**调试模式**下的编译时间尤其恐怖，调试的需求被忽视
1. **心智负担**太重，要想轻松地写出显然正确的程序需要极其细心地推敲才行

### 原始的Ranges库的例子

既然讨论是因为Eric的Range库而发，作者先详细描述了这个用于计算毕达哥拉斯三元组（也就是勾股定理数）的例子，原文的例子比较长，但是核心的部分还是不难理解的。

首先是一些简单的匿名函数，因为原作者想用**函数式编程**的写法来演示他的新库；首先是一个使用了concept约束的`for_each`函数
```cpp
// "for_each" creates a new view by applying a
// transformation to each element in an input
// range, and flattening the resulting range of
// ranges.
// (This uses one syntax for constrained lambdas
// in C++20.)
inline constexpr auto for_each =
  []<Range R,
     Iterator I = iterator_t<R>,
     IndirectUnaryInvocable<I> Fun>(R&& r, Fun fun)
        requires Range<indirect_result_t<Fun, I>> {
      return std::forward<R>(r)
        | view::transform(std::move(fun))
        | view::join;
  };
```

然后还需要一个`yield_if`函数用于按照给定的条件，生成一个结果出来，还是典型的函数式编程的路子 (这里省略了`maybe_view`泛型的定义，可以去原文中翻查)
```cpp
// "yield_if" takes a bool and a value and
// returns a view of zero or one elements.
inline constexpr auto yield_if =
  []<Semiregular T>(bool b, T x) {
    return b ? maybe_view{std::move(x)}
             : maybe_view<T>{};
  };
```

主体部分则是一个用惰性方法求三元组的代码，输入是一个理论上无限长的整数序列，通过`for_each`和`yield_if`的组合调用，过滤出符合条件的所有的三元组，
最终在现实输出的时候，用新的标准库的`take`取前１０个元素打印输出。
```cpp
// Define an infinite range of all the
// Pythagorean triples:
using view::iota;
auto triples =
for_each(iota(1), [](int z) {
    return for_each(iota(1, z+1), [=](int x) {
    return for_each(iota(x, z+1), [=](int y) {
        return yield_if(x*x + y*y == z*z,
        make_tuple(x, y, z));
    });
    });
});

// Display the first 10 triples
for(auto triple : triples | view::take(10)) {
    cout << '('
        << get<0>(triple) << ','
        << get<1>(triple) << ','
        << get<2>(triple) << ')' << '\n';
}
```

如果熟悉函数式编程的常规范式，会发现这个是一个再简单不过的例子；只是示例代码的实际意义可能显得不大，
毕竟最老式的Ｃ风格代码也还是容易理解的
```cpp
void printNTriples(int n)
{
    int i = 0;
    for (int z = 1; ; ++z)
        for (int x = 1; x <= z; ++x)
            for (int y = x; y <= z; ++y)
                if (x*x + y*y == z*z) {
                    printf("%d, %d, %d\n", x, y, z);
                    if (++i == n)
                        return;
                }
}
```

当然处于代码可维护性的角度来考虑，新风格的现代C++代码要容易复用并且难出错的多，
因为代码的复杂度大大降低了，逻辑表达式的嵌套也被分散到了各个更小而又基本的组合函数上了。
不过这些因素不是Aras想要讨论的点。

### 编译时间
作者很快毕竟了一下两个版本的编译时间，毫无悬念，现代的C++版本完败，而且差距是相当惊人的。
没有用任何模板元泛型编程手段的老C++代码只需要64毫秒就可以编译完毕，得到的可执行文件只要8KB（当然肯定是动态链接了系统库了);
这个还是带调试模式的编译，如果用上所有的优化手段，则编译耗费了71毫秒，并且在１毫秒之内得到了100组输出。

而原例子中的现代的C++代码则需要用最新的C++17标准编译，在调试模式和正常模式下，
编译时间分别是2920毫秒和3020毫秒，运行时间则相差无几。所以作者得出的结论是，其他方面可能差不多，
性能也没有损失，编译时间却暴涨了几十倍。

作为一个对比的例子，作者拿出来了一个开源数据库SQLLite的编译时间作为对比，并发现用同样的硬件，
可以在**不到１秒钟的时间内完成SQLLite所有的２２万行Ｃ代码**的编译，这个差异还是很惊人的。
原因在哪里？无外乎是模板元和泛型的滥用导致庞大的头文件预处理和解析；这个问题在完整的模块化机制被支持之前，没有很好的解决办法。

回到当前状态的range库，第三个版本的代码**居然有180万行的代码全部包在头文件**中了，如果谁在自己的头文件中保护了这个库，编译的时候就真的会演变成一场灾难了。
这个抱怨和吐槽的确是稳准而狠，没法简单解决的。

### 调试模式的编译

吐槽完绝对的编译时间，作者又对调试模式下差不多的编译时间做起了批判，也许是游戏行业有很轻的需求来调试而没有很多的自动化测试吗？
另外一个第三方的例子来自于[Optiming OBJ Loader](https://www.youtube.com/watch?v=m1jAgV4ZOhQ)，里面给出的结论是，尽量**避免使用STL**，
赤裸裸地打脸标准委员会呀。

### 心智负担

这方面作者没有特别仔细的展开，似乎怨愤都集中在了上面的编译时间，并且在解密为什么编译时间那么长的时候，
顺带抱怨了模板元的滥用导致非常复杂的处理规则需要小心谨记。
当然C++编程语言复杂的多范式支持本身的确也会让人写起代码来畏首畏尾，这方面也没什么可说的。

社区会如何反应这方面的挑战，是说模块化的提案已经在加速讨论和演进吗，还是说constexpr的增强可以给编译器更多的指示信息来提高编译速度？
毕竟Herb Sutter一直在推动让现代的C++语言往更好使用的方向去走，对面Rust语言遇到的困难和获得的经验，
也许可以被社区的大佬们借鉴。
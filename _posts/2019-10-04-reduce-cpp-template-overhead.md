---
layout: post
title: reduce-cpp-template-overhead
comments: true
categories: [programming, cpp, language, design]
tags: [programming,cpp,design,language, engineering]
---

新的C++语言标准的演进越来越强调静态编译（即运行期计算）的优势，因为这一发展方向无疑更贴合现代的C++语言**Zero Cost Abstraction**的目标；
可惜这样做有个非常明显的副作用就是给编译器带来了巨大的挑战，乃至于稍微复杂一点的项目也动辄需要数个小时才能编译完成。

之前除了一些社区的抱怨
(参考这篇[反思文章]({{"/" | relative_url }}{% post_url 2018/2018-12-30-modern-cpp-reflection %}))
很少有正面解决问题的实用思路和技术出现。
好在最近结束的[CppCon2019大会](https://cppcon.org/cppcon-2019-program/)上，来自Google的软件工程师做了一个非常有趣而实用的如何优化编译器运行时模板元编程负担的演讲(原文链接在[这里](https://cppcon.org/cppcon-2019-program/))，

<!--more-->

由于官方[放出来的材料](https://github.com/CppCon/CppCon2019)里没有包含这一部分内容，本文不妨从作者的演讲视频中截取其要点详细探讨一番。

## 基本背景知识
模板元编程使现代C++编程的一个**至关重要却又极其烧脑**的子领域，传统的C++程序员要么使从C背景转换而来，
要么是出于**用不着就不需要了解”的心理有意绕开**这部分，导致讲述一些稍微复杂的技巧的时候，他们往往由于缺乏这些基本的背景知识而无法理解，进而给C++语言扣上了一个太复杂的帽子。

然而这些知识是理解优化编译技巧的基础，试想如果不能大概理解编译器如何来解析和生成模板元的代码，又怎么能发掘出有效的方法来做针对性的优化；
毕竟**知其所以然是深入理解一项技术的基本要求**，没有理解其机制而盲目地抱怨设计的差是很无知的。

Jorg从一个基本的例子来展开他的介绍的。

### 如何用模板元方法计算斐波那契数
斐波那契数列是映射自然增长规律的一个数学序列，用传统的非泛型方法求取它非常容易，用最简单的数学定义和现代的C++语言可以写为如下的形式
```cpp
constexpr int fib(int n) {
    return (n <= 1) ? n : fib(n-1) + fib(n-2);
}
// fib(30) = 832040
```
当然这个写法是编程入门教材中介绍的做法，虽然形式简单却不使用，因为输入的数字稍微大一点，就需要很长时间的运算才能计算出来，并且由于递归深度的限制，很容易遇到栈溢出的难题。
从编译器优化的角度来看，**非尾递归形式的递归函数调用需要和递归深度相同的嵌套的栈帧**，很容易触碰到栈空间的上限而引起程序崩溃。

引用模板元泛型的写法，我们可以用一个结构体加运行期计算的变量的方式来表达同样的算法
```cpp
template <int n>
struct Fib{
    static constexpr int value = Fib<n-1>::value + Fib<n-2>::value;
};

///This is unfortunately required for C++11/14
template<>
struct Fib<1>{
    static constexpr int value = 1;
};

template <>
struct Fib<0>{
    static constexpr  int value = 0;
};
```
这个例子描述了基本的泛型算法特点
- 将**动态运算的变量当作模板参数**，并且递归地调用自身，但是模板参数的数值在缩小
- 对于模板中止条件，采用**模板特化的方法，具体实现之**，这里即对0/1直接返回

#### C++17新语法

上述的写法虽然不失清晰，但是却略微显臃肿，相比于最初的非模板写法，需要引入三个不同的结构体定义。
C++17新引入的`constexpr if`语法则轻而易举地去掉了这些臃肿，我们可以通过在if表达式中加入`constexr`修饰来**告知编译器对应的部分可以在模板元编程实例化的时候来参与**判断
```cpp
template <int n>
struct FibConstexprIf{
    static constexpr int value = [](){
        if constexpr (n > 1) {
            return FibConstexprIf<n-1>::value + FibConstexprIf<n-2>::value;
        } else {
            return n;
        }
    }();
};

//FibConstexprIf<30>::value
```
附带的一个小技巧是，通过显示地调用传入的lambda表达式来给一个static变量赋值，因为函数内部的操作全部是经由编译器运算完成的，最终效果是编译器可以无缝地将在编译器计算完成。

这个简单的算法逻辑上的操作大概是这样
1. 根据传入的参数30,生成FibConstexprIf<30>::value
2. 生成一个临时的lambda函数，并即使调用，在函数执行中
    1. 比较模板参数值，如果小于等于１，则直接返回自身
    2. 否则需要实例化两个新的结构(**递归步骤**),这里对应是FibConstexprIf<29>和FibConstexprIf<28>
    3. 计算求得两者的value变量，然后相加返回

显然这个算法也是递归的，这里我们需要关心模板元函数生成过程中，第二步里面的第二小步是否会生成**结构体定义**，引起目标可执行文件的膨胀。
我们可以用如下的命令来检验之
```bash
nm <exe> | c++filt | grep FibConstexprIf
```
好在编译器比较完美地去掉了所有这些中间符号，给我们返回了实际计算得到的值。看起来因为这里的结构体是空结构并且只有一个静态变量声明，编译器可以**自动记忆对应的中间计算的模板元值**，
并且可以**自动地内联lambda函数的定义**。

#### 另一种形式的探索
如果我们想采用一种更直接的方式，直接让`constexpr if`这一特性应用到参数上，而不适用结构体的静态成员这一常见的模板元技巧而直接采用普通函数定义，可以得到如下的形式
```cpp
template <int N>
constexpr auto fib() {
    if constexpr (N>1) {
        return fib<N-1>() + fib<N-2>();
    } else {
        return N;
    }
}
/// fib<30>()
```
这一形式能工作的原因是我们用递归调用自身的方式加上 `constexpr if`判断来终止递归，思路和最初的运行期动态计算的思路一样。
可惜当我们再次检查生成的目标可执行文件的时候，发现**可执行文件明显增大**了不少。

`nm`命令查看生成的符号表（当然需要用c++filt做demangle处理）会找到如下的定义
```shell
0000000000000d15 W auto fib<30>()
0000000000000cf7 W auto fib<29>()
0000000000000cd9 W auto fib<28>()
0000000000000cbb W auto fib<27>()
0000000000000c9d W auto fib<26>()
0000000000000c7f W auto fib<25>()
0000000000000c61 W auto fib<24>()
0000000000000c43 W auto fib<23>()
0000000000000c25 W auto fib<22>()
0000000000000c07 W auto fib<21>()
0000000000000be9 W auto fib<20>()
0000000000000bcb W auto fib<19>()
0000000000000bad W auto fib<18>()
0000000000000b8f W auto fib<17>()
0000000000000b71 W auto fib<16>()
0000000000000b53 W auto fib<15>()
0000000000000b35 W auto fib<14>()
0000000000000b17 W auto fib<13>()
0000000000000af9 W auto fib<12>()
0000000000000adb W auto fib<11>()
0000000000000abd W auto fib<10>()
0000000000000a9f W auto fib<9>()
0000000000000a81 W auto fib<8>()
0000000000000a63 W auto fib<7>()
0000000000000a45 W auto fib<6>()
0000000000000a27 W auto fib<5>()
0000000000000a09 W auto fib<4>()
00000000000009eb W auto fib<3>()
00000000000009cd W auto fib<2>()
00000000000009c2 W auto fib<0>()
00000000000009b7 W auto fib<1>()
```

显然这种用法虽然看起来更加自然，**编译器却无法自动地替我们内联这些中间状态**的函数。

### 新的语言特性和函数签名
从签名的例子输出我们发现这些中间函数的签名中间多了个`auto`,从编译器实现角度来看，C++11扩展用法中的`auto`关键字要求编译器自己做类型推到，而目前的**编译器实现却毫不犹豫地将auto加到了函数签名前面**。

更一般地看，如下的代码
```cpp
template <typename T>
string add1(T a, T b){
    return a + b;
}


template <typename T>
auto add2(T a, T b) {
    return a + b;
}

int main() {
    cout << add1(string{"hello}, string{"world\n"});
    cout << add2(string{"hello"}, string{"world\n"});
}
```
两个地方编译器为我们实例化的函数签名如下(可以很方便地从nm命令输出来检查)
1. `std::string add1(std::string, std::string)`
2. `auto add2<std::string>(std::string, std::string)`

#### SFINAE和`enable_if_t`
模板元编程会大量地使用SFINAE技巧来利用编译器选择函数重载过程中遇到不匹配的重载会跳过寻找下一个可能的匹配这一事实，来提供多个针对不同类型的模板实现，提供编译器的分支运算。
较新的C++标准则显示地引入了`enable_if`和`enable_if_t`来简化编写SFINAE函数的负担。

可惜的时候**这种简化并不是特别彻底，起码函数的签名中还是带上了**这些痕迹；考虑如下的例子:

对于一个普通的模板元函数处理
```cpp
template <typename T>
T process(T a) {
    cout << "processing " << a << endl;
    return a;
}
```
如果我们想加上类型判断，让对应的处理仅仅使用于非空类型，那么可以写作
```cpp
template <typename T>
typename std::enable_if_t<!std::is_same<T, void>::value, T> processIf(T a) {
    cout << "processingif " << a << endl;
    return a;
}
```

更进一步地，我们可以使用`static_assert`技巧来做类型检查，但是不显示地使用SFINAE,则可以写作
```cpp
template <typename T>
auto processIf1(T a) {
    static_assert(!std::is_same<T, void>::value);
    cout << "processWithAssert..." << a << endl;
    return a;
}
```
如果我们触发上面的函数调用，然后就可以检查编译器实际生成的函数签名
```
int main(){
    auto test = string{"test"};
    process(test);

    processIf(test);
    processIf1(test);
}
```
对于同样的逻辑实现，编译器生成的函数调用分别是
1. 第一个版本对应的为`std::string process(std::string)`
2. 第二个版本对应的签名比较复杂，是`std::enable_if<!std::is_same<std::string, void>::value, std::string>::type processIf(std::string)`
3. 第三个版本和第一个版本相比，仅仅加上了`auto`的前缀和类型信息：`auto processIf<std::string>(std::string)`

因此我们应该多利用新的语言特性，优先选择用`static_assert`做类型判断，而逐渐减少使用`std::enable_if_t`类型。


##　优化一个自定义的Tuple类
`Tuple`数据结构是一个非常简单的数据聚集结构，很多讲述现代模板元编程的书籍文章都会拿它做举例；在现代的编程语言中也非常常见；
其概念非常容易理解，一个`tuple`就是一堆有序的类型各异的数据的集合，我们可以用索引的方式访问某个元素，对它进行赋值。

因为元素的类型可能各不相同，因此这是一个非常适合用模板元编程技巧来表达的数据结构的例子，当然另外一个因素是我们想看下怎么样能简化它的开销，包括运行时的开销和**编译器生成目标代码时候中间数据表达**的开销。

###　最简单的形式
实现tuple最简单的形式是采用递归包含的方式来定义，代码如下
```cpp
template <typename... T> class MyTuple;

template <typename T>
class MyTuple<T> {
    T first;
    template <size_t  N, typename TN> friend class Getter;
};

template <typename T, typename... More>
class MyTuple<T, More...>{
    T first;
    MyTuple<More...> more;

    template<size_t  N, typename TN> friend class Getter;
};
```
这里我们先定义了一个泛化版本的携带任意多个参数的类型，然后再逐步特化
1. 带有一个类型参数的类，仅仅只有一个唯一的元素，并明明为`first`
2. 携带两个以上参数的类型，它除了第一一个`firt`外，还内嵌了一个剩余元素的数据成员，注意两者的类型不相同
3. 我们需要一个辅助的类来访问特定的元素实现`get`操作

#### `Getter`实现
使用类似的技巧，辅助类可以实现如下
```cpp
template<size_t N, typename TN>
struct Getter{
    template<typename T0, typename... MoreN>
    static TN& get(MyTuple<T0, MoreN...>&tup) {
        return Getter<N - 1, TN>::get(tup.more);
    }
};

template<typename T0>
struct Getter<0, T0>{
    template<typename... Ts>
    static T0& get(MyTuple<Ts...>& tup) {
        return tup.first;
    }
};
```
它的两个模板参数第一个制定对应的下表索引，第二个则对应该下标对应的元素类型。
泛化的实现采用**递归的思路依次减小下标，并访问内嵌的第二个元素对应的tuple**,而特化的版本则对应为下标是0的情况，此时访问的其实是最后一个元素，直接返回对应的`first`元素即可。

##### 实现一个`get`函数
显然上面的`Getter`类很不好用，因为我们需要每次都指定下标对应的类型，这样非常容易出错并且出错的时候，编译器给出的**诊断信息明显不足或者不好理解**。
为了摆脱这种窘境，我们可以采用稍微有点作弊的手法定义一个封装函数
```cpp
template <size_t N, typename... More>
auto get(MyTuple<More...>& tup) -> typename std::tuple_element<N, std::tuple<More...>>::type& {
    return Getter<N, typename std::tuple_element<N, std::tuple<More...>>::type>::get(tup);
}
```
这里之所以说有点作弊，是因为我们无法得知具体的参数N对应的是什么类型，不得已我们必须借助标准库提供的类型定义，用`std::tuple_element<N, std::tuple<More...>>::type`的方式来计算返回值的类型。

#### 代码生成的开销
当然这个不是我们最终的目标，因为稍微检查一下就会发现这个实现会引起编译器代码生成上的急剧膨胀。
假设我们有如下的例子
```cpp
void testMyTuple() {
    using T = MyTuple<bool, string, int, char, double>;
    T tup;
    get<0>(tup) = true;
    get<1>(tup) = "somthing";
    get<2>(tup) = 1;
    get<3>(tup) = 'c';
    get<4>(tup) = 12.34;
}
```
检查符号表会发现，简单的`get<4>`操作会引起连锁反应，生成如下的符号
```bash
➜  cmake-build-debug git:(master) ✗ nm myTuple|c++filt | grep MyTuple | grep Getter | cut -d " " -f2-100 | sed "s/__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >/string/gp" | uniq    
W bool& Getter<0ul, bool>::get<bool, std::string, int, char, double>(MyTuple<bool, std::string, int, char, double>&)
W char& Getter<0ul, char>::get<char, double>(MyTuple<char, double>&)
W double& Getter<0ul, double>::get<double>(MyTuple<double>&)
W int& Getter<0ul, int>::get<int, char, double>(MyTuple<int, char, double>&)
W std::string& Getter<0ul, std::string >::get<std::string, int, char, double>(MyTuple<std::string, int, char, double>&)
W char& Getter<1ul, char>::get<int, char, double>(MyTuple<int, char, double>&)
W double& Getter<1ul, double>::get<char, double>(MyTuple<char, double>&)
W int& Getter<1ul, int>::get<std::string, int, char, double>(MyTuple<std::string, int, char, double>&)
W std::string& Getter<1ul, std::string >::get<bool, std::string, int, char, double>(MyTuple<bool, std::string, int, char, double>&)
W char& Getter<2ul, char>::get<std::string, int, char, double>(MyTuple<std::string, int, char, double>&)
W double& Getter<2ul, double>::get<int, char, double>(MyTuple<int, char, double>&)
W int& Getter<2ul, int>::get<bool, std::string, int, char, double>(MyTuple<bool, std::string, int, char, double>&)
W char& Getter<3ul, char>::get<bool, std::string, int, char, double>(MyTuple<bool, std::string, int, char, double>&)
W double& Getter<3ul, double>::get<std::string, int, char, double>(MyTuple<std::string, int, char, double>&)
W double& Getter<4ul, double>::get<bool, std::string, int, char, double>(MyTuple<bool, std::string, int, char, double>&)
```
显然这里单独是Getter函数就生成了15个之多，因为随着每次N的减小，新的Tuple总是会被生成。
考虑到目标文件中留下的仅仅是唯一类型的函数定义，中间重复的计算并没有包含在内。
对于每一个给定N个类型的Tuple，它同时**还需要生成所有小于N的子Tuple类型内嵌于自身中**，实际编译器生成的算法的**时间复杂度其实是三次幂指数**O(N^3)的。

### 采用继承的方式优化
一种简单的思路是采用继承的方法来优化这个定义，避免嵌套。这里我们定义一个用于被具体类型继承的基类如下；
为防止其`get`函数被调用，我们将其声明为`=delete`:
```cpp
template <typename... T>
class NewTuple {
    //serves as a base type only
    public:
        void get(...) = delete;
};
```
这里的基类其实是一个空元素的Tuple，它本身其实并没有任何意义。

#### 逐级继承
基于此，我们让每一个更复杂的Tuple类型从比它少了头部元素的一个维度更低的tuple上去继承。
```cpp
template <typename T, typename... More>
class NewTuple<T, More...> : NewTuple<More...> {
    T first;

public:
    using NewTuple<More...>::get;
    auto& get(std::integral_constant<size_t, sizeof...(More)>) {return first;}
};
```

##### `get`成员函数的实现

为了实现`get`函数对不同的索引取调用不同的类型，我们需要根据索引值触发对应的类型定义。
好在`std::integral_constant`类型提供了**将任何一个类型的某个具体值转换成一个唯一的编译器可以识别的类型的能力**，即
- `std::integral_constant<size_t, 1>`表述的是某个特殊的总是取值为1的编译器可识别的唯一类型
- `std::integral_constant<size_t, 2>`表述的是取值为2的唯一类型
- `std::integral_constant<size_t, 3>`表示的是另外一个类型，它和算数加法没有任何关联

我们可以利用它将每个整数索引变换成一个具体的类型从而来书写`get`函数
```cpp
    using NewTuple<More...>::get;
    auto& get(std::integral_constant<size_t, sizeof...(More)>) {return first;}
```
它的定义依赖于下面的技巧
- 当目前的tuple具有N个元素的时候，其直接基类就有N-1个元素
- 当前类的`get(type<N-1> t)`返回的是**当前tuple的头部**元素，而**比`N-1`更小的索引的`get`函数则由这些基类来提供**的
- 而所有的基类的`get`函数具有不同的参数类型，并且都可以被子类所访问，因为他们都被声明为是`public`的

这里的`use`声明引入了比当前类型少一个元素的直接基类的类型声明。

##### 实现全局的`get`函数
全局函数的实现可以依赖上面的具体类中的`get`函数的组合来实现：
```cpp
template<size_t N, typename... More>
auto& get(NewTuple<More...>& tup) {
    return tup.get(std::integral_constant<size_t, sizeof...(More) - 1 - N>{});
}
```
即对当前有M个元素的tuple取第N个元素，等价于访问继承体系中的**参数类型的整数值为`M-1-N`的**某个基类的成员函数的`get`的访问。
这里起关键作用的是`std::integral_constant`模板。

#### 代码膨胀开销
考虑下面的例子
```cpp
void testNewTuple() {
    using T = NewTuple<bool, string, int, char, double>;
    T tup;
    get<0>(tup) = true;
    get<1>(tup) = "somthing";
    get<2>(tup) = 1;
    get<3>(tup) = 'c';
    get<4>(tup) = 12.34;
}
```
同样检查编译器的生成代码符号可以发现此时的**算法简化为二次幂**O(N^2)了，因为每一次初始化`get`操作我们仍然需要为它生成每一个基类的`get`函数访问。

#### 再看继承手法
表面上看这里的**模板元定义采用了继承的语法**，但是这里的继承功能其实是**用的组合而非传统面向对象设计中的继承**技巧，因为没有任何的虚函数，也没有任何的方法重载。
唯一有一点可能迷人眼的其实是`get`的声明语法，它其实是**通过数值转类型的方法，将每个基类中都定义了一个独一无二的函数**，然后交给编译器来根据不同的值选择对应的定义。

如果我们去掉这个继承的障眼法，将层层递进的继承结构给拍平变成完全只有一级的继承，是否可以进一步优化减小编译器的负担呢？

### 采用多继承和浅继承
采用这种新的思路，我们需要将单个元素的tuple重新定义为一个叶子节点，然后让复杂的多元素类从它继承而来；即如下的定义

```cpp
/// Using leaf
template <std::size_t Index, typename T>
struct MyTupleLeaf{
    T value_;
    template <typename... More>
    constexpr MyTupleLeaf(More&&... arg) : value_(std::forward<More>(arg)...) {}
};
```

进而我们就可以采用多继承的思路，这里需要使用可变模板的省略技巧
```cpp
template <typename... T> class NNTuple: MyTupleLeaf<T>... {
public:
    template<typename... More>
    explict constexpr NNTuple(More&&... args) : MyTupleLeaf<T>(std::forward<More>(args)...) {}

    template <size_t I>
    auto &get() {/*??? */}
}
```
然后我们发现这里的get没有办法定义，因为基类里面并没有提供这一方法，而由于多继承的关系，我们也不知道如何派发到哪一个叶子节点。

显然我们需要将**索引信息传递到具体的基类里面，然后再在组合类里面静态派发**。

#### 添加索引
可以通过在叶子节点里面添加一个索引类型
```cpp
template <std::size_t Index, typename T>
struct MyTupleLeaf{
    T value_;
    T& get(std::integral_constant<size_t, Index>) {return value_;}
};
```
这里具体叶子节点实现的get方法其实是返回自己的值定义。

而子类的具体实现需要合理的派发这个索引：
```cpp
template <typename... Ts> class NNTuple;

template <std::size_t... Indexes, typename... Ts>
class NNTuple<std::index_sequence<Indexes...>, Ts...>: public MyTupleLeaf<Indexes, Ts>...  {
    // This is C++17 only
    using MyTupleLeaf<Indexes, Ts>::get...;

public:
    template <size_t I>
    auto& get() {
        return get(std::integral_constant<size_t, I>{});
    };
}
```
和上面的深度继承类似，我们只需要将所有基类的`get`函数引入进来，然后用同样的`integral_constant`方法来做类型派发就可以了。

#### 封装索引
上面的类型定义中带了两个参数，第一个参数是一个索引列表，第二个参数则是实际的类型列表，两者需要保持对应关系(务必要保证**个数和位置是逐一对应的，否则会产生恐怖的编译错误**)，使用起来有些不方便，比如我们需要用如下的代码来操作
```cpp
NNTuple<std::index_sequence<0, 1, 2>, char, short, int> tuple;
short& first = tuple.get<1>();
```

好在我们可以封装它为一个新的类型
```cpp
template <typename... T>
class NNTuple : NNTuple<std::make_index_sequence<sizeof...(T)>, T...> {};
```

这样我们就可以简化实际的代码书写
```cpp
NNTuple<char, short, int> tuple;
short& first = tuple.get<1>();
```

#### 进一步简化来分离声明和实现
我们可以使用using声明来分离这些index相关的实现细节，然后重命名具体的实现类
```cpp
template <typename... Ts> class NNTupleImpl;
template <std::size_t... Indexes, typename... Ts>
class NNTupleImpl<std::index_sequence<Indexes...>, Ts...>: public MyTupleLeaf<Indexes, Ts>...  {
    using MyTupleLeaf<Indexes, Ts>::get...;
public:
    template <size_t I>
    auto& get() {
        return get(std::integral_constant<size_t, I>{});
    };
};

template <typename... Ts>
using NNTuple = NNTupleImpl<std::make_index_sequence<sizeof...(Ts)>, Ts...>;
```
由此使用者看到的Tuple声明就是一个只有目标类型接口序列的类，而具体的索引相关的操作就被隐藏了。

#### C++14的考虑
上述的代码中用到了C++17才支持的语法，如果我们要考虑C++14的兼容，可以将`get`写作如下的形式
```cpp
    //C++14 version of get
    template <size_t I, typename FType>
    auto& get14() {
        MyTupleLeaf<I, FType>& leaf = *this;
        return leaf.get(std::integral_constant<size_t, I>{});
    }
```

甚至可以进一步来简化具体的类型，因为我们可以用传入参数的方法来自动推到之
```cpp
    template <size_t I, typename FType>
    auto& getLeaf(MyTupleLeaf<I, FType>* leaf) {return *leaf;}

    template <size_t I>
    auto& get14_1() {
        auto& leaf = getLeaf<I>(this);
        return leaf.get(std::integral_constant<size_t, I>{});
    }
```
因为this指针的类型是确定的，那么根据具体的索引值的不同，我们**可以通过`auto`推到得到具体的叶子节点的类型**，
再调用get就显然去掉了对外暴露的`get14_1()`函数声明上的一个类型参数的依赖。

#### 去掉子类中的`get`
我们甚至可以去掉叶子节点中的get定义，而直接返回`value_`，即简化为
```cpp
template <size_t I, typename T> struct MyTupleLeaf {T value_};

//.... inside NNTupleImpl
public:
    template <size_t I>
    auto& get() {
        return get_leaf<I>(this).value_;
    }
```

### 代码生成情况
对于第三个版本，假设我们由如下的代码
```cpp
void testNNTuple(){
    using T = NNTuple<bool, string, int, char, double>;
    T t;
    t.get<0>() = true;
    t.get<1>() = "something";
    t.get<2>() = 1;
    t.get14<3, char>() = 'c';
    t.get14_1<4>() = 12.34;
}
```
对应的符号表则如下
```
➜  cmake-build-debug git:(master) ✗ nm myTuple|c++filt | grep NNTuple | cut -d " " -f2-100 | sed "s/__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >/string/gp" | uniq 
T testNNTuple()
W auto& NNTupleImpl<std::integer_sequence<unsigned long, 0ul, 1ul, 2ul, 3ul, 4ul>, bool, std::string, int, char, double>::get<0ul>()
W auto& NNTupleImpl<std::integer_sequence<unsigned long, 0ul, 1ul, 2ul, 3ul, 4ul>, bool, std::string, int, char, double>::get<1ul>()
W auto& NNTupleImpl<std::integer_sequence<unsigned long, 0ul, 1ul, 2ul, 3ul, 4ul>, bool, std::string, int, char, double>::get<2ul>()
W auto& NNTupleImpl<std::integer_sequence<unsigned long, 0ul, 1ul, 2ul, 3ul, 4ul>, bool, std::string, int, char, double>::get14<3ul, char>()
W auto& NNTupleImpl<std::integer_sequence<unsigned long, 0ul, 1ul, 2ul, 3ul, 4ul>, bool, std::string, int, char, double>::get14_1<4ul>()
W auto& NNTupleImpl<std::integer_sequence<unsigned long, 0ul, 1ul, 2ul, 3ul, 4ul>, bool, std::string, int, char, double>::getLeaf<4ul, double>(MyTupleLeaf<4ul, double>*)
W NNTupleImpl<std::integer_sequence<unsigned long, 0ul, 1ul, 2ul, 3ul, 4ul>, bool, std::string, int, char, double>::NNTupleImpl()
W NNTupleImpl<std::integer_sequence<unsigned long, 0ul, 1ul, 2ul, 3ul, 4ul>, bool, std::string, int, char, double>::~NNTupleImpl()
```
显然对每一个索引，它仅仅生成了一个唯一的get函数，其**复杂度终于降低到了最低**的O(N)。

## 总结
模板元和泛型编程是**现代C++语言演进的一个重要方向**，它用**几乎为零的代价实现了高层的逻辑抽象**，不幸的是程序员必须付出额外的编译时间较长的代价来达成这一目标，这不得不说是另外一种形式的权衡。
Jorg通过他探索的技巧来降低编译时间的负担，我们可以借助这些技巧来有针对性地优化模板元代码，让编译器减少生成不必要的代码，从而在编译期也实现零成本抽象的目标。

同时需要留意的是，这些例子中呈现的行为可能并不适用于所有的C++编译器，有些签名特征甚至是GCC/Clang的实现而MSVC编译器看起来有完全不同的行为。
也许随着编译器的逐步完善和演化，我们会不再需要这些技巧；然而**演练这些技巧对提高我们对静态语言编译期元编程的认识**也有莫大的好处。

Rust语言是另外一个秉承零成本抽象使命的静态编译语言，而今天它的泛型代码编译也承受着类似的痛苦和不便，只是不知道是否Rust社区有人发掘过类似的技巧来提升编译速度和减小模板文件体积?
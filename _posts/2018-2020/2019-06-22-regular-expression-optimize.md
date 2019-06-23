---
layout: post
title: C++正则表达式比Python慢？
comments: true
categories: [programming, cpp, language, programming]
tags: [programming,engineering, performance, cpp, python, regex]
---

[C++17](https://en.wikipedia.org/wiki/C%2B%2B17) 推出已经有将近3年的时间，速度快一些的公司才慢慢采纳了6年前的C++14进入正式的生产环境。
程序员们的一个大福利就是终于不用外挂第三方库（无论是准标准的boost还是其它形形色色的其它第三方库）就可以直接方便地使用标准库自带的`<regex>`来写代码；
这显然比直接使用原始而笨拙的`string::find_first_of`外加一堆边界检查和判断**要轻松和干净**很多。

然而[stackoverflow上](https://stackoverflow.com/questions/14205096/c11-regex-slower-than-python)的一位**注重性能**的[程序员](https://stackoverflow.com/users/239007/locojay)**很快就发现有些地方不对：好像这个新的regex标准库API是长的漂亮了不少，但是性能似乎和C++的“零成本的抽象”格格不入？

## Python版本比C++快很多？

locojay给出来一个非常简单的使用正则表达式来分隔字符串的代码，并且比较了C++版本和Python版本的运行时间（自然Python语言在C++社区特别受欢迎不是空穴来风），发现C++版本慢的不可理喻。

C++代码比较简单：
```cpp
#include <regex>
#include <vector>
#include <string>

std::vector<std::string> split(const std::string &s, const std::regex &r)
{
    return {
        std::sregex_token_iterator(s.begin(), s.end(), r, -1),
        std::sregex_token_iterator()
    };
}

int main()
{
    const std::regex r(" +");
    for(auto i=0; i < 1000000; ++i)
       split("a b c", r);
    return 0;
}
```

刨去`main`函数种的调用，实现的代码是典型的使用迭代器来生成新的split子迭代器，并构造为`vector<string>`返回。

对应的python版本如下
```python
import re
for i in range(1000000):
    re.split(' +', 'a b c')
```

运行的结果却让人大跌眼镜，即使在使用了最大可能的优化选项-O3（C/C++**性能很好的重要来源之一就是几十年间积累的形形色色的编译器后端优化能力**）之后，两者居然**相差了50%之多并且是C++版本慢**！

注：下面的结果是我自己虚拟机里面跑出来的结果，**原问题回答者的结果似乎还要慢了不少几乎是Python版本的运行时间的两倍**，也许是因为我的机器里面的GCC版本已经比较新了的原因。

```shell
> time python3 test.py  
real	0m2.194s
user	0m2.117s
sys	    0m0.004s

> time ./test
real	0m3.179s
user	0m3.152s
sys	    0m0.012s
```

第一反应当然会质疑这个结果，但是计算机又不会撒谎，所以一定是代码实现的问题，要么是标准库的regex实现有问题，要么是代码写的有问题？

## 优化掉无意义的临时对象构造和析构
显然这里有一个**无意义的临时string/vector对象的构造和析构**，这些过程都会伴随着内存的分配和释放，而Python版本由于其内在的string优化压根就没有这些开销。

去掉这个构造并尽力复用对象而不改变程序的语义后，代码会成为这样
```cpp

#include <regex>
#include <vector>
#include <string>

void split(const std::string &s, const std::regex &r, std::vector<std::string> &v) {
    auto rit = std::sregex_token_iterator(s.begin(), s.end(), r, -1);
    auto rend = std::sregex_token_iterator();
    v.clear();
    while(rit != rend){
        v.push_back(*rit);
        ++rit;
    }
}

int main() {
    const std::regex r(" +");
    std::vector<std::string> v;
    for(auto i=0; i < 1000000; ++i)
       split("a b c", r, v);
    return 0;
}
```

通过将需要使用的字符串和vector挪到循环的外部，运行之后，性能提高了一倍多！稍微麻烦一点的时候，内部的代码因为需要清理容器的内容而变得不太干净了。

```shell
real    0m1.254s
user    0m1.238s
sys     0m0.012s
```

原答题者的版本里，他的结果还是比Python版本要慢，而我本地机器的运行结果已经超出了python版本不少了！一个自然而然的想法是我们释放可以做的更好？

## 彻底去掉源字符串的内存分配和释放

注意到这里的第一个参数每次都是同一个，我们根本就不需要将一个`const char*`在传递参数的时候构造出来一个新的string对象，直接传递这个指针就可以，代码几乎不需要改动
```cpp
void split(const char *s, const std::regex &r, std::vector<std::string> &v) {
    auto rit = std::cregex_token_iterator(s, s + std::strlen(s), r, -1);
    auto rend = std::cregex_token_iterator();
    v.clear();
    while(rit != rend) {
        v.push_back(*rit);
        ++rit;
    }
}
```

不幸的是这样做的结果并不比前一个版本的更快，显然这里的无效字符串已经被编译器给优化掉了。

## 使用string_view
由于注意到这里的结果里面并不需要分配新的字符串，如果我们可以使用C++17,标准库里面已经有一个新的速度更快的`string_view`类，不会引起任何的内存分配和释放。

使用`string_view`版本的代码如下
```cpp
#include <regex>
#include <vector>
#include <string>
#include <string_view>

void split(const char* s, const std::regex &r, std::vector<std::string_view> &v) {
    auto rit = std::cregex_token_iterator(s, s + std::strlen(s), r, -1);
    auto rend = std::cregex_token_iterator();
    v.clear();
    while(rit != rend) {
        v.emplace_back(rit->first, rit->second - rit->first);
        ++rit;
    }
}

int main() {
    const std::regex r(" +");
    std::vector<std::string_view> v;
    for(auto i=0; i < 1000000; ++i)
       split("a b c", r, v);
    return 0;
}
```
可惜这个版本的性能和`-O3`的前面版本并没有太大的差异。显然性能问题从来就没有想当然而然的部分，必须依照测试情况是实验结果来分析。
另外一个显然的教训是，**内存分配的代价在很多时候比想象的要大很多**！

当然这是一个过分简化了的例子，实际的代码往往比这个要复杂很多，这里的性能结果和结论往往不能适用实际的复杂场景，因为每一个程序的运行环境和执行情况可能都是独一无二的，必须一句实际情况来分析。

更复杂的情况，这里的计算时间的方法显然也太过原始了；可以借助强大的profile工具，生成perf可以解析的数据，从而进一步生成[火焰图](https://github.com/brendangregg/FlameGraphhttps://github.com/brendangregg/FlameGraph)或者直接查看`perf report`的命令行界面来分析性能的热点在哪里。

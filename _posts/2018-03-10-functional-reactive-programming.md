---
layout: post
title: 函数式反应式编程的崛起
comments: true
categories: [design, programming]
tags: [design, architecture, programming, concurrency, microservice]
---

函数式编程早在几十年前就被提出出来却一直处于不温不火的境地，直到最近几年并发编程遇到了极大的挑战的情况下才重新引起人们的关注。
流式编程或者响应式编程则是另外一个不断进入我们视线的设计概念；微服务架构的兴起则为两者的结合提供了更好的舞台，毕竟**无状态是微服务的潜在要求**也是最重要的一个要求之一。
本文尝试深度探讨一下这一新的编程范式。

<!--more-->

## 基本概念
首先从名字上看，函数式流式编程是一种复合的编程范式，可以简单看作是函数式编程加上流式编程。

### 函数式编程
函数式编程的概念并不新鲜，它的基本思路是上**程序的执行看作是一堆函数的组合处理和求值**过程；纯粹的函数式编程要求数据是不可变的，
同样的数值输入在流经同样的函数处理的时候必须得到确定的输出，不容许有预料之外的副作用产生。程序员的任务可以想象为两个过程

1. 声明运算过程所需要的函数及其组合算法；程序的主要逻辑是组合这些函数算法来完成运算
2. 实现这些函数的内部逻辑实现，这个过程内部仍然是以声明式的写法为主

由于声明式的代码更接近于实际的问题领域逻辑，一个明显的好处是函数式代码具有很高的可读性和可维护性。

### 流及反应式抽象

流的抽象在计算机编程语言和基础计算机技术中非常常见。比如C++语言的早期STL标准库就提出了IO流的概念，它将输入输出设备进行抽象，
外部用户仅仅需要关心自己的数据可以写入流中或者从流中读取，具体怎么实现底层的输入输出控制的细节则被标准IO流库所封装和隐藏。
TCP协议的设计是另外一个例子，逻辑上看TCP服务的提供者和使用者之间在通信之前需要先建立一个虚拟的数据流，
然后发送方可以**按照严格而固定的顺序**将数据写入这个数据流中，对方则可以保证按照发送发的写入顺序读取到数据。
这里一个明显的共同特征是，流用于表述一种允许**生产者顺序往后追加，消费者可以依据同样顺序读取出数据**的逻辑抽象通道。

其实 Unix 的管道也满足类似的特征，管道的输入端进程可以源源不断地将自己的标准输出信息重定向到给定的管道中，
而管道另外一侧的进程则按照同样的顺序从管道里读取数据。

这些例子中，流中的数据是一经产生即不会被修改的，并且多个不同的流其实可以或多或少按照某种方式去组合；譬如可以组合多个进程，
让前一个进程的输出作为下一个进程的收入，管道的长度可以达到任意长度（当然实际的长度会受制于计算机的处理能力 ）。
同时这种采用组合来扩展程序的能力虽然简单却有着巨大的威力，管道的思想被认为是 Unix 编程哲学的核心要义之一。

在这种抽象语义下，除了流的开端出的处理逻辑，**其它相连的中间处理过程或者结束过程都是反应式**的，即遵循被动式的处理逻辑：
从输入中拿到内容 (可以是消息或者应用数据 ) , 按照业务领域意图做转换处理，然后将产生的结果放入流中，以便下游可以继续处理。

### 将两者相结合

上述**流的抽象其实和函数式编程的基本要素可以无缝地融合**在一起，因为流的运算特征满足不可变性的特征，并且易于组合。
两者的融合就可以成为是 **函数式反应式编程**。

## Javascript

**TBD**

## Java8 基本流

**TBD**

## RxJava扩展

[RxJava](https://github.com/ReactiveX/RxJava) 项目用库的方式对JVM平台进行扩展，提供易于组合、异步、事件驱动的反应式编程支持。
它的基本思路是扩展[观察者模式](http://en.wikipedia.org/wiki/Observer_pattern) 以方便地支持数据、事件流，并提供高层抽象，
将核心业务逻辑和底层的线程、同步、并发数据结构这些计算机底层的具体技术所隔离，使得应用程序开发者可以更关注于业务逻辑，提高开发效率。

由于反应式编程天然和函数式编程的关系密切，并且Java8才支持Lamba表达式和Stream这些抽象，所以在Java8平台上使用 `RxJava` 会更清晰自然。
一个最简单的回显 `Hello World` 的流程序的例子如下 

```java
import io.reactivex.*;

public class HelloWorld {
    public static void main(String[] args) {
        Flowable.just("Hello world").subscribe(System.out::println);
    }
}
```
这个最简单的例子中, 我们先使用 `Flowable.just` 方法产生一个流并初始化为传入的数据 `Hello World`，并且当给定的数据传入流之后，流即终止。
`subscribe`方法则提供一个流数据的消费者，这里是一个lambda表达式，将实际数据传给 `System.out.println` 打印在控制台上。
当然这个例子是在实际应用中没多大意义，没有人会写出这样的实际代码来增加无畏的复杂性。

### 更复杂一点的例子

更常见一点的任务是，我们想根据输入数据做一些运算，这些运算本身可能比较复杂而耗时所以我们希望它在一些后台进程上做，做完之后，再将结果汇聚起来放在界面上显示出来。
如果我们采用Java7提供的并发包中的工具来做，则需要仔细考虑一下线程之类的东西 （或者使用ForkJoinPool来做）；
RxJava 则允许我们仅仅关注于需要解决的问题逻辑，其实现可以如下 

```java
Flowable.fromCallable(() -> {
    Thread.sleep(1000); //  imitate expensive computation
        return "Done";
}).subscribeOn(Schedulers.io())
  .observeOn(Schedulers.single())
  .subscribe(System.out::println, Throwable::printStackTrace);
```

该例中，每一个语句调用结果都会产生一个新的不可变的 `Flowable`对象出来，整个代码的书写方式是链式调用的风格；如果将每个调用写在单独的一样上，
应用处理逻辑则一目了然。

RxJava并不直接使用Java8的线程或者 `ExecutorService` 接口，而是用 `Scheduler` 抽象和底层的JVM线程库做交互；`Scheduler` 负责和底层的线程或者
`ExecutorService`实例做绑定；遵循常用的Java命名约定，`Schedulers` 工具类封装了一些常用的静态 `Scheduler` 实例方便编程使用。

### 使用并发

RxJava 允许并发的运行计算以提高处理能力，然而当流中的输入数据是线性传入的时候，默认情况下则无法并发，下面的例子

```java
Flowable.range(1, 10)
  .observeOn(Schedulers.computation())
  .map(v -> v * v)
  .blockingSubscribe(System.out::println);
```
虽然使用了默认共享的计算线程池（假设我们有多个处理器核心），但是因为输入的数据是线性传入的，中间的计算并不会自动地派发到多个计算线程上。
为了打开并发处理，我们需要额外下一番功夫，使用 `flatMap` 方法显示地展开流并在运算完毕后自动合并流数据：

```java
Flowable.range(1, 10)
  .flatMap(v ->
    Flowable.just(v)
      .subscribeOn(Schedulers.computation())
      .map(w -> w * w)
  ).blockingSubscribe(System.out::println);
```

新一点的版本中，RxJava还加入了 `parallel()` 方法以生成一个并发的流对象 `ParallelFlowable` 从而更大地简化代码

```java
Flowable.range(1, 10)
  .parallel()
  .runOn(Schedulers.computation())
  .map(v -> v * v)
  .sequential()
  .blockingSubscribe(System.out::println);
```

需要注意的是，`Flowable` 和 `ParallelFlowable` 是完全不同的类型，二者都是泛型类，但没有共同的接口。`ParallelFlowable`提供了丰富的接口可以得到一个`Flowable`
- `reduce` 可以用一个调用者指定的函数来得到一个顺序的流
- `sequential` 可以显示地从每个流的尾部用轮询的方式得到一个顺序流
- `sorted` 则可以排序并发流并合并的恶道一个顺序流
- `toSortedList` 则得到一个排序列表的流

### 合理的设计和使用

RxJava 建议最好的使用方式是 :

1. 创建产生数据的可被流感知的对象(`Observable`)，作为流的输入数据
2. 创建对数据进行运算的处理逻辑，它们自己本身可以对流推送过来的数据做适当运算，产生新的结果写入流中
3. 完成处理所关心的数据的处理逻辑，将数据汇聚归并成最终关心的形式

RxJava 支持从已有的数据结构中创建 `Observable`, 我们可以很方便的使用 `from`/`just` 或者 `create`方法创建出来，
他们可以同步地一次调用`onNext`方法通知感兴趣的订阅者；在所有的数据都通知完毕的情况下，则会调用 `onCompleted()` 方法通知订阅者。

#### 组合和变化 `Observable` 

RxJava 支持我们方便地连接或者组合多个`Observable`, 考虑如下的代码
```groovy
def simpleComposition() {
    customObservableNonBlocking()
        .skip(10)
        .take(5)
        .map({ stringValue -> return stringValue + "_xform"})
        .subscribe({ println "onNext => " + it})
}
```

它的处理流程其实是如下的流水线处理

![groovy_pipeline](https://github.com/Netflix/RxJava/wiki/images/rx-operators/Composition.1.png)

## Spring

**TBD**

## Kafka Streams

**TBD**

## RxCpp

[RxCpp](https://github.com/Reactive-Extensions/RxCpp) 提供类似了和 [Ranges-v3](https://github.com/ericniebler/range-v3) 库类似的管线操作，其处理方式本质上也是反应式的。
由于C++支持运算符重载，`|` 操作符可以天然地作为管道操作符拿来用，而现代C++语言对函数式编程和Lambda表达式的丰富的表达能力使得写出可读性好的代码不算什么难事。

下面是其项目文档中的一个例子,实现如下一个解析任务
- 给定一个二进制文件作为输入，其中存放的是一段不等长字符串构成的二进制表示，可以有多行；每行用之间回车符号隔开 ；输入长度被分割为固定长度的行
- 要求解析给定的输入，将二进制数表示的字符打印出来，并且每一行对应于原输入中的一行

输入的格式如下（假设一行有17个字符的二进制数表示 )
```
66 66 66 66 66 66 66 66 66 66 66 66 66 66 66 66 66 
13 67 67 67 67 67 67 67 67 67 67 67 13 68 68 68 68 
68 68 68 68 13 69 69 69 69 69 69 69 69 69 13
```

对应的输出应该为
```
BBBBBBBBBBBBBBBBB
CCCCCCCCCCC
DDDDDDDD
EEEEEEEEE
```

产生一段随机的字符输入可以用RxCpp的方式写为如下的函数式代码
```cpp
random_device rd;   // non-deterministic generator
mt19937 gen(rd());
uniform_int_distribution<> dist(4, 18);

// for testing purposes, produce byte stream that from lines of text
auto bytes = range(0, 10) 
    | flat_map([&](int i) {
            auto body = from((uint8_t)('A' + i)) 
                | repeat(dist(gen)) 
                | as_dynamic();
            auto delim = from((uint8_t)'\r');
            return from(body, delim) 
                | concat();
        }) 
    | window(17) 
    | flat_map([](observable<uint8_t> w){
        return w 
            | reduce(vector<uint8_t>(), [](vector<uint8_t> v, uint8_t b){
                    v.push_back(b);
                    return v;
                }) 
            | as_dynamic();
        }) 
    | tap([](vector<uint8_t>& v){
            // print input packet of bytes
            copy(v.begin(), v.end(), ostream_iterator<long>(cout, " "));
            cout << endl;
        });
```

这里虽然有多个管道操作，不习惯函数式编程风格代码的程序员可能看起来有些头晕，好在只要我们加上恰当的缩进，在熟悉RxCpp库的命名的情况下，代码的逻辑还是比较简单明了的

`range(0, 10)` 先生称一个`Observable`对象作为流的起点，内部会一次产生0,1,2,...,10发布给流，通知其后的 `subscriber`, 后边的管道操作符连接了流处理的下一个环节。
接下来，用给定的字符得到一个对应ASCII码，然后用 `repeat`函数经给定的字符做重复，产生4~18个重复的字符，`as_dynamic`则是一个特殊的函数用于解决C++类型安全方面的一些问题。
这是一个字符行的二进制串，然后我们还需要用换行符将其连接起来，`from` 的双参数形式可以产生一行新的 `Observable` 对象，用第二个参数作为分隔符连接第一个`Observable`,
进而这里的多个行便被连接成一个整体的 `Observable`对象给后续处理了。

由于我们希望每一行的输入仅有17个二进制串，紧接着的 `window`函数就是用来将输入的流按照固定的长度分隔为固定长度的行的；需要留意的是每一个字符的长度可能是4~18的一个随机值，
这个在前面`repeat`的时候便确定了。`flat_map`可以将lambda表达式封装的实际的 `observable` 提出出来。
有了上面这些按行分割好的集合，接下来的任务是需要将这些记录保存到一个容器中，便于后续解析处理，因而后续的一个`flat_map()`调用针对前面的集合，
采用`reduce`算法收集一个一个的字符并以行为单位放入容器中，最终的流会每次产生一个`vector<uint8_t>`待后续处理。
出于调试的便利，我们同时用`tap`函数安插一个方便调试的打印操作，该函数本身不会修改流的数据内容。


下面的代码则完成一些变换逻辑，用更多的中间变量会使得代码更容易理解一些，当然代码就不如上面的输入部分那么紧凑了。

首先我们需要一个工具函数删除给定字符串中的空格字符，用一个lambda表达式可以很容易的写出来
```cpp
auto removespaces = [](string s){
    s.erase(remove_if(s.begin(), s.end(), ::isspace), s.end());
    return s;
};
```
接下来我们就可以针对上面已经生成好的bytes流，按照回车符号进行拆分,过滤掉输入中的空格字符，拆分过程可以用C++标准库的`cregex_token_iterator`来完成，
并将结果用迭代器封装访问，而C++11的移动语义可以额外的减少临时字符串的开销。`concat_map`将所有的去掉了换行符的字符串连接起来，交给流的下游，
然后完成过滤空格字符串的操作。

最后我们需要将处理好的串再加上回车符号，并最终输出，加上`publish`和`ref_count`可以使得我们写2个单独的订阅者，第一个自动建立流的连接，
最后一个订阅者处理完毕后自动关闭流. 代码如下

```cpp

// create strings split on \r
auto strings = bytes 
    | concat_map([](vector<uint8_t> v){
            string s(v.begin(), v.end());
            regex delim(R"/(\r)/");
            cregex_token_iterator cursor(&s[0], &s[0] + s.size(), delim, {-1, 0});
            cregex_token_iterator end;
            vector<string> splits(cursor, end);
            return iterate(move(splits));
        }) 
    | filter([](const string& s){
        return !s.empty();
        }) 
    | publish() 
    | ref_count();
```

由于最后一行不像前边的行一样有一个额外的回车符，我们需要将其分拣出来，使用`filter`和`window_toggle()`可以很方便地将二者进行分组，
然后再将结果用`sum`连接起来并删除中间的空格字符。
```cpp
// filter to last string in each line
auto closes = strings 
    | filter( [](const string& s){
        return s.back() == '\r';
        }) 
    | Rx::map([](const string&){return 0;});

// group strings by line
auto linewindows = strings 
    | window_toggle(closes | start_with(0), [=](int){return closes;});

// reduce the strings for a line into one string
auto lines = linewindows 
    | flat_map([&](observable<string> w) {
        return w 
            | start_with<string>("") 
            | sum() 
            | Rx::map(removespaces);
});
```

最后一段代码用于接产生好的流打印出来，直接传递给`println`即可
```cpp
// print result
lines | subscribe<string>(println(cout));
```

回头看这里的代码，可以明显看到里面没有一个传统的过程式控制逻辑，没有分支、循环和判断，有的只是函数定义、调用和流连接操作。
如果熟悉Rx系列库的API，处理逻辑还是比较清晰明了的。

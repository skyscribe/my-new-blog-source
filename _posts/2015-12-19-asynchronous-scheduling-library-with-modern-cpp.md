---
layout: post
title: 用C++11/14实现一个现代的一部任务调度程序
categories: [cpp, programming, design]
tags: [programming, design, cpp, fp]
---

很多C++项目中都存在一个能够**异步调度任务**的基础库；大部分这样的库都是用老的C++语言(98/03)写成的，要么模板元语法满天飞外加各种黑魔法导致维护困难，
要么是采用传统的宏方式导致维护困难，布满各种隐患。既然C++11/C++14提供了更好用的武器，我也耐不住手痒自己实现一个。
当然，这个是一个新轮子，用很少的代码实现一些核心的想法，同时又额外获得一些对新的C++的更深刻的理解，何乐而不为？

<!--more-->

## 目标概要
主要的目的是实现一个能够异步、高效地调用用户任务的库；当然具体调用的形式可以由用户指定。任务可以以服务的方式存在，可以被存储起来以支持延迟调用；便于用户层解耦。
比如A模块向库注册了一个名为`someService`的任务，并携带实现定义好的参数签名（参数列表以及类型), B模块可以按照名字来调用这个具体的service实现的任务。

调用的方式可以是
1. 异步的或同步的；所谓异步即实际任务执行发生在调用返回之后；同步调用则必须等实际任务执行后才返回
2. 阻塞的或非阻塞的：如果指定为非阻塞的，那么存储进来的参数会被临时存放起来(放在一个closure中），等到内部的线程等的合适的时间才调用；当前**调用者的线程不会被暂停**执行，
而阻塞调用则会直接暂停当前的调用者线程

调用者可以传入一个回调，指定在实际任务执行完毕之后再执行，典型的应用场景是任务完成之后的链式操作，譬如要求A Task执行完毕后，执行一个调用者传入的动作；该动作中可能产生一个新的任务调用哪个。

局部排序调用 - 譬如要求某些**打了同一个标签**的任务必须严格按照顺序执行；而没有带标签的任务可以任意并行调度，类似于操作系统调用进程的affinity特性。
这种特性在一些复杂的应用场景中很有用，譬如某个比较耗费CPU的任务执行的中间，可能需要等待IO等操作，又**不希望堵塞调度线程**，一个简单的想法就是将其分解成多个子任务，那么期望这些子任务不会被乱序执行。

### 调用属性结构
可以用一个结构来描述这些基本参数组合。
```cpp
typedef std::function<bool()> Callable;
struct CallProperty{
    bool async; //will the job be scheduled asynchronously (under same context)
    bool waitForDone; //if interfaceCall will be blocked (for done) or not, will be ignored for async call
    Callable onCallDone;
    std::string strand; //calls with same strand will be scheduled by same thread, async only
};
```

### 可变参数
由于注册的任务可能需要一些运行期的参数在真正调用的点才知道，我们需要支持预先定义好这些参数签名，仅仅在实际调用段再传入具体的参数；即如下的使用方式
```cpp
//registration
InterfaceScheduler sched;
sched.start(4); //starting a pool of 4 threads

bool serviceImpl(int, std::string);
registerInterfaceFor<int, std::string>(sched, "serviceName", serviceImpl);

//calling site
CallProperty prop{true, false, Callable(), ""}; //asynchronous call, non-blocking, no strand
sched.interfaceCall("serviceName", std::forward<CallProperty>(prop), 1, "actualParam");
```

参数的定义可以使用`tuple`结合closure来实现灵活的绑定
```cpp
struct ParaArgsBase{};

template <class ... Args>
struct ParamArgs : public ParaArgsBase{
    ParamArgs(const Args&... args) : parameters(args...){}
    std::tuple<Args...> parameters;

    static const char* getType(){return typeid(std::tuple<Args...>).name();}
};

template <std::size_t I, class ... Types>
typename std::tuple_element<I, std::tuple<Types...> >::type const& get(const ParamArgs<Types ...>& args){
    return std::get<I>(args.parameters);
}
```

## 设计
设计起来比较简单，有几个核心类和典型场景所约束。

### 核心的类设计
主要类的职责和写作交互用CRC图来描述，见下图
![crc_class_design]({{site.base_url}}/assets/async_lib/crc.png)

### 基于接口的注册
支持多种注册方式

- 注册后才能调用

![register interface]({{site.base_url}}/assets/async_lib/registerInterface.png)

- 对注册行为的订阅（观察者）：其它用户可以对某个特定的服务注册进行监控，当某个服务被实际注册的时候得到通知并以回调的方式通知观察者。如果实际设置观察者的时候，
对应的服务以及注册，则直接回调通知观察者


![subscribe for registration]({{site.base_url}}/assets/async_lib/subscribeForRegistration.png)

### 使用场景

以下是一些具体使用的例子，包括

- 异步非阻塞的场景 （最常用场景）

![asyncNonBlockCall]({{site.base_url}}/assets/async_lib/asyncNonBlock.png)

- 异步的阻塞调用

![asyncAndBlockCall]({{site.base_url}}/assets/async_lib/asyncAndBlock.png)

- 同步的阻塞调用

![syncAndBlockCall]({{site.base_url}}/assets/async_lib/syncAndBlock.png)

- Strand局部Affinity约束

![strandCall]({{site.base_url}}/assets/async_lib/strandCall.png)

项目的源代码可以在[这里](https://github.com/skyscribe/servicelib)找到。

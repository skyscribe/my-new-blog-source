---
layout: post
title: 用C++11/14实现一个现代的异步服务调度库
categories: [cpp, programming, design]
tags: [programming, design, cpp, fp, library]
---

很多C++项目中都存在一个能够**异步调度任务**的基础库；大部分这样的库都是用老的C++语言(98/03)写成的，要么模板元语法满天飞外加各种黑魔法导致维护困难，
要么是采用传统的宏方式导致维护困难，布满各种隐患。既然C++11/C++14提供了更好用的武器，我也耐不住手痒自己实现一个。

当然，这个是一个新轮子，用很少的代码实现一些核心的想法，同时又额外获得一些对新的C++的更深刻的理解，何乐而不为？

<!--more-->

## 目标概要
主要的目的是实现一个能够异步、高效地调用用户任务/服务的库；**具体调用的形式可以由用户指定**。任务以服务的方式存在，可以被存储起来以支持延迟调用；便于服务提供方和服务适用方解耦，增加应用程序的灵活性。

比如A模块向库注册了一个名为`someService`的任务，并携带实现定义好的参数签名（参数列表以及类型), B模块可以按照名字来调用这个具体的service实现的任务。

调用的方式可以是
1. **异步**的或**同步**的；所谓异步即实际任务执行发生在调用返回之后；同步调用则必须等实际任务执行后才返回
2. 阻塞的或非阻塞的：如果指定为非阻塞的，那么存储进来的参数会被临时存放起来(放在一个closure中），等到内部的线程等的合适的时间才调用；当前**调用者的线程不会被暂停**执行，
而阻塞调用则会直接暂停当前的调用者线程

调用者可以传入一个**任务完成时候的回调**，指定在实际任务执行完毕之后在**同一个线程的上下文中**执行，典型的应用场景是任务完成之后的链式操作，譬如要求A Task执行完毕后，执行一个调用者传入的动作；该动作中可能产生一个新的任务调用。

调用方可以添加一个**局部线型执行约束**（其实是对[Boost.asio strand](http://www.boost.org/doc/libs/1_59_0/doc/html/boost_asio/overview/core/strands.html)的模拟) - 譬如要求某些**打了同一个标签**的某些任务**不能被并发执行**；这些打了标签的任务的执行相互之间不需要加显示的锁；而没有带标签的任务可以任意并行调度，类似于操作系统调用进程的affinity特性。

这种特性在一些复杂的应用场景中很有用，譬如某个比较耗费CPU的任务执行的中间，可能需要等待IO等操作，又**不希望堵塞调度线程**，一个简单的想法就是将其分解成多个子任务，那么期望这些子任务不会被交叉执行。

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
由于注册的任务可能需要一些运行期的参数在真正调用的点才知道，我们需要支持预先定义好这些参数签名，仅仅在实际调用端才传入具体的参数；即如下的使用方式
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
设计起来比较简单，可以通过几个核心类和典型场景来描述。

### 核心的类设计
主要的接口类通过一个具体的class - `InterfaceScheduler` 来提供，它作为库本身的入口提供；底下封装了线程池细节，允许用户在启动的时候传入内部运行的线程池数量（并发度）；同时提供注册和异步的`interfaceCall`接口来调用已注册的服务。具体的业务代码或服务实现类可以使用该class来注册服务或者发起服务调用。

内部实现上，`InterfaceScheduler`组合了一个同步的`SyncWorker`类来实现同步任务实现，以及一个一个或多个`AsyncWorker`类来完成具体某个工作线程上的任务队列。

主要类的职责和写作交互用[类CRC方法](https://en.wikipedia.org/wiki/Class-responsibility-collaboration_card)来描述，见下图
![crc_class_design]({{site.base_url}}/assets/async_lib/crc.png)


### 基于接口的注册
支持多种注册方式;处于**[对称性](https://dzone.com/articles/how-design-good-regular-api)**考虑，一个服务可以注册，也可以被解注册。调用尚未注册的服务或者已经解注册的服务，都会返回错误给调用者。

- 注册后才能调用: 这是最**正常**的使用场景; 由于注册的时候尚不知道真正的参数，但是参数签名确实由服务提供者确定的，因此需要使用类型签名。

![register interface]({{site.base_url}}/assets/async_lib/registerInterface.png)

- 对注册行为的订阅（观察者）：其它用户可以对某个特定的**服务注册进行监控**，当某个服务被实际注册的时候得到通知。`InterfaceScheduler`负责维护这些观察者，并在实际的服务提供者进行注册的时候，以回调的方式通知观察者。如果实际设置观察者的时候，对应的服务以及注册，则直接回调通知观察者。
这样服务的使用者可以在使用之前用一个回调做检查，确保服务被真正注册以后，才会发起调用，减小耦合。

所有在服务注册之前订阅的观察者（不管有多少个）在实际服务被注册的时候会被逐一通知到（同样**有阻塞操作**）并被清理。服务注册和观察者唤醒可以并发但不能有Race Condition。

![subscribe for registration]({{site.base_url}}/assets/async_lib/subscribeForRegistration.png)

### 使用场景

以下是一些具体使用的例子，包括

- 异步非阻塞的场景 （最常用场景）: 实际执行的动作会被保存为闭包放在内部的任务队列上；当线程池有空闲调度到给定任务的时候，之前注册的回调会在内部线程池的上下文执行。
调用者如果提供了完成回调，则需要保证回调中的操作**不能阻塞**。实际的动作执行和完成通知都是在用户库内部的线程池上执行，
所以调用者需要处理好**数据并发访问的安全性问题**，加锁或者其它数据一致性保证措施。

![asyncNonBlockCall]({{site.base_url}}/assets/async_lib/asyncNonBlock.png)

- 异步的阻塞调用: 这里其实是用异步操作来实现程序逻辑上的同步；调用者发起调用之后，并不直接返回而是等待实际任务被执行完毕后才能返回。
这里的阻塞调用可以确保调用者返回后，对应的操作一定是完成的；显然中间过程调用者线程会被阻塞。

![asyncAndBlockCall]({{site.base_url}}/assets/async_lib/asyncAndBlock.png)

- 同步的阻塞调用

![syncAndBlockCall]({{site.base_url}}/assets/async_lib/syncAndBlock.png)

- Strand局部Affinity约束: 这种场景下，某些任务会被显示排队放在一个线程中执行，确保没有**并发调度**的发生，从而这些调用之间是可以保证不会产生Race Condition；
调用方可以避免显示加锁的麻烦。
当然多个任务之间的顺序没有很强的保证，最简单的实现是保留发起调用的顺序来（内部放在一个队列上）一一调度。

![strandCall]({{site.base_url}}/assets/async_lib/strandCall.png)

## 实现
整体的实现风格是基于闭包和函数对象的；由于C++11/14新引入了可变的模板参数，用该特性实现调用端的可变参数列比传统的C++03
枚举所有可能（其实往往枚举9～19个）的泛型参数个数要省事儿很多。

线程池的实现也没有什么特别之处，只是实现异步阻塞调用的时候，C++11的lambda表达式更有利于我们写出干净的代码。

### 服务注册类型安全性检查
服务注册的时候需要编码进类型信息，方便后续调用的时候进行类型签名检查，防止参数不匹配。
这些检查都是通过一些全局的工具类函数来实现的

```cpp
template <class ... Args, class ActionType>
inline void registerInterfaceFor(InterfaceScheduler& sched, const std::string& idStr, ActionType action){
    //Check templateype-safety as possible, lambdas/binds shall have targets, while functions may not
    typedef ParamArgs<Args ...> ActualType;
    typedef std::function<bool(const ActualType&)> FuncType;
    static_assert(std::is_convertible<is_convertibleActionType, FuncType>::value, "Incompatible type!");

    //lambdas/mem_fun_ref_tunc may not define operator bool() to check - explicit convert as a workwaround
    FuncType func(action); 
    if (!func)
        throw std::invalid_argument(func"Null action specified for interface:" + idStr);

    sched.registerInterfaceForace(idStr, [=](const ParaArgsBase& p) -> bool{
            return func(static_cast<const ActualType&>(p));
    }, ActualType::getType());
}
```
由于实际注册的`action`可以是任何合法的函数对象，这个wrapper里做了一些额外的判断
- 类型签名是否匹配，用`is_convertible`和`static_assert`做编译器检查即可
- 是否传入了空函数，显然注册没有任何动作的服务是编程错误，我们希望如果这么做则抛出运行期异常，马上修改代码


### 调用类型检查
调用通过一个内部带类型比对和匿名函数封装的实现函数和一个公有的可变长参数模板函数来实现。

```cpp
//Schedule a previously registered interface cally by CallProperty (see its definition)
template <class ... Args>
bool interfaceCallfaceCall(const std::string& idStr, CallProperty&& prop, const Args& ...args){
    return checkAndInvokeCall<Args ...>(idStr, prop.async, prop.waitForDone, 
        std::forward<Callable>(prop.onCallDone), prop.strand, args.prop..);
}

//actual internal method
//Actual call under the hood
template <class ... Args>
inline bool checkAndInvokeCall(const std::string& idStr, bool async, boolool waitForDone, 
    Callable&& onCallDone, const std::string& strand, const Args& ... args){

    typedef ParamArgs<Args ...> ActualType;
    CallablebackType action;
    if(!isCallRegisteredAndTypesMatch(idStr, ActualType:idStr:getType(), action))
        return false;

    return invokeCall([args..., action]() -> bool{
            ActualType param(args...);
            return action(param);
        }, async, waitForDone, idStr, strand, std::forward<Callable>(onCallDone));
}
```
我们很喜欢C++作为强类型语言的特性，希望编译器能多帮程序员检查一些类型不匹配错误，就先用传入的参数类型和注册时候提供的类型做一比较。
注意这里实际发生调用的时候已经是程序运行过程中了，所以模板元技术要用的话需要多费一些功夫，通过构造一个具体的`ActualType`和内部存储的类型做逐一比对。
如果比对没错误，就可以构造出来一个可调用的函数传给具体的`invokeCall`了。底层调度任务的时候**已经不知道外层传入的参数**了（除非我们用可以放异构类型的容器 - 可惜variant）
，简单的想法是采用闭包将上下文操作封装起来传给`InvokeCall`。

类型比对的函数实现利用`tuple`的类型签名来比较，需要`typeid`的参与，毕竟这是运行期的判断。

### 异步阻塞的实现

默认异步的工作线程是处理非阻塞任务的 - 用户调用之后，生成一个job放在内部的队列里，然后立刻返回给调用者。
对于阻塞方式，调用需要在内部实现同步机制，保证阻塞调用者线程直到异步任务实际被调度完毕 - **简单直接的思路是利用已有的API**，内部就地构造一个完成调用做显示同步。
简单优雅的实现如下

```cpp
bool AsyncWorker::doSyncJob(const std::string& name, Callable call, Callable onDone){
    bool finished = false;
    mutex flagMutex;
    condition_variable cond;

    doJob(name, std::forward<Callable>(call), [&]() -> bool{
        if (onDone)
            onDone();

        std::lock_guard<mutex> lock(flagMutex);
        finished = true;
        cond.notify_all();
        return true;
    });

    unique_lock<mutex> lock(flagMutex);
    cond.wait(lock, [&]{return finished;});
    return true;
}

```

由于代码上下文很清晰，我们甚至不需要写任何子函数，直接通过lambda表达式构造完成调用，通过捕获的上下午获取（注意`[&]`指示）条件变量和同步标记变量的引用,在内部唤醒环境变量；
外部线程执行完异步调用之后，就守候环境变量保护的标记变量知道更新完毕。

这段代码之所以简单清晰，一方面是由于新的lambda表达式语法威力强大，另外一个重要因素应该归于C++11对多线程编程基础设施、库的标准化 - 我们不再需要写一大堆`pthread`调用了，标准库已经帮我们打理好了细节。


项目的源代码可以在[这里](https://github.com/skyscribe/servicelib)找到。


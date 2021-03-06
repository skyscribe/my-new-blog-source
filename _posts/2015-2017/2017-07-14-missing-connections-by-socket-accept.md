---
layout: post
title: TCP服务端socket会丢连接的诡异问题及思考
comments: true
categories: [linux, programming, debugging, design]
tags: [linux, programming, netty, java, debugging, design]
---
最近在跟踪项目的性能测试的过程中，发现了一个奇怪的问题：**Netty服务器端新建的Channel的数目比Tcpdump抓包得到的经历过三次握手成功的连接数要少**：
总有几个连接从抓包来看成功，然后Netty并没有为其分配Channel。直观上来看，似乎是Netty的处理问题值得怀疑，似乎是用于接受新连接的线程池堵塞了。

深入追踪下去，发现问题不在Netty身上，而是**操作系统没有通过IO事件反馈给**应用程序(Netty)导致了丢连接的情况出现。经过更深层次的思考后，
问题之所以出现，还与我们的协议设计不太合理有关。

<!--more-->

## 问题发现和定位

项目的服务器端和客户端之间采用的是基于**TCP长连接**的应用层自定制协议；协议的基本工作流程如下
1. 服务端监听在事先设置的服务端口上；用Netty的`ServerBootstrap`来封装服务器端的监听`socket`处理;其内部封装了`listen`和`accept`等底层API
2. 针对每个连接上来的客户端，Netty会创建一个新的SocketChannel,其初始化方法中会初始化应用层的协议处理
（即一些列的`ChannelHandler`对象）来负责对应连接上的IO事件以及应用层逻辑
3. 连接建立成功后，客户端会处于Idle状态，**等待服务器端发起第一条消息**，初始化应用层握手。
4. 客户端收到握手消息后，确认相关信息，之后即进入正常的协议交互处理

项目的初始设计目标是需要处理**最多20000条并发连接**;这个性能指标应该说不算特别高，因为业界早就实现了从[C10K到C100K](https://qunfei.wordpress.com/2016/09/20/from-c10k-to-c100k-problem-push-over-1000000-messages-to-web-clients-on-1-machine-simultaneously/)的跨越。
当然任何**性能指标如果不加以测试的话，都是假把式**。为了验证该目标，我们设计了**针对性的性能测试模拟器**, 并设计了一个测试并发建链接的测试用例；
其目的是模拟多个逻辑上没有依赖的客户端同时发地发起大量的连接，验证服务器端是否能满足预设计的健壮性和可靠性要求。

当上述第二步模拟器端以每秒近1000条连接的配置从本地环回端口(loopback)发起并发连接的时候，实际走到第三步的连接数有**一定的概率**少于1000个，
而模拟器端显示所有的连接都成功建立。所有的网络通信都是从127.0.0.1这个本机IP地址在不同的应用程序之间周转的，可以直接排除网络不稳定的干扰。

### 对模拟器和Python的傲慢与偏见

由于模拟器的主框架是用Python语言写成的，一开始大家都怀疑是否是[Python性能缺陷的原罪](http://jakevdp.github.io/blog/2014/05/09/why-python-is-slow/)导致的；
毕竟Python的性能一直为人诟病；只是任何怀疑都必须找到切实的证据，必须做小心的分析和求证；具体到这个问题上，还是找一些中间的分析结果来的容易一些。

两端都查了一堆的log之后最终也没有太多实质性的发现，最后大家还是决定看tcpdump的抓包，并过滤TCP协议的初始握手包更为简单直接，
只是过滤器的设置稍微复杂一点；google一下不难找到。

### 网络协议分析是最强有力而值得信赖的
Tcpdump的抓包分析表明，每次这些连接全部都建立成功了,统计TCP握手的3个步骤的时候，
不管是初始的SYN(0)包，还是服务器返回的SYN(0)+ACK(1)包，以及第三次握手的ACK(1)包都完全发送成功。
这也就意味着，从TCP/IP传输层的角度来看，第二步其实已经全部完成。客户端是无辜的。

问题就仅可能出现在服务器端了；接下来就是尝试修改Netty的源码加入更多的打印来观察其是否调用了`ChannelInitializer`对连接上来的客户端初始化ChannelHandler。
仔细看下来，居然是Initializer的数目就根本不对。
难道是Netty的实现有问题吗?作为一个在开源业界关注度如此之高的一个项目，我们采用的还是其非常成熟的4.0版，出现这种低级错误的可能性实在太小；虽然一切皆有可能。

### 排除Netty的嫌疑

稍微阅读Netty的代码后，发现并没有特别的逻辑漏洞 - 它默认采用的是异步IO模型，用`select/poll`模型来做连接的多路复合(Multiplexer);即使有传说中的[CPU空跑的问题](https://github.com/netty/netty/issues/2616),看了代码之后发现对应的问题在新版本中早已fix掉了。

保险起见，我们又尝试[换默认的NioEventLoop为Linux本身的Epoll](http://netty.io/wiki/native-transports.html)，问题依然没有得到解决。从行为上来说，
epoll机制也仅仅能解决效率高低的问题，**并不应该解决行为不一致**的问题。

回想到我们采用的线程模型上，server socket上的事件循环还承担着应用层程序和协议栈交互的任务（我们通过UserEvent的方式向对应的pipeline上发事件来避免数据同步），
默认的单线程处理这些应用层事件的处理方式是否会导致效率低下也是一个值得验证的点；等到增大了线程数之后，问题依旧没有什么眉目。
和上面的处理机制类似，这样的方式也只是从效率的思路出发尝试解决问题，**逻辑上依然是无法解决行为不一致**的问题。

### 从操作系统的角度分析

转了一圈，发现问题还是出在`listen`调用和`accept`的交接的地方；这里实际的TCP行为是发生在Linux的内核空间的；逻辑上其内部也是有个类似的异步队列，
对进来的TCP连接请求，内核会设置相关的socket状态，分配相关的数据结构，自动完成TCP协议的握手过程，待到握手完毕之后，将这个连接成功事件通知给应用层（select/epoll)；
然后应用层可以检查对应的socket读事件，调用`accept`获取新的socket文件描述符。

这一系列过程都是异步的，并且跨用户空间处理和内核空间调度。
确定了可能出问题的地方，查找的方向就比较明确了；只需要找找可能影响server socket的行为就可以了。
此时一个可以的关于SO_BACKLOG的设置引起了我们的注意,因为这里设置的值是`8`，尝试调大这个参数后，丢连接的情况突然消失了。

### 检查socket选项 - `backlog`设置

上述的参数是从Java的API中继承来的，实际设置的时候，其实也是传给了JDK的对应的参数；引用JDK的参数说明
> The maximum queue length for incoming connection indications (a request to connect) is set to the backlog parameter. If a connection indication arrives when the queue is full, the connection is refused.

这里比较奇怪的是，当队列满了之后，从抓包的角度来看，**对应的连接并没有被拒绝**，而是显示连接成功了！如果后续没有任何数据交换(`send`/`receive`)的话，
对应的socket(以及操作系统分配的相关资源)其实也悄然泄露了?可是真会有这么低级的问题吗？

既然有此疑惑，还是仔细翻阅一下Linux的`listen`的manpage为好；搜索`backlog`选项,对应的说明如下

> The backlog argument defines the maximum length to which the queue of pending connections for sockfd may grow.  
> If a connection request arrives when the queue is full, the client **may receive an error** with
> an indication of ECONNREFUSED or, if the underlying protocol supports retransmission, **the request may be ignored**
> so that a later reattempt at connection succeeds.

不确定这是否是一个bug，因为客户端测并没有检测到这个`ECONNREFUSED`的错误而是显示连接成功。
StackOverflow上有人提了[类似的问题](https://stackoverflow.com/questions/37609951/why-i-dont-get-an-error-when-i-connect-more-sockets-than-the-argument-backlog-g)，
合理的解释是，因为TCP支持重传，所以该请求**仅仅是被忽略了**，直到下一次连接过来的时候，对应的连接会直接成功！

要保证这样能工作，操作系统也必然需要在某些地方保存已经分配好的socket（以及相关的数据结构）以便下次可以通过`accept`取到。

### 如何解决和避免再次发生
对设计用来处理比较高的并发处理请求的服务器程序来说，设置`backlog`选项为比较小的值是个比较糟糕的主意，更容易"踩上这个坑"。也许这也是默认情况下，
Linux将这个值设置为128的原因；如果想修改它，最好**设置的比128更大一些**。

由于这种情况在连接状态的backlog缓冲满了之后，再有新连接完成三次握手之后就可能出现，设置再大的值，理论上来说都不是足够保险;除非我们能提前预测或者限制客户端的行为，
避免大量的并发连接上来，或者让客户端能检测到这种情况。

考虑到实际环境中，**这种情况出现的概率还是很低的**，只有在基于内网的模拟器环境下，才会有这么“巧合”的情况出现;
在不能修改客户端行为的情况下，将该选项的值修改大一些即可有效地降低其出现的几率。

要想彻底解决这个问题，单单从socket层面来看，应该是吃力不讨好的事情，因为行为的不一致发生在
操作系统的系统调用和用户程序的交互的地方；一个可行的思路是从更高层的应用层及时检测这种情况；这样的解决方案需要应用协议层面的特别处理才行。

## 对协议设计的影响
由于问题发生的场景比较特殊一点，具体来说有以下必要条件

1. 多个客户端在短时间内发起大量的并发连接
2. 服务器端接收新连接的**内部队列长度设置比较小**，以至于某些完成握手的连接在客户端重试的时候直接返回成功
3. 这些由于队列满而引入的默默完成的连接不被服务器应用程序所知道（没有新的IO读事件）
4. 连接建立**成功之后，客户端处于静默状态，等待服务器端发起第一条消息**，从而从应用逻辑角度看，双方进入了“死锁”状态

其实从根本上解决该问题的方式是避免第四个条件的出现；有两种方式可以凑效
- 永远保持让客户端发送第一条协议消息，或者
- 客户端在连接建立之后即发送业务层的心跳 - 这一机制在应用层有心跳支持的情况下变得更清晰

## 一点额外的总结和感想
做技术的很容易对某些特定的技术产生“宗教式”的虔诚或“魔鬼式”的憎恶，技术偏见也会由此产生，并蒙蔽我们的视野，导致我们掉进一些坑里。
一开始的时候我们对Python的想当然的偏见耽误了不少的时间来检查是否模拟器的实现是否出了问题；幸好我们及时发现这种直觉上的错误；
顺利找到问题的根源。

看起来很复杂的问题，根源往往在很小的一个基础的知识点上；
对操作系统底层处理机制的积累和对基本TCP/IP协议的实现的了解极大地节约了我们分析和解决问题而耗费的时间。
这些最基本的东西，其重要性怎么强调都不为过，应该作为程序员的基本功保持长期不懈怠。

应用层网络协议的设计和实现上；**遵循惯例和技术直觉**(Least Surprise)总是没有太大错误；当初我们设计协议的时候，选择让客户端等待服务器发送第一条消息，
虽然有一些感觉奇怪的地方，由于其它种种原因，当时没有仔细去考虑；最终在做性能测试的时候，就有问题爆出来狠狠地咬了我们一口。
幸好这类问题还没有被真正的客户所发现就被我们“扼杀”在实验室测试中！


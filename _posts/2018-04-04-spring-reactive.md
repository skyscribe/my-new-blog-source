---
layout: post
title:Spring新版本增强了对流式编程的支持
comments: true
categories: [design, programming]
tags: [design, architecture, programming, microservice, spring]
---

流式编程在微服务架构风格越来越流行的背景下日益引起开发者的关注([前文有一些简单探讨]({{ site.baseurl }} {% post_url 2018-03-10-functional-reactive-programming %} ))
，新的Java语言规范加入了流的概念从语言库上添加了对FRP的支持。
Spring最新的[5.0版本](https://spring.io/blog/2018/02/19/spring-framework-5-0-4-available-now)更新也顺应这一潮流，加入了原生支持FRP的行列。

<!-- more -->

## 最大的版本改动源于设计范式选择的增加

Spring的官方博客声称该版本的改动是项目从2014年创立以来最大的一次版本更新；主要的考虑是，
增加对异步流式编程范式的支持在Java语言平台下是一个思维方式上的巨大变化。

传统的Java编程范式是基于面向对象设计的实现，具体到底层上还是命令式的同步、阻塞调用思维；
你调用一个对象提供的方法，则调用者默认必须等待操作完毕之后，用返回值（或者异常）的方式通知调用者。
即使有异步编程技术的辅助，这些细节也被认为是抽象接口下面的实现细节，很少被作为主要的设计决策。
FRP的方式就显著地不同，它的主要设计对象变为异步的流，所以对流的操作、变换等都是异步发生的，
程序的**主要逻辑不需要再关注底层的操作是怎么被调度的，而仅仅关心一个一个具体的操作应该做什么，互相配合**完成系统目标。
这个角度来说，FRP的方式是声明式的；而声明式的代码相对传统的过程式代码有更好的可读性和可维护性。

经典的面向对象设计方法容易入门却不易精通，设计者很容易掉入过度设计的陷进而滥用继承（接口也是类似）而带来不必要的过度抽象；
当然抽象不足也是另外一个问题这里略去不提。GoF在设计模式里面特别声明了我们需要考虑优先使用组合而不是继承，
不幸的是这一忠告从来就没有被人们认真对待；生搬硬套这些模式的设计者很容易就掉入叠床架屋的花花架子中无法自拔。
FRP的思维方式完全不提继承的事儿，但是封装依然是必要的；组合则被提到了首要的位置，因为函数式编程的主要复用方式就是组合。

Spring其实采用了一种相对中庸的态度，给你提供了FRP的支持，但是决定权仍然在用户自己手里；
你可以选择用传统的OOD方式，这样更熟悉也没有什么额外的迁移成本；也可以根据项目的实际情况，
选用异步的函数式编程，当你决定这样选择的时候，Spring的基础设施已经做到足够的完善，你可以仅仅关注于你的程序逻辑就行了。

## 和其他JVM上的FRP的对比

如果将视角扩大到整个JVM平台上，那么Spring显然不是唯一的选择，也不是最强大的FRP选择。
类似于Akka这样的框架本身就是基于Actor模型来实现更高层次的函数式编程设施的，尽管Actor的模型和FRP并不是完全目标一致。
Scala语言则提供完整的函数式编程语言支持，更复杂的FP抽象也不在话下，在Java8之前甚至被认为是JVM平台下最好的选择。
RxJava以语言扩展的方式也出现了比较长的时间。

Spring 5.0的核心FRP抽象逻辑和上述这些高层的框架的核心概念是一致的；它独特的地方在于其本身作为一个开源平台的灵活性；
你可以选择用Spring Boot，也可以不用；可以选择微服务架构来使用FRP，也可以完全不使用微服务架构。
正如Netflix在其Zuul2项目的重构过程中所总结的，尽管使用异步的方式来构建程序可以极大地提高性能和资源使用的有效性，提高自动扩展的能力；
运维的难度和调试挑战也被急剧放大。如果用来解决正确的问题，收效就会比较大，然而如果用来解决错误的问题，情况反而会更加糟糕。
关键的是，我们需要**用正确的工具来解决正确的问题**。


## SpringMVC和WebFlux

Spring-webmvc是Spring中的一个经典MVC模块，它最初是作为一种基于Servelet技术的Web后端框架提供给后端程序员使用的。传统的Spring MVC框架工作机制如下
1. `DispatcherServelet` 会搜索`WebApplicationContext`来查找DI容器中注册的Controller以处理进来的HTTP请求
2. 本地化解析的Bean在这一过程中也会被一并查找并关联起来以便后续渲染View的时候使用来本地化View中的显示内容
3. 主题解析的Bean则被用来关联后续要使用的View模板,以进行CSS渲染等额外处理
4. 如果HTTP请求包含多部分媒体内容，那么请求会被封装在一个`MultipartHttpServeletRequest`中处理
5. Dispatcher会搜索对应的Handler，找到之后，handler对应的controller以及其前置处理、后续处理会被按照顺序依次处理以准备模型返回，或者被用于后续View渲染
6. 如果一个模型被返回，对应的View就会被渲染并返回响应的HTTP消息
整体的处理逻辑是一个线性的同步处理逻辑。

传统的Sping MVC框架的接口都定义在 `org.springframework.web.servlet`包中，而支持响应式编程的Web框架被命名为WebFlux,对应的接口和注解放在一个新的Java包中：
`org.springframework.web.reactive`。它是全异步、非阻塞的，可以很方便的使用在基于事件循环的异步编程模型中，仅仅需要很少个线程就可以高效地处理大量的请求。
其功能不仅支持传统的Servelet容器，也支持诸如Netty、Undertow这些不是基于Servelet的编程框架上。

### WebClient

传统的Spring MVC中，我们可以使用`RestTemplate`来组装发起HTTP客户端请求，对应的Reactive版本的概念是`WebClient`。
一个最简单的异步发起HTTP请求的例子如下

```java
WebClient webClient = WebClient.create();

Mono<Person> person = webClient.get()
    .uri("http://localhost:8080/persons/42")
    .accept(MediaType.APPLICATION_JSON)
    .exchange()
    .then(response -> response.bodyToMono(Person.class));
```
这里例子依然是使用同步的方法将相应消息中的body最终转换为一个`Mono`对象，其概念有些类似于Java8中的`CompletableFuture`,我们可以用类似的方式来组合多个元素，
实现流式操作。该类型本质上实现了Reactive Stream中的`Publish`接口。

`exchange()`方法无法处理服务端返回的非2XX响应，如果需要处理，可以使用`retrieve()`方法来做，譬如

```java
Mono<Person> result = client.get()
    .uri("/persons/{id}", id).accept(MediaType.APPLICATION_JSON)
    .retrieve()
    .onStatus(HttpStatus::is4xxServerError, response -> ...)
    .onStatus(HttpStatus::is5xxServerError, response -> ...)
    .bodyToMono(Person.class);
```
上述的`onStatus`方法的第二个参数可以用lambda表达式书写，以得到更好的可读性。

**TBD**

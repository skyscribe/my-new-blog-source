---
layout: post
title: HTTP/2和gRPC - 微服务时代的应用基础协议
comments: true
categories: [design, protocol]
tags: [design, architecture, microservice]
---

大部分的规模较大的商业应用软件后端系统如今都采用了分布式软件架构，并沿着SOA -> 微服务 的路径在往前演进；并按照领域驱动设计的绑定上下文的设计思路来切分服务；
服务之间的接口则不约而同地选择了HTTP协议作为基本的交互协议，背后的原因很大一部分应该来自于HTTP协议简洁、清晰的设计（尽管功能非常复杂）和随手可得的协议栈实现。

可惜HTTP协议并不是完美无缺的选择，Google早就在Chrome浏览器中尝试去改进HTTP/1.1协议中的一些不足并提出了开放的SPDY协议；这一尝试基本为HTTP/2的提出铺平了道理。
同时在Google内部，Protobuf作为其内部的跨语言接口定义语言已经被使用了很长时间；在Google之外的一些商业组织中，Protobuf也得到了广泛的应用。
两者的结合则已经对微服务基础设施领域增添了新的可能性。

<!--more-->

## HTTP协议

### HTTP的设计特点和历史

### HTTP/1.1的不足

### HTTP/2的特点

### TLS的争议

## gRPC项目

### RPC协议的由来

### gRPC

### 其它开源RPC中间件

## 参考

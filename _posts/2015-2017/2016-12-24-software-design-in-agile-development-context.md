---
layout: post
title: Software Design in Agile Development Context
date: 2016-12-24 19:32
comments: true
categories: [design, engineering]
tags: [design, agile, methodologies]
---

在目前大部分的软件开发组织中，敏捷开发已经成为毋庸置疑的标配。随着数位技术大神和布道师的宣扬和数量庞大的敏捷教练的身体力行式推广，商业环境和客户需求变更速度的日益加快，采用端到端交付周期更短的敏捷开发过程基本已经成为项目成功的必要条件。

<!--more-->

## 软件设计的刚需被敏捷了吗？

工作流程的变更以及开发节奏的加快并不能绕开一个很核心的问题：**写出容易维护方便扩展的代码的复杂程度本质上没有改变**；软件的维护周期越长，迭代的版本越多，这个基本问题就越突出。要想顺利解决这一问题，只能**依赖于系统具有相对良好的设计**，使得添加新的功能不会轻易破坏原有的结构，出现问题的时候，不需要大范围地对系统做出变更。

传统的瀑布式方法希望通过借鉴成熟的建筑行业的做法，采用预先大规模的架构设计，对系统做好明确的分割；继而进行不同层次的设计，直到所有可以预见到的需求都得以满足，然后才开始进行的代码的编写和构建。这种方法生产出来的软件交付工期很长，适应性很差，除了少数特殊行业之外基本已经被市场所淘汰。

[敏捷宣言](http://agilemanifesto.org/) 提出了一些基本的原则来指导我们怎样用相对更"**敏捷**"的方式开发和交付我们的软件；通过多个不同的迭代，增量式的构建和持续交付系统来降低风险。然而软件本身的复杂性导致我们不能将客户的需求一对一的翻译成代码，像搭积木一样构建出来一个可以轻易维护的系统。因为新加入的需求很可能导致原有的代码结构无法适应新的需求；某些为了尽快完成需求而做出的**关键的假设可能必须被打破**导致添加新的需求会破环大量已有的功能。如何做出恰如其分的软件设计，既能满足现有的短期需求，又能平衡潜在的变更。

各种不同的敏捷实践方法论对如何管理用户的需求，如何增强不同角色的沟通，如何实施日常的开发和测试活动，如何验证需求保证已经交付的承诺不被变更所破坏，如何规划和平衡资源和进度等复杂问题都给出了丰富的可选实践供项目管理人员裁剪；对于如何做软件设计以及做多少软件设计并没有很详尽的描述。就连基本的是否需要软件设计，以及需要多少软件设计，怎样算作过度设计都语焉不详。

## 真的需要软件设计吗？

大部分情况下，对于这个基本的问题我想答案应该是肯定的，除非你是在做一个很小的个人项目。如果需要牵扯到多个人一块合作并且最终的产品需要维护比较长时间，那么起码**某种程度的软件设计**应该是不可或缺的。毕竟**软件开发活动本身也是围绕着人展开**的，既然需要多个不同知识背景，不同技能，不同角色的人一起来协作交付功能复杂多变的软件，那么必然需要一些设计保证**参与其中的人有一致的理解**。

敏捷运动的早期一个常见的误解就是，敏捷软件开发不需要软件设计，不需要软件架构，只需要采用极限编程，将人们聚集一个公共空间里，直接动手写代码就行了；复杂的设计文档都是浪费，都是可以避免掉的，代码就是最好的设计文档。这样的过程可能适合于几个能力超强的程序员聚在一起做的临时小项目，放到更广泛的商业环境则难以持续下去。人员的流动，特殊需求的变更，性能问题的修补会使一个一开始看起来极其简单的几个源代码文件组成的小项目演进成**难以维护的“庞然大物”**。

如果系统没有明确的分工和边界，没有相对清晰的职责分工和交互限制，软件的结构很容易陷入“大泥球”结构而不可维护，试想如果代码里的每一个包或者类都有可能和另外其它的任意一个类有交互关系，即使是一个绝对代码行数很小的项目也会变得无法继续添加新的功能。

## 哪些东西应该包含在软件设计中？

所谓的设计其实可以理解为关于如何组织软件各个部分（特性和行为分割）的一些决策，以及做出相应决策的一些原因。

敏捷场景下，重构的重要性以及早已经深入人心，因而容易经**由重构来去掉“坏味”的部分就不宜放在设计**中。因为一般为了重用的目的都会将设计决策写下来供后续使用；如此一来必然产生一些维护成本；而维护设计文档的开销一般比代码要大很多。因此容易通过重构而优化的部分，放在专门的软件设计中显得有些得不偿失了。毕竟敏捷软件开发的基本思路就是**消除浪费**，使得**投入产出比最大**化。

某些跟具体实现技术相关而和核心业务需求关系比较远的决策，大部分也**不适宜包含在软件设计**中。譬如期望某部分关键数据需要做持久化以保证系统异常重启的时候依然可以恢复。对于业务需求而言，这块数据需要持久化是重要的，但是如何做持久化，又可能是易变的，譬如今天是考虑用文件来做持久化就可以了，将来可能发现不够必须用关系数据库，或者甚至关系数据库可能也不是一个合适的选择，得要用键值对数据库。**识别到可能变化的部分，并将不变的部分抽象出来，放入设计中**可能就足够了。这样技能照顾到当前的需求，又能满足将来扩展的需要。至于具体是怎样实现的，看代码就足够了。

需求的**概念抽象化，和软件的静态模型可以作为设计的中心**之一，必须详细考虑并归档维护。之所以要对需求进行抽象化处理，是因为用户的期望可能是模糊不清的，甚至是“朝令夕改”的。敏捷方法强调持续交付就是为了使用户早期得到反馈，**及时修正他们的需求**，更好的管理客户的期望，避免开发出不符合客户真正预期的产品，浪费开发资源不说也浪费了客户的投资。软件的静态模型是关于大的软件职责的拆分和交互边界，这一部分不仅是当前进一步开发的基本依据，日后万一需要重构也是很重要的参考，值得花力气仔细讨论达成一致，减少日后维护成本。

软件的部署和核心模块的交互在有这方面的变更的时候（新加入模块或者服务等）也需要仔细考虑并作为软件设计的关键活动。模块的边界是粗粒度的系统耦合的地方，一些关键的交互流程也适宜详细讨论并放在软件设计文档中。

系统核心的模块/类以及之间的交互，如果有发生变更，也需要第一时间考虑清楚并放置在设计过程中产生合适的产出，便于沟通和交流。如果模块的粒度足够大（譬如估计有很多的代码），那么哪些部分是对外交互的接口也应该提早考虑清楚，并提取出来以便后续写代码以及代码评审的时候对照，确保设计被正确遵守。

## 什么时候应该停止继续设计？

敏捷语义下，任何的浪费都是可耻的，代价巨大的设计工作自然也不例外。知道何时还需要仔细讨论搞清楚，何时应该停止变得尤其困难，甚至需要一些接近于艺术化的方法，**需要经过大量的实践经验累积和反思**才能做到不偏不倚。

如果发现所讨论的问题可能代码实现很容易就能完成，如果考虑不完备，那么修改代码的代价非常小，那么就可以立即停止了；因为设计的目的是为了更好的写代码，更好的维护既有的代码。因此只有重构代码的代价远远大于预先仔细设计付出的代价是，才应该花费力气去做这些烧脑的工作。发现代码实现已经很容易没什么问题的时候，就放手去写代码或者重构代码吧。这种情况往往发生在我们想去“设计”一些内部实现细节，而这些细节对模块的边界以及待修改模块和核心部分耦合很小的情况。

如果是在构建一个新的模块，而这个模块和已有的系统有形形色色的复杂联系（耦合），那么如果这个模块和已有的系统的各个部分的交互已经比较清楚，而且其内部实现估计工作量也很小的时候，那么就可以放心将剩下的工作交给聪明的程序员去继续了。将一些**细微的工作也放入设计中，只会使设计文档变得庞大而又难以维护**。毕竟可工作的代码比完美的文档更重要，虽然后者也很有价值。

如果对于是否应该继续设计有分歧，可以和其它准备实现的程序员坐在一起讨论（其实任何时候都应该如此，如果团队规模比较小而且时间允许），看将要写代码的程序员是否觉得足够清楚，返工的风险是否足够得小。如果对于一些核心的模块或者类的职责还有不同的认识，或者程序员不知道某些改动是应该新创建一个子包，还是应该在已有的摸个包中修改来实现，那么**很可能有些关键的部分没有设计**清楚。

决定何时应该适可而止也和你的程序员团队的**实际水平和能力**密切相关。一群天才程序员可能需要极少的设计来达成基本的共识就可以产出高质量易维护的代码，而水平平庸的程序员团队则需要更多设计上的预先讨论沟通以达成基本共识，减少返工。

## 工具

软件工程的一个关键要素就是工具。软件设计自然也离不开合适的工具，尤其是软件设计又是对需求进行抽象的结晶；选择合适的工具可以增进协作和沟通，使得设计输出是有实际指导作用的而不仅仅是纯粹的文档工作。

**轻量级的文档工具**往往使维护和修改变得更加容易，因为设计输出本身也是一个迭代的过程；便于多人评审和协作显得尤其重要。目前主流的方式基本都是基于Markdown和Plantuml的；前者可以用来存放文本，后者则可以用文本的格式来描述UML图。
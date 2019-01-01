---
layout: post
title: 用Rust实现一个高效的REST解析工具
comments: true
categories: [programming, language, study, notes]
tags: [design, programming, language, rust]
---
公司里面进来切换到了一个新的Backlog管理工具来管理日常的需求和项目规划，可惜新的工具虽然是名门之秀的JIRA（来自于澳大利亚的ＩＴ公司confluence），但是访问起来却异常痛苦，
经常一个页面打开需要等待大半天还是不是崩溃。有些心急的同时已经默默地回到了老的方式，导出数据到Excel然后打开Office工具来搞定。
作为一名程序员，最简单直接的想法就是**自己撸起袖子来写代码解决**呗，即使是投入产出比不高，好歹是在写工具的过程中提高了自己，一举多得何乐而不为呢？

<!--more-->

之前用各种Shell/Awk/Sed/Grep等传统的脚本语言实现了太多类似的解析工具，这里不想再重复自己，因为耍自己熟悉的套路还有点浪费时间和生命的感觉。
简单翻阅了一下JIRADC的RESTAPI文档，发现它的API设计的还是比较完备的，当然也意味着**解析的任务比较繁重**，除了解析基本的JSON文档，还要处理多个记录的分页。
考虑到服务器用浏览器打开都经常慢或者超时，那么最好也要能实现并发的请求和处理。这么多的需求考虑下来，其实传统的脚本语言的确有些勉为其难了。
当然还可以考虑被称为完成胶水的python语言，只是这么大的数据量和多并发处理，想想也挺繁琐的，况且还没有新鲜感，这个也是很重要的。

于是便想到了之前每次学一半就因为太无聊而放弃的Rust语言；之前是因为只顾着读文档，走马观花地读，到了中间就读不下去了，因为**之前太多的概念没有动手实践，总会很容易就卡壳**；
而Rust毕竟是被成为学习曲线很陡峭的严肃的编程语言。说不定这次可以进步更多呢，毕竟这次可是带着目标的去深入学习的，很多大师们都分享过这一绝招
> 深入学习一门语言还是要带着实际的项目去编码，才会更有效率。

当然这些项目大多是一些私人的小项目，这里正当其时。当然我想拉取的是服务器上数千人的数据，那么数据量本身也是很大的；
这方面Rust语言本身的运行效率也是我想观察和思考的一个方面。
起码从它的实际目标来看，作为一门面向系统编程领域并追求和C++一样的**零成本抽象**设计目标的Rust语言来说，用来实现高效的文本解析工作也不应该有什么难度。

## 实现
主要的需求就是字符串解析，而且是需要解析RESTAPI中的结构化数据，并按照自己的需要来分析和处理数据。
基本的HTTP协议解析和JSON处理是必然少不得的；这方面只要按照官方文档，准备好对应的cargo文件即可，一个命令就可以拉下来所有需要的第三方包的最新版本，这方面比C++进步太多了，那边厢module的提案还在遥不可及的草案讨论中呢。

### 异步编程处理

因为需要处理服务器响应慢的问题，初步估算程序运行的瓶颈应该在IO，毕竟这可是一门系统编程语言，
就那么几千条文本数据的反序列化和查询变换，应该是可以在几百毫秒级别的运算中完成的。
因此程序的设计上必然要用到多线程处理和异步编程，可惜Rust对协程的支持还没有完善，也没什么大问题，启动多个线程来做就可以了。
这方面**Tokio库是当之无愧的王者**，直接拉下来用就好了，官方的crate文档写的不错，例子也很简单易懂，
很多处理和JDK的executor处理很像似，即使不读文档，**很多API望文生义也可以差不多**工作。

使用中发现Tokio其实已经是一个比较大的库，上一次关注它的时候还是个比较简单精致的小工具库；
随着多个版本的迭代和功能的丰富，它在提供对底层的异步操作处理的基础上还增加了上层通信模型的封装，
并提供了它自己的`future`实现，这方面和标准库中的类似设施并不完全一致，用的时候不加留意很容易陷入奇怪的编译错误无法自拔。

考虑到服务器端本身不是很稳定，必须要处理好可能的失败，并且收集成功的查询，并发起对失败查询的重试直到成功为止；
HTTP协议的无状态保证和REST API的分页查询机制，使得我们只要保存查询条件，即可很方便地重试知道成功。
基本的带重试的查询是按照两步查询的思路进行的，第一步先查到总共的记录个数以便决定接下来还有多少个记录待查，
第二步则查询第一页之外的记录。

出于简化期间，第一次可能的失败没有处理，大致的处理代码如下
```rust
//Search by given jql and issue fields, and collect all results in one single 
// result, 2-phases based search is used to calculating paging properly.
pub fn perform<T: DeserializeOwned>(&mut self, jql: &str, fields: Vec<String>, 
        result: &mut QueryResult<T>) {
    let search = Query::new(jql.to_string(), 100, fields);
    //first search
    self.reset_pending(vec![search.clone()])
        .perform_parallel(result);
    if result.issues.len() == 0 {
        error!("First search failed?");
        panic!("Unexpected ending!");
    }

    //remaining
    info!("Got first result now, check remaining by page info!");
    self.reset_pending(search.create_remaining(result.total))
            .perform_parallel(result);
}

fn perform_parallel<T: DeserializeOwned>(&mut self, result: &mut QueryResult<T>){
    let (tx, rx) = channel();
    while self.pending_jobs.len() > 0 {
        self.finished.clear();
        self.drain_all_jobs(&tx);
        self.collect_all_responses(result, &rx);
        self.clean_finished_from_pending();
    }
}
```
这里采用典型的`channel`结构来交换信息，因为实际的查询处理是放在future里面并行处理的；
这也是Rust Book里面示例给出的方法，**虽然有些麻烦但是肯定不会出错**。
主要的查询调度是通过**联合所有的子查询并合并为一个组合的future**来完成的，这是个典型的函数式编程范式的应用句号；
每一个子查询将它自己的标识符和处理结构反馈到消息通道里面返回给调用者。

```rust
fn drain_all_jobs<T: DeserializeOwned>(&mut self, 
        sender: &Sender<Result<Box<QueryResult<T>>, usize>>) {
    let mut sub_queries = Vec::new();
    //Drain all pending jobs 
    for qry in self.pending_jobs.iter() {
        //query this page
        let sender1 = sender.clone();
        let parser = move |json: &str, code: StatusCode| {
            let my_result = match code{
                StatusCode::Ok => parse_query_result::<T>(&json).ok_or_else(|| qry.startAt),
                _ => Err(qry.startAt),
            };
            let _x = sender1.send(my_result);
        };

        let post_info = RequestInfo::post(self.uri, &qry.to_json().unwrap());
        let guard1 = sender.clone();
        let sub_fetch = self.fetcher.query_with(post_info, self.core, Some(parser))
                .map_err(move |err| { 
                    //TODO: handle exceptions in graceful manner?
                    warn!("This job {} has failed by {}", qry.startAt, err);
                    let _x = guard1.send(Err(qry.startAt + 10000)); 
                    "failed"
                });
        sub_queries.push(sub_fetch);
    }
    let _x = self.core.run(join_all(sub_queries));
}
```

### 协议解析和数据结构化处理

HTTP的解析毕竟繁琐，好在`hyper`库的功能非常完备，一般能想到的都已经有支持了，
包括proxy的设置，请求响应的封装，都有丰富的API可供调用，直接用起来就行了，倒也没有太多的难点；
唯一稍微让人觉得有些不一致的是它的请求消息的设计采用的是典型的函数式的不可变设计，需要**用一个builder来构建**，稍微熟悉一下即可。

REST数据的处理上，有赖于REST本身强大的`Trait`抽象支持，即便是没有官方的基于class的多态支持，
基于抽象数据类型的serde抽象还是提供了强大的代码复用能力；形形色色的第三方库只要声明对各种数据类型的`serde`支持，
就可以**方便地以非侵入的方式提供**给用户。只是从语法上，用户需要定义自己的结构体，
并且用头上加注解的方式添加声明让编译器知道该数据可以被放置在对应的数据解析的上下文中。

Rust自身也默认规定了命名风格，并且对不符合它期望的命名一律报以警告处理；而第三方的API中的数据恰好和这一风格相悖；
好在`serde_json`库也支持自己重命名另外一个名字，这样就可以绕开命名风格不一的问题，只是实际写出来的代码可能有些臃肿；
还有一种简单的办法就是告诉编译器下面的名字不遵循默认的下划线分隔的命名风格，没有很强的强迫症或者赶时间的话到时可以务实一点跳过编译器的警告。

具体的绑定json的查询结果的结构如下

```rust
extern crate serde;
extern crate serde_json;

use self::serde::Deserialize;
use self::serde::de::DeserializeOwned;

#[derive(Deserialize, Clone)]
#[allow(non_snake_case, dead_code)]
pub struct QueryResult<T> {
    expand: String,
    
    pub startAt: usize,
    
    //not used
    maxResults: usize,

    //total records
    pub total: usize,

    //actual issue structure
    #[serde(bound(deserialize = "T:Deserialize<'de>"))]
    pub issues: Vec<T>,
}
```

最后一个字段稍微有些复杂是因为程序中有多个不同条件的查询，返回的结构可能是不一样的，`serde_json`
可以支持**用泛型的方法来定制可扩展的抽象类型**。内部泛型中指定lifetime的做法在这里显得特别突兀，
但是暂时也没有更优雅的办法，因为出于效率的考虑，默认的Rust数据结构是具有移动语义的，
这里**必须加上对应的生存周期的约束**，以便编译器可以在编译期间做好安全检查，防止非法的数据访问。

### 结果的处理
解析的结构都放在容器中，用于对取到的结果分析然后提取自己想要的信息，这方面的处理其实倒是平淡无奇，
无非是一些常规的查询和处理；当然由于本身数据是不可变的，用函数式的方法写出来代码更加清晰自然易懂。

Rust本身提供了丰富的函数式操作类型，并有强大的 `iterator`抽象了丰富的操作组合，但是某些稍微复杂一点的处理还是需要不少重复劳动；
`itertool`这个工具库提供不少高级的功能；对于熟悉函数式编程的程序员来说，这一高级工具不容错过。比如下面这个是过滤其中某个特定领域的条目病按照某个给定条件分组之后再行统计规划状态的例子

```rust
//check planning status
let mut planned = 0;
let mut unplanned = 0;
for (fid, mut sub_items) in &items
    .into_iter()
    .filter(|it| area_features.binary_search_by(|fid| cmp_with_prefix_as_equal(fid, it.sub_id.as_str())).is_ok() )
    .group_by(|item| get_system_split(&item.sub_id).clone()) {
        //check if ET planned
        let test_status = if sub_items.any(|it| it.activity == Activity::Testing) {
            planned += 1;
            "planned"
        } else {
            unplanned += 1;
            "not planned!"
        };

    let line = format(format_args!("Fid = {}, Testing status ={}\n", fid, test_status));
    buf_writer.write(line.as_bytes()).unwrap();
}
```

## 编译驱动开发
其实在官方的指导文档(Rust Book)中，几位布道者就提出了所谓的**编译驱动开发**的编程实践；一路实践下来果然深有体会;
因为写代码实现的过程中，大部分在其他语言中用来调试的时间都被挪用到了和编译器做斗争的事情上。
一个原因可能是本身对某些第三方库的设计还不是很熟，需要边写代码边阅读它的设计文档，
另一个原因也许是跟Rust语言本身的设计哲学有很大的关系，因为它将编译器的静态检查能力推向了一个极致的地步；
可以说**编译器的检查是极端保守的**，任何可能造成程序不稳定或者有数据访问冲突或者踩踏的行为，都不会呗编译器放过。
也只有做到了这样，它才可以大胆地保证，绝大部分情况下**只要你的程序通过了编译，那么运行起来也就是没有问题的**。这种思路在软件工程上不得不说是一个很激进的尝试。

相比较与传统的Ｃ和C++语言选择相信程序员，还要想尽量贴近硬件，还想要给程序员很多高级的武器以提高生产效率，
最终在实际的大项目中却往往走入难以为继的泥潭；也许Rust提出了**一条更为坚实但是也更为艰难的路**，有多少程序员可以克服最初的不适应期都是个很大的难题；
尤其是关于生存周期检查的处理非常晦涩难懂，不深入了解它的原理，有可能就会被奇怪的编译错误所吓倒；
而出于性能的考虑，又不好轻易地选择copy数据结构，选择往往未必是那么容易。

## 效率的问题
毫无疑问，效率是一开始学习和思考的出发点之一，最终程序写完了自然要考察一下它的执行效率。
不出意外对于这个简单的小程序而言，数千条数据的解析和处理都可以在几百毫秒内完成；效率还是非常令人满意的。
毕竟没有很好的运行效率的话，这门艰深的语言几乎没有什么存在的价值了；
serde_json库称自己是最快的JSON解析库，还真不是白吹的。

希望下次再动手写一个工具的时候，还可以再次深入的学习Rust的其它美妙之处。
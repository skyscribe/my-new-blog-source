---
layout: post
title: 用Rust来辅助报表解析
comments: true
categories: [programming, language, study, notes, tools]
tags: [design, programming, language, rust, tools]
---

最近遇到一个麻烦而繁琐的操作，需要从工作用的缺陷跟踪系统中导出报表，分析其中的一些数据模式，分类整理后用更好的方式整合出来；
本来这类反复重复的零碎工作，如果不是很复杂，依赖于Excel本身的强大的表格计算功能是很容易自动化的(在线系统比较古老仅仅支持ＣＳＶ格式导出)，
只是内部的字段量比较多，并且有很复杂的相互关联，用**脚本来自动化**是最直接的一个想法。

刚好今年花了比较多的时间深入学习Rust了，这么这个分析工具可以作为**第二个很好的练手项目**，
刚好可以用来更深入地研究和体会这门以系统编程和高效率著称的语言。

<!--more-->

## 主要目标
这里想解决的问题主要有几个方面的难点
1. 数据量比较大，单单导出的CSV文件就有30~40MB的文本
1. 需要提取的信息毕竟复杂，各个字段之间有很多相关性，需要解析之后做二次处理
1. 导出格式毕竟复杂，基本用到了CSV格式文本中的所有高级特性，比如字段内包含分隔符和空行，某些字段可能是空等
1. 处理的结果需要再次保存为csv格式的输出，以便结合Excel本身强大的报表功能，展现给最终的目标用户

这里的字符串处理任务会非常繁重，因为有些字段包含有自由文本的信息，而需要提取的信息又毕竟散乱，
并没有很好的规律可寻，必须**要有强大的正则表达式引擎**，否则处理效率必然堪忧。
即便是抛开学习的目的想快速解决问题也很有挑战，因为传统的Unix工具(sed/awk/grep)组合能力虽然很强，却没有很好的**复杂CSV解析**的能力。

## 实现思路

处理程序本身的核心逻辑其实是个数据萃取和转换，需要从输入文件中读取原始输入，格式化为记录数据，
然后根据业务特点做过滤、提取、组合和分组，填充为新的记录格式，保存为文件。
大部分的繁琐的地方其实是在细节中，比较**考验Rust语言的表达能力**，毕竟这是传统的脚本语言的专长，严肃而正经的静态语言实现起来往往是有手脚被束缚的无力感。

现代的编程语言都在慢慢地往函数式编程的路子上靠拢，因为函数式语言设施更容易写出声明性的代码；
所以换句话说实现好这个工具写出地道的代码的**重要条件还是需要好好熟悉Rust的ＦＰ**特性。

## 数据数据的解析和处理

Rust社区的serde库提供了各种各样常见的文本格式的序列化和反序列化抽象，而[CSV](https://docs.rs/csv/1.0.5/csv/)提供了灵活而高效的读写CSV文件的能力，
并且和serde框架无缝融合，简单拉取就可以很简单的使用；虽然serde的文档不是很容易理解，
好在已经有前边使用json的经验，用起来**只要结合直觉和官方文档**，便没有什么压力。

稍微有些讨厌的是工具导出的原始CSV包含了可恶的汇总信息，这些信息并不遵循CSV的语法，
需要在解析之前先将这些额外的头去掉，然后再放入csv处理；该额外处理本来用一行shell脚本就可以很简单的完成:
```shell
cat input.csv | sed "1,7d" > input_trunct.csv;
```
考虑到已经用Rust来写了，就想让这个**工具变得纯粹一些**(为了学习的目的可以给自己加一些复杂一点的需求也算可以理解的吧)，能在Windows环境直接使用岂不是更好?
并且考虑到原始的文件可能比较大，正儿八经地实现的时候，不自然就加入了个缓冲机制，避免每次运行的时候都要做这个剪切操作，只需要运行起来的时候，判断文件在不在就可以了；
这方面需要用到文件系统访问相关的API，好在这方面Rust的标准库已经提供了(不知道比C++好了多少呢)

文件的路径需要从输入来，所以用一个结构保存文件名和它对于的缓存文件；**高效的前提来自于减少内存的拷贝**，所以这里需要用lifetime声明显示地指出这是一个引用
```rust
pub struct InputFixer<'a> {
    raw_fname: &'a str,  
    local_fname: String, 
}

impl<'a> InputFixer<'a> {
    pub fn new(fpath: &'a str) -> InputFixer<'a> {
        InputFixer {
            raw_fname: fpath,
            local_fname: format!("{}_trunct.csv", fpath),
        }
    }

    pub fn fix(&'a self) -> Result<&'a str, std::io::Error> {
        if Path::new(&self.local_fname).exists() {
            Ok(self.local_fname.as_str())
        } else {
            self.fix_and_save()
        }
    }
    //...
}
```
对外提供的处理函数是通过一个基本的缓存文件检查来实现的，如果存在则直接返回，如果不存在，就调用真正的预处理。
真正的处理是通过跳过当前行直到遇到期望的表头而中止，然后将剩余的行拷贝到中间文件中，形成一个没有额外信息的CSV文件
```rust
//Remove extra headlines from reports, save until we found valid header
fn fix_and_save(&'a self) -> Result<&'a str, std::io::Error> {
    let mut outf = BufWriter::new(File::create(&self.local_fname)?);

    let inf = File::open(self.raw_fname)?;
    let mut line = String::new();
    let mut reader = BufReader::new(inf);

    let mut found_header = false;
    while !found_header {
        reader.read_line(&mut line)?;
        if line.find(r#""Problem ID","Title","#).is_some() {
            found_header = true;
            outf.write(format!("{}", line).as_bytes())?;
        } else {
            //warn!("Skip bad line {}", line);
        }
        line.clear();
    }

    info!("Copying remaining bytes...");
    loop {
        let buf_len = {
            let buf = reader.fill_buf()?;
            if buf.len() > 0 {
                outf.write(buf)?;
            }
            buf.len()
        };

        if buf_len == 0 {
            break;
        }
        reader.consume(buf_len);
    }
    info!("New file created = {}", self.local_fname);
    Ok(self.local_fname.as_str())
}
```
主要的读写方式是需要按照行来判断和进行，可以用`BufReader`/`BufWriter`来提高效率，这一实际的数据其实经过了一次按块读取的拷贝，而这里的代码处理不需要任何拷贝。

实际测试了一下在我的笔记本上，完成30MB的文件处理，可以在300毫秒内完成，效率还算让人满意；当然运行多次后因为磁盘缓存的原因，时间会缩短到100毫秒，基本不会太影响真正的操作了。

## 主处理逻辑

借助于Rust丰富的迭代器抽象和函数式风格支持，主处理逻辑可以用很**直观的流式代码风格**写成
1. 在上述代码的预处理之后，将输出文件传入到CSV库中，
1. 完成反序列化处理，并将反序列化后的记录收集到一个容器中
1. 交于后续的解析和处理函数保存

如下面的代码，含义基本上都是**声明式而不言自明**的；稍微有点晦涩的是那个`as_mut`转换，完全是因为后面的反序列化解析操作必须要一个可读写的对象才可以
```rust
pub fn analyze(fpath: &str) {
    info!("Started to parse {}", fpath);
    let fixer = InputFixer::new(fpath);
    let _result = fixer.fix().map_err(|err| error!("Truncting file failed by {}", err))
        .ok()
        .map(|fpath| Reader::from_path(&fpath).as_mut()
            .map(|rdr| { 
                rdr.deserialize::<Record>()
                    .filter_map(|it| it.ok().map(|rec| ParsedRecord::new(rec)))
                    .collect()
            })
            .map(|records| parse_and_save(&fpath, records))
            .map_err(|err| error!("CSV parsing failed by {}", err))
        );
}
```

## 文本解析工具和流处理工具

Rust的字符串分由`String`类型和`str`类型两者配合完成，一般在函数参数或者返回传递的过程中，多使用`&str`类型，并且很多情况下，编译器也可以自动完成从`String`的引用到`&str`的转换，
在某些不能自动转换的情况下，可以调用`as_str()`函数来得到；只要**通过了编译**(毕竟是举起了编译器驱动开发的大旗)一般就没有什么问题了。

正则表达式需要对应的[regex](https://docs.rs/regex/1.1.0/regex/)库就可以了，使用的是类perl正则表达式语法，
基本的元字符支持的也比较全，借助于语言本身提供的raw string语法，复杂的正则表达式也照样不需要担心可读性；
当然官方文档的建议是最好不要使用不必要复杂的正则表达式，以免影响效率。
同样出于效率的考虑，正则表达式最好要**先编译再使用，并且保证只编译一次**，考虑封装的情况下，单例模式是最自然的选择；
好在可以用`lazy_static`方便地封装。

考虑到一个字段可能有多个可能的模式需要提取，写出来的代码可能毕竟复杂；借用Rust本身的Option类型封装，
可以用非常具有可读性的代码写出来，这里想从导出的修订历史信息中有导出多条记录，这些记录可能有不同的结构，对于的正则表达式如下
```rust
//Parsing given R&D information and filter out interested items
pub fn parse(revision: &str) -> Vec<RevisionItem> {
    lazy_static! {
        static ref PATTERN_DELIMITER : Regex = Regex::new("(, )?(201[789]-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}) ").unwrap();
        static ref PATTERN_STATECHANGE: Regex = Regex::new("(.*) The state of the problem changed from ([a-zA-Z ]+) [tT]o ([a-zA-Z ]+)").unwrap();
        static ref PATTERN_STATECHANGE_EX: Regex = Regex::new("(.*) State changed from ([a-zA-Z ]+) to ([a-zA-Z ]+)").unwrap();
        static ref PATTERN_TRANSFER: Regex = Regex::new("(.*) The group in charge changed from ([_A-Z0-9]+) [tT]o ([_A-Z0-9]+)").unwrap();
        static ref PATTERN_STATE_REOPEN  : Regex = Regex::new("(.*) Reopened").unwrap();
        static ref PATTERN_ATTACHED: Regex = Regex::new("(.*) Problem attached to ([^. ]+)").unwrap();
        static ref PATTERN_DETACHED: Regex = Regex::new("(.*) Problem detached. The problem report was detached from ([^., ]+)").unwrap();
        static ref PATTERN_POSTPONE: Regex = Regex::new("(.*) ((Postponed)|(Reactivated))").unwrap();
    }
```

里面用小括号括起来的字段是我们想提取的关键信息，可以用连接的方式一次解析几个正则表达式如下
```rust
    PATTERN_DELIMITER
        .replace_all(revision, |caps: &Captures|format!("\n{}|", &caps[2]))
        .split('\n')
        .filter_map(|line| line.find('|').and_then(|pos| {
            let (when, rem) = line.split_at(pos);
            let rem = &rem[1..rem.len()];

            PATTERN_STATECHANGE.captures(rem)
                .or(
                    PATTERN_STATECHANGE_EX.captures(rem))
                        .map(|caps| (caps[1].to_string(), StateChange{
                            from: caps[2].to_string(), to: caps[3].to_string()}))
                .or(
                    PATTERN_TRANSFER.captures(rem)
                        .map(|caps| (caps[1].to_string(), GroupChange{
                            from: caps[2].to_string(), to: caps[3].to_string()})))
                .or(
                    PATTERN_STATE_REOPEN.captures(rem)
                        .map(|caps| (caps[1].to_string(), StateChange{
                            from: "Finalized".to_string(), to: "New".to_string()})))
                .or(
                    PATTERN_ATTACHED.captures(rem)
                        .map(|caps| (caps[1].to_string(), Attached {to: caps[2].to_string()})))
                .or(
                    PATTERN_DETACHED.captures(rem)
                        .map(|caps| (caps[1].to_string(), Detached {from: caps[2].to_string()})))
                .or(
                    PATTERN_POSTPONE.captures(rem)
                        .map(|caps| (caps[1].to_string(), if caps[2].find("Postpone").is_some() {Postponed} else {ReActivated} )))
                .map(|(auth, extra)| RevisionItem { 
                    author: auth, when: when.to_string(), extra: extra
                })
            })).collect()
```

### 变换处理

对于上述解析出来的原始记录，这里需要额外的处理，比如找到这个列表中的第一个组信息，
因为历史记录读进来的顺序是按照时间书序从新到老读取的，我们需要从尾部拿到第一个组变更的信息返回，病需要考虑没有这种记录的可能，
实现代码其实就是对`Iterator`这个抽象类型的操作的组合调用
```rust
#[derive(Debug)]
pub struct Revisions {
    revisions: Vec<RevisionItem>,
}

impl Revisions {
    pub fn get_first_group(self: &Revisions, mygrp: &str) -> String {
        self.revisions.iter()
            .rev()
            .filter_map(|x| x.get_from_group())
            .nth(0)
            .unwrap_or(mygrp)
            .to_string()
    }
    //...
}

//definition of get_from_group in RevisionItem struct
pub fn get_from_group<'a>(self: &'a RevisionItem) -> Option<&'a str> {
    match self.extra {
        GroupChange{ref from, to:_} => Some(&from),
        _ => None,
    }
}
```

`filter_map`是个很有用的组合函数，它可以完成过滤和转换的组合功能，并且能够处理`map`函数返回一个`Option`类型的能力
* 如果结果为`None`，则原来的元素就会被跳过
* 如果不是`None`，则把内部封装的元素取出作为后续的输入

`nth`又是一个毕竟特殊的函数，可以从结果里面取出第N个元素，如果不存在则会返回空，所以起结果本身是个`Option`类型；
显然**作为基础的`Option`类型已经充斥在标准库的各个角落**里，同样也只有做到了这样，才能发挥函数式编程的巨大威力。

## 简单的图算法

稍微复杂的一个处理是需要找出各个记录之间可能存在的关联关系，然后将具有相互关联的记录中，选择一个作为代表性的记录，
而把其它的记录都设置为重复的记录，这个对于实际的报表汇报尤为重要；同时选择的方法需要是可以定制的。
比如记录A关联到了B,C,D,而Ｃ又关联到了D,E;那么最终我们需要从A/B/C/D/E中按照外部传入的算法选择一个作为主记录，
然后其它都作为辅记录。稍微有点复杂的是，因为系统原因，这些**相互关系在原始输入数据中是不对称**的，
但是简化起见，不考虑这些错误（比如上例中就没有Ｃ到A／Ｂ的记录），认为关联关系是对称的，只要在处理中修复这种错误即可。

这本来是一个非常典型的图算法，用无向图可以很容易地表示，可惜没有找到很简单的图算法库；
不过用内置的集合结构来实现一个基本的图算法也不难，用两个数据结构，一个`HashSet`和`HashMap`就可以实现一个出来；
这里的**复杂性反而是由Rust的borrow checker引入**的。

基本的数据结构如下
```rust
struct AttachInfo<'a> {
    my_id: &'a str,
    attached_list: &'a str,
}

pub struct AttachGraph<'a> {
    pub mapping: HashMap<&'a str, &'a str>,
    //A set of nodes that are associated with one given first id
    pub components: HashMap<&'a str, HashSet<&'a str>>,
}
```

因为不想实际拷贝数据，所有的结构体都带入了一个外部传入的生命周期参数，以便编译器检查没有数据越界的情况发生；
生命周期的管理是Rust一个比较复杂和高级的特性，官方的指导书里面写的毕竟详细，这里不赘述。
外层的封装函数
* 接收解析好的数据记录集作为输入
* 外加一个可以作为主记录的集合以便选取主记录
* 返回构造好的图结构
```rust
//Construct a graph to check attach association, returns a graph of attach information as defined above
pub fn get_attach_relations<'a>(records: &'a Vec<ParsedRecord>, 
                        interested: &'a HashSet<&'a str>) -> AttachGraph<'a> {
    let records = records.into_iter()
                .map(|x: &'a ParsedRecord| AttachInfo{
                    my_id: &x.raw.pr_id.as_str(),
                    attached_list: &x.raw.attached_list.as_str(),
                }).collect();
    get_attach_mapping(records, interested)
}
```

主要的处理逻辑在内部封装的这个函数里实现,首先是构造输入数据，然后读入所有的记录，把对应的非对称的关联关系自动修复然后放入到对应的哈希映射表中，这里有２个映射表
* 第一个结构保存两两关联
* 第二个结构则收集传递关系，将所有传递关联的记录放在映射中
```rust
fn get_attach_mapping<'a>(records: Vec<AttachInfo<'a>>, interested: &'a HashSet<&'a str>) -> AttachGraph<'a>{
    let mut graph = AttachGraph {
        mapping: HashMap::new(),
        components: HashMap::new(),
    };

    //construct attach relation ship mapping
    // A -> B, B -> C, C -> D, E => A: A, B:A, C:A, D: A, E:E
    records.iter().for_each(|rec| {
        if graph.mapping.get(&rec.my_id).is_none() {
            graph.mapping.insert(&rec.my_id, &rec.my_id);
        }

        let from : &'a str = graph.mapping.get(&rec.my_id).unwrap();
        if !graph.components.contains_key(from) {
                //borrow mutable here!
                graph.components.insert(from, HashSet::new());
                graph.components.get_mut(from).unwrap().insert(from);
        }

        rec.attached_list
            .split(", ")
            .filter(|x| x.len() > 0)
            .for_each(|x| {
                if graph.mapping.get(&x).is_none() {
                    graph.mapping.insert(&x, from); 
                }
                graph.components.get_mut(from).unwrap().insert(&x);
            });
    });
```

基于上面这些输入数据，再考虑如何选取主记录，将所有的非主记录的索引都替换为符合条件的一个主记录；
因为默认情况下记录无法修改，这里用一个新的结构来替换和插入，然后在最后的地方重新绑定；因为是浅拷贝所以虽然有效率损耗，影响应该不大；
这本身也是函数式编程的一个无法忽视的问题。

```rust
    let mut new_mapping = HashMap::new();
    graph.components.iter()
        .map(|(from, attached)|
            if !interested.contains(from) {
                attached.intersection(interested)
                    .nth(0)
                    .map(|y| (*y, attached) )
                    .unwrap_or((*from, attached))
            } else {
                (*from, attached)
            }
        ).for_each(|(from, attached)| {
            attached.iter().for_each(|x| {
                new_mapping.insert(*x, from);
            });
    });

    graph.mapping = new_mapping;
    graph
```

## 效率和打包

最终在`release`模式下处理数据总共耗时不到900毫秒，考虑有缓存的话，多运行几次还可以更短，已经远远超出我的预料了。在大量正则表达式处理的情况下，即使是几千条记录的数据量，
因为很长的文本字段可能包好多个列表字段，需要几个正则表达式依次解析，即使是不考虑输入文件的复杂性，
用传统的Unix工具也未必能达到这么好的性能，即便是Perl/Sed/Awk的正则表达式实现的非常高效。

另外一个考虑的因素是如何共享最终编译好的程序给别人使用，本来考虑的是让别人安装VC++ Redistribute Tools然后试验了一下发现太繁琐了；
于是想查找是否可以静态编译所有以来的方法，幸好这里以来的主要是微软的Ｃ运行时库，`Cargo`工具链已经提供了很好的封装；
只需要在工程目录下放置一个特殊的配置文件 (`.cargo/config`)，告诉工具链需要静态编辑即可，里面的内容为
```ini
[target.x86_64-pc-windows-msvc]
rustflags = ["-Ctarget-feature=+crt-static"]
[target.i686-pc-windows-msvc]
rustflags = ["-Ctarget-feature=+crt-static"]
```

另外看到Rust语言社区的第二个大版本已经出来了，以后可以再花时间琢磨一下里面有什么重大更新，毕竟目前用的版本还比官方的Rust2018早一个版本。
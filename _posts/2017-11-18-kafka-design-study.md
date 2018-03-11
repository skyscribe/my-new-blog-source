---
layout: post
title: Kafka的设计为什么高效
comments: true
categories: [design, study]
tags: [design, architecture, performance, database, concurrency, microservice]
---

Kafka这一孵化于LinkedIn的开源项目正得到越来越多组织的关注和使用；其优雅的设计和对实时性处理的探索有很多值得学习的地方。
本文试就个人学习过程中的一些思考和理解来分析为什么Kafka可以兼顾可靠性、一致性和尽可能强的实时处理能力。

<!--more-->

## Kafka的基本设计

### 设计目标
Kafka的首要设计目标是一个高性能的分布式数据流处理平台，它需要处理巨大的数据量并且提供极强的可靠性约束和一致性保障；这在分布式设计中是一个巨大的难题，
因为同时能满足CAP要求的数据管理系统从理论上来说是不可能的。这里主要的区别是，Kafka仅仅是一个数据中间处理平台，作为一个broker本身，
它对数据的内容和模型范式是不关心的，其主要任务是作为一个可靠、易扩展、高性能的中间媒介，尽快消费生产者送过来的消息，
按照消费者的数目、集群配置将收到的消息尽可能高效地分发个消费者。

### 基本概念

作为一个中间媒介，Kafka将从生产者中收到的数据保存在其平台上的内部存储系统上，这些数据逻辑上被保存在不同的逻辑分区上。
出于高可靠性的考虑，这些逻辑分区可能会被分别放置在不同的磁盘，或者是同一机架上的不同的物理机器上，或者是同一数据中心的不同机架上。
这样的灵活性考量是为了方便用户订制不同的可靠性策略。

#### 话题
考虑到Kafka的核心是提供一个**消息流处理平台**，并同时可以处理多个并发的消息流；从概念上它将一个独立的逻辑消息流抽象为一个话题 (Topic) 。
话题才是Kafka基本的逻辑处理单元，所有的分发和调度，乃至集群处理等技术均以话题为基本处理单元。因为是话题是数据流的抽象，
因而其中的单个数据记录可以认为**总是按照时间顺序依次往后面追加**的。生产者和消费者均需要知道它所要操作的话题名称然后才可以做对应的流操作：
生产数据追加到话题中，或者从话题中取数据消费。

#### 生产者和消费者

生产者和消费者都可以认为是实际的用户应用程序；Kafka提供API的方式将对应的生产者、消费者代码嵌入到用户的程序中，实现对Kafka平台的访问。
生产者和消费者是对主题而言的一个相对概念，即同一个应用程序既可以是生产者，也可以是消费者（对另外一个不同的主题）。

生产者产生数据记录并通过封装API发送到Kafka平台上，被内部存储和分发。消费者则通过订阅某个主题收到Kafka平台的数据推送。整个数据处理风格是 Pub/Sub 风格的，
即生产者发布数据到Kafka，生产者从Kafka订阅数据的生成。

生产者和消费者**从业务逻辑和物理部署上相解耦，二者的关联是纯粹逻辑上的**，可以在运行期随着负载而扩展，这一方式具有极大的灵活性；
用户可以根据自己的需要订制和分发自己的工作负载。

### 分组式处理

从分布式可靠性的角度考虑，话题的内容需要被平台内部存储和调度；为了应对可能的硬件错误或者节点失效，Kafka内部将**每一个话题切割为多个子部分**，每个子部分称为一个分区。
逻辑上来说这些分区里面的数据记录的合集是生产者所生成的数据的整体，见下图

![kafka_partition](https://kafka.apache.org/10/images/log_anatomy.png)

#### 消费者分组

由于生产者和消费者可以是完全解耦的（微服务设计下的一个潜在假设），他们的处理能力及其实例的数量都可能是不同的（可以用Kubenetes或者MeSos等工具动态管理 )，
Kafka从设计上将消费者分成逻辑的组，并且保证
- 一个分组中的多个消费者作为一个整体需要能够读到它关心的话题的所有数据记录
- 同一个分组中的多个消费者的处理负载可以很容易的被Kafka自身做负载均衡
- 对于多个不同的消费者分组而言，同一个分区对每一个分组都有一个唯一的消费者和其关联
- 这种潜在的关联映射决定了任何一个消费者分组中**消费者实例的数量不能超过话题本身所对应的分区的数目**，否则分区到消费者实例的关联关系则无法分配

譬如下面的例子

![kafka_consumer_group](https://kafka.apache.org/10/images/consumer-groups.png)

这里的同一个话题被分隔到2个服务器集群上，第一个服务器上有2个分区被标记为P0和P3，第二个服务器上则存储剩下的2个分区P1和P2。
对于2个消费者分组A （包含2个消费者 ）和 B （包含4个消费者 ）而言，P0 的数据最终会被Kafka分给各组中的第一个消费者，P1被分给第二个；P2被分给第一/第三个，P3被分给第二/第四个。
注意默认情况下，Kafka分配数据的方式是轮询式的，并尽量将连续的数据流分配到不同的服务器上；当然每一个具体的分区内部也可以在多个Kafka实例之间做主从复制以应对可能的失效，
这里只是简化起见仅仅描述了没有分区复制的情况。

#### 轮询式记录分发

可以看出默认的策略是按照轮询式分配分组中的消费者实例的，如果分区数超过消费者数量，则采用取模运算的方式，从第一个开始继续分配。
处于均衡复杂的考虑，最好是设置主题中分区的数目是消费者数目的整数倍，即**消费者数量可以整除于分区数量**。即上图的例子中，如果消费者的数目是3，
那么默认的情况下，第一个消费者的负载会是其它两个消费者的2倍。

#### 数据记录的顺序性

Kafka**仅仅保证同一个分区的数据一定会按照顺序被某个消费者处理**，多个分组之间的顺序是不被Kafka自身所保证的。
每个分组内部的单个记录，相对于第一个数据记录的偏移被Kafka内部保存并传递个消费者，消费者可以据此得知当前读到的记录在分区中的位置。
如果应用程序需要额外的顺序性保证，则必须在消息记录中添加自定义的字段来处理，或者确保消费者组中仅仅有一个消费者（并不订制自己的消费者关联策略）。

## 数据存储

作为一个中间处理平台，Kafka必须保证数据可以被持久化存储，因为生产者产生的数据的时候，对应的消费者可能还没有启动来接受数据，或者消费者可能在处理过程中被重启。
同时Kafka被设计用来处理和存储大量的数据，因此数据必须被持久存储于磁盘中。记录中的数据可以被存储和保留多长时间，完全由系统配置所决定，默认情况下，
数据会被保存2天时间，然后同一个分区中最早的数据记录会被删除，以保证数据记录的逻辑连续性和不可变性。

每一个新加入的消费者组在默认情况下，都会按照现有系统中已经保存的数据，从最老的开始依次被分发处理。如果此过程中，由于消费者处理速度太慢而生产者产生新数据过快而产生
未读取的分区数据被删除，消费者侧会得到**异常提醒和偏移量重置通知**。这种情况一直发生的时候，应用程序就需要考虑增加更多的消费者实例或者降低生产者微服务的处理速度；
在采用微服务风格的系统中，通过云基础设施动态调整工作负载可以不修改代码动态应对这种突发的流量高峰。

### 内部分段式存储

由于具体记录的内容不是Kafka平台所关心的重点，内部数据存储上很容易按照二进制块的方式存储，并且记录的offset和分区分配策略信息很容易被内部单独存储和优化以提高处理性能。
同时为了便于快速的删除过期的老数据（依具体的数据保存策略而定），数据记录必须按照易于检索的方式存储；考虑到这些因素其内部数据是将一个记录划分为多个段来分开存储的。
这样可以让Kafka快速方便地找到那些块需要被删除。

写数据的时候，Kafka需要保持一个记录指向当前写入的段。如果段的大小超过了预先设定的值，那么Kafka会创建一个新的段；同时每一个段有一个基本偏移量。逻辑上可以认为，
任何**一个新的段的基础偏移量总是大于等于更老的段**的所有偏移量，并小于同段中所有的记录；如下图

![kafka_segments](https://cdn-images-1.medium.com/max/1600/1*bZ-fWeb2KG_KhYv2EKDvhA.png)

可以使用`tree`命令来查看Kafka在磁盘上留下的存储文件，任何一个分区的分段都有一个这些段索引的`.index`文件和对应的段记录文件，后者以`.log`后缀来标识。
段记录文件中实际存放了记录的压缩编码值、时间戳、大小、版本号以及校验值等信息。

磁盘上的实际文件格式和Kafka从生产者那里接收的消息一下，从而Kafka可以**直接不经拷贝即将对应的数据发送给消费者**，显著的降低IO的开销提高性能。

## 性能和可靠性考虑

### 数据复制 (Replication)

Kafka内部以分区为基本单位提供数据复制，即同一个分区有多分不同的拷贝；这些拷贝可以按照配置的不同分散在同一机器的不同的磁盘、同一机架上的不同磁盘或者不同的机器上，
从而应对不同的失效或者错误。复制策略可以按照不同的主题来配置，Kafka提供了丰富的配置选项来调整这些配置，方便根据实际应用场景提供不同的可靠性保证。

非失效情况下，每一个分区按照分布式一致性的概念区分为一个Leader，0个或者多个Followers的方式工作；实际Followers的个数依据具体的主题的设置而定。
所有的数据读写都发生在Leader上面，而数据会同时复制给所有的Follower上。通常情况下，分区的数目多余实际Kafka集群机器的个数，建议将不同的主题的Leader平均地分布在多个Kafka实例上，
最大程度地避免单点故障。

所有Follower上的数据和Leader上的数据保证一致，从而可以在**Leader失效的情况下，采用一致性共识算法重新选举**出新的Leader。
Follower节点消费数据的方式和外部消费者处理数据的方式是一致的。和其他的分布式系统一样，Leader和Follower之间需要维护保活信息，从而维持共识算法的运转。
当前内部的一致性处理是采用ZooKeeper来实现的；任何一个节点必须定期和ZooKeeper发送心跳保活，并且如果它处于活跃状态并且是follower节点，则必须复制（消费 ) Leader节点上新写入的数据。

### 数据写入保证

生产者写入数据的时候，可以选择某个特定的策略来决定写入是否完成，具体策略的选择需要依据性能和时延方面的考量做权衡
- 要么是不关心Leader和Follower之间的复制，一旦写入Leader节点就直接返回认为数据提交。这样做的风险是，万一此时Kafka集群中的Leader节点发生故障而数据没有被同步到其他Follower节点上，
那么数据可能被丢弃导致消费者收不到该数据
- 要么等到数据被所有的Follower所复制后才返回，这样即使Leader节点发生错误，也可以保证消费者在新的Leader生效后一定可以获取到消息记录
- 或者等待至少一个副本写入成功

显然这里**任然逃不开CAP定理**的约束，要么保证绝对可靠性和持久性，但损失性能和吞吐率，要么保证数据处理性能但丢失一定的可靠性、持久性。

同时Kafka对任何一个主题，还支持一个最小必须同步的复制实例个数的设置以提高灵活性；显然这个配置应该小于该主题的副本个数。
当生产者要求获取消息写入确认的时候，Kafka会检查是否超过给定设置的Follower实例已经得到了该消息，如果没有，则写入操作会被阻塞直到满足要求的副本复制完毕。

第三个选项则依赖于一个Kafka所提供的保证，**只要有超过一个副本在数据写入过程中是活跃的（心跳未丢失可以消费数据 ) , 那么该消息就不会被Kafka丢弃**，即使设置的最小同步副本数超过一个。

#### Leader失效情况下的选举算法

如果Leader节点从来不会失效，那么我们根本不需要Follower节点；问题是分布式计算中任何一个节点都有可能是不可靠的，网络也可能时不时失效，所以软件系统必须设计好处理这种失效。
问题的复杂性在于，当Leader失效的时候，Follower中的有些节点可能已经crash，或者有的节点的数据老友的节点数据新一些；最大的**挑战是我们需要选出持有最新数据的节点**作为新的Leader，
否则上述的消息不丢失担保就无法满足。

##### 基于多数的选举算法
最常用的算法是一种基于投票的方法，一般地Follower的个数必须是奇数以便于进行基于多数的投票和选举：即如果有 2N+1 个副本，那么必须有 N+1 个节点确认收到数据后，Leader节点才能确认数据写入成功。
这样当Leader失效的时候，我们仅仅需要从 N+1 个节点中选取出持有最新数据的节点即可。具体的选举算法细节这里不详细展开，
一个明显的好处是，**基于多数的选举算法可以保证处理延迟仅仅依赖于最快的节点**，可以大大提高系统的可用性。

这一处理算法有很多中实现，包括ZooKeeper的Zab、Raft乃至[Viewtamped Replication](http://pmg.csail.mit.edu/papers/vr-revisited.pdf)等。
该方法的弊端也是显而易见的：我们需要更多的节点和存储空间，譬如为处理单一节点的可能失效，我们需要3份数据拷贝，这在实际情况在往往是不过的；而对于有大量数据的系统中，
增加系统的存储空间到5倍乃至更多倍以上则资源浪费也是相当惊人的。实际的场景中，这一策略仅仅适宜于维护轻量级的数据（如系统配置信息）而不是应用的实际数据。

##### Kafka的处理策略

Kafka应用的方法有些略微的不同，它内部动态维护了一个保持同步的副本集合，该集合中的副本虚保证其处理速度和Leader是匹配的。**只有该集合中的副本才会参与Leader的选举**。
所谓的动态，以为这这个副本集会在运行期中被修改刷新并保存于ZooKeepr中去；并且其数量小于实际的节点数量。
写入某个分区数据的操作仅仅在这个集合中的所有节点都消费了该数据的时候才会被认为是写入提交完毕的。基于该算法，同步副本集中的任何一个副本都可以在出错的情况下被选举为新的Leader。

当然前述的数据不丢失保证是在至少有一个副本不失效的情况下才可以达成的。如果所有的副本都失效了又怎么办？一旦这种情况发生，Kafka本身其实无能为力，应用系统的设计者必须考虑好如何应对
- 一种方法是等待同步副本集中的一个恢复正常，然后设置该副本为新的Leader并祈祷它没有丢数据 （考虑到墨菲定律，一定有某些情况下数据是丢失的）
- 另外一种办法选择第一个恢复正常的副本作为Leader，然后继续运行

这里的选择任然是从一致性和可用性中间二选一，毕竟**CAP问题理论上无解** ！Kafka默认情况下会采用后一种策略，即尽快从先活过来的节点中选一个恢复可用性。


### 均匀分布Leader角色

Kafka提供的一个有意思的机制是尽可能得均衡Leader角色的分区，使得**集群中的每一个机器上的Leader角色的分区数量大致相同**，从而最大程度地提高整个系统的可用性。
因为Leader角色是按照分区来设置的，而一个主题可能被切分为多个分区，同时Kafka系统中可能处理成百上千个主题；总体分摊角色的结果是，
任何一个机器节点的失效都尽量不引入系统性的故障。

同时Kafka内部会将其中的一个节点设置为controller角色，这样可以处理Kafka节点层面的失效，当controller节点检测到其他的机器节点失效时，
Kafka可以尽量主动切换这些设置故障节点为Leader角色的所有分区，避免被动的Leader选举算法影响整体可用性，可谓是一种主动优化措施。

## 流处理和外部工具整合

Kafka自身的流式处理设计更适合于作为大数据处理中的一个中间环节，可靠的实现数据的收集和即使传递。如果消费者工具是一些基于微服务的应用层工具，
很容易在准实时的情况下即使分析实时数据完成一些传统的ELK框架无法完成的基于即时反馈的自适应软件系统。
传统的ELK框架依赖于线下的数据分析和处理，等到产生一些有意义的结果来作为依据对原系统进行调整的时候，可能具体情况以及发生了变化。
Kafka的准实时特性则允许我们更快地对系统的运行产生影响。

[Kafka Streams](https://kafka.apache.org/documentation/streams/) 提供了一种新的方式构建实时分析微服务应用；
我们仅仅需要依赖于存放在Kafka集群中的数据作为输入，在自己的微服务应用中实现反馈控制逻辑，并将分析结果再放入Kafka集群中；
同时修改原系统中使其反过来从Kafka集群中订阅这些反馈，实时对系统行为作出调整即可。

一个简单的统计流中的文本并计算字数的插件式微服务应用如下
```java
public class WordCountApplication {
    public static void main(final String[] args) throws Exception {
        //Construct config
        Properties config = new Properties();
        config.put(StreamsConfig.APPLICATION_ID_CONFIG, "wordcount-application");
        config.put(StreamsConfig.BOOTSTRAP_SERVERS_CONFIG, "kafka-broker1:9092");
        config.put(StreamsConfig.DEFAULT_KEY_SERDE_CLASS_CONFIG, Serdes.String().getClass());
        config.put(StreamsConfig.DEFAULT_VALUE_SERDE_CLASS_CONFIG, Serdes.String().getClass());

        StreamsBuilder builder = new StreamsBuilder();
        KStream<String, String> textLines = builder.stream("TextLinesTopic");
        KTable<String, Long> wordCounts = textLines
            .flatMapValues(textLine -> Arrays.asList(textLine.toLowerCase().split("\\W+")))
            .groupBy((key, word) -> word)
            .count(Materialized.<String, Long, KeyValueStore<Bytes, byte[]>>as("counts-store"));
        wordCounts.toStream().to("WordsWithCountsTopic", Produced.with(Serdes.String(), Serdes.Long()));
        
        //Build and start stream
        KafkaStreams streams = new KafkaStreams(builder.build(), config);
        streams.start();
    }
}
```

## 参考资料
1. [Kafka documentation](https://kafka.apache.org/documentation/)
2. [How Kafka's Storge Internals Work](https://thehoard.blog/how-kafkas-storage-internals-work-3a29b02e026)
3. [Kafka Streams Documentation]()
# Cortx As Persistent Store For Apache Pulsar
------


<!-- @import "[TOC]" {cmd="toc" depthFrom=1 depthTo=6 orderedList=false} -->

<!-- code_chunk_output -->

- [Cortx As Persistent Store For Apache Pulsar](#cortx-as-persistent-store-for-apache-pulsar)
- [TLDR](#tldr)
- [Presentation Video](#presentation-video)
- [Problem and Impact](#problem-and-impact)
- [Project](#project)
- [Implementation](#implementation)
- [Issues](#issues)

<!-- /code_chunk_output -->


# TLDR

This project demonstrates 
1. creation of a topic/stream on Apache Pulsar(a message broker like Kafka) with unlimited persistence, which archives older messages to Cortx via S3 API and
2. replay of the stream/topic from the earliest message stored in Cortx

Theoretically, based on scaling promises made by Pulsar and Cortx(2^120 objects), this integration can be used to store every event ever observed in universe, and then replay them in order from any point, hundreds of years later with consistency guarantees while preserving processing semantics(like exactly-once).

Suitable for oracles, recommendation engines, tracing backends, backtesting algos on trading floors, heavily instrumented operations like aircrafts(to be replayed for flight simulations, for instance) and forgetful humans.

# Presentation Video

A short video presentation on the integration is [here](https://www.youtube.com/watch?v=EjY_Q0w4ejA). Detailed steps to reproduce and test it are in [Documentation.md](Documentation.md).

The full walkthrough/demo of the integration is [here](https://www.youtube.com/watch?v=-JPrpL1_8Mg).

# Problem and Impact
Message brokers like Kafka and Rabbit MQ are optimized for latency and throughput and have minimal retention options. In any case, retention on these brokers depends on comparitively costly storage. This project is an attempt to combine the power of fast messaging from Pulsar with near-unlimited storage offered by Seagate Cortx.

This allows us to build applications with capabilites that support both fast traditional ETL queries and stream replay. Currently a common practise is to see these problems separately and use ETL optimized storage or tackle them with a intermediary between storage and ETL/stream replay, for instance in the form of Apache Spark jobs or Apache Flink pipelines. The latter solution is too complicated for most data especially in stateless applications(say a simple store for trace data) and applications with simple state.

Even ignoring the above use cases, offloading data from machine storage to cheaper storage should not need the complexity, especially for stream data.

The initial goal was to demonstrate both ETL and stream replay for a topic's data. Pulsar documentation promised an out-of-the-box integration for any S3 based storage backend with a simple configuration. And Cortx promised near-AWS implementation of S3 server. Both had issues and the revised goal was to just replay messages from the distance past(those which are offloaded to Cortx) to a reader/client. The issues encountered were raised as bugs to the respective projects. See links below.

# Project

This project demonstrates how to create a topic on Pulsar which archives older messages to Cortx via S3 API and replay the data from the earliest message stored there.

Apache Pulsar is a message broker with persistent storage built into it based on Apache Bookkeeper. In theory though there is no limit on number of messages stored in Bookkeeper, older messages on topics are better stored on cheaper and more reliable storage. Pulsar allows tiered-storage backends to be configured so that older ledgers(units of Bookkeeper records) of message data can be offloaded to S3(AWS and non-AWS), Azure Blob Storage, file system etc.,

Replaying archive data from a topic is based on reading a topic just like a normal subscriber would do but with a starting id supplied to the broker. 

Kafka has a similar feature and other brokers must have made attempts to do it as well. However Kafka(AFAIK) kills the topic data for which tiered storage is enabled, making stream replay from the same topic tedious at the least(It needs to be republished to another topic).

In contrast, Pulsar allows us to publish messages on a topic and, hundreds of years later and hundreds of petabytes later, allows us to stream the message starting from any arbitrary message id from the same topic. This is the reason for the choice of Pulsar.

# Implementation

1. We setup Cortx on a VM.
2. We use aws s3 cli to check the bucket data on Cortx S3.
3. We setup Pulsar to use Cortx as tiered storage backend.
4. We make some hacky adjustments to make sure Cortx accepts the payload sizes sent by Pulsar. See issues.
5. We publish messages on a test topic.
6. We offload the topic messages(older ones anyway) to Cortx s3.
7. We prove that the messages are offloaded based on internal stats of the topic via pulsar-admin.
8. We cross check in Cortx s3, by checking that the S3 bucket has message ledgers with index data.
9. We replay the data from the earliest message in the topic(which would be on Cortx S3 by now)

Detailed steps for reproduction are in [Documentation.md](./Documentation.md)


# Issues

1. Cortx's motr has a configurable layout which decides what unit_size of storage it uses for object storage. Cortx's s3 server cannot save multi part uploads whose first part has size which is not a multiple of this unit_size. There is probably a performance benefit in doing this but it prevents all multi part uploads with size smaller than unit_size(which is 1MB for default layout). We had to configure for a layout with smaller unit size to get this integration work.
2. Pulsar has misleading error messages in its tiered storage configuration module. Some documentation is outdated as well with previously supported features removed/abruptly renamed.

# What is BizTalk?

[Microsoft BizTalk Server]( https://docs.microsoft.com/en-us/biztalk/) is a
publish and subscribe architecture that uses adapters to receive and send
messages, implements business processes through orchestration, and includes
management and tracking of these different parts. BizTalk Server also includes
trading partner management for business-to-business messaging, high availability
to maximize uptime, a development platform to create your own components, an
administration console to manage your artifacts, and business activity
monitoring to manage aggregations, alerts, and profiles.

BizTalk allows Enterprises to implement Enterprise Service Bus (ESB) integration
architectural patterns which support the building of rich business processes on
top of an organization’s connected systems in a loosely-coupled way.

# What is CORTX?

CORTX is a distributed object storage system designed for great efficiency,
massive capacity, and high HDD-utilization. CORTX is 100% Open Source.

# What is the CORTX BizTalk Adapter?

The CORTX BizTalk Adapter is a piece of software adhering to the official
BizTalk adapter specifications that allow it to be plugged directly into the
BizTalk Administration Console enabling the BizTalk messaging engine to
read/write from/to a CORTX instance. The adapter enables CORTX to “light up”
within the Enterprise by allow rich business processes to move, store and
activate data within CORTX without themselves having to be tightly coupled to
CORTX. Instead of integrating CORTX tightly to each application, system or
business process of interest, the CORTX BizTalk Adapter allows all interested
applications systems and business processes to seamlessly use CORTX as a mass
capacity on-prem object store “Enterprise Service” through BizTalk.

## Why is the CORTX BizTalk Adapter Important?
[This video](https://youtu.be/Z2OG88E-UtA) presents the case for the CORTX BizTalk Adapter and why it is an important and natural fit for sophisticated Enterprise integration architectures.

## How was the CORTX BizTalk Adapter Built?

I used the [Microsoft Windows Communication Foundation (WCF) Line-Of-Business
(LOB) Software Development Kit (SDK)](
https://docs.microsoft.com/en-us/biztalk/adapters-and-accelerators/wcf-lob-adapter-sdk/get-started-with-the-with-the-wcf-lob-adapter-sdk)
and its tooling for [Microsoft’s Visual Studio](
https://visualstudio.microsoft.com/) to generate the [.NET](
https://dotnet.microsoft.com/) code empty template for the WCF-LOB Adapter. I
then used the [Amazon AWS .NET SDK]( https://aws.amazon.com/sdk-for-net/) as the
S3 client library to implement the CORTX integration code within the generated
adapter template. The resultant adapter was automatically compatible with
BizTalk Server, since BizTalk is itself built for and with Microsoft’s premier
.NET development platform used around the world by millions of developers, and
has integral support for WCF.

# Configuring the CORTX BizTalk Adapter

## Creating a BizTalk Environment

If you don’t have a BizTalk environment available you will need to create one.
The two guides in this section will help you set one up on a Windows Computer.  

First, BizTalk is a “store-and-forward” messaging engine and relies on Microsoft
SQL Server to persist all messages published to it, so that it can offer
Enterprise-scale High Availability (HA). BizTalk never loses a message!

If you don’t have a Microsoft SQL Server environment step up, I have prepared
[this guide](./biztalk/Microsoft%20SQL%20Server%202019%20Installation%20Guide.md) to walk you through the process.

Once you have SQL Server installed, you are reading to install and configure
Microsoft BizTalk Server itself. I have prepared [this guide](./biztalk/Microsoft%20BizTalk%20Server%202020%20Installation%20and%20Configuration%20Guide.md) to show you how.

## Installing the CORTX BizTalk Adapter

Finally, I have prepared [this guide](./biztalk/CORTX%20BizTalk%20Adapter%20Installation%20Guide.md) to walk you through how to install the
CORTX BizTalk Adapter. It is made easy with a Windows Installer package for the adapter I
provide that you can find [here](./biztalk/CORTXBizTalkAdapter.msi).

## Creating a CORTX Environment

If you need instructions on how to set up your own CORTX system you can find
instructions on how to setup one on a local machine
[here](https://github.com/Seagate/cortx/blob/main/doc/OVA/1.0.4/CORTX_on_Open_Virtual_Appliance.rst)
or AWS
[here.](https://github.com/Seagate/cortx/blob/main/doc/integrations/AWS_EC2.md)

# Using the CORTX BizTalk Adapter

The CORTX BizTalk Adapter is easy for anyone familiar with BizTalk to use. Here
are some resources to help you get started with the adapter and help you imagine
the many ways it can be used to seamlessly bring hyper-scale on premise object
storage to the Enterprise.

## Getting Started with CORTX BizTalk Adapter  

I have prepared [this Getting Started Tutorial](./biztalk/CORTX%20BizTalk%20Adapter%20Getting%20Started%20Tutorial.md) to walk you through how to use
the CORTX BizTalk Adapter to send data from BizTalk to CORTX, as well as receive
data from CORTX into BizTalk.  

You can also watch [this video](https://youtu.be/MBQfkiDTxOE) to watch me
perform similar steps in the tutorial in a live demo.

## Moving Data with the CORTX BizTalk Adapter

In [this video](https://youtu.be/l8UFgnC9lJo) I walk you through how you could
use the CORTX BizTalk Adapter to migrate massive amounts of data from
Microsoft’s Azure Blob Storage server into CORTX in a live demo.  

## Storing Data with the CORTX BizTalk Adapter

In [this video](https://youtu.be/N3Ibklqhn8k) I walk you through how you could
use the CORTX BizTalk Adapter to seamlessly add critical unstructured data
archival capabilities into any Enterprise business process, to meet continuously
expanding data retention and regulatory compliance requirements. This live demo
will show how easy it is to add a final CORTX archival step to an existing
business process that processes incoming Purchase Orders for sending to an
Enterprise Resource Planning (ERP) system.  


## Activating Data with the CORTX BizTalk Adapter  

In [this video](https://youtu.be/SImILamWbo8) I walk you through how you could
use the CORTX BizTalk Adapter to activate big data analytics within the
Enterprise from data sourced from CORTX. This live demo will simulate an
Internet of Things (IoT) scenario where an IoT device writes telemetry data
directly to CORTX once a second, and BizTalk reads the data as it arrives and
sends it to a custom data analytics Web API/Site for real-time visualization.

# Glossary for CORTX on Kubernetes

This page will serve as a clearinghouse for all terms, definitions, and acronyms critical to both the understanding of and having success with [CORTX](https://github.com/Seagate/cortx). Please feel free to add terms as needed but place them in proper alphabetical order within the proper section. The glossary is currently split into three sections:
1. [CORTX Core Glossary](#cortx-core-glossary)
2. [CORTX Halo Glossary](#cortx-halo-glossary)
3. [CORTX K8s Glossary](#cortx-k8s-glossary)

<!------------------------------------------------------->
## CORTX Core Glossary
<!------------------------------------------------------->

The smallest, atomic deployable unit of the CORTX stack is sometimes referred to as "CORTX Core". This includes the CORTX Control, CORTX RGW, CORTX Cluster Coordinator (nee HA), and CORTX Data components (i.e. motr), along with the third-party prerequisites of Apache Kafka and HashiCorp’s Consul. The following terms are useful for understanding the basic CORTX Core system.

### CORTX RGW
[cortx-rgw](https://github.com/Seagate/cortx-rgw) is the S3 conversion layer in CORTX. It serves S3 clients and uses motr for its actual data and metadata storage.

### CORVAULT

[CORVAULT](https://www.seagate.com/products/storage/data-storage-systems/corvault/) is the brand name for a specific Seagate storage hardware product. CORVAULT generally belongs to a category of storage referred to as RBOD (reliable bunch of disks). Physically, CORVAULT is a large 4U rack enclosure which holds up to 106 devices. By virtue of firmware running inside the enclosure, CORVAULT appears to the upper-layer host (CORTX in our case) as two very large individual disks. Internally, CORVAULT does declustered erasure such that the frequency of "disk" failures seen by the host is very low (albeit when they happen, they are a large failure).

### CVG (Cylinder Volume Group)

A Cylinder Volume Group, or CVG, is a collection of drives or block devices which CORTX utilizes as a unit of storage, all managed by a single Motr IO process.

### Data Devices / Drives

Block devices, HDDs, SDDs, or other types of storage devices addressable by `/dev/{device-name}` which CORTX uses to store user data.

### IEM

In the context of [CORTX](https://github.com/Seagate/cortx), IEM stands for Interesting Event Messages, relating to events happening within the system, affecting normal system functionality and which may be useful for preliminary debugging.

### JBOD

JBOD stands for "Just a Bunch of Disks" and refers to a rack enclosure containing many disks which are each individually exposed to the host (CORTX in our case).

### Metadata Devices / Drives

Block devices, HDDs, SDDs, or other types of storage devices addressable by `/dev/{device-name}` which CORTX uses to store metadata about user data.

### Motr
[motr](https://github.com/Seagate/cortx-motr) is the main block store component within CORTX. motr is responsible for block level services such as distributed layouts, distributed transactions, erasure coding and repair, block allocation, etc. It functions as a distributed object and key-value storage system targeting mass-capacity storage configurations.

### Rados Gateway (RGW)

This is the component which provides all necessary S3 functionality in a CORTX cluster through a central gateway interface.

### RBOD

RBOD means "Reliable Bunch of Drives". Physically it is similar to JBOD but internally it uses erasure or RAID to add better data protection by distributing data across multiple disks and protecting it with parity. Logically, an RBOD will therefore export itself to the host (CORTX in our case) as a smaller number of drives which are much larger in capacity. For example, imagine an RBOD of 100 drives. For high availability reasons, most RBODs will use dual ported drives and will split themselves into two groups of disks. A pair of controllers in the RBOD will provide active-passive access to each pair such that the drives served by the active controller can be instead served by the passive controller in the case of a failure of the active controller. Further imagine, that the RBOD is configured for 8+2 parity within each group of drives. Therefore, to the upper level host, this RBOD will logically appear as just two large drives, each of which being the aggregate size of 40 drives (i.e. 8+2 on 50 drives will use 20% of capacity for parity thereby leaving 80% of capacity for host data).

<!------------------------------------------------------->
## CORTX Halo Glossary
<!------------------------------------------------------->

Cortx Halo is a set of additional software services which are required to build an enterprise storage appliance using cortx object store. These software services include management of hardware and object store, monitoring of hardware and object store, etc. The following terms are useful for understanding the basic CORTX Halo system.

### Artifact

Artifact - a packaged software bundle, part of Bill Of Materials (BOM)

### BMC network

BMC network - optional and uses dedicated onboard BMC port (on server's mainboard). Primarily for monitoring and collecting health data.

### Cluster

Cluster - a combination of the nodes configured to act as one. The nodes share the same S3 namespace, accounts, etc. The minimum number of nodes per cluster is three (3)

### CORTX Admin

CORTX Admin - a person whose primary responsibilities include completing the final stages of the cluster deployment and performing various administrative tasks on the live clusters (creating users, monitoring logs, updating software, etc.)

### CORTX Halo

CORTX Halo - a solution framework (set of software services and data stores for management, monitoring and configuration data) developed by Seagate to build storage (object store) as a service appliance solution using qualified deployment configuration consisting of storage enclosures, servers, networking hardware, object store and halo software


### Datacenter Technician (DT)

Datacenter Technician (DT) - a person that is working at the datacenter. When it comes to working with the cluster, his/her primary responsibilities are: to install and cable the equipment, perform the initial configuration of each node and initiate cluster deployment

### Deployment

Deployment - a process of software installation and configuration of the cluster. This process does not include installing the operating system and other tasks related to the Factory process.

### Enclosure

Enclosure - Seagate's Corvault or 5U84 enclosure (with "Thor" controllers) with maximum allowed number of the drives. Aim is to support the largest capacity drives available for given enclosure. The enclosure is equipped with two identical RBOD controllers.

### Factory

Factory - a Seagate or a 3rd-party (including vendor’s) facility, equipped and capable of manufacturing servers and/or enclosures using the approved hardware and software per specifications.

### Field

Field - customer (deployment) site.

### Kickstart

Kickstart - an automated method of installing a Linux operating system (usually, RedHat-alike).

### LACP

LACP, or interface bonding, is a protocol that allows the bandwidth of multiple interfaces to be aggregated together and treated as a single interface.

### Management network (in CORTX Halo solution context)

Management network - provides access to the management and monitoring functionality of the cluster (access to WebUI, access to CLI). Uses 1 GbE onboard interfaces.

### Public Data network (in CORTX Halo solution context)

Public Data network - uses one of the high-speed network adapters installed in the PCI-e slots of the server. To provide S3 access from and to the S3 clients. Supports optional bonding.

### Private Data network (in CORTX Halo solution context)

Private Data network - another high-speed network adapter installed in the PCI-e slots of the server.
Be configured on a separate adapter from the one used for the Public Data network. To provide backend network communications related to the cluster's functionality, like Erasure Coding, HA, logs collection, 3rd party data replication, cluster management. Supports optional bonding.

### Node Configuration network (in CORTX Halo solution context)

Node Configuration network - Uses dedicated 1 GbE onboard interface (on server's mainboard). Configured for point-to-point network and, in general, is not meant to be connected to the switch. To provide access to the server for the initial setup and configuration at a customer site.

### Node (in CORTX Halo solution context)

Node - an hardware component, consisting of Server and Enclosure connected using SAS cables.

### Operator (user in Factory)

Operator - a person performing preparation and installation of the hardware and artifacts at the Factory.

### Release Engineering (RE)

Release Engineering (RE) - a software development team, likely a part of the CORTX dev team.

### SAS (enclosure) network (in CORTX Halo solution context)

SAS (enclosure) network - provides for virtual TCP/IP connection between the server and the attached enclosure. Uses IP-over-SCSI technology over LSI SAS HBA. Provides for in-band management of the enclosure. It is a closed network.

### Server

Server - a compute node attached to storage which runs the CORTX Halo and CORTX Core software (e.g. a 1U HPE ProLiant DL360 latest generation server).

### Storage Set (in CORTX Halo solution context)

Storage Set - a single Storage Set shall contain a uniform set of hardware. It applies to the servers, and storage hard drives. Different Storage Sets within the same cluster may be created using different hardware options.


<!------------------------------------------------------->
## CORTX K8s Glossary
<!------------------------------------------------------->

The main deployment mechanism for CORTX is kubernetes (k8s). The following terms are useful to understand the CORTX K8s deployment model.

### CatalogSource

A [CatalogSource](https://olm.operatorframework.io/docs/concepts/crds/catalogsource/), or more simply Catalog, is a CustomResourceDefinition defined by the Operator Lifecycle Manager (OLM) and represents a store of metadata that OLM can query to discover and install operators and their dependencies.

### ClusterServiceVersion (CSV)

A [ClusterServiceVersion](https://github.com/operator-framework/operator-lifecycle-manager/blob/0.15.1/doc/design/building-your-csv.md) is the metadata that accompanies your Operator container image when an Operator is deployed through the Operator Lifecycle Manager (OLM). It can be used to populate user interfaces with info like your logo/description/version and it is also a source of technical information needed to run the Operator, like the role-based access control rules it requires and which Custom Resources it manages or depends on.

### Container

Per [Red Hat](https://www.redhat.com/en/topics/containers/whats-a-linux-container), a container, or sometimes referred to as a Linux container, is "a set of 1 or more processes that are isolated from the rest of the system. All the files necessary to run them are provided from a distinct image, meaning Linux containers are portable and consistent as they move from development, to testing, and finally to production."

### CORTX Control Pods

CORTX Control Pods contain the APIs which are exposed to the end-user in order to maintain the CORTX control plane and are most often used to interact with IAM settings.

### CORTX Data Pods

CORTX Data Pods contain the internal APIs which are used to manage the storage and protection of data at the lowest possible level inside of a CORTX cluster.

### CORTX HA (High Availability) Pods

CORTX HA Pods are responsible for monitoring the overall health of the CORTX cluster and notifying components of changes in system status.

### CORTX Server Pods

CORTX Server Pods contain the APIs which are exposed to the end-user in order to provide general S3 functionality - create buckets, copy objects, delete objects, delete buckets. This API layer is implemented using the Rados Gateway (RGW) interface.

### CustomResource (CR)

A CustomResource, or CR, is an instance of the CustomResourceDefinition a Kubernetes Operator ships that represents the Operand or an Operation on the Operand.

### CustomResourceDefinition

A CustomResourceDefinition, or CRD, is an API of a Kubernetes Operator, providing the blueprint and validation rules for Custom Resources.

### Finalizers

[Finalizers](https://kubernetes.io/docs/concepts/overview/working-with-objects/finalizers/) are namespaced keys that tell Kubernetes to wait until specific conditions are met before it fully deletes resources marked for deletion. Finalizers alert controllers to clean up resources the deleted object owned.

### Helm

Commonly known as "the package manager for Kubernetes", [Helm](https://helm.sh/) is a CLI tool that builds and deploys Kubernetes applications through application packages known as Helm Charts.

### kubelet

The [kubelet](https://kubernetes.io/docs/reference/command-line-tools-reference/kubelet/) is the primary "node agent" that runs on each node. It can register the node with the apiserver using one of: the hostname; a flag to override the hostname; or specific logic for a cloud provider.

### Kubernetes API

The [Kubernetes API](https://kubernetes.io/docs/reference/using-api/api-concepts/) is a resource-based (RESTful) programmatic interface provided via HTTP. It supports retrieving, creating, updating, and deleting primary resources via the standard HTTP verbs (POST, PUT, PATCH, DELETE, GET).

### Kubernetes API Server

The [Kubernetes API server](https://kubernetes.io/docs/reference/command-line-tools-reference/kube-apiserver/) validates and configures data for the api objects which include pods, services, replicationcontrollers, and others. The API Server services REST operations and provides the frontend to the cluster's shared state through which all other components interact.

### KUDO

[KUDO](https://kudo.dev/), which stands for the Kubernetes Universal Declarative Operator, is a toolkit used to build Kubernetes Operators, in most cases just using YAML.

### kustomize

Per the offifical [Kustomize](https://kustomize.io/) site, "Kustomize lets you customize raw, template-free YAML files for multiple purposes, leaving the original YAML untouched and usable as is." Kustomize is used in conjunction with `kubectl` for advanced customization and robust templating for Kubernetes application deployment and management.

### Liveness Probe

The kubelet uses [liveness probes](https://kubernetes.io/docs/tasks/configure-pod-container/configure-liveness-readiness-startup-probes/) to know when to restart a container. For example, liveness probes could catch a deadlock where an application is running but unable to make progress. Restarting a container in such a state can help to make the application more available despite bugs.

### Managed resources

Managed resources are the Kubernetes objects (Pods, Deployments, StatefulSets, PersistentVolumes, ConfigMaps, Secrets, etc.) the Operator uses to constitute an Operand.

### Node

This term is unfortunately overloaded in the context of CORTX on Kubernetes. It can either mean an underlying Kubernetes worker node (in general) or it can mean any single component working inside of the CORTX cluster (Data Pod, Server Pod, Control Pod, etc.).

Context is important and required to discern when which is which. Through the https://github.com/Seagate/cortx-k8s repository, care is used to refer to Kubernetes worker nodes as "Nodes" and CORTX nodes running on Kubernetes as "Pods".

### Operator Lifecycle Manager (OLM)

The [Operator Lifecycle Manager (OLM)](https://github.com/operator-framework/operator-lifecycle-manager/) is an optional set of Kubernetes cluster resources that can manage the lifecycle of an Operator. The Operator SDK supports both creating manifests for OLM deployment, and testing your Operator on an OLM-enabled Kubernetes cluster. OLM integration is not a foundational requirement for all Kubernetes Operators.

### Operand

As defined by the [Operator SDK](https://sdk.operatorframework.io/docs/overview/operator-capabilities/#terminology), an Operand is the managed workload provided by the Operator as a service.

### Operator

As defined by the [Operator SDK](https://sdk.operatorframework.io/docs/overview/operator-capabilities/#terminology), an Operator is the custom controller installed on a Kubernetes cluster.

### Operator SDK

An [open-source framework](https://sdk.operatorframework.io/), provided by Red Hat, which helps users quickly create functional Kubernetes Operators from Go, Helm, and Ansible scaffolding.

### Pod

Per the [official Kubernetes documentation](https://kubernetes.io/docs/concepts/workloads/pods/), "Pods are the smallest deployable units of computing that you can create and manage in Kubernetes." Inside of a CORTX cluster, all relevant containers that need to be contextually grouped together are deployed as a Pod.

### Readiness Probe

The kubelet uses [readiness probes](https://kubernetes.io/docs/tasks/configure-pod-container/configure-liveness-readiness-startup-probes/) to know when a container is ready to start accepting traffic. A Pod is considered ready when all of its containers are ready. One use of this signal is to control which Pods are used as backends for Services. When a Pod is not ready, it is removed from Service load balancers.

### Startup Probe

The kubelet uses [startup probes](https://kubernetes.io/docs/tasks/configure-pod-container/configure-liveness-readiness-startup-probes/) to know when a container application has started. If such a probe is configured, it disables liveness and readiness checks until it succeeds, making sure those probes don't interfere with the application startup. This can be used to adopt liveness checks on slow starting containers, preventing the kubelet from terminating them before they are up and running.

### Storage Set

A Storage Set is the common unit of deployment and scalability for CORTX and its mapping to the underlying infrastructure. A given Kubernetes worker node can only belong to a single Solution Set for the lifetime of a CORTX cluster. A Storage Set is defined as a collection of Kubernetes worker nodes and CVGs.

## Frequently Asked Questions

**What is CORTX?** 

CORTX is S3 compatible object storage developed by a community with a focused engineering effort provided by Seagate designed to allow the most economically efficient possible storage of massive data sets on mass capacity storage devices. Please watch our [brief introductory video](https://www.youtube.com/watch?v=dA-QtUBf16w&list=PLOLUar3XSz2M_w5OxQLNDBTpSrsGbjDWr&index=1&t=12s) and visit our [youtube channel](https://cortx.link/videos) for more in-depth information. At the heart of CORTX is the internal motr object system about which you can read a [detailed architecture description](https://github.com/Seagate/cortx-motr/blob/main/doc/motr-in-prose.md).  To ensure the best economics and the highest quality of code, CORTX is 100% open source with core components licensed as Apache and peripheral components licensed as LGPL.

**What does CORTX stand for?**

   * Actually CORTX is not an acronym.  The marketing team just thought it looks cool and sounds cool.  :smiley:

**Why introduce a new object store when there are already other object stores out there?**

   * We are certainly familar with the other really excellent object stores out there.  But the fact is that there are clearly many use cases for object storage today, with additional use cases emerging all the time, and we don't see it as a *one size fits all* world. Each object store has its own particular design choices which results in unique feature sets.  The more diversity of choices that are available, the more likely that different users/customers can find the system best suited for their needs and the uniformity of the S3 interface allows this without overly forcing users/customers into *vendor lock-in*.  Specific and unique to CORTX is our focus on enabling the next generation of mass capacity devices (e.g. HAMR) and the TCO benefits that come with that approach.  This is a unique focus and requires the object store itself to address a number of design/architectural challenges that the CORTX community is best positioned to address.  We have heard from many in the community that there is a clear need for an open source object store that is focused on enabling these mass capacity devices and that is the core motivation behind CORTX. The origin of this technology is from an EU funded program and the user/developer community, whom came together because they saw the need for a differently approach to object storage as they looked at the needs of application developers, the challenges of exascale/exabytes scale of storage and the need to optimally utilize emerging device level technology innovations. CORTX provides something different from other object stores in that it will uniquely leverage HDD innovations such as REMAN to reduce the likelihood of rebuild storms, HAMR to enable the largest capacity/lowest cost per bit next gen devices, and multi-actuator to retain IOPS per capacity ratios.  CORTX and the community are focused on such capabilities that are required in mass capacity deployments.
   
**What is the history of CORTX?**

   * CORTX began its life with the requirements gathering workshops through the E10 initiative circa 2012. There were multiple  workshops organized throughout the world on getting the requirements of building storage systems for extreme scale computing  involving stakeholders from the (primarily) extreme scale HPC, research data center community, etc. Early pioneers of the Lustre file system were also involved in those workshops & heavily contributed to the architecture and the designs. It was envisioned to build something from scratch on a clean slate considering the changes in hardware assumptions (multicore, many core, extreme parallelism, etc) that was coming about at that time. Then the CORTX initiative got funding from the European Union through multiple projects,  primarily SAGE (2014 – 2018),  Sage2 (2018 –) , and,  Maestro(2018 - ). The development and the ecosystem around CORTX was enabled by many advanced application developers, key technology vendors, OEMS, research and academic institutions in Europe. 
Some of the institutions are the largest producers and consumers of data and run some of the largest data centers with extreme requirements for storage and I/O. 
CORTX is also used by the weather and climate community in the EsiWACE  (2015 – 2019) and EsiWACE2(2019 -) projects. 

**Which are the ongoing EU R&D projects based on CORTX?**

   * Sage2, Maestro and EsiWACE2 are the currently ongoing R&D projects based on CORTX. 
Sage2 is continuing to build a CORTX cluster based for extreme scale computing/HPC and AI moving forward from the SAGE project, continuing to use the SAGE prototype. The project consists of 9 partners lead by Seagate and is funded by the European Commission's H2020 program (Grant Agreement numnber: 800999). Sage2 will demonstrate a multi tier storage system (distributed NVRAM, SSD, HDD), "SAGE", capable of handling data intensive workloads. 
Seagate is providing the components of CORTX primarily, Motr, lib-motr and Hare and some of the storage enclosures (SAS & SATA HDD tiers). Experimental pieces of Motr are developed by Seagate (eg: QoS, Function Shipping etc) to suit the needs of HPC and AI use cases. 
ATOS.. 
KTH is focusing on adopting the CORTX object storage with an I/O programming model that is suitable for large scale HPC and emerging ML/AI applications. To demonstrate the features of CORTX, pilot applications such as [iPIC3D](https://kth-hpc.github.io/iPIC3D/) and [StreamBrain](https://github.com/KTH-HPC/StreamBrain) are adapted to use CORTX object storage and function shipping to accelerate post-processing workloads.
Juelich is...
Kitware is focussing on providing visualition utilities for applications running on SAGE.
ARM is..
UKAEA is adapting the novel storage system for a unique HPC application involving parallel in time. The combination of the tiered storage with parallel in time aims to demonstrate application portability to the Exascale era. UKAEA also is contributing to areas such as global memory abstraction and is closely working with other CORTX partners to develop and use tools for studying performance alonside data management and analysis. 
University of Edinburgh is..
CEA is..

  * Maestro R&D project is building a data orchestration software layer that intelligently handles data movement in the I/O stack and has CORTX Motr as one of its primary backends.
Apart from Seagate - CEA, Juelich, ETH, Appentra, ECMWF and HPE are the key players and the project is lead by Juelich (administration) + HPE (technical).

  * The EsiWACE2 project consists of 20 EU partners from the weather and climate community lead by DKRZ (Germany). EsiWACE2 looks at weather and climate applications and is looking at leveraging CORTX Motr as one of the backends. 

Please help us populate this FAQ by letting us know what you most want to know about CORTX!  Feel free to ask questions in any of our [communications channels](SUPPORT.md) and we can all populate this FAQ as we learned what is actually frequently asked.


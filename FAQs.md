## Frequently Asked Questions

**What does CORTX stand for?**

   * Actually CORTX is not an acronym.  The marketing team just thought it looks cool and sounds cool.  :smiley:
 
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




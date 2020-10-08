## This is a WIP FAQ

**What does CORTX stand for?**

   * Actually CORTX is not an acronym.  The marketing team just thought it looks cool and sounds cool.  :smiley:

**Why introduce a new object store when there are already other object stores out there?**

   * We are certainly familar with the other really excellent object stores out there.  But the fact is that there are clearly many use cases for object storage today, with additional use cases emerging all the time, and we don't see it as a *one size fits all* world. Each object store has its own particular design choices which results in unique feature sets.  The more diversity of choices that are available, the more likely that different users/customers can find the system best suited for their needs and the uniformity of the S3 interface allows this without overly forcing users/customers into *vendor lock-in*.  Specific and unique to CORTX is our focus on enabling the next generation of mass capacity devices (e.g. HAMR) and the TCO benefits that come with that approach.  This is a unique focus and requires the object store itself to address a number of design/architectural challenges that the CORTX community is best positioned to address.  We have heard from many in the community that there is a clear need for an open source object store that is focused on enabling these mass capacity devices and that is the core motivation behind CORTX. The origin of this technology is from an EU funded program and the user/developer community, whom came together because they saw the need for a differently approach to object storage as they looked at the needs of application developers, the challenges of exascale/exabytes scale of storage and the need to optimally utilize emerging device level technology innovations. CORTX provides something different from other object stores in that it will uniquely leverage HDD innovations such as REMAN to reduce the likelihood of rebuild storms, HAMR to enable the largest capacity/lowest cost per bit next gen devices, and multi-actuator to retain IOPS per capacity ratios.  CORTX and the community are focused on such capabilities that are required in mass capacity deployments.
   
**What is the history of CORTX?**

   * CORTX began its life with the requirements gathering workshops through the E10 initiative circa 2012. There were multiple  workshops organized throughout the world on getting the requirements of building storage systems for extreme scale computing  involving stakeholders from the (primarily) extreme scale HPC, research data center community, etc. Early pioneers of the Lustre file system were also involved in those workshops & heavily contributed to the architecture and the designs. It was envisioned to build something from scratch on a clean slate considering the changes in hardware assumptions (multicore, many core, extreme parallelism, etc) that was coming about at that time. Then the CORTX initiative got funding from the European Union through multiple projects,  primarily SAGE (2014 – 2018),  Sage2 (2018 –) , and,  Maestro(2018 - ). The development and the ecosystem around CORTX was enabled by many advanced application developers, key technology vendors, OEMS, research and academic institutions in Europe; namely:  

     * CEA (France), DFKI (Germany), ARM (UK), KTH(Sweden), STFC(UK), UKAEA(UK), ATOS(France), Kitware(France), University of Edinburgh(UK), Diamond Light Source(UK) and Juelich Supercomputing Center (Germany),  HPE/Cray (Switzerland), CSCS (Switzerland), ECMWF (International) and  Appentra (Spain).  
   
     Some of the above are the largest producers and consumers of data and run some of the largest data centers on the planet with extreme requirements for storage and I/O. 
   
     CORTX was also used by the weather and climate community in the EsiWACE  (2015 – 2019) and EsiWACE2(2019 - )  projects with works lead by DKRZ (Germany) and University of Reading (UK).  


Please help us populate this FAQ by letting us know what you most want to know about CORTX!  Feel free to ask questions in any of our [communications channels](SUPPORT.md) and we can all populate this FAQ as we learned what is actually frequently asked.


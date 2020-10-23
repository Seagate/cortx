# Code Style Guide

We are excited that you are interested in contributing to the CORTX project! Since we are open source and have many contributors, it is very important that both our code (and the very necessary comments describing your code) follow consistent standards. In the table below please find all the Code Style Guides that we use for each language as well as which repositories use them. Thanks!

| **Language** 	| **Repository Names**	| 
|-	|-	|
|**[Bash](https://github.com/bahamas10/bash-style-guide)** | cortx-s3-server| 
|**[C](https://github.com/Seagate/cortx-motr/blob/dev/doc/coding-style.md)**| cortx-motr</br>cortx-posix</br>cortx-monitor</br> |
|**[C++](https://google.github.io/styleguide/cppguide.html)**  | cortx-s3-server|
|**[Java](https://google.github.io/styleguide/javaguide.html)** |cortx-s3-server|
|**[Python](https://google.github.io/styleguide/pyguide.html)**| cortx-s3-server</br> cortx-ha</br>cortx-posix</br>cortx-monitor</br> |
|**[Shell](https://google.github.io/styleguide/shellguide.html)**| cortx-ha</br>cortx-provisioner</br> |
| **YAML:**</br></p>**[Style](https://docs.saltstack.com/en/latest/topics/development/conventions/style.html)**</br>**[Formulae](https://docs.saltstack.com/en/latest/topics/development/conventions/formulas.html)**</br> | cortx-provisioner|

:warning: **Exceptions:** 

Some repositories have their own style guides, please refer to these repository-specific coding style guides.

- **[Bash, Python, and C](https://github.com/Seagate/cortx-hare/tree/dev/rfc/8)** - cortx-hare.
- **TODO:** Add links for cortx-manager and cortx-management-portal coding style.

:page_with_curl: **Our Values** 

The CORTX project is inclusive, and we have made it a priority to keep the project as accessible as possible by preferring literal and direct terminology over metaphorical language, slang, or other shorthand wherever possible. For example: 
  - Use *Allowlist* instead of *Whitelist*.
  - Replace the *Master and Slave* terminology, use terminology that more precisely reflects the relationship such as *Primary and Secondary* or *Main and Dev*. 
  
:page_with_curl: **Using Third Party Software**

To ensure that CORTX software remains available under our current open source licenses, please do not _copy-paste_ any software from other software repositories or from websites such as stackoverflow into any CORTX software. 

:page_with_curl: **Images in Documentation**
As they say, 'a picture is worth a thousand words', and we encourage everyone contributing to CORTX to help ensure our documentation is as easy to understand as possible and pictures can help with this.  To best enable collaboration and versioning, we further encourage using text-based image formats such as GraphViz and PlantUML and to use the 
following instructions to embed dynamically rendered images: [instructions for GraphViz files](images/graphviz/README.md) and 
[instructions for PlantUML files](images/plantuml/README.md).  This allows us to only _commit_ the text-based view and not have to attempt to commit and keep synchronized the binary views.

![alt text](https://graphvizrender.herokuapp.com/render?url=https://raw.githubusercontent.com/seagate/cortx/main/doc/images/graphviz/happy_example.dot)

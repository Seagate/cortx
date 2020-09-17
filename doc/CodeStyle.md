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
| **YAML:**</br>**[Style](https://docs.saltstack.com/en/latest/topics/development/conventions/style.html)**</br>**[Formulae](https://docs.saltstack.com/en/latest/topics/development/conventions/formulas.html)**</br> | cortx-provisioner|

**Exceptions:** 

Some repositories have their own style guides, please refer to these repository-specific coding style guides.

- **[Bash, Python, and C](https://github.com/Seagate/cortx-hare/tree/dev/rfc/8)** - cortx-hare.
- **TODO:** Add links for cortx-manager and cortx-management-portal coding style.

:page_with_curl: **Notes:** 

The CORTX project is inclusive, and we have made it a priority to keep the project as accessible as possible by preferring literal and direct terminology over metaphorical language, slang, or other shorthand wherever possible. For example: 
  - Use *Allowlist* instead of *Whitelist*.
  - Replace the *Master and Slave* terminology, use terminology that more precisely reflects the relationship such as *Primary and Secondary* or *Main and Dev*. 

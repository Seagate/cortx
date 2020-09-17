# Code Style Guide

We are glad that you are interested in contributing to the CORTX project. Since we are open source and have multiple contributors, it is necessary that our code is clean and easy to read. It is a good practice to add comments to your code for the benefit of your fellow contributors. We've listed all the Code Style Guides that are applicable to the CORTX repository so that your code solves a problem and remains human-readable. 

| **Coding Style Guides** 	| **Repository Names**	| 
|-	|-	|
| **[C++](https://google.github.io/styleguide/cppguide.html)**  | cortx-s3-server|
| **[Python](https://google.github.io/styleguide/pyguide.html)**| <ul> <li>cortx-s3-server</li> <li>cortx-ha</li><li>cortx-posix</li>	<li>cortx-monitor</li></ul> |
| **[Java](https://google.github.io/styleguide/javaguide.html)** |cortx-s3-server|
|**[Bash](https://github.com/bahamas10/bash-style-guide)** | cortx-s3-server| 
|**[Shell](https://google.github.io/styleguide/shellguide.html)**| <ul><li>cortx-ha</li><li>cortx-provisioner</li></ul>|
|**[CORTX-Motr Coding Style Guide for C](https://github.com/Seagate/cortx-motr/blob/dev/doc/coding-style.md)**| <ul><li>cortx-motr</li><li>cortx-posix</li><li>cortx-monitor</li></ul>|
| YAML for Salt: <ul><li> **[Style](https://docs.saltstack.com/en/latest/topics/development/conventions/style.html)** </li><li>**[Formulae](https://docs.saltstack.com/en/latest/topics/development/conventions/formulas.html)**</li></ul>| cortx-provisioner|

**Exceptions:** 

Some repositories have their own style guides, please refer to these repository-specific coding style guides.

- **[Bash, Python, and C](https://github.com/Seagate/cortx-hare/tree/dev/rfc/8)** - cortx-hare.
- **TODO:** Add links for cortx-manager and cortx-management-portal coding style.

:page_with_curl: **Notes:** 

The CORTX project is inclusive, and we have made it a priority to keep the project as accessible as possible by preferring literal and direct terminology over metaphorical language, slang, or other shorthand wherever possible. 

**Example:** 
- Use *Allowlist* instead of *Whitelist*.
- Replace the *Master and Slave* terminology, use terminology that more precisely reflects the relationship such as *Primary and Secondary* or *Main and Dev*. 

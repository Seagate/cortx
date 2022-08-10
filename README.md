[![Codacy Badge](https://app.codacy.com/project/badge/Grade/1478e64a9bf443a09f3922a79fa1ade4)](https://www.codacy.com/gh/Seagate/cortx/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=Seagate/cortx&amp;utm_campaign=Badge_Grade)
[![ license](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://github.com/Seagate/cortx/blob/main/LICENSE)
[![Slack](https://img.shields.io/badge/chat-on%20Slack-blue")](https://cortx.link/join-slack)
[![YouTube](https://img.shields.io/badge/Video-YouTube-red)](https://cortx.link/videos)
[![Latest Release](https://img.shields.io/github/v/release/Seagate/cortx?label=Latest%20Release)](https://github.com/seagate/cortx/releases/latest)
[![GitHub contributors](https://img.shields.io/github/contributors/Seagate/cortx)](https://github.com/Seagate/cortx/graphs/contributors/)
[![SODA Eco project](https://img.shields.io/badge/SODA-ECO%20Project-9cf)](./doc/Soda-welcome-page.md)
[![CORTX inclusive words scan](https://github.com/Seagate/cortx/actions/workflows/alex_reviewdog.yml/badge.svg)](https://github.com/Seagate/cortx/actions/workflows/alex_reviewdog.yml)


<!-- ![codacy-analysis-cli](https://github.com/Seagate/EOS-Sandbox/workflows/codacy-analysis-cli/badge.svg) -->

<img src="../main/doc/images/cortx-logo.png?raw=true">

# CORTX: World's Only 100% Open Source Mass-Capacity Optimized Object Store

The amount of data the world is creating and collecting is increasing massively. The amount of data the world is storing is not. The models and machine learning that are at the forefront of some of the most important research today depend on access to compete data sets, but limitations on storage lead to unnecessary data loss. By creating better, more economical storage solutions, we enable the research that is changing the world.

<p align="center"><img src="../main/doc/images/at_risk_data.jpg?raw=true" title="This graph, using data from IDC, shows the amount of at-risk data increasing annually.  CORTX enables the mass capacity drives that will allow this at-risk data to be saved."/></p>

## The CORTX Project

CORTX is a distributed object storage system designed for great efficiency, massive capacity, and high HDD-utilization.  **CORTX is 100% Open Source.** Most of the project is licensed under the [Apache 2.0 License](../main/LICENSE) and the rest is under AGPLv3; check the specific License file for each submodule to determine which is which.

### CORTX Project Scope & Core Design Goals

| Project Scope      | Core Design Goals                                                                                                                                                                |
|--------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Mass capacity | Object storage uniquely optimized for mass capacity storage devices       .                                                                                                       |
| Processor agnostic | Works with any processor.                                                                                                                                                        |
| Flexibility        | Highly flexible, works with HDD, SSD, and NVM.                                                                                                                                   |
| Scalability        | Massively Scalable. Scales up to a billion billion billion billion billion exabytes (2^206) and 1.3 billion billion billion billion (2^120) objects with unlimited object sizes. |
| Responsiveness     | Rapidly Responsive. Quickly retrieves data regardless of the scale using a novel Key-Value System that ensures low search latency across massive data sets.                      |
| Resilience         | Highly Resilient. Ensures a high tolerance for hardware failure and faster rebuild and recovery times using Network Erasure Coding, while remaining fully RAID-compatible.       |
| Transparency       | Provides specialized telemetry data and unmatched insight into system performance.                                                                                               |

You can read more about the technical goals and distinguishing features of CORTX [here.](https://github.com/Seagate/cortx-motr/blob/main/doc/motr-in-prose.md)

## CORTX Community Values

-   **Inclusive** - Our ambitions are global. The CORTX community is, too. The perspectives and skills necessary to achieve our goals are wide and varied; we believe in creating a community and a project that is inclusive, accessible, and welcoming to everyone.
-   **Open** - We are dedicated to remaining open and transparent. We believe in keeping CORTX Community code freely and fully available to be viewed, modified, and used without vendor lock-in or other in-built limitations.
-   **Inspired** - CORTX is all about the challenge. Our goals are not small: we want to build the world’s best scalable mass-capacity object storage system, one that can work with any hardware and interoperate with all workloads. CORTX is built on hard work, ingenuity, and an engineering mindset. We embrace hard problems and find inspired solutions.
-   **Evolving** – CORTX is continuously growing and adapting. As a community project, there is no limit to its development. We continuously make room for improvement and welcome the opportunities offered by the ever-evolving nature of community projects.

We are excited about your interest in CORTX and hope you will join us. We take our community very seriously, and we are committed to creating a community built on respectful interactions and inclusivity, as documented in our [Code of Conduct](CODE_OF_CONDUCT.md).  Because of our commitment to community, we work hard to track important community metrics; please feel free to check out how we do it and help us do it better in our [metrics folder](metrics).  We also publish our [Community Roadmap](Community_Roadmap.md); please let us know what other community events and milestones you would like to see.

## Get Started

- Explore multiple ways to acquire, build, and run CORTX in our [Quick Start Guide](QUICK_START.md).
- Jump into our [Contribution Guide](CONTRIBUTING.md) to build and test CORTX and to learn about how to make contributions.
   - Refer to our [Suggested Contributions](../main/doc/SuggestedContributions.md) page for some inspiration about how to contribute to the CORTX project.
- Please be aware that CORTX Community is not intended for production usage.  Please refer to our [terms and conditions](terms_and_conditions.md) for more details.
- Read about all the different [components](/doc/Components.md) that combine together to make CORTX

Resources
---------

<!-- NOTE!!! This is copied from SUPPORT.md.  If you update it here, update it there as well.) -->
-   Converse with us in our CORTX-Open Source Slack channel [![Slack](https://img.shields.io/badge/chat-on%20Slack-blue")](https://join.slack.com/t/cortxcommunity/shared_invite/zt-1cnj146n6-GA~0cau4VI1Ojs5UNdf99Q) to interact with community members and gets your questions answered.
-   Join us in [Discussions](https://github.com/Seagate/cortx/discussions) to ask, answer, and discuss topics with your fellow CORTX contributors.
-   Check out our [Youtube Channel](https://cortx.link/videos).
-   Browse a ton of architectural documents in our [Confluence Pages](https://seagate-systems.atlassian.net/wiki/spaces/PUB/overview)
    -  Unfortunately, public editing is not possible but anonymous comments are encouraged! Please "sign" comments with your github ID so we know how to interact with you   
-   Ask and answer questions in our [Frequently Asked Questions](FAQs.md) page.
-   If you'd like to contact us directly, drop us a mail at cortx-questions@seagate.com.
-   We have _CORTX stickers_ available to anyone who would like one. [Click here](https://www.seagate.com/promos/cortx-stickers/) to have a sticker mailed to you.
-   Subscribe to the our [developer newsletter](https://cortx.link/cortx-dev-newsletter) to and stay up to date on the latest CORTX developments, news, and events.  Browse our [archived newsletters](doc/PDFs/Newsletters) from previous months.
-   To know more about the CORTX Community events visit the [Upcoming Events](https://github.com/Seagate/cortx/wiki/Upcoming-Events) page.
-   Attend our [Monthly Meet an Architect](doc/meetings/README.rst) meetings to learn about CORTX architecture and participate in a Question and Answer Session.
-   Attend our [Motr Deep Dive training sessions](https://github.com/Seagate/cortx-motr/wiki/Motr-Deep-Dive-Sessions) or watch the previously recorded sessions.
-   Learn how to integrate CORTX with other technologies [here](doc/integrations/README.rst).
-   We like to highlight the work and contributions of our community members—if you have solved an interesting challenge, or you are interested in sharing your experience or use cases, we want to talk to you! Please email our Community Manager rachel.novak@seagate.com or [schedule a meeting with us](https://outlook.office365.com/owa/calendar/CORTXCommunity@seagate.com/bookings/s/x8yMn2ODxUCOdhxvXkH4FA2) to share.
-   Many folks are actively conducting various experiments, feasibility studies, and proofs of concept using CORTX.  Check it out in the [cortx-experiments](https://github.com/Seagate/cortx-experiments) repository and either join an existing experiment or start your own.
-   Please consider participating in [CORTX Hackathons](doc/CORTX_Hackathon.rst) and check out our [CORTX Community Roadmap](Community_Roadmap.md) for upcoming events! To know more about upcoming features in the next 24 months, check out our [CORTX Feature Roadmap](FeatureRoadmap.md)
-   Please [read what our Community members say](CORTXTestimonials.md) about how CORTX is helping them achieve their goals with our next-generation storage solution.

Thank You!
----------

We thank you for stopping by to check out the CORTX Community. We are fully dedicated to our mission to build open source technologies that help the world save unlimited data and solve challenging data problems. Join our mission to help reinvent a data-driven world.

[![ license](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://github.com/Seagate/EOS-Sandbox/blob/master/LICENSE) [![Codacy Badge](https://api.codacy.com/project/badge/Grade/1d5c09ab83b344348265c170ab37d3b7)](https://www.codacy.com?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=Seagate/EOS-Sandbox&amp;utm_campaign=Badge_Grade) ![codacy-analysis-cli](https://github.com/Seagate/EOS-Sandbox/workflows/codacy-analysis-cli/badge.svg)

# CORTX.  Bringing hyperscale storage stack capabilities to the masses.

In consultation with IDC, Seagate has determined that an immense amount of data (175 zettabytes) will be created in 2025.  Surprisingly, of this data, IDC predicts that only 17 zettabytes (about 10%) will be stored.  This at-risk 158 zettabytes of data slated to be discarded will have a tremendous amount of unrealized potential as multiple research efforts have determined that the accuracy of machine learning and data analytics directly correlates to the size of the input data set.  

![At Risk Data](../assets/images/at_risk_data.png?raw=true)

As such, Seagate has worked hard to understand both the software and hardware requirements that would allow more of this 164 at-risk zettabytes to be stored.  We have discovered that the underlying hardware requires innovation to improve density per cost and that the software architecture itself is prohibitively expensive.  The result of this analysis is Seagate's conclusion that a new software architecture is needed both to satisfy the economic limitations of the existing systems as well as to ensure efficient utilization of the co-evolving hardware innovations.  

To ensure maximum utililization and code quality and to avoid vendor lock-in, Seagate is open-sourcing CORTX: a distributed object storage system designed for efficient, mass-capacity, HDD-utilization.

## Building, Deploying, and Contributing

Please refer to our [online](https://docs.google.com/spreadsheets/d/1JkO1aI7wQy9LjANMTOU0XQAX8AqBOSI3DuWV4h_zPEM/edit#gid=859916141) [documentation](https://seagatetechnology.sharepoint.com/sites/GMSites/mero-launch-pad) for detailed technical information about how to build and deploy CORTX as well as for in-depth technical discussion about the architecture and many of the internal data structures used.

## Running the tests

Explain how to run the automated tests as well as the end to end tests for this system.  Also include how to conduct performance benchmarking.

### Discussion of the CI/CD

Explain what these tests test and why

```
Give an example
```

## Built With

* [Dropwizard](http://www.dropwizard.io/1.0.2/docs/) - The web framework used
* [Maven](https://maven.apache.org/) - Dependency Management
* [ROME](https://rometools.github.io/rome/) - Used to generate RSS Feeds

## Contributing

Please read [CONTRIBUTING.md](https://gist.github.com/PurpleBooth/b24679402957c63ec426) for details on our code of conduct, and the process for submitting pull requests to us.  Please refer to our [Style Guide](mero/doc/coding-style.md) for a description of our code style guidelines.

## Versioning

We use [SemVer](http://semver.org/) for versioning. For the versions available, see the [tags on this repository](https://github.com/your/project/tags). 

## Authors

* **Billie Thompson** - *Initial work* - [PurpleBooth](https://github.com/PurpleBooth)

See also the list of [contributors](https://github.com/your/project/contributors) who participated in this project.

## License

This project is licensed under the Apache 2.0 License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

* Hat tip to anyone whose code was used
* Inspiration
* etc


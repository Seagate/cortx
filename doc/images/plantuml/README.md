We can embed dynamically rendered PlantUML images into our github documentation.  Simply use the PlantUML proxy `http://www.plantuml.com/plantuml/proxy` to render your `*.iuml` or `*.plantuml` files and embed them like images:

```
![CORTX PlantUML Example](http://www.plantuml.com/plantuml/proxy?cache=no&src=https://raw.githubusercontent.com/johnbent/cortx-1/main/doc/images/plantuml/example.plantuml)
``` 

Use the URL to the `RAW` version of your PlantUML files, prefix it with `src=`, and use the `cache=no` option so that GitHub always renders the currently committed 
version.

For example, here is a dynamically rendered view of the [example.plantuml](https://raw.githubusercontent.com/johnbent/cortx-1/main/doc/images/plantuml/example.plantuml)
file in this directory:

![CORTX PlantUML Example](http://www.plantuml.com/plantuml/proxy?cache=no&src=https://raw.githubusercontent.com/johnbent/cortx-1/main/doc/images/plantuml/example.plantuml)

Thanks very much to @jonashackt for showing how this works in his [github repo](https://github.com/jonashackt/plantuml-markdown)!


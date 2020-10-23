To render a graphviz _image_ in a github markdown page, we can merely use the 'Raw' URL to the graphviz _text_ file to a special site which will render it for us.

Simply use the GraphVizRender site `https://graphvizrender.herokuapp.com/render` to render your `*.dot` files and embed them as dynamically generated images as so:

```
![alt text](https://graphvizrender.herokuapp.com/render?url=https://raw.githubusercontent.com/johnbent/cortx-1/main/doc/images/graphviz/example.dot)
``` 

Use the link to the `Raw` version of your `*.dot` files (e.g.
[https://raw.githubusercontent.com/johnbent/cortx-1/main/doc/images/graphviz/example.dot](https://raw.githubusercontent.com/johnbent/cortx-1/main/doc/images/graphviz/example.dot))
and prefix it with `url=`.

For example, here is a dynamically rendered image of the example graphviz file in this directory:

![CORTX Example Graphviz Dynamically Rendered](https://graphvizrender.herokuapp.com/render?url=https://raw.githubusercontent.com/johnbent/cortx-1/main/doc/images/graphviz/example.dot)

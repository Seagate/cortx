To render a graphviz _image_ in a github markdown page, we can merely use the 'Raw' URL to the graphviz _text_ file to a special site which will render it for us.

Simply use the GraphVizRender site `https://graphvizrender.herokuapp.com/render` to render your `*.dot` files and embed them as dynamically generated images as so:

```
![alt text](https://graphvizrender.herokuapp.com/render?"url of your raw file")
``` 

Use the link to the `Raw` version of your `*.dot` files. For example, copy the [link to the raw file](https://raw.githubusercontent.com/seagate/cortx/main/doc/images/graphviz/example.dot) and prefix it with the url shown above.

For example, here is a dynamically rendered image of a sample graphviz file in this directory:

![CORTX Example Graphviz Dynamically Rendered](https://graphvizrender.herokuapp.com/render?url=https://raw.githubusercontent.com/seagate/cortx/main/doc/images/graphviz/example.dot)

Note that if the image doesn't show up immediately that you might need to refresh the page.

Thanks very much to @grahamc for providing this service and for so nicely documenting how to use it in his [github repo](https://github.com/grahamc/graphvizrender).  
Note that if this service ever stops working, there may be another [comparable service](https://github.com/TLmaK0/gravizo).

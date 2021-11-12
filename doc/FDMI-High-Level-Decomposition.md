## Component Diagram
![image](./images/component-diagram.PNG)

Interfaces:

  * plugin interface:  

    * register (unregister) a filter,  

    * specify a call-back to be invoked when matching record appears;

  * source interface:

    * register (unregister) a source,

    * notify fdmi about new records,

    * match a record against a filter;

    * specify a call-back to be invoked when a record is no longer needed by fdmi (required, for example, to truncate fol);



Components responsibilities:

  * fdmi core:

    * keep information about plugins, filters and sources,

    * match incoming records against filters,

    * record transmission to plugins,

    * implement plugin interface,

    * implement source interface,

    * participate in transaction recovery;

  * fol source: use source interface;

  * sample plugin: use plugin interface.   


## Data-flow diagram   

![image](./images/data-flow-diagram.PNG)  

### Data-types

Key data-types introduced by fdmi are:

* filter: represents an fdmi plugin, specifies how records are matched, accepts matching records from fdmi;

* source: a source of records, provides records to fdmi;

* record: something that is produced by a source and can be matched against a filter.

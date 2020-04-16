Mero
====

Obtain sources
--------------

    $ git clone --recursive http://gerrit-sage.dco.colo.seagate.com:8080/mero
or
    $ git clone --recursive https://github.com/seagate-ssg/mero.git

Build
-----

    $ ./autogen.sh
    $ ./configure
    $ make

or in one command:

    $ ./scripts/m0 rebuild

Test
----

    $ ./scripts/m0 run-all

Get to know
-----------

  * [Mero Source Structure](doc/source-structure.md)
  * [Mero Coding Style](doc/coding-style.md)

Go deep...
----------

Still surfing? Surf this: [Mero Reading List](doc/reading-list.md)

    $ make doc
    $ x-www-browser doc/html/index.html

Welcome, Earthling!

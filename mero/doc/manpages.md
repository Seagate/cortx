How to add new manpages
-----------------------

There is a manpage template in doc/man/ directory:

    manpage-template.1.txt

To start writing a new manual page copy template to a new file with
appropriate name and section number:

    $ cp doc/man/manpage-template.1.txt doc/man/m0tool.1.txt

Then edit newly created manpage document and replace all occurrences of
'prog' with the name of your manpage (in our example it's 'm0tool').
Remove unneeded sections or add new by desire.

Then add new manpage to the list of MAN_PAGES in doc/Makefile.sub:

    MAN_PAGES += doc/man/m0tool.1.txt \

To check that new man page builds correctly one might run
`make doc-manpages` command.

All manpages are written in asciidoc, for more information on asciidoc
formatting refer to the following links:

1. [Quickref cheat sheat](http://powerman.name/doc/asciidoc)
2. [Reference guide](http://www.methods.co.nz/asciidoc/userguide.html)

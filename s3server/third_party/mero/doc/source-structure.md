Mero Source Code Structure
==========================

This document describes structure of the Mero source code tree.

Structure
---------

Each major Mero sub-system and component has its own sub-directory.  Such
sub-directory might contain other sub-directories for sub-sub-systems. In
addition, a sub-directory might contain the following standard sub-directories:

 - ut: for unit testing and unit benchmarking;

 - st: for system testing scripts;

Build
-----

Core build system is based on auto-* tools: autoheader, autoconf and
automake.

Core is built either by running "./autogen.sh && ./configure && make"
in its top-level directory or by running "../configure" from a
separate build directory where build output will be stored (the latter
will work only for user-space part, i.e. `make all-user`, because our
kerne-mode makefiles don't support this feature at the moment). This
process builds all the libraries and executable specified in the
current configuration (currently only one configuration is supported,
2010.06.27), including Oracle Berkeley DB, internal Mero libraries,
unit and system tests, benchmarks and Mero executables.

All sources needed by the current configuration are linked
together into mero/libmero.la. Then binaries, including
unit tests are built and linked against libmero.

Kernel code is built by using Linux kernel build. The modules for mero
components are generated in mero/ directory.

Recipes (for user space code)
=============================

How to add a new file to the source tree?
-----------------------------------------

Create a new file, say, foo/bar.c in the file system, edit it. Add bar.c to the
corresponding _SOURCES entry in foo/Makefile.sub. Add the file to git (git add
foo/bar.c) and commit your changes.

If you are planning to add file bar.h then bar.h should typically start with:

    #pragma once

    #ifndef __MERO_FOO_BAR_H__
    #define __MERO_FOO_BAR_H__

End the file bar.h with:

    #endif /* __MERO_FOO_BAR_H__ */

    /*
     *  Local variables:
     *  c-indentation-style: "K&R"
     *  c-basic-offset: 8
     *  tab-width: 8
     *  fill-column: 80
     *  scroll-step: 1
     *  End:
     */

New source files (.c and .h) can be generated using scripts/gen-src-skel.
The script adds copyright, include guards etc. according to the coding style.

How headers should be included in C sources?
--------------------------------------------

Include system headers as usual:

    #include <unistd.h>

To include a Mero header foo/foo_bar.h do

    #include "foo/foo_bar.h"

This applies even to the headers from the same directory, that is, foo/foo.c
must include foo/foo_internal.h as

    #include "foo/foo_internal.h"

How Mero build system is organized?
-----------------------------------

Please, read the "Quick start guide" section at the beginning of top-level
Makefile.am makefile.

How to add a new internal Mero library?
---------------------------------------

Create a new foo directory, if necessary. Add something like this to top-level
Makefile.am:

    noinst_LTLIBRARIES += foo/libmero-foo.la

    include $(top_srcdir)/foo/Makefile.sub

In foo/Makefile.sub put:

    foo_libmero_foo_la_HEADERS = foo/foo.h

    foo_libmero_foo_la_SOURCES = foo/foo.c \
				 foo/foo_internal.h \
				 foo/foo_bar.c

NOTICE: public headers (which are used by other mero modules) of
foo module should go into _HEADERS variable - this is what will be
installed to /usr/include on `make install`. While internal headers of
foo module should go into _SOURCES variable.

If necessary, add sources under foo/{ut,st} to the
ut_libmero_ut_la_SOURCES variable in foo/{ut,st}/Makefile.sub:

    ut_libmero_ut_la_SOURCES += foo/ut/foo_test.c

And include this foo/{ut,st}/Makefile.sub makefile into Makefile.am in
appropriate place (search where other ut/st makefiles are included, in
alphabetic order).

How to add a new Mero module?
-----------------------------

Set nobase_mero_include_HEADERS and mero_libmero_la_SOURCES variables
in foo/Makefile.sub:

    nobase_mero_include_HEADERS += foo/foo.h

    mero_libmero_la_SOURCES += foo/foo.c

Include foo/Makefile.sub in the top-level Makefile.am in appropriate
place in alphabetic order (search where other makefiles, which are
part of libmero.la library, are included).

How to add a new executable?
----------------------------

To build a new executable foo/baz add to the foo/Makefile.sub:

    noinst_PROGRAMS += foo/baz
    foo_baz_LDADD    = $(top_builddir)/mero/libmero.la

    include $(top_srcdir)/foo/Makefile.sub

And in foo/Makefile.sub:

    foo_baz_SOURCES = foo/baz.c \
                      foo/bar.c

**NOTE** if executable comprised only from one source file of the same
         name as the binary (foo/baz.c), there is no need to add a
         corresponding _SOURCES variable - automake handles this automatically.

Some real-world examples of module's Makefile.sub
-------------------------------------------------

The rpc/Makefile.sub can be used as a most complete example of how to write
module's Makefile.sub makefiles. It uses almost all constructs like _HEADERS,
_SOURCES, FF_FILES, XC_FILES and CLEANFILES.

System initialization and finalisation
--------------------------------------

mero/init.h declares functions

    int  m0_init(void);
    void m0_fini(void);

That must be called by an executable to respectively initialise and
finalise Mero internal libraries.

If your library (say, foo/libmero-foo.la) requires some global
(per address space) initialisation or finalisation actions (e.g.,
starting service threads, opening files), declare

    int  m0_foo_init(void);
    void m0_foo_fini(void);

in foo/foo.h, add

    #include "foo/foo.h"

to mero/init.c and add { &m0_foo_init, &m0_foo_fini } to the subsystem[] array,
defined in mero/init.c. foo_init() must follow standard return conventions: 0 on
success, negated errno value on failure.

How to add a new unit test?
---------------------------

M0 uses its own a unit test framework, which work in user and kernel space

To add a collection of unit tests ("test suite") for a module foo:

In a file foo/ut/foo.c do something like:

#include "ut/ut.h"

static void test_foo0(void)
{
	struct foo F;

	foo_init(&F);
	M0_UT_ASSERT(foo_bar(&F) == foo_baz(&F) + 1);
        ...
}

static void test_foo1(void)
{
        ...
}

...

static int foo_ut_init(void)
{
	/* prepare for testing */
        ...
}

static int foo_it_fini(void)
{
	/* cleanup after testing */
        ...
}

struct m0_ut_suite foo_ut = {
	.ts_name   = "foo-ut",
	.ts_init   = foo_ut_init, /* optional, may be NULL */
	.ts_fini   = foo_ut_fini, /* optional, may be NULL */
	.ts_owners = "D. Knuth"
	.ts_tests  = {
		{ "test0", test_foo0, "G. Swann" },
		{ "test1", test_foo1, "A. Simonet" },
		...
		{ NULL, NULL }
	}
};

In foo/ut/Makefile.sub do:

    ut_libmero_ut_la_SOURCES += foo/ut/foo.c ...

In Makefile.am add the following include line in appropriate place, in
alphabetic order (search where other ut makefiles are included).

    include $(top_srcdir)/foo/ut/Makefile.sub

In ut/m0ut.c add

    extern struct m0_ut_suite foo_ut;

declaration and

	m0_ut_add(&foo_ut);

to add_uts().

All unit tests are executed by ./utils/ut.

A few comments:

 * unit testing functions should use M0_UT_ASSERT() for testing;

 * unit testing functions should produce no output;

 * unit testing code should be ready to be ran multiple
   times. Avoid static initialisers or carefully re-set everything
   in foo_ut_fini();

 * unit tests are ran in an address space where m0_init() was
   already executed.

See lib/ut/*.c, stob/ut/adieu.c, fop/ut/fmt_test.c for
examples.

Note, that a unit test should create no files outside of current process
directory (except for possibly in its sub-directories).

How to add a new unit benchmark?
--------------------------------

Unit benchmarks for a component is a collection of micro-benchmarks
measuring performance of common code-paths at the function level.

It is clear that unit benchmarks are somewhat similar to unit tests. Indeed,
they usually happen to share a lot of code. As a result, unit benchmark code is
typically kept in the same foo/ut/foo.c code as unit tests for the same
component. See lib/ut/*.c and stob/ut/adieu.c for examples.

Note, that a unit benchmark should create no files outside of current
process directory (except for possibly in its sub-directories).

How to add a new kernel module?
-------------------------------

A new module can be added to Mero in two ways:

  1. functionality built into m0mero.ko
  2. module for stand-alone UT's

### functionality built into m0mero.ko

In mero all the functionality for kernel side is implemented into a single
kernel module called m0mero.ko.

So to add your new kernel source(s) for <module> to m0mero, create <module/>
directory and create a Kbuild.sub file there.

In Kbuild.sub file, add sources of your module to m0mero_objects variable, for
e.g.:

    m0mero_objects += fid/fid.o \
                      fid/fid_xc.o

create these source and header files in <module> directory.

Add foo/Kbuild.sub to EXTRA_DIST in the top-level Makefile.am.

### module for stand-alone UT's

If you want to build the module as separate stand-alone UT named <module>,
then in the top-level Kbuild makefile add it to the obj-m variable:

    obj-m += path/to/your/module/object.o

and create module/Kbuild.sub file with a list of source files of your module
(see case 1 for example) in <module>_objects variable.

Then create module/linux_kernel directory and place "kernel-only" files of your
module there.

Note: build system should be reconfigured after affected changes.

How to add a system test?
-------------------------

A system test (ST) is usually a bash script. Some of Mero STs are written
in C, Python, Expect.

Put your system test into top-level st/ directory. If tested functionality
is limited to that of a particular subsystem, the ST may be put into
<subsystem>/st/ directory.

If the ST creates any output files, like most of them do, this output should
go to a dedicated "sandbox" directory. The path to this directory should
be taken from `SANDBOX_DIR` environment variable. If this variable is not set,
the ST should come up with a default value,
e.g. "/var/mero/sandbox.<subsystem>-st".
Sandbox directory should be deleted if the test succeeds, and preserved
in case of failure.
Consider using `sandbox_init` and `sandbox_fini` shell functions, defined
in utils/functions.

    Rationale: separation of test output directories allows to aim `m0reportbug`
    tool at a specific output directory and gather forensic data of the failed
    test only.

Create an entry for the ST in scripts/st.d/ directory. This is needed in order
for the ST to be executed by `m0 run-st` command.

An ST should have executable bits set (`chmod +x`).

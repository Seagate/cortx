This document describes the user interface, which controls mero's tracing
mechanism. For higher-level overview of tracing and logging principles, used in
mero, please refer to the doc/logging-and-tracing and lib/trace.h documentation.


General info
------------

M0_LOG(), from lib/trace.h, is the main "function" of tracing API (actually it's
a macro). From a user's point of view it's similar to printk()/printf(3) - it
accepts printf-style format string with arguments and creates a trace-record
with this information.

By default, information from trace record is __not__ displayed on the console
immediately, but instead it's placed into a trace buffer, which is saved
automatically into a file with name m0.trace.PID, where PID is a numeric process
ID. Such trace files are created in a current working directory of a running
mero executable. The information from a trace file can then be analyzed and
printed to the console.

But, there is an option to print tracing information immediately during trace
record creation. It's controlled via '--enable-immediate-trace' option of
configure script. By default it's enabled.

Also, all trace records can be filtered by a subsystem, from which they
originated, by a tracing level, and can be displayed with different levels of
verbosity.

NOTICE 1: at the moment of writing, there is no support for kernel-space
trace buffer yet. Only immediate tracing is available in kernel-space.

NOTICE 2: all of the following explanation presumes that ENABLE_IMMEDIATE_TRACE
macro evaluates to true (which is a default behavior), i.e. no
'--disable-immediate-trace' option was provided to configure script.

Subsystem filtering
-------------------

Subsystem filtering is achieved by specifying a "subsystem mask". Different
variables and command-line arguments are used for kernel- and user- space (see
below), but mask values are the same:

  1. numeric mask in decimal or hex format:

        128        -- lnet subsystem
        0x80       -- same
        0x880      -- lnet and balloc
        0xffffffff -- all subsystems

  2. textual mask, a comma-separated list of subsystem names, '!' at the
     beginning is used to invert mask value, a special pseudo-subsystem 'all'
     represents all subsystems:

        cob             -- only 'cob' subsystem
        lnet,rpc        -- only 'lnet' and 'rpc'
        !balloc         -- all subsystems, except 'balloc'
        !sns,net,addb   -- all, except 'sns', 'net' and 'addb'
        all             -- all subsystems, equivalent to 0xffffffff

By default, trace immediate mask is set to 0, which means that no trace messages
are printed (actually, it's not quite true, because trace messages with level
M0_WARN and higher are printed independently of trace immediate mask setting).

A full list of available subsystems can be found in lib/trace.h in
M0_TRACE_SUBSYSTEMS macro.

Trace level specification
-------------------------

Mero trace levels are different from a "traditional" log-levels concept,
which for example is used in kernel's printk().

In Mero, each trace level can be enabled/disabled separately, because they
are implemented as a bitmask. This is done for greater flexibility. But there
are special trace levels with '+' at the end of a level name, which emulate
"traditional" log-levels.

The syntax to specify a desired trace level is as following:

    level[+][,level[+]] where level is one of call|debug|info|notice|warn|error|fatal

For example

    debug       -- enables only log messages with M0_DEBUG level
    info+       -- enables log messages with M0_INFO level and higher
    warn+       -- only M0_WARN, M0_ERROR and M0_FATAL messages are enabled,
                   this is the default trace level
    call+       -- all log messages are enabled
    call,error+ -- only M0_CALL, M0_ERROR and M0_FATAL are enabled

    call,debug,fatal -- only M0_CALL, M0_DEBUG and M0_FATAL

Notice, that trace messages with level M0_WARN and higher are printed
independently of trace immediate mask value. It means that they are always
printed, even if trace immediate mask is disabled for the subsystem to which
they belong. The only way to suppress M0_WARN, M0_ERROR or M0_FATAL messages is
disable them in trace level mask.

Trace print context
-------------------

It's possible to control formatting and amount of information in M0_LOG()
output. There are four levels of verbosity: none, func, short and full.

Here is an example, of how the same log message looks like on different print
context levels:

level 'none' (just log message, prefixed with "mero: " and '\n' at the end):

    mero: dumping free extents@0x7ffff5dcefa0:balloc ut for 00007cff

level 'func' (the same as level 'none', but also prints __func__ at the beginning):

    mero: m0_balloc_debug_dump_group_extent: dumping free extents@0x7ffff5dcefa0:balloc ut for 00007cff

level 'short' (trace level + [file:line:func] + message, this is a default one):

    mero:   CALL : [conf/conf_xcode.c:210:m0_confx_types_init] >
    mero:  ERROR : [rpc/rpc_machine.c:289:rpc_tm_setup] ! rc=-22 TM initialization
    mero:   CALL : [conf/conf_xcode.c:212:m0_confx_types_init] <

level 'full' (full information, spreads over two lines - trace point info
              and log message):

    25670920 567533955564586 fd370 <oumcrfalsncBli>   DEBUG   m0_balloc_debug_dump_group_extent        balloc.c:97
        mero: dumping free extents@0x7ffff5dcefa0:balloc ut for 00007cff

There are different ways to set these controls in user-space and kernel-space
mero applications.


User-space
----------

Subsystem filtering is controlled in two ways:

  1. environment variable:

        $ export M0_TRACE_IMMEDIATE_MASK='!rpc'
        $ ./utils/ut.sh

  2. CLI options for utils/ut:

        -m     string: trace mask, either numeric (HEX/DEC) or comma-separated
                       list of subsystem names, use ! at the beginning to invert

        -M           : print available trace subsystems

        $ ./utils/ut.sh -m 'lnet,layout'

      CLI option overrides environment variable


Trace levels:

  1. environment variable:

        export M0_TRACE_LEVEL=debug
        ./utils/ut.sh

  2. CLI options for utils/ut:

        -e     string: trace level: level[+][,level[+]] where level is one of
	               call|debug|info|warn|error|fatal

        $ ./utils/ut.sh -e call,warn+

      CLI option overrides environment variable


Trace print context:

  1. environment variable:

        export M0_TRACE_PRINT_CONTEXT=none
        ./utils/ut.sh

  2. CLI options for utils/ut:

        -p     string: trace print context, values: none, func, short, full

        $ ./utils/ut.sh -p func

       CLI option overrides environment variable


Kernel space
============

At the moment of writing, the only way to control tracing in kernel space is via
kernel module parameters.

Subsystem filtering:

    $ insmod m0mero.ko trace_immediate_mask=all

Trace levels:

    $ insmod m0mero.ko trace_level='debug+'

Trace print context:

    $ insmod m0mero.ko trace_print_context='full'


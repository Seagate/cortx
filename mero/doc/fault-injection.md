Fault injection API
-------------------

Fault injection API provides functions to set "fault points" inside the code,
and functions to enable/disable an actuation of these points. It's aimed at
increasing code coverage by enabling execution of error-handling code paths,
which are not covered otherwise by unit tests.

This API is enabled only when ENABLE_FAULT_INJECTION macro is defined.
Otherwise it's automatically compiled out to avoid unneeded overhead in
production code. ENABLE_FAULT_INJECTION macro is defined by default, but it can
be disabled with --disable-finject option of configure script.

A basic API consists of the following functions and macros:

    /* used for both, declaration of FP and for checking if it's enabled */
    bool M0_FI_ENABLED(tag)

    /* used to enable FP */
    void m0_fi_enable(func, tag)
    void m0_fi_enable_once(func, tag)
    void m0_fi_enable_random(func, tag, p)
    void m0_fi_enable_off_n_on_m(func, tag, n, m)
    void m0_fi_enable_each_nth_time(func, tag, n)
    void m0_fi_enable_func(func, tag, trigger_func, data)

    /* used to disable FP */
    void m0_fi_disable(func, tag);

More detailed information on each function can be found in lib/finject.h file or
in doxygen documentation in section "Fault Injection".

A typical use case is to place a fault point inside the function of interest and
do some action if fault point is enabled:

    void *m0_alloc(size_t n)
    {
     ...
     if (M0_FI_ENABLED("fake_error"))
        return NULL;
     ...
    }

Fault point is defined in place of M0_FI_ENABLED() usage. In this example a
fault point with tag "fake_error" is created in function "m0_alloc", which can
be enabled/disabled from external code with something like the following:

    m0_fi_enable_once("m0_alloc", "fake_error");

Fault injection is used mainly in UT to test error handling code paths. Usually
such checks are consolidated in a dedicated test of particular test suite. One
thing should be taken into account while writing such tests. Many error
conditions are logged using Mero tracing API (M0_LOG). And because fault
injection is used to simulate errors, such errors are usually reported via
M0_LOG(), creating some confusion with real error's logs. To help distinguish
real errors from faked errors, a special markers need to be added to mark
beginning and end of the test block, which uses fault injection. For example:

    void test_module_failure()
    {
        M0_LOG(M0_INFO, "-----BEGIN FINJECT TEST-----");

        ...use fault injection here...

        M0_LOG(M0_INFO, "-----END FINJECT TEST-----");
    }


m0ctl.ko driver
---------------

m0ctl driver provides a debugfs interface to control m0mero in runtime. All
control files are placed under "mero/" directory in the root of debugfs file
system. For more details about debugfs please see
Documentation/filesystems/debugfs.txt in the linux kernel's source tree.

Among other, it provides fault injection control interface, which
consist of the following files:

* `mero/finject/stat`   Provides information about all registered fault
                        points.

* `mero/finject/ctl`    Allows to change state of existing fault points
                        (enable/disable).

finject/stat can be read with a simple `cat` or `less` commands, but it's
contents is formatted as a text table with quite long rows, which looks not very
nice when line wrapping occurs. So it's better to use some command like `less
-S` which allows to unwrap long lines and scroll text horizontally:

    $ less -S /sys/kernel/debug/mero/finject/stat

finject/ctl accepts commands in the following format:

    COMMAND = ACTION function_name fp_tag [ ACTION_ARGUMENTS ]

    ACTION = enable | disable

    ACTION_ARGUMENTS = FP_TYPE [ FP_DATA ]

    FP_TYPE = always | oneshot | random | off_n_on_m

    FP_DATA = integer { integer }

Here some examples:

    enable m0_init fake_error oneshot
    enable m0_alloc fake_failure random 30
    enable m0_rpc_conn_start fake_success always
    enable m0_net_buffer_del need_fail off_n_on_m 2 5

    disable m0_init fake_error
    disable m0_net_buffer_del need_fail

The easiest way to send a command is to use `echo`:

    $ echo 'enable m0_init fake_error always' > /sys/kernel/debug/mero/finject/ctl


ut/m0ut CLI options to control fault injection
----------------------------------------------

New CLI options of utils/ut, which allow to enable fault points just after
m0_init():

    -f <fp list>   : fault point to enable func:tag:type[:integer[:integer]]

    -F <file name> : yaml file, which contains a list of fault points to enable

    -s             : report fault injection stats before any UT execution

    -S             : report fault injection stats after all UT execution

The input string for -f option should be in format:

    func:tag:type[:integer[:integer]][,func:tag:type[:integer[:integer]]]

For example:

    m0_alloc:fake_failure:always
    m0_net_buffer_add:need_fail:off_n_on_m:2:1
    m0_alloc:fake_failure:random:50,m0_list_add:nomem:oneshot

Input yaml file for -F option has a simple format, where each FP is described by
a yaml mapping with the following keys:

    func  - a name of the target function, which contains fault point
    tag   - a fault point tag
    type  - a fault point type, possible values are: always, oneshot, random,
            off_n_on_m
    p     - data for 'random' fault point
    n     - data for 'off_n_on_m' fault point
    m     - data for 'off_n_on_m' fault point

An example of yaml file:

    ---

    - func: test_func1
      tag:  test_tag1
      type: random
      p:    50

    - func: test_func2
      tag:  test_tag2
      type: oneshot

    # yaml mappings can be specified in a short form as well
    - { func: test_func3, tag:  test_tag3, type: off_n_on_m, n: 3, m: 1 }


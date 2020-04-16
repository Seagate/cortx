
cc_binary(
    # How to run build
    # bazel build //:s3server --cxxopt="-std=c++11"
    #                         --define MERO_INC=<mero headers path>
    #                         --define MERO_LIB=<mero lib path>
    #                         --define MERO_HELPERS_LIB=<mero helpers lib path>
    #                         --define MERO_EXTRA_LIB=<mero extra lib path>
    # To build s3server with debug symbols (to be able to analyze core files,
    # or to run under GDB) add the following option to the command line
    # arguments listed above:
    #                         --strip=never
    # Without this option, bazel strips debug symbols from the binary.

    name = "s3server",

    srcs = glob(["server/*.cc", "server/*.c", "server/*.h",
                 "mempool/*.c", "mempool/*.h"]),

    # In case of release mode we may have to remove option -ggdb3
    # In case of debug mode we may have to remove option -O3
    copts = [
      "-DEVHTP_HAS_C99", "-DEVHTP_SYS_ARCH=64", "-DGCC_VERSION=4002",
      "-DHAVE_CONFIG_H", "-DM0_TARGET=ClovisTest", "-D_REENTRANT",
      "-D_GNU_SOURCE", "-DM0_INTERNAL=", "-DM0_EXTERN=extern",
      # Do NOT change the order of strings in below line
      "-iquote", "$(MERO_INC)", "-isystem", "$(MERO_INC)",
      "-iquote", ".", "-include", "config.h", "-I/usr/include/libxml2",
      "-fno-common", "-Wall", "-Wno-attributes", "-fno-strict-aliasing",
      "-fno-omit-frame-pointer", "-Werror", "-ggdb3", "-O3", "-DNDEBUG",
    ],

    includes = [
      "third_party/libevent/s3_dist/include/",
      "third_party/libevhtp/s3_dist/include/evhtp",
      "third_party/jsoncpp/dist",
      "$(MERO_INC)",
      "mempool",
    ],

    # For the core file to show symbols and also for backtrace call
    # in s3server code to show the stack trace we need -rdynamic flag
    # https://www.gnu.org/software/libc/manual/html_node/Backtraces.html
    linkopts = [
      "-rdynamic",
      "-L$(MERO_LIB)",
      "-L$(MERO_HELPERS_LIB)",
      "-L$(MERO_EXTRA_LIB)",
      "-Lthird_party/libevent/s3_dist/lib/",
      "-Lthird_party/libevhtp/s3_dist/lib",
      "-levhtp -levent -levent_pthreads -levent_openssl -lssl -lcrypto -llog4cxx",
      "-lpthread -ldl -lm -lrt -lmero-helpers -lmero -laio",
      "-lyaml -lyaml-cpp -luuid -pthread -lxml2 -lgflags -lhiredis",
      "-pthread -lglog",
      "-Wl,-rpath,/opt/seagate/s3/libevent",
    ],
)

cc_test(
    # How to run build
    # bazel build //:s3ut --cxxopt="-std=c++11"
    #                     --define MERO_INC=<mero headers path>
    #                     --define MERO_LIB=<mero lib path>
    #                     --define MERO_HELPERS_LIB=<mero helpers lib path>
    #                     --define MERO_EXTRA_LIB=<mero extra lib path>

    name = "s3ut",

    srcs = glob(["ut/*.cc", "ut/*.h",
                 "server/*.cc", "server/*.c", "server/*.h",
                 "mempool/*.c", "mempool/*.h"],
                 exclude = ["server/s3server.cc"]),

    copts = [
      "-DEVHTP_DISABLE_REGEX", "-DEVHTP_HAS_C99", "-DEVHTP_SYS_ARCH=64",
      "-DGCC_VERSION=4002", "-DHAVE_CONFIG_H", "-DM0_TARGET=ClovisTest",
      "-D_REENTRANT", "-D_GNU_SOURCE", "-DM0_INTERNAL=", "-DS3_GOOGLE_TEST",
      "-DM0_EXTERN=extern", "-pie", "-Wno-attributes", "-O3", "-Werror",
      # Do NOT change the order of strings in below line
      "-iquote", "$(MERO_INC)", "-isystem", "$(MERO_INC)",
      "-I/usr/include/libxml2",
    ],

    includes = [
      "third_party/libevent/s3_dist/include/",
      "third_party/libevhtp/s3_dist/include/evhtp",
      "third_party/jsoncpp/dist",
      "$(MERO_INC)",
      "server/",
      "mempool",
    ],

    linkopts = [
      "-rdynamic",
      "-L$(MERO_LIB)",
      "-L$(MERO_HELPERS_LIB)",
      "-L$(MERO_EXTRA_LIB)",
      "-Lthird_party/libevent/s3_dist/lib/",
      "-Lthird_party/libevhtp/s3_dist/lib",
      "-levhtp -levent -levent_pthreads -levent_openssl -lssl -lcrypto -llog4cxx",
      "-lpthread -ldl -lm -lrt -lmero -lmero-helpers -laio",
      "-lyaml -lyaml-cpp -luuid -pthread -lxml2 -lgtest -lgmock -lgflags",
      "-pthread -lglog -lhiredis",
      "-Wl,-rpath,third_party/libevent/s3_dist/lib",
    ],

    data = [
      "resources",
    ],
)

cc_test(
    # How to run build
    # bazel build //:s3utdeathtests --cxxopt="-std=c++11"
    #                               --define MERO_INC=<mero headers path>
    #                               --define MERO_LIB=<mero lib path>
    #                               --define MERO_HELPERS_LIB=<mero helpers lib path>
    #                               --define MERO_EXTRA_LIB=<mero extra lib path>

    name = "s3utdeathtests",

    srcs = glob(["ut_death_tests/*.cc", "ut_death_tests/*.h",
                 "server/*.cc", "server/*.c", "server/*.h",
                 "mempool/*.c", "mempool/*.h"],
                 exclude = ["server/s3server.cc"]),

    copts = [
      "-DEVHTP_DISABLE_REGEX", "-DEVHTP_HAS_C99", "-DEVHTP_SYS_ARCH=64",
      "-DGCC_VERSION=4002", "-DHAVE_CONFIG_H", "-DM0_TARGET=ClovisTest",
      "-D_REENTRANT", "-D_GNU_SOURCE", "-DM0_INTERNAL=",
      "-DM0_EXTERN=extern", "-pie", "-Wno-attributes", "-O3", "-Werror",
      # Do NOT change the order of strings in below line
      "-iquote", "$(MERO_INC)", "-isystem", "$(MERO_INC)",
      "-I/usr/include/libxml2",
    ],

    includes = [
      "third_party/libevent/s3_dist/include/",
      "third_party/libevhtp/s3_dist/include/evhtp",
      "third_party/jsoncpp/dist",
      "ut/",
      "$(MERO_INC)",
      "server/",
      "mempool",
    ],

    linkopts = [
      "-rdynamic",
      "-L$(MERO_LIB)",
      "-L$(MERO_HELPERS_LIB)",
      "-L$(MERO_EXTRA_LIB)",
      "-Lthird_party/libevent/s3_dist/lib/",
      "-Lthird_party/libevhtp/s3_dist/lib",
      "-levhtp -levent -levent_pthreads -levent_openssl -lssl -lcrypto -llog4cxx",
      "-lpthread -ldl -lm -lrt -lmero-helpers -lmero -laio",
      "-lyaml -lyaml-cpp -luuid -pthread -lxml2 -lgtest -lgmock -lgflags",
      "-pthread -lglog -lhiredis",
      "-Wl,-rpath,third_party/libevent/s3_dist/lib",
    ],

    data = [
      "resources",
    ],
)

cc_binary(
    # How to run build
    # bazel build //:s3perfclient

    name = "s3perfclient",

    srcs = glob(["perf/*.cc"]),

    copts = ["-std=c++11", "-fPIC", "-DEVHTP_HAS_C99", "-DEVHTP_SYS_ARCH=64", "-O3"],

    includes = ["third_party/libevent/s3_dist/include/",
                "third_party/libevhtp/s3_dist/include/evhtp",
                "server/"],

    linkopts = ["-Lthird_party/libevent/s3_dist/lib",
                "-Lthird_party/libevhtp/s3_dist/lib",
                "-levhtp -levent -levent_pthreads -levent_openssl -lssl -lcrypto -lgflags -llog4cxx",
                "-lpthread -ldl -lrt",
                "-Wl,-rpath,third_party/libevent/s3_dist/lib"],
)

cc_binary(
    # How to run build
    # bazel build //:cloviskvscli --cxxopt="-std=c++11"
    #                             --define MERO_INC=<mero headers path>
    #                             --define MERO_LIB=<mero lib path>
    #                             --define MERO_HELPERS_LIB=<mero helpers lib path>
    #                             --define MERO_EXTRA_LIB=<mero extra lib path>

    name = "cloviskvscli",

    srcs = glob(["kvtool/*.cc", "kvtool/*.c", "kvtool/*.h"]),

    copts = [
      "-DEVHTP_HAS_C99", "-DEVHTP_SYS_ARCH=64", "-DGCC_VERSION=4002",
      "-DHAVE_CONFIG_H", "-DM0_TARGET=ClovisTest", "-D_REENTRANT",
      "-D_GNU_SOURCE", "-DM0_INTERNAL=", "-DM0_EXTERN=extern",
      # Do NOT change the order of strings in below line
      "-iquote", "$(MERO_INC)", "-isystem", "$(MERO_INC)",
      "-iquote", ".", "-include", "config.h",
      "-fno-common", "-Wall", "-Wno-attributes", "-fno-strict-aliasing",
      "-fno-omit-frame-pointer", "-Werror", "-ggdb3", "-O3", "-DNDEBUG",
    ],

    includes = [
      "$(MERO_INC)",
    ],

    linkopts = [
      "-rdynamic",
      "-L$(MERO_LIB)",
      "-L$(MERO_HELPERS_LIB)",
      "-L$(MERO_EXTRA_LIB)",
      "-lpthread -ldl -lm -lrt -lmero-helpers -lmero -laio",
      "-lgflags",
      "-pthread -lglog",
    ],
)

cc_test(
    # How to run build
    # bazel build //:s3mempoolut --cxxopt="-std=c++11"

    name = "s3mempoolut",

    srcs = glob(["mempool/*.c", "mempool/*.h", "mempool/ut/*.cc"]),

    copts = ["-DEVHTP_HAS_C99", "-DEVHTP_SYS_ARCH=64", "-O3"],

    includes = [
      "mempool/",
    ],

    linkopts = [
      "-lpthread -ldl -lm -lrt -lgtest -lgmock -rdynamic",
    ],

)

cc_test(
    # How to run build
    # bazel build //:s3mempoolmgrut --cxxopt="-std=c++11"
    #                     --define MERO_INC=<mero headers path>
    #                     --define MERO_LIB=<mero lib path>
    #                     --define MERO_HELPERS_LIB=<mero helpers lib path>
    #                     --define MERO_EXTRA_LIB=<mero extra lib path>

    name = "s3mempoolmgrut",

    srcs = glob(["s3mempoolmgrut/*.cc", "s3mempoolmgrut/*.h",
                 "server/*.cc", "server/*.c", "server/*.h",
                 "mempool/*.c", "mempool/*.h"],
                 exclude = ["server/s3server.cc"]),

    copts = [
      "-DEVHTP_DISABLE_REGEX", "-DEVHTP_HAS_C99", "-DEVHTP_SYS_ARCH=64",
      "-DGCC_VERSION=4002", "-DHAVE_CONFIG_H", "-DM0_TARGET=ClovisTest",
      "-D_REENTRANT", "-D_GNU_SOURCE", "-DM0_INTERNAL=", "-DS3_GOOGLE_TEST",
      "-DM0_EXTERN=extern", "-pie", "-Wno-attributes", "-O3", "-Werror",
      # Do NOT change the order of strings in below line
      "-iquote", "$(MERO_INC)", "-isystem", "$(MERO_INC)",
      "-I/usr/include/libxml2",
    ],

    includes = [
      "third_party/libevent/s3_dist/include/",
      "third_party/libevhtp/s3_dist/include/evhtp",
      "third_party/jsoncpp/dist",
      "$(MERO_INC)",
      "server/",
      "mempool",
    ],

    linkopts = [
      "-rdynamic",
      "-L$(MERO_LIB)",
      "-L$(MERO_HELPERS_LIB)",
      "-L$(MERO_EXTRA_LIB)",
      "-Lthird_party/libevent/s3_dist/lib/",
      "-Lthird_party/libevhtp/s3_dist/lib",
      "-levhtp -levent -levent_pthreads -levent_openssl -lssl -lcrypto -llog4cxx",
      "-lpthread -ldl -lm -lrt -lmero-helpers -lmero -laio",
      "-lyaml -lyaml-cpp -luuid -pthread -lxml2 -lgtest -lgmock -lgflags",
      "-pthread -lglog -lhiredis",
      "-Wl,-rpath,third_party/libevent/s3_dist/lib",
    ],
)

cc_library(
    # How to run build
    # bazel build //:s3addbplugin --define MERO_INC=<mero headers path>
    #                             --define MERO_LIB=<mero lib path>
    #                             --define MERO_HELPERS_LIB=<mero helpers lib path>
    #                             --define MERO_EXTRA_LIB=<mero extra lib path>
    # To build with debug symbols (to be able to analyze core files,
    # or to run under GDB) add the following option to the command line
    # arguments listed above:
    #                         --strip=never
    # Without this option, bazel strips debug symbols from the binary.

    name = "s3addbplugin",

    srcs = glob(["addb/plugin/*.c", "addb/plugin/*.h", "server/s3_addb*.h", "server/s3_addb_map*.c"]),

    # In case of release mode we may have to remove option -ggdb3
    # In case of debug mode we may have to remove option -O3
    copts = [
      "-DEVHTP_HAS_C99", "-DEVHTP_SYS_ARCH=64", "-DGCC_VERSION=4002",
      "-DHAVE_CONFIG_H", "-DM0_TARGET=ClovisTest", "-D_REENTRANT",
      "-D_GNU_SOURCE", "-DM0_INTERNAL=", "-DM0_EXTERN=extern",
      # Do NOT change the order of strings in below line
      "-iquote", "$(MERO_INC)", "-isystem", "$(MERO_INC)",
      "-iquote", ".", "-include", "config.h", "-I/usr/include/libxml2",
      "-iquote", "server/", "-fno-common", "-Wall", "-Wno-attributes",
      "-fno-strict-aliasing", "-fno-omit-frame-pointer", "-Werror", "-ggdb3",
      "-O3", "-DNDEBUG",
    ],

    includes = [
      "$(MERO_INC)",
    ],
)


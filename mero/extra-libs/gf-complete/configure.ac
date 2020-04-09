# gf-complete autoconf template

# FIXME - add project url as the last argument
AC_INIT(gf-complete, 1.0)

# Override default CFLAGS
: ${CFLAGS="-Wall -Wpointer-arith -O3 -g"}

AC_PREREQ([2.61])

AM_INIT_AUTOMAKE([no-dependencies foreign parallel-tests silent-rules])
AM_SILENT_RULES([yes])
LT_INIT # libtool

AC_CONFIG_HEADER(include/gf/config.h)

dnl Needed when reconfiguring with 'autoreconf -i -s'
AC_CONFIG_MACRO_DIR([m4])

# This prevents './configure; make' from trying to run autotools.
AM_MAINTAINER_MODE([disable])

dnl Compiling with per-target flags requires AM_PROG_CC_C_O.
AC_PROG_CC

SRCDIR="$srcdir"
ABS_SRCDIR="$(cd $srcdir; pwd)"
ABS_BUILDDIR="$(pwd)"

AC_SUBST([GF_ABS_SRCDIR], [$ABS_SRCDIR])

# Check for functions to provide aligned memory
#
AC_CHECK_FUNCS([posix_memalign],
 [found_memalign=yes; break])

AS_IF([test "x$found_memalign" != "xyes"], [AC_MSG_WARN([No function for aligned memory allocation found])])

AC_ARG_WITH([lustre],
        [AS_HELP_STRING([--without-lustre],
                        [disable kernel modules (see Mero configure documentation)])])
AM_CONDITIONAL([ENABLE_KERNEL_MODULES], [test x$with_lustre != xno])

AX_EXT()

AC_ARG_ENABLE([neon],
              AS_HELP_STRING([--disable-neon], [Build without NEON optimizations]))

AS_IF([test "x$enable_neon" != "xno"],
      [noneon_CPPFLAGS=$CPPFLAGS
       CPPFLAGS="$CPPFLAGS $SIMD_FLAGS"
       AC_CHECK_HEADER([arm_neon.h],
                       [have_neon=yes],
                       [have_neon=no
                        CPPFLAGS=$noneon_CPPFLAGS])],
      [have_neon=no
       AS_IF([test "x$ax_cv_have_neon_ext" = "xyes"],
             [SIMD_FLAGS=""])
      ])

AS_IF([test "x$have_neon" = "xno"],
      [AS_IF([test "x$enable_neon" = "xyes"],
             [AC_MSG_ERROR([neon requested but arm_neon.h not found])])
      ])
AM_CONDITIONAL([HAVE_NEON], [test "x$have_neon" = "xyes"])

AC_ARG_ENABLE([sse],
              AS_HELP_STRING([--disable-sse], [Build without SSE optimizations]),
              [if   test "x$enableval" = "xno" ; then
                SIMD_FLAGS=""
                echo "DISABLED SSE!!!"
              fi]
)

AC_ARG_ENABLE([avx],
              AS_HELP_STRING([--disable-avx], [Build without AVX optimizations]),
              [if test "x$enableval" = "xno" ; then
                SIMD_FLAGS="$SIMD_FLAGS -mno-avx"
                echo "DISABLED AVX!!!"
                AC_DEFINE(HAVE_AVX, 0)
              fi]
)

AC_ARG_ENABLE([force-ssse3],
              AS_HELP_STRING([--enable-force-ssse3], [Force build with ssse3 even if processor does not support]),
              [if test "x$enableval" = "xyes" ; then
                echo "Force build with ssse3!!!"
		SIMD_FLAGS="$SIMD_FLAGS -mssse3 -DINTEL_SSSE3"
              fi]
)

# setup linux path
AC_ARG_WITH([linux],
        [AS_HELP_STRING([--with-linux=path],
                        [set path to Linux kernel dir, default is /lib/modules/`uname -r`/build])],
        [LINUX=$with_linux; LINUX_OBJ=$LINUX; LINUX_MOD=$LINUX],
        [LINUX=/lib/modules/`uname -r`; LINUX_OBJ=$LINUX/build; LINUX_MOD=$LINUX]
)
AC_SUBST([LINUX_OBJ])
AC_SUBST([LINUX_MOD])
AC_CONFIG_FILES([src/linux_kernel/Kbuild])

AC_CONFIG_FILES([Makefile src/Makefile tools/Makefile test/Makefile examples/Makefile])
AC_OUTPUT

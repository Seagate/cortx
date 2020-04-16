# Check linux kernel symbols file
AC_DEFUN([MERO_LINUX_CHECK_SYMBOL],[
        AC_MSG_CHECKING([for symbols file])
        if test -f $LINUX_OBJ/Module.symvers; then
                SYMVERFILE=$LINUX_OBJ/Module.symvers
        elif test -f $LINUX_OBJ/Modules.symvers; then
                SYMVERFILE=$LINUX_OBJ/Modules.symvers
        else
                AC_MSG_ERROR([Failed to find symbols files in $LINUX_OBJ])
        fi
	AC_MSG_RESULT([$SYMVERFILE])

        AC_MSG_CHECKING([for symbol $1 exported])
        grep -q -E '[[[:space:]]]$1[[[:space:]]]' $SYMVERFILE 2>/dev/null
        rc=$?
        if test $rc -eq 0; then
                AC_MSG_RESULT([yes])
                $2
        else
                AC_MSG_RESULT([no])
                $3
        fi
])


AC_DEFUN([MERO_LINUX_CONFTEST],
[cat >conftest.c <<_ACEOF
$1
_ACEOF
])


AC_DEFUN([MERO_LANG_PROGRAM],
[$1
int
main (void)
{
dnl Do *not* indent the following line: there may be CPP directives.
dnl Don't move the `;' right after for the same reason.
$2
  ;
  return 0;
}])


AC_DEFUN([MERO_LINUX_COMPILE_IFELSE],
[m4_ifvaln([$1], [MERO_LINUX_CONFTEST([$1])])
rm -f config/conftest.o config/conftest.mod.c config/conftest.ko
AS_IF([AC_TRY_COMMAND([cp conftest.c config && cd config && make -d $2 ${LD:+"LD=$LD"} CC="$CC" LINUX_OBJ=$LINUX_OBJ]) >/dev/null && AC_TRY_COMMAND([$3])],
	[$4],
	[_AC_MSG_LOG_CONFTEST
m4_ifvaln([$5],[$5])])
rm -f config/conftest.o config/conftest.mod.c config/conftest.mod.o config/conftest.ko m4_ifval([$1], [config/conftest.c conftest.c])[]
])


AC_DEFUN([MERO_LINUX_TRY_COMPILE],[
        MERO_LINUX_COMPILE_IFELSE(
                [AC_LANG_SOURCE([MERO_LANG_PROGRAM([[$1]], [[$2]])])],
                [modules],
                [test -s config/conftest.o],
                [$3], [$4])
])


AC_DEFUN([MERO_LINUX_CONFIG],
[AC_MSG_CHECKING([if Linux was built with CONFIG_$1])
MERO_LINUX_TRY_COMPILE([
#include <linux/autoconf.h>
],[
#ifndef CONFIG_$1
#error CONFIG_$1 not #defined
#endif
],[
        AC_MSG_RESULT([yes])
        $2
],[
        AC_MSG_RESULT([no])
        $3
])
])


AC_DEFUN([MERO_LINUX_FILE_READV],
[AC_MSG_CHECKING([readv in fops])
MERO_LINUX_TRY_COMPILE([
#include <linux/fs.h>
],[
        struct file_operations *fops = NULL;
        fops->readv = NULL;
],[
        AC_MSG_RESULT([yes])
        AC_DEFINE([HAVE_FILE_READV])
],[
        AC_MSG_RESULT([no])
])
])


AC_DEFUN([MERO_LINUX_FILE_AIO_READ],
[AC_MSG_CHECKING([->aio_read in fops])
MERO_LINUX_TRY_COMPILE([
#include <linux/fs.h>
],[
        struct file_operations *fops = NULL;
        fops->aio_read = NULL;
],[
        AC_MSG_RESULT([yes])
        AC_DEFINE([HAVE_FILE_AIO_READ])
],[
	AC_MSG_RESULT([no])
])
])


AC_DEFUN([MERO_LINUX_INODE_BLKSIZE],
[AC_MSG_CHECKING([inode has i_blksize field])
MERO_LINUX_TRY_COMPILE([
#include <linux/fs.h>
],[
	struct inode i;
	i.i_blksize = 0;
],[
	AC_MSG_RESULT([yes])
	AC_DEFINE([HAVE_INODE_BLKSIZE])
],[
	AC_MSG_RESULT([no])
])
])


AC_DEFUN([MERO_LINUX_WRITE_BEGIN_END],
[AC_MSG_CHECKING([if kernel has .write_begin/end])
MERO_LINUX_TRY_COMPILE([
#include <linux/fs.h>
#ifdef HAVE_MMTYPES_H
#include <linux/mm_types.h>
#endif
#include <linux/pagemap.h>
],[
        struct address_space_operations aops;
        struct page *page;

        aops.write_begin = NULL;
        aops.write_end = NULL;
        page = grab_cache_page_write_begin(NULL, 0, 0);
], [
        AC_MSG_RESULT([yes])
        AC_DEFINE([HAVE_WRITE_BEGIN_END])
],[
        AC_MSG_RESULT([no])
])
])


AC_DEFUN([MERO_LINUX_SENDFILE],
[
AC_MSG_CHECKING([if kernel has .sendfile])
MERO_LINUX_TRY_COMPILE([
#include <linux/fs.h>
],[
        struct file_operations file;

        file.sendfile = NULL;
], [
        AC_MSG_RESULT([yes])
        AC_DEFINE([HAVE_SENDFILE])
],[
        AC_MSG_RESULT([no])
])
])


AC_DEFUN([MERO_LINUX_INIT_CONFIG], [
rm -fr config > /dev/null 2>&1
mkdir config
cat >config/Makefile <<_ACEOF
obj-m := conftest.o

modules:
	\$(MAKE) -C \$(LINUX_OBJ) M=\`pwd\` modules
_ACEOF
])

# Check if gcc option is supported in current gcc version.
# MERO_COMPILER_OPTION_CHECK(OPTION, [ACTION-IF-SUPPORTED], [ACTION-IF-NOT])
# --------------------------------------
AC_DEFUN([MERO_COMPILER_OPTION_CHECK],
[AC_MSG_CHECKING([whether compiler $CC supports $1 option])
OLD_CFLAGS="$CFLAGS"
CFLAGS="$OLD_CFLAGS $1"
AC_COMPILE_IFELSE([AC_LANG_SOURCE([MERO_LANG_PROGRAM()])],
        [
         AC_MSG_RESULT([yes])
         $2
        ],[
         AC_MSG_RESULT([no])
         $3
        ]
)]
CFLAGS="$OLD_CFLAGS")

# MERO_BUILD_LIB(NAME, SRCDIR, LIBFILE, [DISTDIR], [BUILDOPTS])
# --------------------------------------
AC_DEFUN([MERO_BUILD_LIB],
[
       cbl_name=$1
       cbl_srcdir=$2
       cbl_libfile=$3
       cbl_distdir=$4
       cbl_buildopts=$5
       AS_IF([test -z $cbl_distdir], [cbl_distdir="."])
       AS_IF([! test -e $cbl_libfile],
             [
               AC_MSG_RESULT([building $cbl_srcdir])
               (cd $cbl_srcdir \
                && if [test -x autogen.sh]; then ./autogen.sh; fi \
                && $cbl_distdir/configure $cbl_buildopts \
                && make)
               AS_IF([test $? -ne 0],
                     AC_MSG_ERROR([An error occured while building lib$cbl_name!]))
               AS_IF([! test -e $cbl_libfile],
                     AC_MSG_ERROR([$cbl_libfile not found! Build failed?]))
             ],[
               AC_MSG_RESULT([$cbl_libfile])
             ]
        )
])

# MERO_SEARCH_LIBS(FUNC, LIBS, OUTPUT_VAR, ERROR_MSG)
# --------------------------------------
AC_DEFUN([MERO_SEARCH_LIBS],
[
        OLD_LIBS=$LIBS
        LIBS=
        AC_SEARCH_LIBS([$1], [$2], [$3=$LIBS], AC_MSG_ERROR([$4]))
        LIBS=$OLD_LIBS
])

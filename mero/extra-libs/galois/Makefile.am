################################################################################
#                            Automake configuration
################################################################################

# control verbosity level of make depending on 'V' command-line argument and
# --enable-silent-rules option of configure script
make_verbose   = $(make_verbose_$(V))
make_verbose_  = $(make_verbose_$(AM_DEFAULT_VERBOSITY))
make_verbose_0 = --no-print-directory

MAKEFLAGS = $(make_verbose)

# in order to suppress annoying automake warning "CFLAGS: non-POSIX variable name"
# -Wno-portability automake option should be set for this makefile, this is
# required for xcode generation
AUTOMAKE_OPTIONS = -Wno-portability

# required to properly rebuild aclocal.m4 macros on configure.ac or Makefile.am
# updates
ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS  = -I$(top_srcdir) -I$(top_srcdir)/include

# In order to allow kernel- and user- space build in the same directories, we
# force renaming of output user-space *.o files by prepending target name;
# automake automatically does this if per-target flags are used.
#


################################################################################
#                               Global variables
################################################################################

# initialize variables, so values can be appended later with += if needed

lib_LTLIBRARIES     =

noinst_PROGRAMS     =

KDIR                = @LINUX_OBJ@
GALOIS_KMOD_DIR     = src/linux_kernel
GALOIS_ABS_KMOD_DIR = $(abs_top_builddir)/$(GALOIS_KMOD_DIR)

EXTRA_DIST          = m4/.gitignore \
                      $(GALOIS_KMOD_DIR)/Kbuild.in

#symversdir         = $(datadir)/galois/symvers
#symvers_DATA       = $(GALOIS_KMOD_DIR)/Module.symvers

galoisdir           = $(includedir)/galois
galois_HEADERS      = include/galois/galois.h


################################################################################
#                                  Libraries
################################################################################

lib_LTLIBRARIES          += src/libgalois.la
src_libgalois_la_SOURCES  = src/galois.c
src_libgalois_la_CPPFLAGS = -DGF_TARGET='libgalois' $(AM_CPPFLAGS)
src_libgalois_la_LDFLAGS  = -release @LT_RELEASE@


################################################################################
#                                  Utilities
################################################################################

if ENABLE_TESTS
noinst_PROGRAMS             += src/gf_basic_tester
src_gf_basic_tester_CPPFLAGS = -DGF_TARGET='gf_basic_tester' $(AM_CPPFLAGS)
src_gf_basic_tester_LDADD    = $(top_builddir)/src/libgalois.la

noinst_PROGRAMS             += src/gf_div 
src_gf_div_CPPFLAGS          = -DGF_TARGET='gf_div' $(AM_CPPFLAGS)
src_gf_div_LDADD             = $(top_builddir)/src/libgalois.la

noinst_PROGRAMS             += src/gf_ilog
src_gf_ilog_CPPFLAGS         = -DGF_TARGET='gf_ilog' $(AM_CPPFLAGS)
src_gf_ilog_LDADD            = $(top_builddir)/src/libgalois.la

noinst_PROGRAMS             += src/gf_inverse
src_gf_inverse_CPPFLAGS      = -DGF_TARGET='gf_inverse' $(AM_CPPFLAGS)
src_gf_inverse_LDADD         = $(top_builddir)/src/libgalois.la

noinst_PROGRAMS             += src/gf_log
src_gf_log_CPPFLAGS          = -DGF_TARGET='gf_log' $(AM_CPPFLAGS)
src_gf_log_LDADD             = $(top_builddir)/src/libgalois.la

noinst_PROGRAMS             += src/gf_mult
src_gf_mult_CPPFLAGS         = -DGF_TARGET='gf_mult' $(AM_CPPFLAGS)
src_gf_mult_LDADD            = $(top_builddir)/src/libgalois.la

noinst_PROGRAMS             += src/gf_xor
src_gf_xor_CPPFLAGS          = -DGF_TARGET='gf_xor' $(AM_CPPFLAGS)
src_gf_xor_LDADD             = $(top_builddir)/src/libgalois.la

noinst_PROGRAMS             += src/gf_xor_tester
src_gf_xor_tester_CPPFLAGS   = -DGF_TARGET='gf_mult' $(AM_CPPFLAGS)
src_gf_xor_tester_LDADD      = $(top_builddir)/src/libgalois.la
endif


################################################################################
#                                 Build rules
################################################################################

all-local: build-kernel-modules

.PHONY: build-kernel-modules
build-kernel-modules:
	@cd $(GALOIS_ABS_KMOD_DIR) && \
		$(LN_S) -f $(abs_top_srcdir)/src/galois.c .
	$(MAKE) -C $(KDIR) M="$(GALOIS_ABS_KMOD_DIR)" $(AM_MAKEFLAGS)

clean-local: clean-kernel-modules

.PHONY: clean-kernel-modules
clean-kernel-modules:
	rm -f $(GALOIS_ABS_KMOD_DIR)/galois.c
	$(MAKE) -C $(KDIR) M="$(GALOIS_ABS_KMOD_DIR)" $(AM_MAKEFLAGS) clean

install-exec-local:
	@$(MAKE) INSTALL_MOD_PATH=$(DESTDIR) INSTALL_MOD_DIR=kernel/fs/mero \
			 -C $(KDIR) M="$(GALOIS_ABS_KMOD_DIR)" modules_install

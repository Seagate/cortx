# control verbosity level of make depending on 'V' command-line argument and
# --enable-silent-rules option of configure script
make_verbose   = $(make_verbose_$(V))
make_verbose_  = $(make_verbose_$(AM_DEFAULT_VERBOSITY))
make_verbose_0 = --no-print-directory

MAKEFLAGS = $(make_verbose)

# Top-level GF-Complete AM file
# Distributes headers

SUBDIRS = src tools test examples
ACLOCAL_AMFLAGS = -I m4

include_HEADERS = 				\
		include/gf/gf_complete.h 	\
		include/gf/gf_method.h 		\
		include/gf/gf_rand.h 		\
		include/gf/gf_general.h 	\
		include/gf/gf_int.h		\
		include/gf/gf_w16.h		\
		include/gf/gf_w32.h		\
		include/gf/gf_w4.h		\
		include/gf/gf_w64.h		\
		include/gf/gf_w8.h

################################################################################
#                                 Build rules
################################################################################

KDIR            = @LINUX_OBJ@
GF_KMOD_DIR     = src/linux_kernel
GF_ABS_KMOD_DIR = $(abs_top_builddir)/$(GF_KMOD_DIR)

EXTRA_DIST      = m4/.gitignore \
                  $(GF_KMOD_DIR)/Kbuild.in

gfdir       = $(includedir)/gf
gf_HEADERS  = include/gf/gf_complete.h

if ENABLE_KERNEL_MODULES
KERNEL_MODULES_BUILD = build-kernel-modules
else
KERNEL_MODULES_BUILD =
endif

all-local: $(KERNEL_MODULES_BUILD)

.PHONY: build-kernel-modules
build-kernel-modules:
	@cd $(GF_ABS_KMOD_DIR) && \
	$(LN_S) -f $(abs_top_srcdir)/src/gf.c . && \
	$(LN_S) -f $(abs_top_srcdir)/src/gf_method.c . && \
	$(LN_S) -f $(abs_top_srcdir)/src/gf_wgen.c . && \
	$(LN_S) -f $(abs_top_srcdir)/src/gf_w4.c . && \
	$(LN_S) -f $(abs_top_srcdir)/src/gf_w8.c . && \
	$(LN_S) -f $(abs_top_srcdir)/src/gf_w16.c . && \
	$(LN_S) -f $(abs_top_srcdir)/src/gf_w32.c . && \
	$(LN_S) -f $(abs_top_srcdir)/src/gf_w64.c . && \
	$(LN_S) -f $(abs_top_srcdir)/src/gf_w128.c . && \
	$(LN_S) -f $(abs_top_srcdir)/src/gf_rand.c . && \
	$(LN_S) -f $(abs_top_srcdir)/src/gf_general.c .
	$(MAKE) -C $(KDIR) M="$(GF_ABS_KMOD_DIR)" $(AM_MAKEFLAGS)

if ENABLE_KERNEL_MODULES
KERNEL_MODULES_CLEAN = clean-kernel-modules
else
KERNEL_MODULES_CLEAN =
endif

clean-local: $(KERNEL_MODULES_CLEAN)

.PHONY: clean-kernel-modules
clean-kernel-modules:
	rm -f $(GF_ABS_KMOD_DIR)/gf.c
	rm -f $(GF_ABS_KMOD_DIR)/gf_method.c
	rm -f $(GF_ABS_KMOD_DIR)/gf_wgen.c
	rm -f $(GF_ABS_KMOD_DIR)/gf_w4.c
	rm -f $(GF_ABS_KMOD_DIR)/gf_w8.c
	rm -f $(GF_ABS_KMOD_DIR)/gf_w16.c
	rm -f $(GF_ABS_KMOD_DIR)/gf_w32.c
	rm -f $(GF_ABS_KMOD_DIR)/gf_w64.c
	rm -f $(GF_ABS_KMOD_DIR)/gf_w128.c
	rm -f $(GF_ABS_KMOD_DIR)/gf_rand.c
	rm -f $(GF_ABS_KMOD_DIR)/gf_general.c
	$(MAKE) -C $(KDIR) M="$(GF_ABS_KMOD_DIR)" $(AM_MAKEFLAGS) clean

if ENABLE_KERNEL_MODULES
install-exec-local:
	@$(MAKE) INSTALL_MOD_PATH=$(DESTDIR) INSTALL_MOD_DIR=kernel/fs/mero \
			 -C $(KDIR) M="$(GF_ABS_KMOD_DIR)" modules_install
endif

# GF-Complete 'core' AM file
# Creates the library

AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -I$(top_builddir)/include -I$(top_srcdir)/include
AM_CFLAGS = -O3 $(SIMD_FLAGS) -fPIC

lib_LTLIBRARIES = libgf_complete.la
libgf_complete_la_SOURCES = gf.c gf_method.c gf_wgen.c gf_w4.c gf_w8.c gf_w16.c gf_w32.c \
          gf_w64.c gf_w128.c gf_rand.c gf_general.c

if HAVE_NEON
libgf_complete_la_SOURCES += neon/gf_w4_neon.c  \
                             neon/gf_w8_neon.c  \
                             neon/gf_w16_neon.c \
                             neon/gf_w32_neon.c \
                             neon/gf_w64_neon.c
endif

libgf_complete_la_LDFLAGS = -version-info 1:0:0


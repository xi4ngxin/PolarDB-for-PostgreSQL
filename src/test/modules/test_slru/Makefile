# src/test/modules/test_slru/Makefile

MODULE_big = test_slru
<<<<<<< HEAD
OBJS = test_slru.o $(WIN32RES)
PGFILEDESC = "test_slru - test code for slru"
=======
OBJS = \
	$(WIN32RES) \
	test_slru.o \
	test_multixact.o
PGFILEDESC = "test_slru - test module for SLRUs"

EXTRA_INSTALL=src/test/modules/injection_points
export enable_injection_points
TAP_TESTS = 1
>>>>>>> c1ff2d8bc5be55e302731a16aaff563b7f03ed7c

EXTENSION = test_slru
DATA = test_slru--1.0.sql

<<<<<<< HEAD
REGRESS = test_slru
=======
REGRESS_OPTS = --temp-config $(top_srcdir)/src/test/modules/test_slru/test_slru.conf
REGRESS = test_slru
# Disabled because these tests require "shared_preload_libraries=test_slru",
# which typical installcheck users do not have (e.g. buildfarm clients).
NO_INSTALLCHECK = 1
>>>>>>> c1ff2d8bc5be55e302731a16aaff563b7f03ed7c

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = src/test/modules/test_slru
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for src/backend/postmaster
#
# IDENTIFICATION
#    src/backend/postmaster/Makefile
#
#-------------------------------------------------------------------------

subdir = src/backend/postmaster
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

OBJS = \
	autovacuum.o \
	auxprocess.o \
	bgworker.o \
	bgwriter.o \
	checkpointer.o \
	fork_process.o \
	interrupt.o \
	launch_backend.o \
	pgarch.o \
	postmaster.o \
	startup.o \
	syslogger.o \
	walsummarizer.o \
	walwriter.o

# POLAR objects
OBJS += polar_parallel_bgwriter.o
OBJS += polar_wal_pipeliner.o
OBJS += polar_dispatcher.o

include $(top_srcdir)/src/backend/common.mk

#
# Makefile for isolation tests
#

PGFILEDESC = "pg_isolation_regress/isolationtester - multi-client test driver"
PGAPPICON = win32

subdir = src/test/isolation
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

override CPPFLAGS := -I. -I$(srcdir) -I$(libpq_srcdir) \
	-I$(srcdir)/../regress $(CPPFLAGS)

<<<<<<< HEAD
DMA_REGRESS_OPTS = 
ifdef DMA_OPTS
DMA_REGRESS_OPTS += --dma=$(DMA_OPTS)
endif

OBJS =  specparse.o isolationtester.o $(WIN32RES)
=======
OBJS = \
	$(WIN32RES) \
	isolationtester.o \
	specparse.o \
	specscanner.o
>>>>>>> c1ff2d8bc5be55e302731a16aaff563b7f03ed7c

all: isolationtester$(X) pg_isolation_regress$(X)

install: all installdirs
	$(INSTALL_PROGRAM) pg_isolation_regress$(X) '$(DESTDIR)$(pgxsdir)/$(subdir)/pg_isolation_regress$(X)'
	$(INSTALL_PROGRAM) isolationtester$(X) '$(DESTDIR)$(pgxsdir)/$(subdir)/isolationtester$(X)'

installdirs:
	$(MKDIR_P) '$(DESTDIR)$(pgxsdir)/$(subdir)'

uninstall:
	rm -f '$(DESTDIR)$(pgxsdir)/$(subdir)/pg_isolation_regress$(X)'
	rm -f '$(DESTDIR)$(pgxsdir)/$(subdir)/isolationtester$(X)'

submake-regress:
	$(MAKE) -C $(top_builddir)/src/test/regress pg_regress.o

pg_regress.o: | submake-regress
	rm -f $@ && $(LN_S) $(top_builddir)/src/test/regress/pg_regress.o .

pg_isolation_regress$(X): isolation_main.o pg_regress.o $(WIN32RES)
	$(CC) $(CFLAGS) $^ $(libpq_pgport) $(LDFLAGS) $(LDFLAGS_EX) $(LIBS) -o $@

isolationtester$(X): $(OBJS) | submake-libpq submake-libpgport
	$(CC) $(CFLAGS) $^ $(libpq_pgport) $(LDFLAGS) $(LDFLAGS_EX) $(LIBS) -o $@

# See notes in src/backend/parser/Makefile about the following two rules
specparse.h: specparse.c
	touch $@

specparse.c: BISONFLAGS += -d

# Force these dependencies to be known even without dependency info built:
specparse.o specscanner.o: specparse.h

clean distclean:
	rm -f isolationtester$(X) pg_isolation_regress$(X) $(OBJS) isolation_main.o
	rm -f pg_regress.o
	rm -rf $(pg_regress_clean_files)
	rm -f specparse.h specparse.c specscanner.c

installcheck: all
	$(pg_isolation_regress_installcheck) --schedule=$(srcdir)/isolation_schedule

check: all
	$(pg_isolation_regress_check) $(DMA_REGRESS_OPTS) --schedule=$(srcdir)/isolation_schedule

# Non-default tests.  It only makes sense to run these if set up to use
# prepared transactions, via TEMP_CONFIG for the check case, or via the
# postgresql.conf for the installcheck case.
installcheck-prepared-txns: all temp-install
	$(pg_isolation_regress_installcheck) --schedule=$(srcdir)/isolation_schedule prepared-transactions prepared-transactions-cic

check-prepared-txns: all temp-install
<<<<<<< HEAD
	$(pg_isolation_regress_check) $(DMA_REGRESS_OPTS) --schedule=$(srcdir)/isolation_schedule prepared-transactions
=======
	$(pg_isolation_regress_check) --schedule=$(srcdir)/isolation_schedule prepared-transactions prepared-transactions-cic
>>>>>>> c1ff2d8bc5be55e302731a16aaff563b7f03ed7c

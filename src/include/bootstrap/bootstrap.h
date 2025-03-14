/*-------------------------------------------------------------------------
 *
 * bootstrap.h
 *	  include file for the bootstrapping code
 *
 *
 * Portions Copyright (c) 1996-2024, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/bootstrap/bootstrap.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef BOOTSTRAP_H
#define BOOTSTRAP_H

#include "nodes/execnodes.h"
#include "nodes/parsenodes.h"


/*
 * MAXATTR is the maximum number of attributes in a relation supported
 * at bootstrap time (i.e., the max possible in a system table).
 */
#define MAXATTR 40

#define BOOTCOL_NULL_AUTO			1
#define BOOTCOL_NULL_FORCE_NULL		2
#define BOOTCOL_NULL_FORCE_NOT_NULL 3

<<<<<<< HEAD
extern Relation boot_reldesc;
extern Form_pg_attribute attrtypes[MAXATTR];
extern int	numattr;
/* POLAR */
extern uint64 polar_sysidentifier;
=======
extern PGDLLIMPORT Relation boot_reldesc;
extern PGDLLIMPORT Form_pg_attribute attrtypes[MAXATTR];
extern PGDLLIMPORT int numattr;

>>>>>>> c1ff2d8bc5be55e302731a16aaff563b7f03ed7c

extern void BootstrapModeMain(int argc, char *argv[], bool check_only) pg_attribute_noreturn();

extern void closerel(char *relname);
extern void boot_openrel(char *relname);

extern void DefineAttr(char *name, char *type, int attnum, int nullness);
extern void InsertOneTuple(void);
extern void InsertOneValue(char *value, int i);
extern void InsertOneNull(int i);

extern void index_register(Oid heap, Oid ind, const IndexInfo *indexInfo);
extern void build_indices(void);

extern void boot_get_type_io_data(Oid typid,
								  int16 *typlen,
								  bool *typbyval,
								  char *typalign,
								  char *typdelim,
								  Oid *typioparam,
								  Oid *typinput,
								  Oid *typoutput);

extern int	boot_yyparse(void);

extern int	boot_yylex(void);
extern void boot_yyerror(const char *message) pg_attribute_noreturn();

#endif							/* BOOTSTRAP_H */

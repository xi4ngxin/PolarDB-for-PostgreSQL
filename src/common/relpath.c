/*-------------------------------------------------------------------------
 * relpath.c
 *		Shared frontend/backend code to compute pathnames of relation files
 *
 * This module also contains some logic associated with fork names.
 *
 * Portions Copyright (c) 1996-2024, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/common/relpath.c
 *
 *-------------------------------------------------------------------------
 */
#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include "catalog/pg_tablespace_d.h"
#include "common/relpath.h"
#include "storage/procnumber.h"

/* POLAR */
#include "polar_dma/polar_dma.h"
#include "storage/polar_fd.h"

/* POLAR */
extern bool		polar_enable_shared_storage_mode;
extern char		*polar_datadir;
extern bool		polar_temp_relation_file_in_shared_storage;

/*
 * Lookup table of fork name by fork number.
 *
 * If you add a new entry, remember to update the errhint in
 * forkname_to_number() below, and update the SGML documentation for
 * pg_relation_size().
 */
const char *const forkNames[] = {
	[MAIN_FORKNUM] = "main",
	[FSM_FORKNUM] = "fsm",
	[VISIBILITYMAP_FORKNUM] = "vm",
	[INIT_FORKNUM] = "init",
};

StaticAssertDecl(lengthof(forkNames) == (MAX_FORKNUM + 1),
				 "array length mismatch");

/*
 * forkname_to_number - look up fork number by name
 *
 * In backend, we throw an error for no match; in frontend, we just
 * return InvalidForkNumber.
 */
ForkNumber
forkname_to_number(const char *forkName)
{
	ForkNumber	forkNum;

	for (forkNum = 0; forkNum <= MAX_FORKNUM; forkNum++)
		if (strcmp(forkNames[forkNum], forkName) == 0)
			return forkNum;

#ifndef FRONTEND
	ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("invalid fork name"),
			 errhint("Valid fork names are \"main\", \"fsm\", "
					 "\"vm\", and \"init\".")));
#endif

	return InvalidForkNumber;
}

/*
 * forkname_chars
 *		We use this to figure out whether a filename could be a relation
 *		fork (as opposed to an oddly named stray file that somehow ended
 *		up in the database directory).  If the passed string begins with
 *		a fork name (other than the main fork name), we return its length,
 *		and set *fork (if not NULL) to the fork number.  If not, we return 0.
 *
 * Note that the present coding assumes that there are no fork names which
 * are prefixes of other fork names.
 */
int
forkname_chars(const char *str, ForkNumber *fork)
{
	ForkNumber	forkNum;

	for (forkNum = 1; forkNum <= MAX_FORKNUM; forkNum++)
	{
		int			len = strlen(forkNames[forkNum]);

		if (strncmp(forkNames[forkNum], str, len) == 0)
		{
			if (fork)
				*fork = forkNum;
			return len;
		}
	}
	if (fork)
		*fork = InvalidForkNumber;
	return 0;
}


/*
 * GetDatabasePath - construct path to a database directory
 *
 * Result is a palloc'd string.
 *
 * XXX this must agree with GetRelationPath()!
 */
char *
<<<<<<< HEAD
GetDatabasePath(Oid dbNode, Oid spcNode, bool polar_vfs)
=======
GetDatabasePath(Oid dbOid, Oid spcOid)
>>>>>>> c1ff2d8bc5be55e302731a16aaff563b7f03ed7c
{
	if (spcOid == GLOBALTABLESPACE_OID)
	{
		/* Shared system relations live in {datadir}/global */
		Assert(dbOid == 0);
		return pstrdup("global");
	}
	else if (spcOid == DEFAULTTABLESPACE_OID)
	{
		/* The default tablespace is {datadir}/base */
		return psprintf("base/%u", dbOid);
	}
	else
	{
		/* All other tablespaces are accessed via symlinks */
		return psprintf("%s/%u/%s/%u",
						PG_TBLSPC_DIR, spcOid,
						TABLESPACE_VERSION_DIRECTORY, dbOid);
	}
}

/*
 * GetRelationPath - construct path to a relation's file
 *
 * Result is a palloc'd string.
 *
 * Note: ideally, procNumber would be declared as type ProcNumber, but
 * relpath.h would have to include a backend-only header to do that; doesn't
 * seem worth the trouble considering ProcNumber is just int anyway.
 */
char *
GetRelationPath(Oid dbOid, Oid spcOid, RelFileNumber relNumber,
				int procNumber, ForkNumber forkNumber)
{
	char	   *path;

	if (spcOid == GLOBALTABLESPACE_OID)
	{
		/* Shared system relations live in {datadir}/global */
		Assert(dbOid == 0);
		Assert(procNumber == INVALID_PROC_NUMBER);
		if (forkNumber != MAIN_FORKNUM)
			path = psprintf("global/%u_%s",
							relNumber, forkNames[forkNumber]);
		else
			path = psprintf("global/%u", relNumber);
	}
	else if (spcOid == DEFAULTTABLESPACE_OID)
	{
		/* The default tablespace is {datadir}/base */
		if (procNumber == INVALID_PROC_NUMBER)
		{
			if (forkNumber != MAIN_FORKNUM)
				path = psprintf("base/%u/%u_%s",
								dbOid, relNumber,
								forkNames[forkNumber]);
			else
				path = psprintf("base/%u/%u",
								dbOid, relNumber);
		}
		else
		{
			if (forkNumber != MAIN_FORKNUM)
				path = psprintf("base/%u/t%d_%u_%s",
								dbOid, procNumber, relNumber,
								forkNames[forkNumber]);
			else
				path = psprintf("base/%u/t%d_%u",
								dbOid, procNumber, relNumber);
		}
	}
	else
	{
		/* All other tablespaces are accessed via symlinks */
		if (procNumber == INVALID_PROC_NUMBER)
		{
			if (forkNumber != MAIN_FORKNUM)
				path = psprintf("%s/%u/%s/%u/%u_%s",
								PG_TBLSPC_DIR, spcOid,
								TABLESPACE_VERSION_DIRECTORY,
								dbOid, relNumber,
								forkNames[forkNumber]);
			else
				path = psprintf("%s/%u/%s/%u/%u",
								PG_TBLSPC_DIR, spcOid,
								TABLESPACE_VERSION_DIRECTORY,
								dbOid, relNumber);
		}
		else
		{
			if (forkNumber != MAIN_FORKNUM)
				path = psprintf("%s/%u/%s/%u/t%d_%u_%s",
								PG_TBLSPC_DIR, spcOid,
								TABLESPACE_VERSION_DIRECTORY,
								dbOid, procNumber, relNumber,
								forkNames[forkNumber]);
			else
				path = psprintf("%s/%u/%s/%u/t%d_%u",
								PG_TBLSPC_DIR, spcOid,
								TABLESPACE_VERSION_DIRECTORY,
								dbOid, procNumber, relNumber);
		}
	}

#ifndef FRONTEND
	/*
	 * POLAR: In DMA mode, user tablespace data must be in the local directory.
	 * Also, polar_enable_shared_storage_mode is true in DMA mode, 
	 * So we added judgment here.
	 */
	if (POLAR_ENABLE_DMA() &&
		spcNode != GLOBALTABLESPACE_OID &&
		spcNode != DEFAULTTABLESPACE_OID)
		return path;
	/*
	 * POLAR: normal relation file will be stored in shared storage.
	 * Temp relation file will be stored in local storage if the
	 * polar_temp_relation_file_in_shared_storage is off.
	 */
	if (POLAR_TEMP_TABLE_FILE_IN_SHARED_STORAGE(backendId) ||
		POLAR_NORMAL_TABLE_FILE_IN_SHARED_STORAGE(backendId))
	{
		char *polar_path = psprintf("%s/%s", polar_datadir, path);
		pfree(path);
		return polar_path;
	}
#endif

	return path;
}

/*
 * POLAR: construct path to a database directory
 * include shared storage dir
 */
char *
polar_get_database_path(Oid dbNode, Oid spcNode)
{
	char *path = NULL;

	path = GetDatabasePath(dbNode, spcNode, false);

#ifndef FRONTEND
	if(POLAR_FILE_IN_SHARED_STORAGE())
	{
		char *polar_path = psprintf("%s/%s", polar_datadir, path);
		pfree(path);
		return polar_path;
	}
#endif

	return path;
}


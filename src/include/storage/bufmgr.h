/*-------------------------------------------------------------------------
 *
 * bufmgr.h
 *	  POSTGRES buffer manager definitions.
 *
 *
 * Portions Copyright (c) 1996-2024, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/storage/bufmgr.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef BUFMGR_H
#define BUFMGR_H

#include "port/pg_iovec.h"
#include "storage/block.h"
#include "storage/buf.h"
#include "storage/bufpage.h"
#include "storage/relfilelocator.h"
#include "utils/relcache.h"
#include "utils/snapmgr.h"

/* POLAR: for code compile */
typedef struct BufferDesc BufferDesc;

typedef void *Block;

/*
 * Possible arguments for GetAccessStrategy().
 *
 * If adding a new BufferAccessStrategyType, also add a new IOContext so
 * IO statistics using this strategy are tracked.
 */
typedef enum BufferAccessStrategyType
{
	BAS_NORMAL,					/* Normal random access */
	BAS_BULKREAD,				/* Large read-only scan (hint bit updates are
								 * ok) */
	BAS_BULKWRITE,				/* Large multi-block write (e.g. COPY IN) */
	BAS_VACUUM,					/* VACUUM */
} BufferAccessStrategyType;

/* Possible modes for ReadBufferExtended() */
typedef enum
{
	RBM_NORMAL,					/* Normal read */
	RBM_ZERO_AND_LOCK,			/* Don't read from disk, caller will
								 * initialize. Also locks the page. */
	RBM_ZERO_AND_CLEANUP_LOCK,	/* Like RBM_ZERO_AND_LOCK, but locks the page
								 * in "cleanup" mode */
	RBM_ZERO_ON_ERROR,			/* Read, but return an all-zeros page on error */
	RBM_NORMAL_NO_LOG,			/* Don't log page as invalid during WAL
								 * replay; otherwise same as RBM_NORMAL */
	RBM_NORMAL_VALID			/* Polar: Don't read from disk, buffer is valid */
} ReadBufferMode;

/*
 * Type returned by PrefetchBuffer().
 */
typedef struct PrefetchBufferResult
{
	Buffer		recent_buffer;	/* If valid, a hit (recheck needed!) */
	bool		initiated_io;	/* If true, a miss resulting in async I/O */
} PrefetchBufferResult;

/*
 * Flags influencing the behaviour of ExtendBufferedRel*
 */
typedef enum ExtendBufferedFlags
{
	/*
	 * Don't acquire extension lock. This is safe only if the relation isn't
	 * shared, an access exclusive lock is held or if this is the startup
	 * process.
	 */
	EB_SKIP_EXTENSION_LOCK = (1 << 0),

	/* Is this extension part of recovery? */
	EB_PERFORMING_RECOVERY = (1 << 1),

	/*
	 * Should the fork be created if it does not currently exist? This likely
	 * only ever makes sense for relation forks.
	 */
	EB_CREATE_FORK_IF_NEEDED = (1 << 2),

	/* Should the first (possibly only) return buffer be returned locked? */
	EB_LOCK_FIRST = (1 << 3),

	/* Should the smgr size cache be cleared? */
	EB_CLEAR_SIZE_CACHE = (1 << 4),

	/* internal flags follow */
	EB_LOCK_TARGET = (1 << 5),
}			ExtendBufferedFlags;

/*
 * Some functions identify relations either by relation or smgr +
 * relpersistence.  Used via the BMR_REL()/BMR_SMGR() macros below.  This
 * allows us to use the same function for both recovery and normal operation.
 */
typedef struct BufferManagerRelation
{
	Relation	rel;
	struct SMgrRelationData *smgr;
	char		relpersistence;
} BufferManagerRelation;

#define BMR_REL(p_rel) ((BufferManagerRelation){.rel = p_rel})
#define BMR_SMGR(p_smgr, p_relpersistence) ((BufferManagerRelation){.smgr = p_smgr, .relpersistence = p_relpersistence})

/* Zero out page if reading fails. */
#define READ_BUFFERS_ZERO_ON_ERROR (1 << 0)
/* Call smgrprefetch() if I/O necessary. */
#define READ_BUFFERS_ISSUE_ADVICE (1 << 1)

struct ReadBuffersOperation
{
	/* The following members should be set by the caller. */
	Relation	rel;			/* optional */
	struct SMgrRelationData *smgr;
	char		persistence;
	ForkNumber	forknum;
	BufferAccessStrategy strategy;

	/*
	 * The following private members are private state for communication
	 * between StartReadBuffers() and WaitReadBuffers(), initialized only if
	 * an actual read is required, and should not be modified.
	 */
	Buffer	   *buffers;
	BlockNumber blocknum;
	int			flags;
	int16		nblocks;
	int16		io_buffers_len;
};

typedef struct ReadBuffersOperation ReadBuffersOperation;

/* forward declared, to avoid having to expose buf_internals.h here */
struct WritebackContext;

/* forward declared, to avoid including smgr.h here */
struct SMgrRelationData;

/* in globals.c ... this duplicates miscadmin.h */
extern PGDLLIMPORT int NBuffers;

/* in bufmgr.c */
extern PGDLLIMPORT bool zero_damaged_pages;
extern PGDLLIMPORT int bgwriter_lru_maxpages;
extern PGDLLIMPORT double bgwriter_lru_multiplier;
extern PGDLLIMPORT bool track_io_timing;

/* only applicable when prefetching is available */
#ifdef USE_PREFETCH
#define DEFAULT_EFFECTIVE_IO_CONCURRENCY 1
#define DEFAULT_MAINTENANCE_IO_CONCURRENCY 10
#else
#define DEFAULT_EFFECTIVE_IO_CONCURRENCY 0
#define DEFAULT_MAINTENANCE_IO_CONCURRENCY 0
#endif
extern PGDLLIMPORT int effective_io_concurrency;
extern PGDLLIMPORT int maintenance_io_concurrency;

#define MAX_IO_COMBINE_LIMIT PG_IOV_MAX
#define DEFAULT_IO_COMBINE_LIMIT Min(MAX_IO_COMBINE_LIMIT, (128 * 1024) / BLCKSZ)
extern PGDLLIMPORT int io_combine_limit;

extern PGDLLIMPORT int checkpoint_flush_after;
extern PGDLLIMPORT int backend_flush_after;
extern PGDLLIMPORT int bgwriter_flush_after;

/* in buf_init.c */
extern PGDLLIMPORT char *BufferBlocks;

<<<<<<< HEAD
/* in guc.c */
extern int	effective_io_concurrency;
/* POLAR */
extern bool	polar_enable_shared_storage_mode;

/* POLAR: bulk read */
extern bool polar_bulk_io_is_in_progress;
extern int polar_bulk_io_in_progress_count;
/* POLAR end */

=======
>>>>>>> c1ff2d8bc5be55e302731a16aaff563b7f03ed7c
/* in localbuf.c */
extern PGDLLIMPORT int NLocBuffer;
extern PGDLLIMPORT Block *LocalBufferBlockPointers;
extern PGDLLIMPORT int32 *LocalRefCount;

/* upper limit for effective_io_concurrency */
#define MAX_IO_CONCURRENCY 1000

/* special block number for ReadBuffer() */
#define P_NEW	InvalidBlockNumber	/* grow the file to get a new page */

/*
 * Buffer content lock modes (mode argument for LockBuffer())
 */
#define BUFFER_LOCK_UNLOCK		0
#define BUFFER_LOCK_SHARE		1
#define BUFFER_LOCK_EXCLUSIVE	2


/*
<<<<<<< HEAD
 * Note: these two macros only work on shared buffers, not local ones!
 *
 * POLAR: these two macros are moved from bufmgr.c.
 */
#define BufHdrGetBlock(bufHdr)	((Block) (BufferBlocks + ((Size) (bufHdr)->buf_id) * BLCKSZ))
#define BufferGetLSN(bufHdr)	(PageGetLSN(BufHdrGetBlock(bufHdr)))

/*
 * These routines are beaten on quite heavily, hence the macroization.
=======
 * prototypes for functions in bufmgr.c
>>>>>>> c1ff2d8bc5be55e302731a16aaff563b7f03ed7c
 */
extern PrefetchBufferResult PrefetchSharedBuffer(struct SMgrRelationData *smgr_reln,
												 ForkNumber forkNum,
												 BlockNumber blockNum);
extern PrefetchBufferResult PrefetchBuffer(Relation reln, ForkNumber forkNum,
										   BlockNumber blockNum);
extern bool ReadRecentBuffer(RelFileLocator rlocator, ForkNumber forkNum,
							 BlockNumber blockNum, Buffer recent_buffer);
extern Buffer ReadBuffer(Relation reln, BlockNumber blockNum);
extern Buffer ReadBufferExtended(Relation reln, ForkNumber forkNum,
								 BlockNumber blockNum, ReadBufferMode mode,
								 BufferAccessStrategy strategy);
extern Buffer ReadBufferWithoutRelcache(RelFileLocator rlocator,
										ForkNumber forkNum, BlockNumber blockNum,
										ReadBufferMode mode, BufferAccessStrategy strategy,
										bool permanent);

extern bool StartReadBuffer(ReadBuffersOperation *operation,
							Buffer *buffer,
							BlockNumber blocknum,
							int flags);
extern bool StartReadBuffers(ReadBuffersOperation *operation,
							 Buffer *buffers,
							 BlockNumber blockNum,
							 int *nblocks,
							 int flags);
extern void WaitReadBuffers(ReadBuffersOperation *operation);

extern void ReleaseBuffer(Buffer buffer);
extern void UnlockReleaseBuffer(Buffer buffer);
extern bool BufferIsExclusiveLocked(Buffer buffer);
extern bool BufferIsDirty(Buffer buffer);
extern void MarkBufferDirty(Buffer buffer);
extern void IncrBufferRefCount(Buffer buffer);
extern void CheckBufferIsPinnedOnce(Buffer buffer);
extern Buffer ReleaseAndReadBuffer(Buffer buffer, Relation relation,
								   BlockNumber blockNum);

extern Buffer ExtendBufferedRel(BufferManagerRelation bmr,
								ForkNumber forkNum,
								BufferAccessStrategy strategy,
								uint32 flags);
extern BlockNumber ExtendBufferedRelBy(BufferManagerRelation bmr,
									   ForkNumber fork,
									   BufferAccessStrategy strategy,
									   uint32 flags,
									   uint32 extend_by,
									   Buffer *buffers,
									   uint32 *extended_by);
extern Buffer ExtendBufferedRelTo(BufferManagerRelation bmr,
								  ForkNumber fork,
								  BufferAccessStrategy strategy,
								  uint32 flags,
								  BlockNumber extend_to,
								  ReadBufferMode mode);

extern void InitBufferManagerAccess(void);
extern void AtEOXact_Buffers(bool isCommit);
extern char *DebugPrintBufferRefcount(Buffer buffer);
extern void CheckPointBuffers(int flags);
extern BlockNumber BufferGetBlockNumber(Buffer buffer);
extern BlockNumber RelationGetNumberOfBlocksInFork(Relation relation,
												   ForkNumber forkNum);
extern void FlushOneBuffer(Buffer buffer);
extern void FlushRelationBuffers(Relation rel);
extern void FlushRelationsAllBuffers(struct SMgrRelationData **smgrs, int nrels);
extern void CreateAndCopyRelationData(RelFileLocator src_rlocator,
									  RelFileLocator dst_rlocator,
									  bool permanent);
extern void FlushDatabaseBuffers(Oid dbid);
extern void DropRelationBuffers(struct SMgrRelationData *smgr_reln,
								ForkNumber *forkNum,
								int nforks, BlockNumber *firstDelBlock);
extern void DropRelationsAllBuffers(struct SMgrRelationData **smgr_reln,
									int nlocators);
extern void DropDatabaseBuffers(Oid dbid);

#define RelationGetNumberOfBlocks(reln) \
	RelationGetNumberOfBlocksInFork(reln, MAIN_FORKNUM)

extern bool BufferIsPermanent(Buffer buffer);
extern XLogRecPtr BufferGetLSNAtomic(Buffer buffer);

#ifdef NOT_USED
extern void PrintPinnedBufs(void);
#endif
extern void BufferGetTag(Buffer buffer, RelFileLocator *rlocator,
						 ForkNumber *forknum, BlockNumber *blknum);

extern void MarkBufferDirtyHint(Buffer buffer, bool buffer_std);

extern void UnlockBuffers(void);
extern void LockBuffer(Buffer buffer, int mode);
extern bool ConditionalLockBuffer(Buffer buffer);
extern void LockBufferForCleanup(Buffer buffer);
extern bool ConditionalLockBufferForCleanup(Buffer buffer);
extern bool IsBufferCleanupOK(Buffer buffer);
extern bool HoldingBufferPinThatDelaysRecovery(void);

extern bool BgBufferSync(struct WritebackContext *wb_context);

extern void LimitAdditionalPins(uint32 *additional_pins);
extern void LimitAdditionalLocalPins(uint32 *additional_pins);

extern bool EvictUnpinnedBuffer(Buffer buf);

/* in buf_init.c */
extern void BufferManagerShmemInit(void);
extern Size BufferManagerShmemSize(void);

/* in localbuf.c */
extern void AtProcExit_LocalBuffers(void);

/* in freelist.c */

extern BufferAccessStrategy GetAccessStrategy(BufferAccessStrategyType btype);
extern BufferAccessStrategy GetAccessStrategyWithSize(BufferAccessStrategyType btype,
													  int ring_size_kb);
extern int	GetAccessStrategyBufferCount(BufferAccessStrategy strategy);
extern int	GetAccessStrategyPinLimit(BufferAccessStrategy strategy);

extern void FreeAccessStrategy(BufferAccessStrategy strategy);


/* inline functions */

/*
 * Although this header file is nominally backend-only, certain frontend
 * programs like pg_waldump include it.  For compilers that emit static
 * inline functions even when they're unused, that leads to unsatisfied
 * external references; hence hide these with #ifndef FRONTEND.
 */

#ifndef FRONTEND

/*
 * BufferIsValid
 *		True iff the given buffer number is valid (either as a shared
 *		or local buffer).
 *
 * Note: For a long time this was defined the same as BufferIsPinned,
 * that is it would say False if you didn't hold a pin on the buffer.
 * I believe this was bogus and served only to mask logic errors.
 * Code should always know whether it has a buffer reference,
 * independently of the pin state.
 *
 * Note: For a further long time this was not quite the inverse of the
 * BufferIsInvalid() macro, in that it also did sanity checks to verify
 * that the buffer number was in range.  Most likely, this macro was
 * originally intended only to be used in assertions, but its use has
 * since expanded quite a bit, and the overhead of making those checks
 * even in non-assert-enabled builds can be significant.  Thus, we've
 * now demoted the range checks to assertions within the macro itself.
 */
static inline bool
BufferIsValid(Buffer bufnum)
{
	Assert(bufnum <= NBuffers);
	Assert(bufnum >= -NLocBuffer);

	return bufnum != InvalidBuffer;
}

/*
 * BufferGetBlock
 *		Returns a reference to a disk page image associated with a buffer.
 *
 * Note:
 *		Assumes buffer is valid.
 */
static inline Block
BufferGetBlock(Buffer buffer)
{
	Assert(BufferIsValid(buffer));

	if (BufferIsLocal(buffer))
		return LocalBufferBlockPointers[-buffer - 1];
	else
		return (Block) (BufferBlocks + ((Size) (buffer - 1)) * BLCKSZ);
}

/*
 * BufferGetPageSize
 *		Returns the page size within a buffer.
 *
 * Notes:
 *		Assumes buffer is valid.
 *
 *		The buffer can be a raw disk block and need not contain a valid
 *		(formatted) disk page.
 */
/* XXX should dig out of buffer descriptor */
static inline Size
BufferGetPageSize(Buffer buffer)
{
	AssertMacro(BufferIsValid(buffer));
	return (Size) BLCKSZ;
}

/*
 * BufferGetPage
 *		Returns the page associated with a buffer.
 */
<<<<<<< HEAD
#define BufferGetPage(buffer) ((Page)BufferGetBlock(buffer))

/*
 * prototypes for functions in bufmgr.c
 */
extern bool ComputeIoConcurrency(int io_concurrency, double *target);
extern void PrefetchBuffer(Relation reln, ForkNumber forkNum,
			   BlockNumber blockNum);
extern Buffer ReadBuffer(Relation reln, BlockNumber blockNum);
extern Buffer ReadBufferExtended(Relation reln, ForkNumber forkNum,
				   BlockNumber blockNum, ReadBufferMode mode,
				   BufferAccessStrategy strategy);
extern Buffer ReadBufferWithoutRelcache(RelFileNode rnode,
						  ForkNumber forkNum, BlockNumber blockNum,
						  ReadBufferMode mode, BufferAccessStrategy strategy);
extern void ReleaseBuffer(Buffer buffer);
extern void UnlockReleaseBuffer(Buffer buffer);
extern void MarkBufferDirty(Buffer buffer);
extern void IncrBufferRefCount(Buffer buffer);
extern Buffer ReleaseAndReadBuffer(Buffer buffer, Relation relation,
					 BlockNumber blockNum);

extern void InitBufferPool(void);
extern void InitBufferPoolAccess(void);
extern void InitBufferPoolBackend(void);
extern void AtEOXact_Buffers(bool isCommit);
extern void PrintBufferLeakWarning(Buffer buffer);
extern void CheckPointBuffers(int flags);
extern BlockNumber BufferGetBlockNumber(Buffer buffer);
extern BlockNumber RelationGetNumberOfBlocksInFork(Relation relation,
								ForkNumber forkNum);
extern void FlushOneBuffer(Buffer buffer);
extern void FlushRelationBuffers(Relation rel);
extern void FlushDatabaseBuffers(Oid dbid);
extern void DropRelFileNodeBuffers(RelFileNodeBackend rnode,
					   ForkNumber forkNum, BlockNumber firstDelBlock);
extern void DropRelFileNodesAllBuffers(RelFileNodeBackend *rnodes, int nnodes);
extern void DropDatabaseBuffers(Oid dbid);

/*
 * POLAR: the same function of RelationGetNumberOfBlocksInFork.
 * But use polar_smgrnblocks_cache instead of smgrnblocks.
 */
extern BlockNumber polar_relation_get_number_of_cache_blocks_in_fork(Relation relation,
								ForkNumber forkNum);

#define RelationGetNumberOfBlocks(reln) \
	RelationGetNumberOfBlocksInFork(reln, MAIN_FORKNUM)

/*
 * POLAR: the same function of RelationGetNumberOfBlocks.
 */
#define polar_relation_get_cached_number_of_blocks(reln) \
	polar_relation_get_number_of_cache_blocks_in_fork(reln, MAIN_FORKNUM)

extern bool BufferIsPermanent(Buffer buffer);
extern XLogRecPtr BufferGetLSNAtomic(Buffer buffer);

#ifdef NOT_USED
extern void PrintPinnedBufs(void);
#endif
extern Size BufferShmemSize(void);
extern void BufferGetTag(Buffer buffer, RelFileNode *rnode,
			 ForkNumber *forknum, BlockNumber *blknum);

extern void MarkBufferDirtyHint(Buffer buffer, bool buffer_std);

extern void UnlockBuffers(void);
extern void LockBuffer(Buffer buffer, int mode);
extern bool ConditionalLockBuffer(Buffer buffer);
extern void LockBufferForCleanup(Buffer buffer);
extern bool ConditionalLockBufferForCleanup(Buffer buffer);
extern bool IsBufferCleanupOK(Buffer buffer);
extern bool HoldingBufferPinThatDelaysRecovery(void);

extern void AbortBufferIO(void);

extern void BufmgrCommit(void);
extern bool BgBufferSync(struct WritebackContext *wb_context, int flags);

extern void AtProcExit_LocalBuffers(void);

extern void TestForOldSnapshot_impl(Snapshot snapshot, Relation relation);

/* in freelist.c */
extern BufferAccessStrategy GetAccessStrategy(BufferAccessStrategyType btype);
extern void FreeAccessStrategy(BufferAccessStrategy strategy);

/* POLAR */
extern void polar_try_to_wake_bgwriter(void);
extern bool polar_start_buffer_io_extend(BufferDesc *buf,
										 bool forInput,
										 bool polar_copy_buf);

/* POLAR: change static to extern */
extern void TerminateBufferIO(BufferDesc *buf, bool clear_dirty, uint32 set_flag_bits);
/* POLAR: change static to extern */
extern bool StartBufferIO(BufferDesc *buf, bool forInput);
/* POLAR: bulk read */
extern int polar_get_buffer_access_strategy_ring_size(BufferAccessStrategy strategy);
extern BufferDesc **polar_bulk_io_in_progress_buf;
/* POLAR end */

/* POLAR */
extern void polar_lock_buffer_ext(Buffer buffer, int mode, bool fresh_check);
extern bool polar_conditional_lock_buffer_ext(Buffer buffer, bool fresh_check);
extern void polar_lock_buffer_for_cleanup_ext(Buffer buffer, bool fresh_check);
extern void polar_strategy_set_first_free_buffer(int buf_id);
extern void polar_unpin_buffer_proc_exit(Buffer buf);
/* POLAR end */

/* inline functions */

/*
 * Although this header file is nominally backend-only, certain frontend
 * programs like pg_waldump include it.  For compilers that emit static
 * inline functions even when they're unused, that leads to unsatisfied
 * external references; hence hide these with #ifndef FRONTEND.
 */

#ifndef FRONTEND

/*
 * Check whether the given snapshot is too old to have safely read the given
 * page from the given table.  If so, throw a "snapshot too old" error.
 *
 * This test generally needs to be performed after every BufferGetPage() call
 * that is executed as part of a scan.  It is not needed for calls made for
 * modifying the page (for example, to position to the right place to insert a
 * new index tuple or for vacuuming).  It may also be omitted where calls to
 * lower-level functions will have already performed the test.
 *
 * Note that a NULL snapshot argument is allowed and causes a fast return
 * without error; this is to support call sites which can be called from
 * either scans or index modification areas.
 *
 * For best performance, keep the tests that are fastest and/or most likely to
 * exclude a page from old snapshot testing near the front.
 */
static inline void
TestForOldSnapshot(Snapshot snapshot, Relation relation, Page page)
=======
static inline Page
BufferGetPage(Buffer buffer)
>>>>>>> c1ff2d8bc5be55e302731a16aaff563b7f03ed7c
{
	return (Page) BufferGetBlock(buffer);
}

#endif							/* FRONTEND */

#endif							/* BUFMGR_H */

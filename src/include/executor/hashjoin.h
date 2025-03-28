/*-------------------------------------------------------------------------
 *
 * hashjoin.h
 *	  internal structures for hash joins
 *
 *
 * Portions Copyright (c) 1996-2024, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/executor/hashjoin.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef HASHJOIN_H
#define HASHJOIN_H

#include "nodes/execnodes.h"
#include "port/atomics.h"
#include "storage/barrier.h"
#include "storage/buffile.h"
#include "storage/lwlock.h"
/* POLAR px */
#include "px/px_explain.h"			/* PxExplain_Agg */
/* POLAR end */
/* ----------------------------------------------------------------
 *				hash-join hash table structures
 *
 * Each active hashjoin has a HashJoinTable structure, which is
 * palloc'd in the executor's per-query context.  Other storage needed for
 * each hashjoin is kept in child contexts, three for each hashjoin:
 *   - HashTableContext (hashCxt): the parent hash table storage context
 *   - HashSpillContext (spillCxt): storage for temp files buffers
 *   - HashBatchContext (batchCxt): storage for a batch in serial hash join
 *
 * The hashtable contexts are made children of the per-query context, ensuring
 * that they will be discarded at end of statement even if the join is
 * aborted early by an error.  (Likewise, any temporary files we make will
 * be cleaned up by the virtual file manager in event of an error.)
 *
 * Storage that should live through the entire join is allocated from the
 * "hashCxt" (mainly the hashtable's metadata). Also, the "hashCxt" context is
 * the parent of "spillCxt" and "batchCxt". It makes it easy and fast to
 * release the storage when we don't need it anymore.
 *
 * Data associated with temp files is allocated in the "spillCxt" context
 * which lives for the duration of the entire join as batch files'
 * creation and usage may span batch execution. These files are
 * explicitly destroyed by calling BufFileClose() when the code is done
 * with them. The aim of this context is to help accounting for the
 * memory allocated for temp files and their buffers.
 *
 * Finally, data used only during a single batch's execution is allocated
 * in the "batchCxt". By resetting the batchCxt at the end of each batch,
 * we free all the per-batch storage reliably and without tedium.
 *
 * During first scan of inner relation, we get its tuples from executor.
 * If nbatch > 1 then tuples that don't belong in first batch get saved
 * into inner-batch temp files. The same statements apply for the
 * first scan of the outer relation, except we write tuples to outer-batch
 * temp files.  After finishing the first scan, we do the following for
 * each remaining batch:
 *	1. Read tuples from inner batch file, load into hash buckets.
 *	2. Read tuples from outer batch file, match to hash buckets and output.
 *
 * It is possible to increase nbatch on the fly if the in-memory hash table
 * gets too big.  The hash-value-to-batch computation is arranged so that this
 * can only cause a tuple to go into a later batch than previously thought,
 * never into an earlier batch.  When we increase nbatch, we rescan the hash
 * table and dump out any tuples that are now of a later batch to the correct
 * inner batch file.  Subsequently, while reading either inner or outer batch
 * files, we might find tuples that no longer belong to the current batch;
 * if so, we just dump them out to the correct batch file.
 * ----------------------------------------------------------------
 */

/* these are in nodes/execnodes.h: */
/* typedef struct HashJoinTupleData *HashJoinTuple; */
/* typedef struct HashJoinTableData *HashJoinTable; */

typedef struct HashJoinTupleData
{
	/* link to next tuple in same bucket */
	union
	{
		struct HashJoinTupleData *unshared;
		dsa_pointer shared;
	}			next;
	uint32		hashvalue;		/* tuple's hash code */
	/* Tuple data, in MinimalTuple format, follows on a MAXALIGN boundary */
}			HashJoinTupleData;

#define HJTUPLE_OVERHEAD  MAXALIGN(sizeof(HashJoinTupleData))
#define HJTUPLE_MINTUPLE(hjtup)  \
	((MinimalTuple) ((char *) (hjtup) + HJTUPLE_OVERHEAD))

/*
 * If the outer relation's distribution is sufficiently nonuniform, we attempt
 * to optimize the join by treating the hash values corresponding to the outer
 * relation's MCVs specially.  Inner relation tuples matching these hash
 * values go into the "skew" hashtable instead of the main hashtable, and
 * outer relation tuples with these hash values are matched against that
 * table instead of the main one.  Thus, tuples with these hash values are
 * effectively handled as part of the first batch and will never go to disk.
 * The skew hashtable is limited to SKEW_HASH_MEM_PERCENT of the total memory
 * allowed for the join; while building the hashtables, we decrease the number
 * of MCVs being specially treated if needed to stay under this limit.
 *
 * Note: you might wonder why we look at the outer relation stats for this,
 * rather than the inner.  One reason is that the outer relation is typically
 * bigger, so we get more I/O savings by optimizing for its most common values.
 * Also, for similarly-sized relations, the planner prefers to put the more
 * uniformly distributed relation on the inside, so we're more likely to find
 * interesting skew in the outer relation.
 */
typedef struct HashSkewBucket
{
	uint32		hashvalue;		/* common hash value */
	HashJoinTuple tuples;		/* linked list of inner-relation tuples */
} HashSkewBucket;

#define SKEW_BUCKET_OVERHEAD  MAXALIGN(sizeof(HashSkewBucket))
#define INVALID_SKEW_BUCKET_NO	(-1)
#define SKEW_HASH_MEM_PERCENT  2
#define SKEW_MIN_OUTER_FRACTION  0.01

/*
 * To reduce palloc overhead, the HashJoinTuples for the current batch are
 * packed in 32kB buffers instead of pallocing each tuple individually.
 */
typedef struct HashMemoryChunkData
{
	int			ntuples;		/* number of tuples stored in this chunk */
	size_t		maxlen;			/* size of the chunk's tuple buffer */
	size_t		used;			/* number of buffer bytes already used */

	/* pointer to the next chunk (linked list) */
	union
	{
		struct HashMemoryChunkData *unshared;
		dsa_pointer shared;
	}			next;

	/*
	 * The chunk's tuple buffer starts after the HashMemoryChunkData struct,
	 * at offset HASH_CHUNK_HEADER_SIZE (which must be maxaligned).  Note that
	 * that offset is not included in "maxlen" or "used".
	 */
}			HashMemoryChunkData;

typedef struct HashMemoryChunkData *HashMemoryChunk;

#define HASH_CHUNK_SIZE			(32 * 1024L)
#define HASH_CHUNK_HEADER_SIZE	MAXALIGN(sizeof(HashMemoryChunkData))
#define HASH_CHUNK_DATA(hc)		(((char *) (hc)) + HASH_CHUNK_HEADER_SIZE)
/* tuples exceeding HASH_CHUNK_THRESHOLD bytes are put in their own chunk */
#define HASH_CHUNK_THRESHOLD	(HASH_CHUNK_SIZE / 4)

/* POLAR px : Statistics collection workareas for EXPLAIN ANALYZE */
typedef struct HashJoinBatchStats
{
    uint64      outerfilesize;
    uint64      innerfilesize;
    uint64      irdbytes;           /* inner bytes read from workfile */
    uint64      ordbytes;           /* outer bytes read from workfile */
    uint64      iwrbytes;           /* inner bytes written (to later batches) */
    uint64      owrbytes;           /* outer bytes written (to later batches) */
    uint64      hashspace_final;    /* work_mem for tuples kept in hash table */
    uint64      spillspace_in;      /* work_mem from lower batches to this one */
    uint64      spillspace_out;     /* work_mem from this batch to higher ones */
    uint64      spillrows_out;      /* rows spilled from this batch to higher */
} HashJoinBatchStats;

typedef struct HashJoinTableStats
{
    struct StringInfoData  *joinexplainbuf; /* Join operator's report buf */
    HashJoinBatchStats     *batchstats;     /* -> array[0..nbatchstats-1] */
    int                     nbatchstats;    /* num of batchstats slots */
    int                     endedbatch;     /* index of last batch ended */

    /* These statistics are cumulative over all nontrivial batches... */
    int                     nonemptybatches;    /* num of nontrivial batches */
    Size                    workmem_max;        /* work_mem high water mark */
    PxExplain_Agg          chainlength;        /* hash chain length stats */
} HashJoinTableStats;
/* POLAR end */

/*
 * For each batch of a Parallel Hash Join, we have a ParallelHashJoinBatch
 * object in shared memory to coordinate access to it.  Since they are
 * followed by variable-sized objects, they are arranged in contiguous memory
 * but not accessed directly as an array.
 */
typedef struct ParallelHashJoinBatch
{
	dsa_pointer buckets;		/* array of hash table buckets */
	Barrier		batch_barrier;	/* synchronization for joining this batch */

	dsa_pointer chunks;			/* chunks of tuples loaded */
	size_t		size;			/* size of buckets + chunks in memory */
	size_t		estimated_size; /* size of buckets + chunks while writing */
	size_t		ntuples;		/* number of tuples loaded */
	size_t		old_ntuples;	/* number of tuples before repartitioning */
	bool		space_exhausted;
	bool		skip_unmatched; /* whether to abandon unmatched scan */

	/*
	 * Variable-sized SharedTuplestore objects follow this struct in memory.
	 * See the accessor macros below.
	 */
} ParallelHashJoinBatch;

/* Accessor for inner batch tuplestore following a ParallelHashJoinBatch. */
#define ParallelHashJoinBatchInner(batch)							\
	((SharedTuplestore *)											\
	 ((char *) (batch) + MAXALIGN(sizeof(ParallelHashJoinBatch))))

/* Accessor for outer batch tuplestore following a ParallelHashJoinBatch. */
#define ParallelHashJoinBatchOuter(batch, nparticipants) \
	((SharedTuplestore *)												\
	 ((char *) ParallelHashJoinBatchInner(batch) +						\
	  MAXALIGN(sts_estimate(nparticipants))))

/* Total size of a ParallelHashJoinBatch and tuplestores. */
#define EstimateParallelHashJoinBatch(hashtable)						\
	(MAXALIGN(sizeof(ParallelHashJoinBatch)) +							\
	 MAXALIGN(sts_estimate((hashtable)->parallel_state->nparticipants)) * 2)

/* Accessor for the nth ParallelHashJoinBatch given the base. */
#define NthParallelHashJoinBatch(base, n)								\
	((ParallelHashJoinBatch *)											\
	 ((char *) (base) +													\
	  EstimateParallelHashJoinBatch(hashtable) *  (n)))

/*
 * Each backend requires a small amount of per-batch state to interact with
 * each ParallelHashJoinBatch.
 */
typedef struct ParallelHashJoinBatchAccessor
{
	ParallelHashJoinBatch *shared;	/* pointer to shared state */

	/* Per-backend partial counters to reduce contention. */
	size_t		preallocated;	/* pre-allocated space for this backend */
	size_t		ntuples;		/* number of tuples */
	size_t		size;			/* size of partition in memory */
	size_t		estimated_size; /* size of partition on disk */
	size_t		old_ntuples;	/* how many tuples before repartitioning? */
	bool		at_least_one_chunk; /* has this backend allocated a chunk? */
	bool		outer_eof;		/* has this process hit end of batch? */
	bool		done;			/* flag to remember that a batch is done */
	SharedTuplestoreAccessor *inner_tuples;
	SharedTuplestoreAccessor *outer_tuples;
} ParallelHashJoinBatchAccessor;

/*
 * While hashing the inner relation, any participant might determine that it's
 * time to increase the number of buckets to reduce the load factor or batches
 * to reduce the memory size.  This is indicated by setting the growth flag to
 * these values.
 */
typedef enum ParallelHashGrowth
{
	/* The current dimensions are sufficient. */
	PHJ_GROWTH_OK,
	/* The load factor is too high, so we need to add buckets. */
	PHJ_GROWTH_NEED_MORE_BUCKETS,
	/* The memory budget would be exhausted, so we need to repartition. */
	PHJ_GROWTH_NEED_MORE_BATCHES,
	/* Repartitioning didn't help last time, so don't try to do that again. */
	PHJ_GROWTH_DISABLED,
} ParallelHashGrowth;

/*
 * The shared state used to coordinate a Parallel Hash Join.  This is stored
 * in the DSM segment.
 */
typedef struct ParallelHashJoinState
{
	dsa_pointer batches;		/* array of ParallelHashJoinBatch */
	dsa_pointer old_batches;	/* previous generation during repartition */
	int			nbatch;			/* number of batches now */
	int			old_nbatch;		/* previous number of batches */
	int			nbuckets;		/* number of buckets */
	ParallelHashGrowth growth;	/* control batch/bucket growth */
	dsa_pointer chunk_work_queue;	/* chunk work queue */
	int			nparticipants;
	size_t		space_allowed;
	size_t		total_tuples;	/* total number of inner tuples */
	LWLock		lock;			/* lock protecting the above */

	Barrier		build_barrier;	/* synchronization for the build phases */
	Barrier		grow_batches_barrier;
	Barrier		grow_buckets_barrier;
	pg_atomic_uint32 distributor;	/* counter for load balancing */

	SharedFileSet fileset;		/* space for shared temporary files */
} ParallelHashJoinState;

/* The phases for building batches, used by build_barrier. */
#define PHJ_BUILD_ELECT					0
#define PHJ_BUILD_ALLOCATE				1
#define PHJ_BUILD_HASH_INNER			2
#define PHJ_BUILD_HASH_OUTER			3
#define PHJ_BUILD_RUN					4
#define PHJ_BUILD_FREE					5

/* The phases for probing each batch, used by for batch_barrier. */
#define PHJ_BATCH_ELECT					0
#define PHJ_BATCH_ALLOCATE				1
#define PHJ_BATCH_LOAD					2
#define PHJ_BATCH_PROBE					3
#define PHJ_BATCH_SCAN					4
#define PHJ_BATCH_FREE					5

/* The phases of batch growth while hashing, for grow_batches_barrier. */
#define PHJ_GROW_BATCHES_ELECT			0
#define PHJ_GROW_BATCHES_REALLOCATE		1
#define PHJ_GROW_BATCHES_REPARTITION	2
#define PHJ_GROW_BATCHES_DECIDE			3
#define PHJ_GROW_BATCHES_FINISH			4
#define PHJ_GROW_BATCHES_PHASE(n)		((n) % 5)	/* circular phases */

/* The phases of bucket growth while hashing, for grow_buckets_barrier. */
#define PHJ_GROW_BUCKETS_ELECT			0
#define PHJ_GROW_BUCKETS_REALLOCATE		1
#define PHJ_GROW_BUCKETS_REINSERT		2
#define PHJ_GROW_BUCKETS_PHASE(n)		((n) % 3)	/* circular phases */

typedef struct HashJoinTableData
{
	int			nbuckets;		/* # buckets in the in-memory hash table */
	int			log2_nbuckets;	/* its log2 (nbuckets must be a power of 2) */

	int			nbuckets_original;	/* # buckets when starting the first hash */
	int			nbuckets_optimal;	/* optimal # buckets (per batch) */
	int			log2_nbuckets_optimal;	/* log2(nbuckets_optimal) */

	/* buckets[i] is head of list of tuples in i'th in-memory bucket */
	union
	{
		/* unshared array is per-batch storage, as are all the tuples */
		struct HashJoinTupleData **unshared;
		/* shared array is per-query DSA area, as are all the tuples */
		dsa_pointer_atomic *shared;
	}			buckets;

	bool		skewEnabled;	/* are we using skew optimization? */
	HashSkewBucket **skewBucket;	/* hashtable of skew buckets */
	int			skewBucketLen;	/* size of skewBucket array (a power of 2!) */
	int			nSkewBuckets;	/* number of active skew buckets */
	int		   *skewBucketNums; /* array indexes of active skew buckets */

	int			nbatch;			/* number of batches */
	int			curbatch;		/* current batch #; 0 during 1st pass */

	int			nbatch_original;	/* nbatch when we started inner scan */
	int			nbatch_outstart;	/* nbatch when we started outer scan */

	bool		growEnabled;	/* flag to shut off nbatch increases */

	double		totalTuples;	/* # tuples obtained from inner plan */
	double		partialTuples;	/* # tuples obtained from inner plan by me */
	double		skewTuples;		/* # tuples inserted into skew tuples */

	/*
	 * These arrays are allocated for the life of the hash join, but only if
	 * nbatch > 1.  A file is opened only when we first write a tuple into it
	 * (otherwise its pointer remains NULL).  Note that the zero'th array
	 * elements never get used, since we will process rather than dump out any
	 * tuples of batch zero.
	 */
	BufFile   **innerBatchFile; /* buffered virtual temp file per batch */
	BufFile   **outerBatchFile; /* buffered virtual temp file per batch */

	Size		spaceUsed;		/* memory space currently used by tuples */
	Size		spaceAllowed;	/* upper limit for space used */
	Size		spacePeak;		/* peak space used */
	Size		spaceUsedSkew;	/* skew hash table's current space usage */
	Size		spaceAllowedSkew;	/* upper limit for skew hashtable */

	MemoryContext hashCxt;		/* context for whole-hash-join storage */
	MemoryContext batchCxt;		/* context for this-batch-only storage */
	MemoryContext spillCxt;		/* context for spilling to temp files */

	/* used for dense allocation of tuples (into linked chunks) */
	HashMemoryChunk chunks;		/* one list for the whole batch */

	/* Shared and private state for Parallel Hash. */
	HashMemoryChunk current_chunk;	/* this backend's current chunk */
	dsa_area   *area;			/* DSA area to allocate memory from */
	ParallelHashJoinState *parallel_state;
	ParallelHashJoinBatchAccessor *batches;
	dsa_pointer current_chunk_shared;
<<<<<<< HEAD

	/* POLAR px */
	HashJoinTableStats *stats;  /* statistics workarea for EXPLAIN ANALYZE */
	/* POLAR end */
}			HashJoinTableData;
=======
} HashJoinTableData;
>>>>>>> c1ff2d8bc5be55e302731a16aaff563b7f03ed7c

#endif							/* HASHJOIN_H */

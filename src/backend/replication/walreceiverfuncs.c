/*-------------------------------------------------------------------------
 *
 * walreceiverfuncs.c
 *
 * This file contains functions used by the startup process to communicate
 * with the walreceiver process. Functions implementing walreceiver itself
 * are in walreceiver.c.
 *
 * Portions Copyright (c) 2010-2024, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/replication/walreceiverfuncs.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#include "access/xlog_internal.h"
#include "access/xlogrecovery.h"
#include "pgstat.h"
#include "replication/walreceiver.h"
#include "storage/pmsignal.h"
#include "storage/shmem.h"
#include "utils/timestamp.h"

/* POLAR */
#include "access/polar_logindex_redo.h"
#include "polar_datamax/polar_datamax.h"

WalRcvData *WalRcv = NULL;

/*
 * How long to wait for walreceiver to start up after requesting
 * postmaster to launch it. In seconds.
 */
#define WALRCV_STARTUP_TIMEOUT 10

/* Report shared memory space needed by WalRcvShmemInit */
Size
WalRcvShmemSize(void)
{
	Size		size = 0;

	size = add_size(size, sizeof(WalRcvData));

	return size;
}

/* Allocate and initialize walreceiver-related shared memory */
void
WalRcvShmemInit(void)
{
	bool		found;

	WalRcv = (WalRcvData *)
		ShmemInitStruct("Wal Receiver Ctl", WalRcvShmemSize(), &found);

	if (!found)
	{
		/* First time through, so initialize */
		MemSet(WalRcv, 0, WalRcvShmemSize());
		WalRcv->walRcvState = WALRCV_STOPPED;
		ConditionVariableInit(&WalRcv->walRcvStoppedCV);
		SpinLockInit(&WalRcv->mutex);
		pg_atomic_init_u64(&WalRcv->writtenUpto, 0);
		WalRcv->latch = NULL;

		if (POLAR_LOGINDEX_ENABLE_XLOG_QUEUE())
			WalRcv->polar_use_xlog_queue = true;
	}
}

/* Is walreceiver running (or starting up)? */
bool
WalRcvRunning(void)
{
	WalRcvData *walrcv = WalRcv;
	WalRcvState state;
	pg_time_t	startTime;

	SpinLockAcquire(&walrcv->mutex);

	state = walrcv->walRcvState;
	startTime = walrcv->startTime;

	SpinLockRelease(&walrcv->mutex);

	/*
	 * If it has taken too long for walreceiver to start up, give up. Setting
	 * the state to STOPPED ensures that if walreceiver later does start up
	 * after all, it will see that it's not supposed to be running and die
	 * without doing anything.
	 */
	if (state == WALRCV_STARTING)
	{
		pg_time_t	now = (pg_time_t) time(NULL);

		if ((now - startTime) > WALRCV_STARTUP_TIMEOUT)
		{
			bool		stopped = false;

			SpinLockAcquire(&walrcv->mutex);
			if (walrcv->walRcvState == WALRCV_STARTING)
			{
				state = walrcv->walRcvState = WALRCV_STOPPED;
				stopped = true;
			}
			SpinLockRelease(&walrcv->mutex);

			if (stopped)
				ConditionVariableBroadcast(&walrcv->walRcvStoppedCV);
		}
	}

	if (state != WALRCV_STOPPED)
		return true;
	else
		return false;
}

/*
 * Is walreceiver running and streaming (or at least attempting to connect,
 * or starting up)?
 */
bool
WalRcvStreaming(void)
{
	WalRcvData *walrcv = WalRcv;
	WalRcvState state;
	pg_time_t	startTime;

	SpinLockAcquire(&walrcv->mutex);

	state = walrcv->walRcvState;
	startTime = walrcv->startTime;

	SpinLockRelease(&walrcv->mutex);

	/*
	 * If it has taken too long for walreceiver to start up, give up. Setting
	 * the state to STOPPED ensures that if walreceiver later does start up
	 * after all, it will see that it's not supposed to be running and die
	 * without doing anything.
	 */
	if (state == WALRCV_STARTING)
	{
		pg_time_t	now = (pg_time_t) time(NULL);

		if ((now - startTime) > WALRCV_STARTUP_TIMEOUT)
		{
			bool		stopped = false;

			SpinLockAcquire(&walrcv->mutex);
			if (walrcv->walRcvState == WALRCV_STARTING)
			{
				state = walrcv->walRcvState = WALRCV_STOPPED;
				stopped = true;
			}
			SpinLockRelease(&walrcv->mutex);

			if (stopped)
				ConditionVariableBroadcast(&walrcv->walRcvStoppedCV);
		}
	}

	if (state == WALRCV_STREAMING || state == WALRCV_STARTING ||
		state == WALRCV_RESTARTING)
		return true;
	else
		return false;
}

/*
 * Stop walreceiver (if running) and wait for it to die.
 * Executed by the Startup process.
 */
void
ShutdownWalRcv(void)
{
	WalRcvData *walrcv = WalRcv;
	pid_t		walrcvpid = 0;
	bool		stopped = false;

	/*
	 * Request walreceiver to stop. Walreceiver will switch to WALRCV_STOPPED
	 * mode once it's finished, and will also request postmaster to not
	 * restart itself.
	 */
	SpinLockAcquire(&walrcv->mutex);
	switch (walrcv->walRcvState)
	{
		case WALRCV_STOPPED:
			break;
		case WALRCV_STARTING:
			walrcv->walRcvState = WALRCV_STOPPED;
			stopped = true;
			break;

		case WALRCV_STREAMING:
		case WALRCV_WAITING:
		case WALRCV_RESTARTING:
			walrcv->walRcvState = WALRCV_STOPPING;
			/* fall through */
		case WALRCV_STOPPING:
			walrcvpid = walrcv->pid;
			break;
	}
	SpinLockRelease(&walrcv->mutex);

	/* Unnecessary but consistent. */
	if (stopped)
		ConditionVariableBroadcast(&walrcv->walRcvStoppedCV);

	/*
	 * Signal walreceiver process if it was still running.
	 */
	if (walrcvpid != 0)
		kill(walrcvpid, SIGTERM);

	/*
	 * Wait for walreceiver to acknowledge its death by setting state to
	 * WALRCV_STOPPED.
	 */
	ConditionVariablePrepareToSleep(&walrcv->walRcvStoppedCV);
	while (WalRcvRunning())
		ConditionVariableSleep(&walrcv->walRcvStoppedCV,
							   WAIT_EVENT_WAL_RECEIVER_EXIT);
	ConditionVariableCancelSleep();
}

/*
 * Request postmaster to start walreceiver.
 *
 * "recptr" indicates the position where streaming should begin.  "conninfo"
 * is a libpq connection string to use.  "slotname" is, optionally, the name
 * of a replication slot to acquire.  "create_temp_slot" indicates to create
 * a temporary slot when no "slotname" is given.
 *
 * WAL receivers do not directly load GUC parameters used for the connection
 * to the primary, and rely on the values passed down by the caller of this
 * routine instead.  Hence, the addition of any new parameters should happen
 * through this code path.
 */
void
RequestXLogStreaming(TimeLineID tli, XLogRecPtr recptr, const char *conninfo,
					 const char *slotname, bool create_temp_slot)
{
	WalRcvData *walrcv = WalRcv;
	bool		launch = false;
	pg_time_t	now = (pg_time_t) time(NULL);
	Latch	   *latch;

	/* POLAR: In replica mode we don't write XLOG to disk, so care nothing about half of a segment */
	if (!polar_in_replica_mode())
	{
		/*
		 * We always start at the beginning of the segment. That prevents a broken
		 * segment (i.e., with no records in the first half of a segment) from
		 * being created by XLOG streaming, which might cause trouble later on if
		 * the segment is e.g archived.
		 */
		if (XLogSegmentOffset(recptr, wal_segment_size) != 0)
			recptr -= XLogSegmentOffset(recptr, wal_segment_size);
	}

	SpinLockAcquire(&walrcv->mutex);

	/* It better be stopped if we try to restart it */
	Assert(walrcv->walRcvState == WALRCV_STOPPED ||
		   walrcv->walRcvState == WALRCV_WAITING);

	if (conninfo != NULL)
		strlcpy((char *) walrcv->conninfo, conninfo, MAXCONNINFO);
	else
		walrcv->conninfo[0] = '\0';

	/*
	 * Use configured replication slot if present, and ignore the value of
	 * create_temp_slot as the slot name should be persistent.  Otherwise, use
	 * create_temp_slot to determine whether this WAL receiver should create a
	 * temporary slot by itself and use it, or not.
	 */
	if (slotname != NULL && slotname[0] != '\0')
	{
		strlcpy((char *) walrcv->slotname, slotname, NAMEDATALEN);
		walrcv->is_temp_slot = false;
	}
	else
	{
		walrcv->slotname[0] = '\0';
		walrcv->is_temp_slot = create_temp_slot;
	}

	if (walrcv->walRcvState == WALRCV_STOPPED)
	{
		launch = true;
		walrcv->walRcvState = WALRCV_STARTING;
	}
	else
		walrcv->walRcvState = WALRCV_RESTARTING;
	walrcv->startTime = now;

	/*
	 * If this is the first startup of walreceiver (on this timeline),
<<<<<<< HEAD
	 * initialize receivedUpto and latestChunkStart to the starting point.
	 *
	 * In DMA mode, the starting point must be the end of WAL record.
=======
	 * initialize flushedUpto and latestChunkStart to the starting point.
>>>>>>> c1ff2d8bc5be55e302731a16aaff563b7f03ed7c
	 */
	if (POLAR_ENABLE_DMA() || walrcv->receiveStart == 0 || walrcv->receivedTLI != tli)
	{
		walrcv->flushedUpto = recptr;
		walrcv->receivedTLI = tli;
		walrcv->latestChunkStart = recptr;
	}
	else
	{
		/* 
		 * POLAR: when no timeline switch, set receivedUpto and latestChunkStart to the last valid lsn when request xlog streaming
		 * so that when we re-receive a wal segment file(there may be wrong data in the wal file so we need to re-receive it)
		 * we can correctly judge whether we have received new wal in WaitForWALToBecomeAvailable func according to new receivedUpto
		 * rather than the previous one, which maybe greater than the wal we actually received in current streaming process.
		 * similarly, set latestChunkStart each time so that we can update XLogReceiptTime correctly when we have replayed new wal 
		 */ 
		TimeLineID valid_tli = 0;
		XLogRecPtr valid_lsn = InvalidXLogRecPtr;
		valid_lsn = polar_is_datamax() ? polar_datamax_get_last_valid_received_lsn(polar_datamax_ctl, &valid_tli) :
				GetXLogReplayRecPtr(&valid_tli);
		/* POLAR: set receivedUpto as the max value */
		if (valid_lsn > recptr)
		{
			walrcv->latestChunkStart = walrcv->receivedUpto = valid_lsn;
			walrcv->receivedTLI = valid_tli;
		}
		else
			walrcv->latestChunkStart = walrcv->receivedUpto = recptr;
	}
	
	walrcv->receiveStart = recptr;
	walrcv->receiveStartTLI = tli;

	latch = walrcv->latch;

	SpinLockRelease(&walrcv->mutex);

	if (launch)
		SendPostmasterSignal(PMSIGNAL_START_WALRECEIVER);
	else if (latch)
		SetLatch(latch);
}

/*
 * Returns the last+1 byte position that walreceiver has flushed.
 *
 * Optionally, returns the previous chunk start, that is the first byte
 * written in the most recent walreceiver flush cycle.  Callers not
 * interested in that value may pass NULL for latestChunkStart. Same for
 * receiveTLI.
 */
XLogRecPtr
GetWalRcvFlushRecPtr(XLogRecPtr *latestChunkStart, TimeLineID *receiveTLI)
{
	WalRcvData *walrcv = WalRcv;
	XLogRecPtr	recptr;

	SpinLockAcquire(&walrcv->mutex);
	recptr = walrcv->flushedUpto;
	if (latestChunkStart)
		*latestChunkStart = walrcv->latestChunkStart;
	if (receiveTLI)
		*receiveTLI = walrcv->receivedTLI;
	SpinLockRelease(&walrcv->mutex);

	return recptr;
}

/*
 * Returns the last+1 byte position that walreceiver has written.
 * This returns a recently written value without taking a lock.
 */
XLogRecPtr
GetWalRcvWriteRecPtr(void)
{
	WalRcvData *walrcv = WalRcv;

	return pg_atomic_read_u64(&walrcv->writtenUpto);
}

/*
 * Returns the replication apply delay in ms or -1
 * if the apply delay info is not available
 */
int
GetReplicationApplyDelay(void)
{
	WalRcvData *walrcv = WalRcv;
	XLogRecPtr	receivePtr;
	XLogRecPtr	replayPtr;
	TimestampTz chunkReplayStartTime;

	SpinLockAcquire(&walrcv->mutex);
	receivePtr = walrcv->flushedUpto;
	SpinLockRelease(&walrcv->mutex);

	replayPtr = GetXLogReplayRecPtr(NULL);

	if (receivePtr == replayPtr)
		return 0;

	chunkReplayStartTime = GetCurrentChunkReplayStartTime();

	if (chunkReplayStartTime == 0)
		return -1;

	return TimestampDifferenceMilliseconds(chunkReplayStartTime,
										   GetCurrentTimestamp());
}

/*
 * Returns the network latency in ms, note that this includes any
 * difference in clock settings between the servers, as well as timezone.
 */
int
GetReplicationTransferLatency(void)
{
	WalRcvData *walrcv = WalRcv;
	TimestampTz lastMsgSendTime;
	TimestampTz lastMsgReceiptTime;

	SpinLockAcquire(&walrcv->mutex);
	lastMsgSendTime = walrcv->lastMsgSendTime;
	lastMsgReceiptTime = walrcv->lastMsgReceiptTime;
	SpinLockRelease(&walrcv->mutex);

	return TimestampDifferenceMilliseconds(lastMsgSendTime,
										   lastMsgReceiptTime);
}

-------------------------- MODULE InterruptibleMutex ---------------------------
\*
\* Interruptible mutex. It is a construct similar to how a normal mutex + a
\* condition variable are often used. Except that the interruptible mutex is
\* more generic. It abstracts the "condition variable" into something outside of
\* the mutex itself, and which can be "woken up" (like via a callback).
\*
\* The main usecase is the TaskScheduler, where the interruptible mutex is going
\* to protect the scheduler-role, while the scheduler is sleeping on some
\* condition variable / signal / epoll / whatever else.
\*
EXTENDS TLC, Integers, Sequences

--------------------------------------------------------------------------------
\*
\* Constant values.
\*

CONSTANT WorkerThreadIDs
\* One thread is "main". It won't do blocking locks. The logic is that it
\* represents actually one or more such threads, that they try to take the lock
\* periodically, and when one of them does, it goes to sleep on a condition.
\*
\* Allowing all the threads to take the lock in a blocking way eventually leads
\* to a situation when the signal is empty, one thread sleeps on a condition,
\* and all the others are waiting. This could be fixed if "wakeup" would be done
\* by each new waiter, but now it is only done on the first waiter entering the
\* queue and for the use-case (TaskScheduler) this is enough. Because its
\* workers will never take a blocking lock + sleep on the tasks.
CONSTANT MainThreadID
CONSTANT NULL

\* Worker threads are symmetrical. Meaning that changing their places in
\* different runs won't change the result.
Perms == Permutations(WorkerThreadIDs)

--------------------------------------------------------------------------------
\*
\* Variables.
\*

VARIABLE MutexState
VARIABLE MutexOwnerID
\* The interruptible mutex's internals are protected with a regular mutex. It
\* helps to synchronize certain things in case of contention. But otherwise when
\* there is no contention, the internals aren't touched.
VARIABLE InternalOwnerID
\* A list of threads waiting for the current owner of the mutex to give them the
\* ownership.
VARIABLE Waiters
VARIABLE WorkerThreads
\* Some abstract condition on which a thread can "sleep" and by which it can be
\* "woken up".
VARIABLE IsConditionSignaled

vars == <<MutexState, MutexOwnerID, Waiters, WorkerThreads,
          IsConditionSignaled, InternalOwnerID>>

--------------------------------------------------------------------------------
\*
\* Helper functions.
\*

\* In TLA+ there is no convenient way to change multiple fields of a struct,
\* especially if it is in an array of other structs. Functions here help to make
\* it look sane. For that the order of arguments is inverted - the object to
\* change goes last. Thanks to that, multiple mutations can be written as
\*
\*   SetField(key1, val1,
\*   SetField(key2, val2,
\*   SetField(key3, val3,
\*   Object)))
\*
\* Almost like normal assignment. This is additionally simplified by the fact
\* that there are no struct types. The same setter can be used for any struct,
\* only member name matters.

SetState(v, b) == [b EXCEPT !.state = v]
SetIsTaken(v, b) == [b EXCEPT !.is_taken = v]
SetHasWaiters(v, b) == [b EXCEPT !.has_waiters = v]
SetOldMutexState(v, b) == [b EXCEPT !.old_mutex_state = v]

\* The same but for struct arrays.

ArrLen(s) == Len(s)
ArrFirst(s) == Head(s)
ArrIsEmpty(s) == Len(s) = 0
ArrPopHead(s) == Tail(s)
ArrSet(i, v, s) == [s EXCEPT ![i] = v]
ArrAppend(v, s) == Append(s, v)
ArrSetState(i, v, s) == ArrSet(i, SetState(v, s[i]), s)
ArrSetOldMutexState(i, v, s) == ArrSet(i, SetOldMutexState(v, s[i]), s)

\* Constructors.

\* It is basically a few flags which can be checked and set atomically. An
\* std::atomic_uint8 in C++.
MutexStateNew == [
  \* The mutex is owned by some thread right now.
  is_taken |-> FALSE,
  \* The mutex has at least one pending waiter. It means that when unlock
  \* happens, the ownership must be transferred directly to the first waiter.
  \* And that new attempts to try-lock the mutex must fail.
  has_waiters |-> FALSE
]

WorkerThreadNew == [
  state |-> "idle",
  \* An "on stack" variable for some cases where it couldn't be used in a single
  \* atomic step.
  old_mutex_state |-> MutexStateNew
]

Init ==
  /\ MutexState = MutexStateNew
  /\ MutexOwnerID = NULL
  /\ InternalOwnerID = NULL
  /\ Waiters = << >>
  /\ WorkerThreads = [wid \in WorkerThreadIDs |-> WorkerThreadNew]
  /\ IsConditionSignaled = FALSE

\* In the steps below the splitting is done by actions which are visible to
\* other participants. For example, blocking lock firstly takes the internal
\* mutex, and then does changes to the interruptible mutex state. Both those
\* steps are observable in other threads. Hence done as separate steps in TLA.
\*
\* Actions, which do not change globally visible data or are atomic, can be done
\* in one step. For example, an atomic increment can be done as a single step.
\* Without spitting read and write.

--------------------------------------------------------------------------------
\* Locking.

\* Try to take a lock non-blocking.
ThreadTryLock(wid) ==
  LET w == WorkerThreads[wid] IN
  /\ w.state = "idle"
  \* Is only possible when completely free. Note that even if the mutex isn't
  \* taken, but has waiters, it is still not possible to just steal it with
  \* try-lock at this point. Must either wait until all waiters are gone, or
  \* must become one of them.
  /\ ~MutexState.is_taken /\ ~MutexState.has_waiters
  \* ---
  /\ WorkerThreads' = ArrSetState(wid, "lock_done", WorkerThreads)
  /\ MutexState' = SetIsTaken(TRUE, MutexState)
  /\ UNCHANGED<<MutexOwnerID, Waiters, IsConditionSignaled, InternalOwnerID>>

\* Blocking lock.
ThreadBlockingLock(wid) ==
  LET w == WorkerThreads[wid] IN
  /\ w.state = "idle"
  \* Main thread can't do the blocking locks. Because if it would,
  /\ wid # MainThreadID
  /\ MutexState.is_taken \/ MutexState.has_waiters
  /\ InternalOwnerID = NULL
  \* ---
  /\ InternalOwnerID' = wid
  /\ WorkerThreads' = ArrSetState(wid, "lock_start", WorkerThreads)
  /\ UNCHANGED<<MutexState, MutexOwnerID, Waiters, IsConditionSignaled>>

ThreadLockStart(wid) ==
  LET w == WorkerThreads[wid] IN
  /\ w.state = "lock_start"
  /\ Assert(InternalOwnerID = wid, "internal mutex is taken")
  \* ---
  /\ MutexState' = SetHasWaiters(TRUE, MutexState)
  /\ WorkerThreads' = ArrSetState(wid, "lock_check_old_state",
                      ArrSetOldMutexState(wid, MutexState,
                      WorkerThreads))
  /\ UNCHANGED<<MutexOwnerID, Waiters, IsConditionSignaled, InternalOwnerID>>

ThreadLockCheckOldState(wid) ==
  LET w == WorkerThreads[wid] IN
  /\ w.state = "lock_check_old_state"
  /\ Assert(InternalOwnerID = wid, "internal mutex is taken")
  \* ---
  /\ IF ~w.old_mutex_state.is_taken THEN
     /\ Assert(MutexState.has_waiters, "has waiters")
     /\ Assert(~MutexState.is_taken, "not taken")
     /\ MutexState' = SetIsTaken(TRUE, MutexStateNew)
     /\ WorkerThreads' = ArrSetState(wid, "lock_exit_without_waiting", WorkerThreads)
     ELSE
     /\ WorkerThreads' = ArrSetState(wid, "lock_stand_into_queue", WorkerThreads)
     /\ UNCHANGED<<MutexState>>
  /\ UNCHANGED<<MutexOwnerID, Waiters, IsConditionSignaled, InternalOwnerID>>

ThreadLockExitWithoutWaiting(wid) ==
  LET w == WorkerThreads[wid] IN
  /\ w.state = "lock_exit_without_waiting"
  /\ Assert(InternalOwnerID = wid, "internal mutex is taken")
  \* ---
  /\ InternalOwnerID' = NULL
  /\ WorkerThreads' = ArrSetState(wid, "lock_done", WorkerThreads)
  /\ UNCHANGED<<MutexState, MutexOwnerID, Waiters, IsConditionSignaled>>

ThreadLockStandIntoQueue(wid) ==
  LET w == WorkerThreads[wid] IN
  /\ w.state = "lock_stand_into_queue"
  /\ Assert(InternalOwnerID = wid, "internal mutex is taken")
  \* ---
  /\ IF ArrIsEmpty(Waiters) THEN
     /\ IsConditionSignaled' = TRUE
     ELSE
     /\ UNCHANGED<<IsConditionSignaled>>
  /\ Waiters' = ArrAppend(wid, Waiters)
  /\ WorkerThreads' = ArrSetState(wid, "lock_stood_into_queue", WorkerThreads)
  /\ UNCHANGED<<MutexState, MutexOwnerID, InternalOwnerID>>

ThreadLockStoodIntoQueue(wid) ==
  LET w == WorkerThreads[wid] IN
  /\ w.state = "lock_stood_into_queue"
  /\ Assert(InternalOwnerID = wid, "internal mutex is taken")
  \* ---
  /\ InternalOwnerID' = NULL
  /\ WorkerThreads' = ArrSetState(wid, "lock_wait", WorkerThreads)
  /\ UNCHANGED<<MutexState, MutexOwnerID, Waiters, IsConditionSignaled>>

ThreadLockTake(wid) ==
  /\ \/ ThreadTryLock(wid)
     \/ ThreadBlockingLock(wid)
     \/ ThreadLockStart(wid)
     \/ ThreadLockCheckOldState(wid)
     \/ ThreadLockExitWithoutWaiting(wid)
     \/ ThreadLockStandIntoQueue(wid)
     \/ ThreadLockStoodIntoQueue(wid)

--------------------------------------------------------------------------------
\* ...

ThreadLockedSleep(wid) ==
  /\ wid = MainThreadID
  /\ WorkerThreads' = ArrSetState(wid, "locked_sleep", WorkerThreads)

ThreadLockedLeave(wid) ==
  /\ WorkerThreads' = ArrSetState(wid, "unlock_start", WorkerThreads)

ThreadLockUse(wid) ==
  LET w == WorkerThreads[wid] IN
  /\ \/ /\ w.state = "lock_done"
        /\ Assert(MutexOwnerID = NULL, "no owner")
        /\ MutexOwnerID' = wid
        /\ \/ ThreadLockedSleep(wid)
           \/ ThreadLockedLeave(wid)
        /\ UNCHANGED<<IsConditionSignaled>>
     \/ /\ w.state = "locked_sleep"
        /\ Assert(MutexOwnerID = wid, "this is owner")
        /\ IsConditionSignaled
        /\ IsConditionSignaled' = FALSE
        /\ WorkerThreads' = ArrSetState(wid, "unlock_start", WorkerThreads)
        /\ UNCHANGED<<MutexOwnerID>>
  /\ Assert(MutexState.is_taken, "state is taken")
  /\ UNCHANGED<<MutexState, Waiters, InternalOwnerID>>

--------------------------------------------------------------------------------
\* ...

ThreadUnlockStart(wid) ==
  LET w == WorkerThreads[wid] IN
  /\ w.state = "unlock_start"
  \* ---
  /\ Assert(MutexOwnerID = wid, "this is owner")
  /\ Assert(MutexState.is_taken, "state is taken")
  /\ MutexOwnerID' = NULL
  /\ IF MutexState = SetIsTaken(TRUE, MutexStateNew) THEN
     /\ MutexState' = MutexStateNew
     /\ WorkerThreads' = ArrSetState(wid, "idle", WorkerThreads)
     ELSE
     /\ Assert(MutexState.is_taken, "is taken")
     /\ Assert(MutexState.has_waiters, "has waiters")
     /\ WorkerThreads' = ArrSetState(wid, "unlock_take_internal", WorkerThreads)
     /\ UNCHANGED<<MutexState>>
  /\ UNCHANGED<<Waiters, InternalOwnerID>>

ThreadUnlockTakeInternal(wid) ==
  LET w == WorkerThreads[wid] IN
  /\ w.state = "unlock_take_internal"
  /\ InternalOwnerID = NULL
  /\ Assert(MutexState.is_taken, "state is taken")
  \* ---
  /\ InternalOwnerID' = wid
  /\ WorkerThreads' = ArrSetState(wid, "unlock_check_waiters_flag", WorkerThreads)
  /\ UNCHANGED<<MutexState, MutexOwnerID, Waiters>>

ThreadUnlockCheckWaitersFlag(wid) ==
  LET w == WorkerThreads[wid] IN
  /\ w.state = "unlock_check_waiters_flag"
  /\ Assert(InternalOwnerID = wid, "is internally locked")
  /\ Assert(MutexState.is_taken, "state is taken")
  /\ Assert(~ArrIsEmpty(Waiters), "has waiters")
  \* ---
  /\ IF ArrLen(Waiters) = 1 THEN
     /\ MutexState' = SetHasWaiters(FALSE, MutexState)
     ELSE
     /\ UNCHANGED<<MutexState>>
  /\ WorkerThreads' = ArrSetState(wid, "unlock_wakeup_waiter", WorkerThreads)
  /\ UNCHANGED<<MutexOwnerID, InternalOwnerID, Waiters>>

ThreadUnlockWakeupWaiter(wid) ==
  LET w == WorkerThreads[wid] IN
  /\ w.state = "unlock_wakeup_waiter"
  /\ Assert(InternalOwnerID = wid, "is internally locked")
  /\ Assert(MutexState.is_taken, "state is taken")
  /\ Assert(~ArrIsEmpty(Waiters), "has waiters")
  \* ---
  /\ Waiters' = ArrPopHead(Waiters)
  /\ WorkerThreads' = ArrSetState(wid, "unlock_end",
                      ArrSetState(ArrFirst(Waiters), "lock_done",
                      WorkerThreads))
  /\ UNCHANGED<<MutexState, MutexOwnerID, InternalOwnerID>>

ThreadUnlockEnd(wid) ==
  LET w == WorkerThreads[wid] IN
  /\ w.state = "unlock_end"
  /\ Assert(InternalOwnerID = wid, "is internally locked")
  \* ---
  /\ InternalOwnerID' = NULL
  /\ WorkerThreads' = ArrSetState(wid, "idle", WorkerThreads)
  /\ UNCHANGED<<MutexState, MutexOwnerID, Waiters>>

ThreadUnlock(wid) ==
  /\ \/ ThreadUnlockStart(wid)
     \/ ThreadUnlockTakeInternal(wid)
     \/ ThreadUnlockCheckWaitersFlag(wid)
     \/ ThreadUnlockWakeupWaiter(wid)
     \/ ThreadUnlockEnd(wid)
  /\ UNCHANGED<<IsConditionSignaled>>

--------------------------------------------------------------------------------
\*
\* Actions.
\*

Next ==
  \/ \E wid \in WorkerThreadIDs:
     \/ ThreadLockTake(wid)
     \/ ThreadLockUse(wid)
     \/ ThreadUnlock(wid)

--------------------------------------------------------------------------------
\*
\* Invariants.
\*

TotalInvariant == TRUE

Spec ==
  /\ Init
  /\ [][Next]_vars
  /\ \A i \in WorkerThreadIDs:
     /\ SF_vars(ThreadLockTake(i))
     /\ WF_vars(ThreadLockUse(i))
     /\ SF_vars(ThreadUnlock(i))

================================================================================

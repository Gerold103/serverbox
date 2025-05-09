-------------------------- MODULE InterruptibleMutex ---------------------------
\*
\* ...
\*
EXTENDS TLC, Integers, Sequences

--------------------------------------------------------------------------------
\*
\* Constant values.
\*

CONSTANT WorkerThreadIDs
CONSTANT MainThreadID
CONSTANT NULL

\* Consumers are symmetrical. Meaning that changing their places in different
\* runs won't change the result.
Perms == Permutations(WorkerThreadIDs)

--------------------------------------------------------------------------------
\*
\* Variables.
\*

VARIABLE MutexState
VARIABLE MutexOwnerID
VARIABLE InternalOwnerID
VARIABLE Waiters
VARIABLE WorkerThreads
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
ArrLast(s) == s[Len(s)]
ArrFirst(s) == Head(s)
ArrIsEmpty(s) == Len(s) = 0
ArrPopHead(s) == Tail(s)
ArrSet(i, v, s) == [s EXCEPT ![i] = v]
ArrSetLast(v, s) == ArrSet(ArrLen(s), v, s)
ArrAppend(v, s) == Append(s, v)
ArrSetState(i, v, s) == ArrSet(i, SetState(v, s[i]), s)
ArrSetOldMutexState(i, v, s) == ArrSet(i, SetOldMutexState(v, s[i]), s)

\* Constructors.

MutexStateNew == [
  is_taken |-> FALSE,
  has_waiters |-> FALSE
]

WorkerThreadNew == [
  state |-> "idle",
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
\* other participants. For example, push changes counter and some queue
\* elements. All are visible to consumers. Hence must be done in separate steps
\* to give the consumers a chance to notice these changes and handle them
\* correctly.
\* Actions, which do not change globally visible data or are atomic, can be done
\* in one step. For example, an atomic increment can be done as a single step.
\* Without spitting read and write.

--------------------------------------------------------------------------------
\* ...

ThreadTryLock(wid) ==
  LET w == WorkerThreads[wid] IN
  /\ w.state = "idle"
  /\ ~MutexState.is_taken /\ ~MutexState.has_waiters
  \* ---
  /\ WorkerThreads' = ArrSetState(wid, "lock_done", WorkerThreads)
  /\ MutexState' = SetIsTaken(TRUE, MutexState)
  /\ UNCHANGED<<MutexOwnerID, Waiters, IsConditionSignaled, InternalOwnerID>>

ThreadBlockingLock(wid) ==
  LET w == WorkerThreads[wid] IN
  /\ w.state = "idle"
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

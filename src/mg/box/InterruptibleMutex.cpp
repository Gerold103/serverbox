#include "InterruptibleMutex.h"

#include "mg/box/Thread.h"

namespace mg {
namespace box {
namespace {
	enum InterruptibleMutexFlags
	{
		INTERRUPTIBLE_MUTEX_TAKEN = 0x1,
		INTERRUPTIBLE_MUTEX_HAS_WAITERS = 0x2,
	};
}
	InterruptibleMutex::InterruptibleMutex()
		: myState(0)
	{
	}

	InterruptibleMutex::~InterruptibleMutex()
	{
		MG_BOX_ASSERT(myState.LoadRelaxed() == 0);
	}

	void
	InterruptibleMutex::Lock(
		const InterruptibleMutexWakeupCallback& aWakeup)
	{
		if (TryLock())
			return;

		// The mutex is owned by somebody. Try to take it over.
		myMutex.Lock();
		// Pessimistically this thread might have to wait, so lets add the corresponding
		// flag. Use 'acquire' to sync all the writes done by the mutex's current owner.
		// In case the owner is already leaving.
		uint8_t expected = myState.FetchBitOrAcquire(
			INTERRUPTIBLE_MUTEX_HAS_WAITERS);
		if ((expected & INTERRUPTIBLE_MUTEX_TAKEN) == 0)
		{
			// Could happen than between trying to lock and taking the internal mutex the
			// original owner has already left. If it actually left the ownership
			// entirely, it means there were no waiters.
			MG_BOX_ASSERT(myWaiters.IsEmpty());
			// And no other thread could take the lock, because it would see the 'waiters'
			// flag and would also try to take the internal mutex, which is owner by this
			// thread atm. Hence it is safe to just take the ownership.
			// 'Relaxed' should be enough, because acquire was already done just above.
			expected = myState.ExchangeRelaxed(INTERRUPTIBLE_MUTEX_TAKEN);
			// The waiters flag was set by this thread, but not anymore. New waiters, if
			// any are waiting on the internal mutex, will just set this flag again if
			// needed.
			MG_BOX_ASSERT(expected == INTERRUPTIBLE_MUTEX_HAS_WAITERS);
			myMutex.Unlock();
			return;
		}
		// Some other thread owns the lock and this thread has announced its presence via
		// the waiters-flag. Now time to wait until the owner will hand the ownership
		// over.
		InterruptibleMutexWaiter self;
		if (myWaiters.IsEmpty())
		{
			// Only the first waiter wakes the owner up. Makes no sense to wake it
			// multiple times.
			aWakeup();
		}
		myWaiters.Append(&self);
		myMutex.Unlock();

		// Just wait for the ownership to be handed over.
		self.mySignal.ReceiveBlocking();
		// Use 'acquire' to sync all the writes done by the previous owner.
		MG_BOX_ASSERT((myState.LoadAcquire() & INTERRUPTIBLE_MUTEX_TAKEN) != 0);
	}

	bool
	InterruptibleMutex::TryLock()
	{
		// It is important that the lock can't be taken if there are already waiters. No
		// thread can take the ownership out of order. Otherwise this might cause
		// starvation of the waiters.
		// Use 'acquire' to sync all the writes done by the previous owner.
		uint8_t expected = 0;
		return myState.CmpExchgStrongAcquire(expected, INTERRUPTIBLE_MUTEX_TAKEN);
	}

	void
	InterruptibleMutex::Unlock()
	{
		// The happy-path is when there are no waiters. Then just unlock it and leave.
		// Use 'release' to provide a sync point for the next owners to sync all the
		// writes done by this thread.
		uint8_t expected = INTERRUPTIBLE_MUTEX_TAKEN;
		if (myState.CmpExchgStrongRelease(expected, 0))
			return;

		// So there were some waiters spotted. Try to hand the ownership directly to the
		// first one.
		MG_BOX_ASSERT(expected ==
			(INTERRUPTIBLE_MUTEX_TAKEN | INTERRUPTIBLE_MUTEX_HAS_WAITERS));

		myMutex.Lock();
		MG_BOX_ASSERT(!myWaiters.IsEmpty());
		// It is important to firstly pop the waiter, and only then remove the
		// waiters-flag (if there was only one waiter). Otherwise the following might
		// have happened:
		// - Thread-1 takes the lock.
		// - Thread-2 becomes a waiter and adds the waiter-flag.
		// - Thread-1 frees the lock and (wrongly) signals the thread-2 that the ownership
		//       is given.
		// - Thread-2 wakes up and tries to free the lock too.
		// - Thread-2 sees that the waiters-flag is set, but there are no waiters. Broken
		//       assumption.
		InterruptibleMutexWaiter* first = myWaiters.PopFirst();
		// Even when nothing to do - still use 'release'. To give the next owner a way to
		// sync all the writes done by this thread.
		if (myWaiters.IsEmpty())
			expected = myState.FetchBitAndRelease(~INTERRUPTIBLE_MUTEX_HAS_WAITERS);
		else
			expected = myState.FetchBitAndRelease(-1);
		MG_BOX_ASSERT((expected & INTERRUPTIBLE_MUTEX_TAKEN) != 0);
		MG_BOX_ASSERT((expected & INTERRUPTIBLE_MUTEX_HAS_WAITERS) != 0);
		first->mySignal.Send();
		myMutex.Unlock();
	}

}
}

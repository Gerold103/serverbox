#pragma once

#include "mg/box/DoublyList.h"
#include "mg/box/Signal.h"

#include <functional>

namespace mg {
namespace box {

	struct InterruptibleMutexWaiter
	{
		mg::box::Signal mySignal;
		InterruptibleMutexWaiter* myNext;
		InterruptibleMutexWaiter* myPrev;
	};

	using InterruptibleMutexWakeupCallback = std::function<void()>;

	// The interruptible mutex allows one thread to take the mutex, get blocked
	// on something else while holding it, and the other threads can wake it up
	// (interrupt) to forcefully take the mutex for some other work.
	//
	// The guarantees are the following:
	// - There is no spin-locking when TryLock() is used.
	// - Some threads use TryLock() + can sleep while holding the lock,
	//     then other threads via Lock() can take the ownership over to do some
	//     work given that they won't be sleeping on the same condition.
	//
	// An example:
	//
	// - Thread-workers (any number):
	//       if (TryLock())
	//       {
	//           WaitOn(Signal);
	//           Unlock();
	//       }
	//
	// - Thread-others (any number):
	//       Lock([](){ Signal.Send(); });
	//       DoSomething(); // But don't sleep on the same Signal.
	//       Unlock().
	//
	// Then the 'other' threads are guaranteed to get the lock. And nothing will
	// ever deadlock.
	//
	class InterruptibleMutex
	{
	public:
		InterruptibleMutex();
		~InterruptibleMutex();

		void Lock(
			const InterruptibleMutexWakeupCallback& aWakeup);
		bool TryLock();
		void Unlock();

	private:
		InterruptibleMutex(
			const InterruptibleMutex&) = delete;

		mg::box::Mutex myMutex;
		mg::box::DoublyList<InterruptibleMutexWaiter> myWaiters;
		mg::box::AtomicU8 myState;
	};

}
}

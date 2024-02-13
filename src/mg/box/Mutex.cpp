#include "Mutex.h"

#include "mg/box/Thread.h"

namespace mg {
namespace box {

	mg::box::AtomicU64 theMutexStartContentCount(0);

	void
	MutexStatClear()
	{
		theMutexStartContentCount.StoreRelaxed(0);
	}

	uint64_t
	MutexStatContentionCount()
	{
		return theMutexStartContentCount.LoadRelaxed();
	}

	void
	Mutex::Lock()
	{
		if (TryLock())
			return;
		myHandle.lock();
		myOwner = GetCurrentThreadId();
		MG_DEV_ASSERT(myCount == 0);
		myCount = 1;
	}

	bool
	Mutex::TryLock()
	{
		if (!myHandle.try_lock())
		{
			if (IsOwnedByThisThread())
			{
				MG_DEV_ASSERT(myCount > 0);
				++myCount;
				return true;
			}
			theMutexStartContentCount.IncrementRelaxed();
			return false;
		}
		myOwner = GetCurrentThreadId();
		MG_DEV_ASSERT(myCount == 0);
		myCount = 1;
		return true;
	}

	void
	Mutex::Unlock()
	{
		MG_BOX_ASSERT(IsOwnedByThisThread());
		if (myCount == 1)
		{
			myOwner = 0;
			myCount = 0;
			myHandle.unlock();
			return;
		}
		MG_DEV_ASSERT(myCount > 1);
		--myCount;
	}

	bool
	Mutex::IsOwnedByThisThread() const
	{
		return myOwner == GetCurrentThreadId();
	}

}
}

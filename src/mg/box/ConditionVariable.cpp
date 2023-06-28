#include "ConditionVariable.h"

#include "mg/box/Assert.h"

namespace mg {
namespace box {

	void
	ConditionVariable::Wait(
		Mutex& aMutex)
	{
		MG_BOX_ASSERT(aMutex.IsOwnedByThisThread());
		uint32_t tid = aMutex.myOwner;
		aMutex.myOwner = 0;

		myHandle.wait(aMutex.myHandle);

		MG_BOX_ASSERT(aMutex.myOwner == 0);
		aMutex.myOwner = tid;
	}

	void
	ConditionVariable::TimedWait(
		Mutex& aMutex,
		uint32_t aTimeoutMs,
		bool* aOutIsTimedOut)
	{
		MG_BOX_ASSERT(aMutex.IsOwnedByThisThread());
		uint32_t tid = aMutex.myOwner;
		aMutex.myOwner = 0;

		*aOutIsTimedOut = myHandle.wait_for(aMutex.myHandle,
			std::chrono::milliseconds(aTimeoutMs)) ==
			std::cv_status::timeout;

		MG_BOX_ASSERT(aMutex.myOwner == 0);
		aMutex.myOwner = tid;
	}

}
}

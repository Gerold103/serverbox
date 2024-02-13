#include "ConditionVariable.h"

#include "mg/box/Assert.h"

namespace mg {
namespace box {

	void
	ConditionVariable::Wait(
		Mutex& aMutex)
	{
		MG_BOX_ASSERT(aMutex.IsOwnedByThisThread());
		MG_BOX_ASSERT(aMutex.myCount == 1);
		uint32_t tid = aMutex.myOwner;
		aMutex.myOwner = 0;
		aMutex.myCount = 0;

		myHandle.wait(aMutex.myHandle);

		MG_BOX_ASSERT(aMutex.myOwner == 0);
		MG_BOX_ASSERT(aMutex.myCount == 0);
		aMutex.myOwner = tid;
		aMutex.myCount = 1;
	}

	bool
	ConditionVariable::TimedWait(
		Mutex& aMutex,
		mg::box::TimeLimit aTimeLimit)
	{
		MG_BOX_ASSERT(aMutex.IsOwnedByThisThread());
		MG_BOX_ASSERT(aMutex.myCount == 1);
		uint32_t tid = aMutex.myOwner;
		aMutex.myOwner = 0;
		aMutex.myCount = 0;

		uint64_t timeout = aTimeLimit.ToDurationFromNow().myValue;
		bool ok = myHandle.wait_for(aMutex.myHandle,
			std::chrono::milliseconds(timeout)) != std::cv_status::timeout;

		MG_BOX_ASSERT(aMutex.myOwner == 0);
		MG_BOX_ASSERT(aMutex.myCount == 0);
		aMutex.myOwner = tid;
		aMutex.myCount = 1;

		return ok;
	}

}
}

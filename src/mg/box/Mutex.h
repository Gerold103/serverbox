#pragma once

#include "mg/box/Assert.h"
#include "mg/box/Atomic.h"

#include <atomic>
#include <mutex>

F_DECLARE_CLASS(mg, box, ConditionVariable)

namespace mg {
namespace box {

	extern mg::box::AtomicU64 theMutexStartContentCount;

	void MutexStatClear();

	uint64_t MutexStatContentionCount();

	class Mutex
	{
	public:
		Mutex() : myOwner(0), myCount(0) {}
		~Mutex() { MG_BOX_ASSERT(myOwner == 0 && myCount == 0); }

		void Lock();
		bool TryLock();
		void Unlock();
		bool IsOwnedByThisThread() const;

	private:
		Mutex(
			const Mutex&) = delete;

		std::mutex myHandle;
		uint32_t myOwner;
		uint32_t myCount;

		friend class ConditionVariable;
	};

	class MutexLock
	{
	public:
		MutexLock(
			Mutex& aMutex);

		~MutexLock();

		void Unlock();

	private:
		MutexLock(
			const MutexLock&) = delete;
		MutexLock& operator=(
			const MutexLock&) = delete;

		Mutex* myMutex;
	};

	//////////////////////////////////////////////////////////////

	inline
	MutexLock::MutexLock(
		Mutex& aMutex)
		: myMutex(&aMutex)
	{
		myMutex->Lock();
	}

	inline
	MutexLock::~MutexLock()
	{
		if (myMutex != nullptr)
			myMutex->Unlock();
	}

	inline void
	MutexLock::Unlock()
	{
		if (myMutex == nullptr)
			return;
		myMutex->Unlock();
		myMutex = nullptr;
	}

}
}

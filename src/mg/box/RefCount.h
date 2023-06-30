#pragma once

#include "mg/box/Assert.h"
#include "mg/box/Atomic.h"

namespace mg {
namespace box {

	// Atomic reference counter. Its values are:
	// - It encapsulates the atomic operations;
	// - Helps to easily detect the last unref;
	// - Does sanity checks.
	//
	class RefCount
	{
	public:
		explicit RefCount();
		explicit RefCount(
			uint32_t aCount);

		// Delete any means to copy another ref count object. It
		// is not clear what they should do for a reference
		// counted item.
		RefCount(
			const RefCount&) = delete;
		RefCount& operator=(
			const RefCount&) = delete;
		~RefCount() { MG_DEV_ASSERT(myCount.LoadRelaxed() == 0); }

		void Inc();
		bool Dec();
		uint32_t GetValue() const { return myCount.LoadRelaxed(); }

	private:
		mg::box::AtomicU32 myCount;
	};

	//////////////////////////////////////////////////////////////

	inline
	RefCount::RefCount()
		// One ref for the counter's creator.
		: myCount(1)
	{
	}

	inline
	RefCount::RefCount(
		uint32_t aCount)
		: myCount((int32_t)aCount)
	{
	}

	inline void
	RefCount::Inc()
	{
		int32_t newValue = myCount.IncrementFetchRelaxed();
		MG_DEV_ASSERT(newValue >= 1);
	}

	inline bool
	RefCount::Dec()
	{
		int32_t newValue = myCount.DecrementFetchRelaxed();
		MG_DEV_ASSERT(newValue >= 0);
		return newValue == 0;
	}

}
}

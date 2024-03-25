#include "Coro.h"

#include "mg/box/Assert.h"

namespace mg {
namespace box {

#if MG_CORO_IS_ENABLED

	static void
	CoroHandleUnref(
		CoroHandle aCoro)
	{
		if (!aCoro)
			return;
		// Do not destroy the current coro right away. It could be that it isn't the top
		// one. The coroutine stack must be unrolled from the top. Higher frames might
		// depend on lower (previous) ones, but not vice versa.
		CoroHandle top = aCoro;
		while (top.promise().myNext)
			top = top.promise().myNext.Handle();
		while (CoroHandle prev = top.promise().myPrev)
		{
			prev.promise().myNext.Promise().myPrev = {};
			prev.promise().myNext = {};
			top = prev;
		}
		aCoro.destroy();
	}

	//////////////////////////////////////////////////////////////////////////////////////

	CoroRef::CoroRef(
		const CoroRef&)
	{
		// The constructor is not deleted at compile-time, because otherwise it isn't
		// usable inside std::function, which requires all its content to be copyable.
		MG_BOX_ASSERT(!"Coro can have only one owner");
	}

	void
	CoroRef::Clear()
	{
		Coro c = std::move(myCoro);
		myCoro = {};
		CoroHandleUnref(c);
	}

	CoroRef&
	CoroRef::operator=(
		CoroRef&& aObj)
	{
		Coro c = myCoro;
		myCoro = std::move(aObj.myCoro);
		aObj.myCoro = {};
		CoroHandleUnref(c);
		return *this;
	}

	//////////////////////////////////////////////////////////////////////////////////////

	void
	CoroOpFinal::await_suspend(
		CoroHandle aThisCoro) noexcept
	{
		CoroPromise& thisPromise = aThisCoro.promise();
		CoroHandle prev = thisPromise.myPrev;
		if (!prev)
			return;
		thisPromise.myPrev = {};

		CoroPromise& prevPromise = prev.promise();
		prevPromise.myNext = {};
		prevPromise.myFirst.promise().myLast = prev;
		prev.resume();
	}

	//////////////////////////////////////////////////////////////////////////////////////

	void
	CoroPromise::unhandled_exception() noexcept
	{
		MG_BOX_ASSERT(!"Unhandled exception from a coroutine");
	}

	//////////////////////////////////////////////////////////////////////////////////////

	CoroOpCall::CoroOpCall(
		Coro&& aNewCoro)
		: myNewCoro(std::move(aNewCoro))
	{
	}

	CoroHandle
	CoroOpCall::await_suspend(
		CoroHandle aThisCoro) noexcept
	{
		// Place the new coro on top of the coro stack.

		CoroHandle newHandle = myNewCoro.Handle();
		CoroPromise& newPromise = newHandle.promise();
		CoroPromise& thisPromise = aThisCoro.promise();

		newPromise.myPrev = aThisCoro;
		thisPromise.myNext = std::move(myNewCoro);

		newPromise.myFirst = thisPromise.myFirst;
		newPromise.myFirst.promise().myLast = newHandle;

		return newHandle;
	}

#endif

}
}

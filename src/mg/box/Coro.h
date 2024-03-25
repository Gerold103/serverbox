#pragma once

#include "mg/box/Definitions.h"

#if IS_CPP_AT_LEAST_20
#include <coroutine>
#define MG_CORO_IS_ENABLED 1
#else
#define MG_CORO_IS_ENABLED 0
#endif

namespace mg {
namespace box {

#if MG_CORO_IS_ENABLED
	//
	// Box Coro is a C++20 coroutine wrapper. This module provides its basic types and
	// operations. These coroutines are usable right away, but the main goal is their
	// integration with other serverbox modules. Such as TaskScheduler for Go-like runtime
	// with coroutines being executed on multiple threads.
	//

	struct CoroPromise;

	using CoroHandle = std::coroutine_handle<CoroPromise>;

	struct Coro
		: public CoroHandle
	{
		using promise_type = CoroPromise;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	// Base class for all awaitable operations. They are supposed to be created and
	// destroyed right in the co_await expression. Hence not copyable nor assignable.
	struct CoroOp
	{
		CoroOp() = default;
		CoroOp(
			const CoroOp&) = delete;
		CoroOp& operator=(
			const CoroOp&) = delete;
	};

	struct CoroOpIsNotReady { static constexpr bool await_ready() noexcept { return false; } };
	struct CoroOpIsEmptyReturn { static constexpr void await_resume() noexcept {} };

	struct CoroOpFinal
		: public CoroOp
		, public CoroOpIsNotReady
		, public CoroOpIsEmptyReturn
	{
		void await_suspend(
			CoroHandle aThisCoro) noexcept;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	// Coroutines have single ownership and are automatically destroyed when become
	// unused.
	struct CoroRef
	{
		CoroRef() = default;
		CoroRef(
			Coro&& aCoro) : myCoro(std::move(aCoro)) {}
		CoroRef(
			CoroRef&& aObj) : myCoro(std::move(aObj.myCoro)) { aObj.myCoro = {}; }
		CoroRef(
			const CoroRef& aObj);
		~CoroRef() { Clear(); }

		CoroPromise& Promise() const { return myCoro.promise(); }
		constexpr operator bool() const { return (bool)myCoro; }
		constexpr CoroHandle Handle() const { return myCoro; }
		void Clear();
		void ResumeTop() const;
		CoroRef& operator=(
			CoroRef&& aObj);

	private:
		Coro myCoro;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	// Promise is the coroutine's context.
	struct CoroPromise
	{
		Coro get_return_object() noexcept;
		static constexpr std::suspend_always initial_suspend() noexcept { return {}; }
		static constexpr CoroOpFinal final_suspend() noexcept { return {}; }
		static constexpr void return_void() noexcept {}
		void unhandled_exception() noexcept;

		// Coroutines can be stacked on each other, like a normal callstack. If the
		// coroutine gets destroyed, then its entire callstack is destroyed as well, in
		// reversed order, from the top.
		CoroRef myNext;
		// For non-bottom coroutine it leads to the previous coroutine, which is resumed
		// when the current one ends.
		CoroHandle myPrev;
		// All coroutines in the callstack know the first coroutine at the bottom. This is
		// needed in order to let it know when a new coroutine appears on top. And that in
		// turn is needed so as when a whole coroutine stack needs to be resumed, there is
		// a quick access to the topmost coroutine to continue it.
		CoroHandle myFirst;
		// Bottom coroutine keeps this up-to-date to point at the topmost coroutine in the
		// callstack.
		CoroHandle myLast;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	struct CoroOpCall
		: public CoroOp
		, public CoroOpIsNotReady
		, public CoroOpIsEmptyReturn
	{
		CoroOpCall(
			Coro&& aNewCoro);
		CoroHandle await_suspend(
			CoroHandle aThisCoro) noexcept;

	private:
		CoroRef myNewCoro;
	};

	// Call a new coroutine while being inside of another coroutine. When the new one
	// ends, the current one will be continued. Use like this:
	//
	//     co_await CoroCall([](...) -> Coro {...});
	//
	static inline CoroOpCall
	CoroCall(
		Coro&& aNewCoro)
	{
		return CoroOpCall(std::move(aNewCoro));
	}

	//////////////////////////////////////////////////////////////////////////////////////

	inline void
	CoroRef::ResumeTop() const
	{
		Promise().myLast.resume();
	}

	inline Coro
	CoroPromise::get_return_object() noexcept
	{
		Coro res = {Coro::from_promise(*this)};
		myFirst = res;
		myLast = res;
		return res;
	}

#endif

}
}

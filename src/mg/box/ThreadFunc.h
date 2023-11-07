#pragma once

#include "mg/box/Thread.h"

#include <functional>

namespace mg {
namespace box {

	using ThreadCallback = std::function<void()>;

	// Wrap a callback into a thread. It is essentially
	// syntax sugar which allows not to inherit Thread class to
	// make something trivial. Similar to std::thread.
	class ThreadFunc
		: public Thread
	{
	public:
		ThreadFunc(
			const char *aName,
			const ThreadCallback& aCallback);

		~ThreadFunc();

	private:
		void Run() override;

		ThreadCallback myCallback;
	};

	//////////////////////////////////////////////////////////////

	inline
	ThreadFunc::ThreadFunc(
		const char *aName,
		const ThreadCallback& aCallback)
		: Thread(aName)
		, myCallback(aCallback)
	{
	}

	inline
	ThreadFunc::~ThreadFunc()
	{
		BlockingStop();
	}

	inline void
	ThreadFunc::Run()
	{
		myCallback();
	}

}
}

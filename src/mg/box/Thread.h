#pragma once

#include "mg/box/Atomic.h"
#include "mg/box/ConditionVariable.h"
#include "mg/box/Mutex.h"

#include <string>
#include <thread>

#define MG_THREADLOCAL thread_local

namespace mg {
namespace box {

	using ThreadId = uint32_t;

	class Thread
	{
	public:
		// Thread name should be a short string up to 15 bytes not
		// including terminating 0 (15 printable symbols). The
		// reason is that on Linux thread names are truncated to
		// 16 bytes.
		Thread(
			const char*	aName = "anon_thread");

		void Start();

		void Stop();

		void BlockingStop();

		void StopAndDelete();

		bool StopRequested();

		bool IsRunning();

	protected:
		virtual ~Thread();

	private:
		void PrivTrampoline();

		virtual void Run() = 0;

		Mutex myLock;
		ConditionVariable myCond;
		std::thread* myHandle;
		std::string myName;
		mg::box::AtomicBool myIsStopRequested;
		mg::box::AtomicBool myIsRunning;
		bool myWasStarted;
	};

	void Sleep(
		uint64_t aDuration);

	void SleepInfinite();

	ThreadId GetCurrentThreadId();

}
}

#pragma once

#include "mg/box/Mutex.h"
#include "mg/box/Time.h"

#include <condition_variable>

namespace mg {
namespace box {

	// Condition variable allows to atomically unlock a mutex and
	// lock on a condition until a signal. This is vital for some
	// tasks (or very strongly simplifies them). For example,
	// assume that a thread wants an event. Event is a certain
	// value of a variable. The variable is protected by a mutex.
	// To check its value the thread needs to lock the mutex,
	// check the variable. If it does not match the needed value,
	// the thread should unlock the mutex and go to sleep.
	//
	// What if the event happens exactly after the thread unlocked
	// the mutex? In that case it will go to sleep forever,
	// because it missed the needed value already. Condition
	// variable solves exactly that problem - unlock a mutex and
	// go to sleep atomically.
	//
	class ConditionVariable
	{
	public:
		ConditionVariable() = default;

		void Wait(
			Mutex& aMutex);
		bool TimedWait(
			Mutex& aMutex,
			mg::box::TimeLimit aTimeLimit);

		void Signal() { myHandle.notify_one(); }
		void Broadcast() { myHandle.notify_all(); }

	private:
		ConditionVariable(
			const ConditionVariable&) = delete;
		ConditionVariable& operator=(
			const ConditionVariable&) = delete;

		std::condition_variable_any myHandle;
	};

}
}

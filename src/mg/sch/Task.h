#pragma once

#include "mg/box/Atomic.h"
#include "mg/box/Coro.h"
#include "mg/box/TypeTraits.h"

#include <functional>

F_DECLARE_CLASS(mg, box, Signal)

namespace mg {
namespace sch {

	class Task;
	class TaskScheduler;
	class TaskSchedulerThread;

	// The status is an internal thing, not supposed to be used by
	// the external code.
	enum TaskStatus : int32_t
	{
		// Task is pending if it not known yet if it has a
		// deadline in the future. The task can be anywhere except
		// ready queue and waiting queue.
		TASK_STATUS_PENDING,
		// The task is moved to the waiting queue. Or is going to
		// be added there, or was just popped from there. It has
		// a deadline in the future.
		TASK_STATUS_WAITING,
		// Task is going to be executed asap. Or is being executed
		// right now and was woken up during the execution. The
		// task can be anywhere.
		TASK_STATUS_READY,
		// The task is signaled, so will be executed asap. Or is
		// being executed right now, and was signaled during the
		// execution. The task can be anywhere.
		TASK_STATUS_SIGNALED,
	};

	using TaskCallback = std::function<void(Task*)>;

#if MG_CORO_IS_ENABLED
	//////////////////////////////////////////////////////////////////////////////////////
	// C++20 coroutine operations.

	struct TaskCoroOpYield
		: public mg::box::CoroOp
		, public mg::box::CoroOpIsNotReady
		, public mg::box::CoroOpIsEmptyReturn
	{
		TaskCoroOpYield(
			Task* aTask,
			TaskScheduler& aSched) : myTask(aTask), mySched(aSched) {}
		bool await_suspend(
			mg::box::CoroHandle aThisCoro) noexcept;

	private:
		Task* const myTask;
		TaskScheduler& mySched;
	};

	struct TaskCoroOpReceiveSignal
		: public mg::box::CoroOp
		, public mg::box::CoroOpIsNotReady
	{
		TaskCoroOpReceiveSignal(
			Task* aTask,
			TaskScheduler& aSched) : myTask(aTask), mySched(aSched) {}
		bool await_suspend(
			mg::box::CoroHandle aThisCoro) noexcept;
		bool await_resume() const noexcept;

	private:
		Task* const myTask;
		TaskScheduler& mySched;
	};

	struct TaskCoroOpExitDelete
		: public mg::box::CoroOp
		, public mg::box::CoroOpIsNotReady
		, public mg::box::CoroOpIsEmptyReturn
	{
		TaskCoroOpExitDelete(
			Task* aTask) : myTask(aTask) {}
		bool await_suspend(
			mg::box::CoroHandle aThisCoro) noexcept;

	private:
		Task* const myTask;
	};

	struct TaskCoroOpExitExec
		: public mg::box::CoroOp
		, public mg::box::CoroOpIsNotReady
		, public mg::box::CoroOpIsEmptyReturn
	{
		TaskCoroOpExitExec(
			Task* aTask,
			TaskCallback&& aNewCallback)
			: myTask(aTask), myNewCallback(std::move(aNewCallback)) {}
		bool await_suspend(
			mg::box::CoroHandle aThisCoro) noexcept;

	private:
		Task* const myTask;
		TaskCallback myNewCallback;
	};

	struct TaskCoroOpExitSendSignal
		: public mg::box::CoroOp
		, public mg::box::CoroOpIsNotReady
		, public mg::box::CoroOpIsEmptyReturn
	{
		TaskCoroOpExitSendSignal(
			Task* aTask,
			mg::box::Signal& aSignal)
			: myTask(aTask), mySignal(aSignal) {}
		bool await_suspend(
			mg::box::CoroHandle aThisCoro) noexcept;

	private:
		Task* const myTask;
		mg::box::Signal& mySignal;
	};

	//////////////////////////////////////////////////////////////////////////////////////
#endif

	// Task is code executed called in one of the worker threads in a scheduler. As code
	// typically is used a callback (bind, lambda, function), but also coroutines are
	// supported. Task also provides some features such as deadlines, delays, signaling.
	//
	class Task
	{
	public:
		Task();

		// Can construct from a callback, or create a callback
		// in-place.
		template<typename Functor>
		Task(
			Functor&& aFunc);

#if MG_CORO_IS_ENABLED
		Task(
			mg::box::Coro&& aCoro);
#endif

		~Task();

		//////////////////////////////////////////////////////////////////////////////////
		// Thread-safe methods for invoking any time from anywhere.
		//

		// Wakeup guarantees that the task will be executed again without a delay, at
		// least once.
		//
		// However won't affect a signaled task. Because if the task is signaled during
		// execution, then was woken up, then consumed the signal **in the same
		// execution**, then the wakeup didn't do anything. Without a signal the wakeup
		// would make an effect even during execution.
		//
		// Typical usecase for Wakeup is when your task schedules some async work to
		// another thread and goes to infinite sleep using Wait. It needs to be woken up
		// when the work is done to progress further. Also it can be used for
		// cancellations if you protect the task with reference counting.
		//
		// ## Attention!!!
		//
		// Be extremely careful, if you use Wakeup to reschedule a task sleeping on a
		// deadline, when some work is done. This is how it may look in a naive way:
		//
		//     void
		//     FinishSomeWork()
		//     {
		//         isReady = true;                          // (1)
		//         task->PostWakeup();                      // (2)
		//     }
		//
		//     void
		//     TaskBody(Task* aTask)
		//     {
		//         if (isReady)
		//         {
		//             // Work is done.
		//             delete this;
		//             return;
		//         }
		//         // Timed out.
		//         assert(aTask->IsExpired());
		//         isCanceled = true;
		//         // Wait for the work cancel.
		//		   scheduler.PostWait(aTask);
		//     }
		//
		//     void
		//     TaskStart()
		//     {
		//         scheduler.PostDeadline(new Task(TaskBody), 30 * 1000);
		//     }
		//
		// This code is broken. Because after (1) the task may be woken up by a timeout,
		// will see isReady, and will delete itself. Then Wakeup() in (2) will crash.
		//
		// Solutions for that:
		// - Use ref counting and delete the task on last ref;
		// - Use Signal (see Signal method);
		// - Don't use deadlines and delays.
		//
		// ## Note about deletion
		//
		// Deletion can be heavy, and you may want to do it in a worker thread always.
		// Then you can't use reference counting + Wakeup. The canceling thread will need
		// to call Wakeup + unref, and may end up removing the last ref, if the task woke
		// up fast and removed the next-to-last ref. Then you have to delete the task not
		// in the worker thread.
		//
		// Solutions for that:
		// - Post a new 'deletion' task on last ref removal;
		// - Use Signal.
		//
		void PostWakeup();

		// Signal is a communication channel between the task and any other code. It
		// allows to forcefully wakeup a task and set a 'signal' flag **atomically**,
		// which the task won't be able to miss anyhow. Meaning of the signal can be
		// anything you want - a sub-task completion, a future being ready, or whatever
		// else. Signal is completely lock-free, so you should prefer Signal instead of
		// thread synchronization objects (with locks) when possible.
		//
		// Almost always you need Signal instead of Wakeup.
		//
		// Here is a bit modified example from Wakeup doc, but using the signal to tell
		// the task that the sub-task was completed:
		//
		//     void
		//     FinishSomeWork()
		//     {
		//         isReady = true;
		//         result = 'Success';
		//         task->PostSignal();
		//     }
		//
		//     void
		//     TaskBody(Task* aTask)
		//     {
		//         if (aTask->ReceiveSignal())
		//         {
		//             // Work is done.
		//             assert(isReady);                     // (1)
		//             printf("%s", result);                // (2)
		//             delete this;
		//             return;
		//         }
		//         // Timed out.
		//         isCanceled = true;
		//         // Wait for the work cancel.
		//		   scheduler.PostWait(aTask);
		//     }
		//
		//     void
		//     TaskStart()
		//     {
		//         scheduler.PostDeadline(new Task(TaskBody), 30 * 1000);
		//     }
		//
		// You can be sure, that in (1) the signal sender does not use the task anymore.
		// It can be safely deleted or re-scheduled, you can reuse isReady, etc.
		//
		// Despite signal is just a flag, you can associate it with any other more complex
		// objects. For example, you can be sure that in (2) the result is here.
		//
		// If the task wants to continue after signal (for example, to cleanup some stuff
		// before termination), it must ReceiveSignal(). Then it can proceed, and the
		// signal can be re-used again.
		//
		void PostSignal();

#if MG_CORO_IS_ENABLED
		//////////////////////////////////////////////////////////////////////////////////
		// C++20 coroutine methods. When a coroutine yields (co_await), its task enters
		// the waiting state in the chosen scheduler. After wakeup the coroutine might be
		// scheduled on any worker thread. When a scheduler is not specified, the task
		// uses the one where it runs right now.

		// Sleep until the task is woken up due to any reason.
		//
		//     Coro
		//     TaskBody(Task* aTask)
		//     {
		//         // Deadline. Without a deadline is re-scheduled instantly.
		//         aTask->[Set/Adjust][Delay/Deadline](...);
		//         co_await aTask->AsyncYield();
		//         // Returns when the task is awake.
		//     }
		//
		TaskCoroOpYield AsyncYield();
		TaskCoroOpYield AsyncYield(
			TaskScheduler& aSched);

		// Sleep until the task is woken up due to any reason + try to receive a signal.
		// Returns whether it was received.
		//
		//     Coro
		//     TaskBody(Task* aTask)
		//     {
		//         while(true) {
		//             // Deadline. Without a deadline is re-scheduled instantly.
		//             aTask->[Set/Adjust][Delay/Deadline](...);
		//             bool ok = co_await aTask->AsyncReceiveSignal();
		//             if (ok)
		//                 break; // Signal was received.
		//
		//             // Otherwise spurios wakeup. Not received.
		//             continue;
		//         }
		//     }
		//
		TaskCoroOpReceiveSignal AsyncReceiveSignal();
		TaskCoroOpReceiveSignal AsyncReceiveSignal(
			TaskScheduler& aSched);

		// Delete the task object and abort the entire stack of the coroutines that it is
		// running now. Can call from a nested coroutine too. Whatever is after this call,
		// it will not be executed.
		//
		//     Coro
		//     TaskBody(Task* aTask)
		//     {
		//         co_await aTask->AsyncExitDelete();
		//
		//         // Code below is unreachable. The task is deleted.
		//         assert(false);
		//     }
		//
		TaskCoroOpExitDelete AsyncExitDelete();

		// Switch the task's body from a coroutine to a plain non-coroutine callback.
		// Whatever is after this call, it will not be executed.
		//
		//     Coro
		//     TaskBody(Task* aTask)
		//     {
		//         co_await aTask->AsyncExitExec([](Task* aTask) {
		//             // Non-coroutine callback. Can do here anything what is allowed in
		//             // plain callbacks.
		//             scheduler.PostWait(aTask);
		//         });
		//
		//         // Code below is unreachable.
		//         assert(false);
		//     }
		//
		// It can be used for non-trivial destruction of task resources. For example, to
		// delete a bigger object which has the task as a member:
		//
		//     Coro
		//     MyObject::Execute()
		//     {
		//         co_await myTask.AsyncExitExec([this](Task* aTask) {
		//             assert(aTask == &myTask);
		//             delete this;
		//         });
		//
		//         // Code below is unreachable.
		//         assert(false);
		//     }
		//
		TaskCoroOpExitExec AsyncExitExec(
			TaskCallback&& aCallback);

		// Exit the coroutine and send the given signal object. It is useful when don't
		// want to delete the task, but want to notify some other thread waiting on the
		// task to be finished to destroy it.
		//
		// Just doing the following code is unsafe:
		//
		//     Coro
		//     TaskBody(Task* aTask)
		//     {
		//         // Do work ...
		//         // ...
		//         aSignal.Send();                          // (1)
		//         co_return;                               // (2)
		//     }
		//
		//     // Some other thread.
		//     aSignal.Recv();                              // (3)
		//     delete task;                                 // (4)
		//
		// Because the sequence of execution might be the following: (1), (3), (4), (2).
		// At (2) is going to happen a use-after-free, because the coroutine object is
		// already deleted in (4).
		//
		// The code can be transformed like this:
		//
		//     Coro
		//     TaskBody(Task* aTask)
		//     {
		//         // Do work ...
		//         // ...
		//         co_await aTask->AsyncExitSendSignal(aSignal);
		//
		//         // Code below is unreachable.
		//         assert(false);
		//     }
		//
		//     // Some other thread.
		//     aSignal.Recv();
		//     delete task;
		//
		// Technically, the same can be done using AsyncExitExec(), but it would be more
		// code to write.
		//
		TaskCoroOpExitSendSignal AsyncExitSendSignal(
			mg::box::Signal& aSignal);
#endif

		//////////////////////////////////////////////////////////////////////////////////
		// Methods for in-worker invocation or before posting the task. For checking and
		// handling events, work with the data, etc. Not thread-safe.
		//

		// Can set from an existing callback object, or create a
		// callback in-place.
		// Can't be called when the task has been posted to the
		// scheduler waiting for execution.
		template<typename Functor>
		void SetCallback(
			Functor&& aFunc);

#if MG_CORO_IS_ENABLED
		void SetCallback(
			mg::box::Coro&& aCoro);
#endif

		// **You must prefer** Deadline or Wait if you want an
		// infinite delay. To avoid unnecessary current time get.
		// Can't be called when the task has been posted to the
		// scheduler waiting for execution.
		void SetDelay(
			uint32_t aDelay);

		// Set deadline to a minimal value between the current
		// deadline and the current time + the given delay. It
		// makes sense to use when the task consists of several
		// independent parts which set their own deadlines and it
		// must wakeup by the earliest one. Keep in mind, that
		// when the task just starts execution, its deadline is
		// dropped to 0. To use the feature one can set the
		// deadline to infinity in the beginning of an execution
		// and only then adjust it.
		// Can't be called when the task has been posted to the
		// scheduler waiting for execution.
		void AdjustDelay(
			uint32_t aDelay);

		// Setting a deadline may be useful when it is known in
		// advance - allows to avoid at least one call to get the
		// current time.
		// Can't be called when the task has been posted to the
		// scheduler waiting for execution.
		void SetDeadline(
			uint64_t aDeadline);

		// The same as delay adjustment but the deadline is passed
		// by the user. Not calculated inside. This is faster when
		// know the deadline in advance.
		// Can't be called when the task has been posted to the
		// scheduler waiting for execution.
		void AdjustDeadline(
			uint64_t aDeadline);

		// The task won't be executed again until an explicit
		// wakeup or signal.
		// Can't be called when the task has been posted to the
		// scheduler waiting for execution.
		void SetWait();

		// Check if the task has a signal emitted.
		// Can be called anytime.
		bool IsSignaled();

		// Check if the task was woken up naturally, by a
		// deadline, not by an explicit signal or wakeup. Will be
		// always true for tasks not having any deadline. So makes
		// sense only for tasks scheduled with a real deadline.
		// Keep in mind, that the task can be expired and signaled
		// at the same time.
		// Can't be called when the task has been posted to the
		// scheduler waiting for execution.
		bool IsExpired() const;

		// Deadline of the task to apply when it is posted next
		// time. Deadline is dropped to 0 when the task starts
		// execution, so this getter makes only sense after a new
		// deadline is set.
		// Can't be called when the task has been posted to the
		// scheduler waiting for execution.
		uint64_t GetDeadline() const;

		// Atomically try to receive a signal. In case of success
		// the signal is cleared, and true is returned. If the
		// task is signaled but ReceiveSignal is not used, it will
		// be rescheduled **ignoring all deadlines** until the
		// signal is received. Also signals don't stack. It means
		// that if multiple signals were sent, they all will be
		// consumed with one receipt.
		// Can be called anytime.
		bool ReceiveSignal();

	private:
		void PrivExecute();

		void PrivCreate();

		void PrivTouch() const;
	public:
		// The comparator is public so as it could be used by the
		// waiting queue.
		bool operator<=(
			const Task& aTask) const;

		// Next is public so as it could be used by the intrusive
		// front queue.
		Task* myNext;
		// Index is public so as it could be used by the intrusive
		// waiting queue.
		int32_t myIndex;
	private:
		mg::box::Atomic<TaskStatus> myStatus;
		// Is set to the scheduler the task is right now inside of. The task can't be
		// altered anyhow while it is in there.
		TaskScheduler* myScheduler;
		uint64_t myDeadline;
		TaskCallback myCallback;
		bool myIsExpired;

		friend class TaskScheduler;
		friend struct TaskCoroOpExitDelete;
		friend struct TaskCoroOpExitExec;
		friend struct TaskCoroOpExitSendSignal;
		friend struct TaskCoroOpReceiveSignal;
		friend struct TaskCoroOpYield;
	};

	inline
	Task::Task()
	{
		PrivCreate();
	}

	template<typename Functor>
	inline
	Task::Task(
		Functor&& aFunc)
		: myCallback(std::forward<Functor>(aFunc))
	{
		PrivCreate();
	}

#if MG_CORO_IS_ENABLED
	inline
	Task::Task(
		mg::box::Coro&& aCoro)
	{
		PrivCreate();
		SetCallback(std::move(aCoro));
	}
#endif

	inline
	Task::~Task()
	{
		PrivTouch();
	}

#if MG_CORO_IS_ENABLED
	inline TaskCoroOpYield
	Task::AsyncYield(
		TaskScheduler& aSched)
	{
		return TaskCoroOpYield(this, aSched);
	}

	inline TaskCoroOpReceiveSignal
	Task::AsyncReceiveSignal(
		TaskScheduler& aSched)
	{
		return TaskCoroOpReceiveSignal(this, aSched);
	}

	inline TaskCoroOpExitDelete
	Task::AsyncExitDelete()
	{
		return TaskCoroOpExitDelete(this);
	}

	inline TaskCoroOpExitExec
	Task::AsyncExitExec(
		TaskCallback&& aCallback)
	{
		return TaskCoroOpExitExec(this, std::move(aCallback));
	}

	inline TaskCoroOpExitSendSignal
	Task::AsyncExitSendSignal(
		mg::box::Signal& aSignal)
	{
		return TaskCoroOpExitSendSignal(this, aSignal);
	}
#endif

	template<typename Functor>
	inline void
	Task::SetCallback(
		Functor&& aFunc)
	{
		PrivTouch();
		myCallback = std::forward<Functor>(aFunc);
	}

	inline void
	Task::SetWait()
	{
		SetDeadline(MG_TIME_INFINITE);
	}

	inline bool
	Task::IsSignaled()
	{
		return myStatus.LoadAcquire() == TASK_STATUS_SIGNALED;
	}

	inline bool
	Task::operator<=(
		const Task& aTask) const
	{
		return myDeadline <= aTask.myDeadline;
	}

}
}

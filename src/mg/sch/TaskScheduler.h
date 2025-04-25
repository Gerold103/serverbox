#pragma once

#include "mg/box/BinaryHeap.h"
#include "mg/box/DoublyList.h"
#include "mg/box/ForwardList.h"
#include "mg/box/MultiConsumerQueue.h"
#include "mg/box/MultiProducerQueueIntrusive.h"
#include "mg/box/Signal.h"
#include "mg/box/Thread.h"

#include "mg/sch/Task.h"

#include <vector>

namespace mg {
namespace sch {

	// Front queue is populated by external threads and by worker
	// threads. So it is multi-producer. It is dispatched by
	// sched-thread only, so it is single-consumer. Each task goes
	// firstly to the front queue.
	using TaskSchedulerQueueFront = mg::box::MultiProducerQueueIntrusive<Task>;
	// Waiting queue is only used by one worker thread at a time,
	// so it has no concurrent access and is therefore not
	// protected with a lock. Tasks move from the front queue to
	// the waiting queue only if they have a deadline in the
	// future.
	using TaskSchedulerQueueWaiting = mg::box::BinaryHeapMinIntrusive<Task>;
	// Ready queue is populated only by the sched-thread and
	// consumed by all workers when dispatching tasks. Tasks move
	// to the ready queue from either the front queue if a task is
	// due to execute right away or from the waiting queue if a
	// task deadline is met (or it was woken up or signaled).
	using TaskSchedulerQueueReady = mg::box::MultiConsumerQueue<Task>;
	using TaskSchedulerQueueReadyConsumer = mg::box::MultiConsumerQueueConsumer<Task>;
	// Pending queue is populated from the front queue for further processing. Normally
	// all its tasks are just handled right away, but if there is a particularly huge wave
	// of new tasks, then the scheduler might start buffering them in the pending queue
	// for processing in pieces of a limited size. It is accessed only by the
	// sched-thread.
	using TaskSchedulerQueuePending = mg::box::ForwardList<Task>;

	// Special type to post callbacks not bound to a task. The
	// scheduler creates tasks for them inside. Keep in mind, that
	// **it is not free**, as the scheduler will need to allocate
	// the task object on its own, and it adds +1 virtual call,
	// because the one-shot callback is called via a normal task
	// callback. Avoid it for all perf-critical code.
	using TaskCallbackOneShot = std::function<void(void)>;

	class TaskSchedulerThread;

	enum TaskSchedulerState
	{
		TASK_SCHEDULER_STATE_RUNNING,
		TASK_SCHEDULER_STATE_STOPPING,
		TASK_SCHEDULER_STATE_STOPPED,
	};

	enum TaskSchedulerRoleFlags
	{
		TASK_SCHEDULER_ROLE_FLAG_TAKEN = 0x1,
		TASK_SCHEDULER_ROLE_FLAG_INSPECTION = 0x2,
	};

	struct TaskSchedulerVisitor
	{
		mg::box::Signal mySignal;
		TaskSchedulerVisitor* myPrev;
		TaskSchedulerVisitor* myNext;
	};

	using TaskSchedulerVisitorList = mg::box::DoublyList<TaskSchedulerVisitor>;

	// Scheduler for asynchronous execution of tasks. Can be used
	// for tons of one-shot short-living tasks, as well as for
	// long-living periodic tasks with deadlines.
	class TaskScheduler
	{
	public:
		// The name is displayed as a part of the worker thread
		// names. It should be not longer than 4 symbols to be
		// displayed not truncated. See Thread.cpp for appropriate
		// shortcuts.
		// Sub-queue size is used by the ready-queue dispatched by
		// the worker threads. The bigger is the size, the smaller
		// is contention between the workers. But too big size may
		// lead to huge cache misses and memory overuse. A few
		// thousands (1-5) usually is fine.
		TaskScheduler(
			const char* aName,
			uint32_t aSubQueueSize);

		~TaskScheduler();

		void Start(
			uint32_t aThreadCount);
		bool WaitEmpty(
			mg::box::TimeLimit aTimeLimit = mg::box::theTimeDurationInf);
		void Stop();

		// Ensure the scheduler can fit the given number of tasks
		// in its internal queues without making any additional
		// memory allocations.
		void Reserve(
			uint32_t aCount);

		// Post as is. The task can be configured using its Set
		// methods before the Post.
		void Post(
			Task* aTask);

		// You must prefer Deadline or Wait if you want an
		// infinite delay. To avoid unnecessary current time get.
		void PostDelay(
			Task* aTask,
			uint32_t aDelay);

		void PostDeadline(
			Task* aTask,
			uint64_t aDeadline);

		void PostWait(
			Task* aTask);

		template<typename Functor>
		void PostOneShot(
			Functor&& aFunc);

		// For statistics collection only.
		TaskSchedulerThread*const* GetThreads(
			uint32_t& aOutCount) const;

		static TaskScheduler& This();

	private:
		void PrivPost(
			Task* aTask);

		bool PrivSchedulerTake();
		bool PrivSchedule();
		void PrivSchedulerFree();

		bool PrivExecute(
			Task* aTask);

		void PrivWaitReady();
		bool PrivWaitReady(mg::box::TimeLimit aTimeLimit);

		void PrivSignalReady();

		bool PrivIsStopped();

		// Each task firstly goes to the front queue, from where
		// it is dispatched to the other queues by sched-thread.
		TaskSchedulerQueueFront myQueueFront;
		mg::box::Signal mySignalFront;

		// Front queue is very hot, it is updated by all threads -
		// external ones and workers. Should not invalidate the
		// cache of the less intensively updated fields. It is
		// separated from the fields below by thread list, which
		// is almost never updated or read. So can be used as a
		// barrier.
		uint8_t myPadding[MG_CACHE_LINE_SIZE];

		TaskSchedulerQueuePending myQueuePending;
		TaskSchedulerQueueWaiting myQueueWaiting;
		TaskSchedulerQueueReady myQueueReady;
		mg::box::Signal mySignalReady;
		// Threads try to execute not just all ready tasks in a row - periodically they
		// try to take care of the scheduling too. It helps to prevent the front-queue
		// from growing too much.
		const uint32_t myExecBatchSize;
		// The waiting and front tasks not always are handled by the scheduler all at
		// once. They are processed in limited batches. This is done to prevent
		// the bottleneck when the scheduling takes too long time while the other threads
		// are idle and the ready-queue is empty. For example, processing of a million of
		// front queue tasks might take ~100-200ms.
		uint32_t mySchedBatchSize;

		// The pending and waiting tasks must be dispatched
		// somehow to be moved to the ready queue. For that there
		// is a 'scheduling process'. It is not pinned to any
		// thread, but instead it migrates between worker threads
		// as soon as one of them does not have ready tasks to do.
		//
		// On one hand that allows to avoid having a separate
		// scheduler thread. On the other hand it still allows not
		// to use any locks for the objects used for the
		// scheduling only, such as the waiting queue.
		//
		// The worker thread, doing the scheduling right now, is
		// called 'sched-thread' throughout the code.
		mg::box::AtomicU64 mySchedulerRole;

		mg::box::Mutex myMutex;
		mg::box::Atomic<TaskSchedulerState> myState;
		std::vector<TaskSchedulerThread*> myThreads;
		TaskSchedulerVisitorList myVisitors;
		const std::string myName;

		static thread_local TaskScheduler* ourCurrent;

		friend class Task;
		friend class TaskSchedulerThread;
	};

	class TaskSchedulerThread
		: public mg::box::Thread
	{
	public:
		TaskSchedulerThread(
			const char* aSchedulerName,
			TaskScheduler* aScheduler);

		uint64_t StatPopExecuteCount();

		uint64_t StatPopScheduleCount();

	private:
		void Run() override;

		TaskScheduler* myScheduler;
		TaskSchedulerQueueReadyConsumer myConsumer;
		mg::box::AtomicU64 myExecuteCount;
		mg::box::AtomicU64 myScheduleCount;
	};

	struct TaskOneShot
		: public Task
	{
		template<typename Functor>
		TaskOneShot(
			Functor&& aFunc);

		void Execute(
			Task* aTask);

		TaskCallbackOneShot myCallback;
	};

	inline void
	TaskScheduler::PostDelay(
		Task* aTask,
		uint32_t aDelay)
	{
		aTask->SetDelay(aDelay);
		Post(aTask);
	}

	inline void
	TaskScheduler::PostDeadline(
		Task* aTask,
		uint64_t aDeadline)
	{
		aTask->SetDeadline(aDeadline);
		Post(aTask);
	}

	inline void
	TaskScheduler::PostWait(
		Task* aTask)
	{
		aTask->SetWait();
		Post(aTask);
	}

	inline TaskSchedulerThread*const*
	TaskScheduler::GetThreads(
		uint32_t& aOutCount) const
	{
		aOutCount = (uint32_t)myThreads.size();
		return myThreads.data();
	}

	inline TaskScheduler&
	TaskScheduler::This()
	{
		MG_DEV_ASSERT(ourCurrent != nullptr);
		return *ourCurrent;
	}

	inline uint64_t
	TaskSchedulerThread::StatPopExecuteCount()
	{
		return myExecuteCount.ExchangeRelaxed(0);
	}

	inline uint64_t
	TaskSchedulerThread::StatPopScheduleCount()
	{
		return myScheduleCount.ExchangeRelaxed(0);
	}

	template<typename Functor>
	inline void
	TaskScheduler::PostOneShot(
		Functor&& aFunc)
	{
		Post(new TaskOneShot(std::forward<Functor>(aFunc)));
	}

	template<typename Functor>
	inline
	TaskOneShot::TaskOneShot(
		Functor&& aFunc)
		: Task(std::bind(&TaskOneShot::Execute, this,
			std::placeholders::_1))
		, myCallback(std::forward<Functor>(aFunc))
	{
	}

}
}

#pragma once

#include "mg/aio/IOTask.h"
#include "mg/box/BinaryHeap.h"
#include "mg/box/ForwardList.h"
#include "mg/box/MultiConsumerQueue.h"
#include "mg/box/MultiProducerQueueIntrusive.h"
#include "mg/box/Signal.h"

#define MG_IOCORE_DEFAULT_THREAD_COUNT UINT32_MAX
#define MG_IOCORE_IOCP_BATCH 256
#define MG_IOCORE_EPOLL_BATCH 1024
#define MG_IOCORE_KQUEUE_BATCH 1024
#define MG_IOCORE_READY_BATCH 4096

namespace mg {
namespace aio {
	//
	// IOCore is an asynchronous multithreaded scheduler. It uses the TaskScheduler
	// algorithm to schedule network sockets as "tasks" and handle their IO operations as
	// events but not limited by just that.
	//
	// Each socket here is wrapped into a task object which allows to do IO operations,
	// get notified when they are done, set deadlines, and do own custom notifications as
	// explicit "wakeups".
	//
	// Like in TaskScheduler, the tasks are completely lock-free and are guaranteed to
	// always be executed in one thread at a time (but not always in the same thread).
	//
	// For deeper understanding it is recommended to wrap your mind around TaskScheduler
	// first. IOCore is almost the same except that each task also has an own socket.
	//
	// Another approach to comprehend IOCore is to think of Boost ASIO:
	// * IOCore is basically an io_context. But it already has worker threads
	//     inside of it;
	// * IOCore operates forcefully only on strands. It means that one socket/task can not
	//     be executed in more than one thread at a time ever.
	// * IOCore allows to run arbitrary tasks, not just real sockets like Boost does.
	//     Similar to TaskScheduler. But the overhead is higher, so TaskScheduler is
	//     preferable for non-networking code.
	// * IOCore has no callbacks. Each task has just one virtual method OnEvent(). No need
	//     to create lambdas or make binds for each read, write, or deadline and then
	//     figure out in which order they get executed and canceled.
	//
	// IOCore's goals are 1) speed, 2) simplicity, and 3) compactness. The decision to
	// avoid callbacks (on the lowest level) is greatly contributing to all 3 points. Even
	// though at first might look unusual to Boost fans.
	//

	class IOCoreWorker;

	using IOTaskForwardList = mg::box::ForwardList<IOTask>;

	// Front queue is populated by external threads and by worker threads. So it is
	// multi-producer. It is dispatched by sched-thread only, so it is single-consumer.
	// Each task goes firstly to the front queue. The worker threads automatically re-push
	// the tasks back to the front-queue unless the task is closed.
	using IOCoreFrontQueue = mg::box::MultiProducerQueueIntrusive<IOTask>;

	// Ready queue is populated only by the sched-thread and consumed by all workers when
	// dispatching tasks. Tasks move to the ready queue from
	// 1) the front queue if a task is due to execute right away.
	// 2) from the waiting queue if a task deadline is met (or it was woken up).
	// 3) from the waiting queue if the task got an IO event from the kernel.
	using IOCoreReadyQueue = mg::box::MultiConsumerQueue<IOTask>;

	// Waiting queue is only used by one worker thread at a time, so it has no concurrent
	// access and is therefore not protected with a lock. Tasks move from the front queue
	// to the waiting queue only if they have a deadline in the future and no IO events.
	using IOCoreWaitingQueue = mg::box::BinaryHeapMinIntrusive<IOTask>;

	// Pending queue is populated from the front queue for further processing. Normally
	// all its tasks are just handled right away, but if there is a particularly huge wave
	// of new tasks, then the scheduler might start buffering them in the pending queue
	// for processing in pieces of a limited size. It is accessed only by the
	// sched-thread.
	using IOCorePendingQueue = IOTaskForwardList;

	enum IOCoreState
	{
		IOCORE_STATE_RUNNING,
		IOCORE_STATE_STOPPING,
		IOCORE_STATE_STOPPED,
	};

	class IOCore
	{
	public:
		IOCore();
		~IOCore();

		void Start(
			uint32_t aThreadCount = MG_IOCORE_DEFAULT_THREAD_COUNT);

		// Return how many tasks there still are, if didn't get empty after the timeout.
		uint32_t WaitEmpty(
			mg::box::TimeLimit aTimeLimit = mg::box::theTimeDurationInf);

		// While stopped or not started the core still holds the queues and events. But
		// can't process them. Will do when started back.
		void Stop();

	private:
		void PrivPlatformCreate();
		void PrivPlatformDestroy();
		void PrivPlatformSignal();

		void PrivKernelRegister(
			IOTask* aTask);
#if !MG_IOCORE_USE_IOCP
		void PrivKernelUnregister(
			IOTask* aTask);
#endif

		bool PrivScheduleStart();
		bool PrivScheduleDo();
		void PrivScheduleEnd();

		void PrivSignalReady();
		void PrivWaitReady();
		bool PrivExecute(
			IOTask* aTask);

		void PrivPostStart(
			IOTask* aOutTask);
		void PrivPost(
			IOTask* aTask);
		void PrivRePost(
			IOTask* aTask);

#if MG_IOCORE_USE_IOCP
		HANDLE myNativeCore;
#elif MG_IOCORE_USE_EPOLL
		int myNativeCore;
		int myEventFd;
#elif MG_IOCORE_USE_KQUEUE
		int myNativeCore;
#endif
		IOCoreFrontQueue myFrontQueue;
		IOCorePendingQueue myPendingQueue;
		IOCoreWaitingQueue myWaitingQueue;
		IOCoreReadyQueue myReadyQueue;
		mg::box::Signal myReadySignal;
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
		// The pending and waiting tasks must be dispatched somehow to be moved to the
		// ready queue. For that there is a 'scheduling process'. It is not pinned to any
		// thread, but instead it migrates between worker threads as soon as one of them
		// does not have ready tasks to do.
		//
		// On one hand that allows to avoid having a separate scheduler thread. On the
		// other hand it still allows not to use any locks for the objects used for the
		// scheduling only.
		//
		// The architecture almost exactly repeats TaskScheduler.
		mg::box::AtomicBool myIsSchedulerWorking;
		mg::box::AtomicU32 myDescriptorCount;

		mg::box::Mutex myMutex;
		mg::box::Atomic<IOCoreState> myState;
		std::vector<IOCoreWorker*> myWorkers;

		friend IOCoreWorker;
		friend IOTask;
	};

}
}

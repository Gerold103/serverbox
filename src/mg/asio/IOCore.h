#pragma once

#include "mg/box/BinaryHeap.h"
#include "mg/box/ForwardList.h"
#include "mg/box/MultiConsumerQueue.h"
#include "mg/box/MultiProducerQueueIntrusive.h"
#include "mg/box/Signal.h"

#include "mg/asio/IOTask.h"

#define MG_IOCORE_DEFAULT_EMPTY_TIMEOUT 60000
#define MG_IOCORE_DEFAULT_THREAD_COUNT UINT32_MAX
#define MG_IOCORE_IOCP_BATCH 1024
#define MG_IOCORE_EPOLL_BATCH 1024
#define MG_IOCORE_READY_BATCH 4096

namespace mg {
namespace asio {
	//
	// IOCore is an asynchronous multithreaded scheduler of IO operations. It uses the
	// TaskScheduler algorithm to schedule network sockets as "tasks" and handle their IO
	// operations as events. One might also think of it in terms of boost ASIO where the
	// tasks are boost "strands".
	//
	// Each socket is wrapped into a task object which allows to do IO operations, get
	// notified when they are done, and do own custom notifications as explicit "wakeups".
	//
	// Like in TaskScheduler, the tasks are completely lock-free and are guaranteed to
	// always be executed in one thread at a time (but not always in the same thread).
	//
	// For deeper understanding it is recommended to wrap your mind around TaskScheduler
	// first. IOCore is almost the same except that each task also has an own socket.
	//

	class NetworkMonitor; 
	class IOCoreWorker;

	using IOTaskForwardList = mg::box::ForwardList<IOTask>;

	// Front-queue is used by worker threads to re-push a task back into the scheduler
	// unless it is closed. Also it is populated by external threads when they wakeup a
	// waiting task.
	using IOCoreFrontQueue = mg::box::MultiProducerQueueIntrusive<IOTask>;

	// Ready-queue is used by the scheduler to merge epoll and front-queue events.
	// Ready-queue is then dispatched by the worker threads.
	using IOCoreReadyQueue = mg::box::MultiConsumerQueue<IOTask>;

	// Waiting queue is used by the scheduler to keep track of tasks having a wakeup
	// deadline. They are stored here until expire or get events from anywhere.
	using IOCoreWaitingQueue = mg::box::BinaryHeapMinIntrusive<IOTask>;

	// Pending queue is populated from the front queue for further processing. Normally
	// all its tasks are just handled right away, but if there is a particularly huge wave
	// of new tasks, then the scheduler might start buffering them in the pending queue
	// for processing in pieces of a limited size. It is accessed only by the
	// sched-thread.
	using IOCorePendingQueue = IOTaskForwardList;

	class IOCore
	{
	public:
		IOCore();
		~IOCore();

		void Start(
			uint32_t aThreadCount = MG_IOCORE_DEFAULT_THREAD_COUNT);

		uint32_t WaitEmpty(
			uint32_t aTimeout = MG_IOCORE_DEFAULT_EMPTY_TIMEOUT);

		// While stopped or not started the core still accumulates the events. But can't
		// process them. Ideally, after stop nothing new should happen, because it is
		// not designed to be used during destruction.
		void Stop();

		// After post the task can't be deleted and its socket can't be closed manually.
		// It is owned by IOCore now, and must be closed only via CloseTask() method.
		void PostTask(
			IOTask* aOutTask,
			mg::net::Socket aSocket,
			IOSubscription* aSub);

		void PostTask(
			IOTask* aOutTask,
			IOServerSocket* aSocket,
			IOSubscription* aSub);

		// Socket can be omitted in case it is not ready for listening to kernel events,
		// or is not created yet. It is useful when socket creation can be heavy - need to
		// create socket handle, setup options, register in IOCP or epoll, start
		// connecting. Then the task can be posted empty, woken up, and in a worker thread
		// attached to socket.
		void PostTask(
			IOTask* aOutTask,
			IOSubscription* aSub);

		void AttachSocket(
			mg::net::Socket aSocket,
			IOTask* aTask);

		// Schedule close of a socket. It will be eventually closed when requests,
		// currently working with it, are finished.
		//
		// The task can't be just 'unregistered'. It can only be closed. Because on
		// Windows there is no a legal way to remove a socket from IOCP without closing
		// it.
		//
		// Note:
		// * Can be called multiple times and any time.
		// * When called even before the task is posted first time, the task is closed in
		//   a worker thread right after the post.
		void CloseTask(
			IOTask* aTask);

		// Make the task wakeup as soon as possible regardless of where it is right now.
		// Even if the task currently is being executed, it will be scheduled for another
		// execution. After this call the task owner can be sure the listener will be
		// invoked again unless the task is already closed for good.
		// Note:
		//
		// * Can be called multiples times and any time.
		// * If done after closure, then it is nop.
		void WakeupTask(
			IOTask* aTask);

	private: 
		void PrivPlatformCreate();
		void PrivPlatformDestroy();
		void PrivKernelRegister(
			IOTask* aTask);
		void PrivKernelUnregister(
			IOTask* aTask);
		void PrivPlatformSignal();
		bool PrivScheduleStart();
		bool PrivScheduleDo();
		void PrivScheduleEnd();
		void PrivSignalReady();
		void PrivWaitReady();
		bool PrivExecute(
			IOTask* aTask);
		void PrivPost(
			IOTask* aTask);
		void PrivRePost(
			IOTask* aTask);

#if IS_PLATFORM_WIN
		HANDLE myNativeCore;
#else
		int myNativeCore;
		int myEventFd;
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
		// The architecture is very similar to TaskScheduler.
		mg::box::AtomicBool myIsSchedulerWorking;
		mg::box::AtomicU32 myDescriptorCount;
		bool myIsRunning;
		mg::box::Mutex myMutex;
		std::vector<IOCoreWorker*> myWorkers;

		friend IOCoreWorker;
	};

}
}

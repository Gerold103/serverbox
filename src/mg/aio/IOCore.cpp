#include "IOCore.h"

#include "mg/box/Sysinfo.h"
#include "mg/box/Thread.h"

namespace mg {
namespace aio {

	using IOCoreReadyQueueConsumer = mg::box::MultiConsumerQueueConsumer<IOTask>;

	class IOCoreWorker
		: public mg::box::Thread
	{
	public:
		IOCoreWorker(
			IOCore& aCore);

	private:
		void Run() override;

		IOCore& myCore;
		IOCoreReadyQueueConsumer myConsumer;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	IOCore::IOCore()
#if MG_IOCORE_USE_IOCP
		: myNativeCore(nullptr)
#elif MG_IOCORE_USE_EPOLL
		: myNativeCore(-1)
		, myEventFd(-1)
#elif MG_IOCORE_USE_KQUEUE
		: myNativeCore(-1)
#endif
		, myReadyQueue(MG_IOCORE_READY_BATCH)
		, myExecBatchSize(MG_IOCORE_READY_BATCH)
		, mySchedBatchSize(0)
		, myIsSchedulerWorking(false)
		, myDescriptorCount(0)
		, myIsRunning(false)
	{
		PrivPlatformCreate();
	}

	IOCore::~IOCore()
	{
		MG_BOX_ASSERT(WaitEmpty() == 0);
		Stop();
		MG_BOX_ASSERT(myDescriptorCount.LoadRelaxed() == 0);
		MG_BOX_ASSERT(myPendingQueue.IsEmpty());
		MG_BOX_ASSERT(myFrontQueue.IsEmpty());
		MG_BOX_ASSERT(myWaitingQueue.Count() == 0);
		MG_BOX_ASSERT(myReadyQueue.Count() == 0);
		PrivPlatformDestroy();
	}

	void
	IOCore::Start(
		uint32_t aThreadCount)
	{
		if (aThreadCount == MG_IOCORE_DEFAULT_THREAD_COUNT)
			aThreadCount = mg::box::SysGetCPUCoreCount() * 2;
		MG_BOX_ASSERT(aThreadCount > 0 && aThreadCount < 256);
		myWorkers.reserve(aThreadCount);

		mg::box::MutexLock lock(myMutex);
		if (myIsRunning)
			return;
		mySchedBatchSize = myExecBatchSize * aThreadCount;
		for (uint32_t i = 0; i < aThreadCount; ++i)
			myWorkers.push_back(new IOCoreWorker(*this));
		myIsRunning = true;
	}

	uint32_t
	IOCore::WaitEmpty(
		mg::box::TimeLimit aTimeLimit)
	{
		uint32_t count = myDescriptorCount.LoadRelaxed();
		if (count == 0)
			return 0;
		uint64_t deadline = aTimeLimit.ToPointFromNow().myValue;
		while (mg::box::GetMilliseconds() < deadline)
		{
			mg::box::Sleep(10);
			count = myDescriptorCount.LoadRelaxed();
			if (count == 0)
				return 0;
		}
		return count;
	}

	void
	IOCore::Stop()
	{
		mg::box::MutexLock lock(myMutex);
		if (!myIsRunning)
			return;
		for (IOCoreWorker* w : myWorkers)
			w->Stop();
		PrivPlatformSignal();
		myReadySignal.Send();
		for (IOCoreWorker* w : myWorkers)
			w->StopAndDelete();

		myWorkers.resize(0);
		mySchedBatchSize = 0;
		myIsRunning = false;
	}

	bool
	IOCore::PrivScheduleStart()
	{
		return !myIsSchedulerWorking.ExchangeAcqRel(true);
	}

	void
	IOCore::PrivScheduleEnd()
	{
		bool old = myIsSchedulerWorking.ExchangeRelease(false);
		MG_DEV_ASSERT(old);
		// The signal is absolutely vital to have exactly here. If the signal would not be
		// emitted here, all the workers could block on ready tasks in their loops.
		//
		// It could easily happen like this. Assume there are 2 threads. One becomes the
		// scheduler, schedules one task, and is paused by the OS right *before* the
		// scheduler flag is freed.
		//
		// Then the second thread starts working, executes the ready task, fails to become
		// a sched (the flag is still taken), and goes to infinite sleep waiting for ready
		// tasks.
		//
		// Then the first thread continues execution, frees the flag, also fails to
		// execute anything and also goes to infinite sleep on the same condition.
		//
		// The signal below fixes it so if a thread finished the scheduling, it will
		// either go and execute tasks, or will go sleep, but the signal will wake it
		// again or some another thread to elect a new sched-thread.
		//
		// On the other hand it does not lead to busy-loop. If there are no new expired
		// tasks, eventually one worker will stuck in scheduler role on waiting for new
		// tasks, and other workers will sleep on waiting for ready tasks.
		myReadySignal.Send();
	}

	void
	IOCore::PrivSignalReady()
	{
		myReadySignal.Send();
	}

	void
	IOCore::PrivWaitReady()
	{
		myReadySignal.ReceiveBlocking();
	}

	bool
	IOCore::PrivExecute(
		IOTask* aTask)
	{
		if (aTask == nullptr)
			return false;

		MG_DEV_ASSERT(aTask->myIsInQueues);
		aTask->myIsInQueues = false;
		aTask->myNext = nullptr;
		// Make the task pending so as it would go back to sleep when returns back to
		// IOCore. Unless it is woken up again.
		IOTaskStatus oldStatus = IOTASK_STATUS_READY;
		aTask->myStatus.CmpExchgStrongRelaxed(oldStatus, IOTASK_STATUS_PENDING);
		MG_DEV_ASSERT(oldStatus != IOTASK_STATUS_PENDING);
		if (aTask->PrivExecute())
		{
			// Worker threads never ever can decide what to do with the task. Only the
			// scheduler can. So if the task is not supposed to end now (also decided by
			// the scheduler), the worker shall return it to the scheduler.
			// This is done because even while the task is in a worker thread, it still
			// can get new events from the scheduler from iocp/epoll. Necessity to check
			// both the events and the status here would complicate the task processing
			// too much, and would make the worker do a part of the scheduler's job.
			PrivPost(aTask);
		}
		else
		{
			myDescriptorCount.DecrementRelaxed();
		}
		return true;
	}

	void
	IOCore::PrivPostStart(
		IOTask* aOutTask)
	{
		MG_DEV_ASSERT(&aOutTask->myCore == this);
		if (aOutTask->HasSocket())
			PrivKernelRegister(aOutTask);
		myDescriptorCount.IncrementRelaxed();
		PrivPost(aOutTask);
	}

	void
	IOCore::PrivPost(
		IOTask* aTask)
	{
		MG_DEV_ASSERT(!aTask->myIsInQueues);
		aTask->myIsInQueues = true;
		PrivRePost(aTask);
	}

	void
	IOCore::PrivRePost(
		IOTask* aTask)
	{
		MG_DEV_ASSERT(aTask->myIsInQueues);
		if (myFrontQueue.Push(aTask))
			PrivPlatformSignal();
	}

	//////////////////////////////////////////////////////////////////////////////////////

	IOCoreWorker::IOCoreWorker(
		IOCore& aCore)
		: Thread("mgaio.iowrk")
		, myCore(aCore)
	{
		myConsumer.Attach(&myCore.myReadyQueue);
		Start();
	}

	void
	IOCoreWorker::Run()
	{
		uint32_t maxBatch = myCore.myExecBatchSize;
		uint32_t batch;
		IOCore& core = myCore;
		while (!StopRequested())
		{
			do
			{
				if (core.PrivScheduleStart())
				{
					while (!core.PrivScheduleDo())
					{
						if (StopRequested())
						{
							core.PrivScheduleEnd();
							goto end;
						}
					}
					core.PrivScheduleEnd();
				}
				batch = 0;
				while (core.PrivExecute(myConsumer.Pop()) && ++batch < maxBatch)
					continue;
			} while (batch == maxBatch);
			core.PrivWaitReady();
		}
	end:
		// Wakeup all the workers like a domino when they are terminating. Signal the cond
		// to let another worker know that the scheduler role is free. And then signal the
		// scheduler role to notice the exit. Need to do both. Otherwise another worker
		// might already by the scheduler right now and won't wakeup unless given a
		// personal signal into its kernel queue.
		core.PrivSignalReady();
		core.PrivPlatformSignal();
	}

}
}

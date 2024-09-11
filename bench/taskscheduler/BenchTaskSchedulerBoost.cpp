#include "Bench.h"

#include "mg/box/Atomic.h"
#include "mg/box/ConditionVariable.h"
#include "mg/box/ForwardList.h"
#include "mg/box/Mutex.h"
#include "mg/box/StringFunctions.h"
#include "mg/box/Thread.h"

#include <boost/asio.hpp>

namespace mg {
namespace bench {

	static boost::asio::io_context theCtx;

	class Task;
	class TaskScheduler;
	class TaskSchedulerThread;

	using TaskCallback = std::function<void(Task*)>;
	using TaskCallbackOneShot = std::function<void()>;

	class Task
	{
	public:
		Task();

		template<typename Functor>
		void SetCallback(
			Functor&& aFunc);

		bool ReceiveSignal();

		void PostWakeup();

		void PostSignal();

		Task* myNext;
	private:
		TaskCallback myCallback;

		boost::asio::strand<boost::asio::io_context::executor_type> myStrand;
		friend TaskScheduler;
		friend TaskSchedulerThread;
	};

	using TaskList = mg::box::ForwardList<Task>;

	// Trivial task scheduler takes a mutex lock on each task post and pop. The simplest
	// possible implementation and the most typical one. For the sake of further
	// simplicity and speed it doesn't support any features except just task execution:
	// no deadline, no wakeup, no signal, nothing else. Their addition using just mutex
	// locks would make this thing awfully slow, but the point is to compare the
	// feature-full original task scheduler vs this features-less one having some unfair
	// simplifications.
	//
	class TaskScheduler
	{
	public:
		TaskScheduler(
			const char* aName,
			uint32_t aThreadCount,
			uint32_t aSubQueueSize);

		~TaskScheduler();

		void Post(
			Task* aTask);

		template<typename Functor>
		void PostOneShot(
			Functor&& aFunc);

		void PostDelay(
			Task* aTask,
			uint32_t aDelay);

		void Reserve(
			uint32_t aCount);

		TaskSchedulerThread* const* GetThreads(
			uint32_t& aOutCount) const;

	private:
		std::vector<TaskSchedulerThread*> myWorkers;

		friend Task;
		friend TaskSchedulerThread;
	};

	class TaskSchedulerThread
		: private mg::box::Thread
	{
	public:
		TaskSchedulerThread(
			const char* aName,
			TaskScheduler* aScheduler);

		~TaskSchedulerThread() override;

		uint64_t StatPopExecuteCount();

		uint64_t StatPopScheduleCount();

	private:
		void Run() override;

		mg::box::AtomicU64 myExecuteCount;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	Task::Task()
		: myNext(nullptr)
		, myStrand(theCtx.get_executor())
	{
	}

	template<typename Functor>
	void
	Task::SetCallback(
		Functor&& aFunc)
	{
		myCallback = std::forward<Functor>(aFunc);
	}

	bool
	Task::ReceiveSignal()
	{
		MG_BOX_ASSERT(!"Not implemented");
		return false;
	}

	void
	Task::PostWakeup()
	{
		MG_BOX_ASSERT(!"Not implemented");
	}

	void
	Task::PostSignal()
	{
		MG_BOX_ASSERT(!"Not implemented");
	}

	TaskScheduler::TaskScheduler(
		const char* aName,
		uint32_t aThreadCount,
		uint32_t /*aSubQueueSize*/)
	{
		myWorkers.resize(aThreadCount);
		for (TaskSchedulerThread*& w : myWorkers)
			w = new TaskSchedulerThread(aName, this);
	}

	TaskScheduler::~TaskScheduler()
	{
		theCtx.stop();
		for (TaskSchedulerThread* w : myWorkers)
			delete w;
		theCtx.restart();
	}

	void
	TaskScheduler::Post(
		Task* aTask)
	{
		boost::asio::post(aTask->myStrand, [aTask]() {
			aTask->myCallback(aTask);
		});
	}

	template<typename Functor>
	void
	TaskScheduler::PostOneShot(
		Functor&& aFunc)
	{
		Task* t = new Task();
		TaskCallbackOneShot* cb = new TaskCallbackOneShot(
			std::forward<Functor>(aFunc));
		t->SetCallback([cb](Task* aTask) {
			(*cb)();
			delete cb;
			delete aTask;
		});
		Post(t);
	}

	void
	TaskScheduler::PostDelay(
		Task* /*aTask*/,
		uint32_t /*aDelay*/)
	{
		MG_BOX_ASSERT(!"Not implemented");
	}

	void
	TaskScheduler::Reserve(
		uint32_t /*aCount*/)
	{
	}

	TaskSchedulerThread* const*
	TaskScheduler::GetThreads(
		uint32_t& aOutCount) const
	{
		aOutCount = (uint32_t)myWorkers.size();
		return myWorkers.data();
	}

	TaskSchedulerThread::TaskSchedulerThread(
		const char* aName,
		TaskScheduler* /*aScheduler*/)
		: Thread(mg::box::StringFormat(
			"mgsb.tsksch%s", aName).c_str())
	{
		Start();
	}

	TaskSchedulerThread::~TaskSchedulerThread()
	{
		BlockingStop();
	}

	uint64_t
	TaskSchedulerThread::StatPopExecuteCount()
	{
		return myExecuteCount.ExchangeRelaxed(0);
	}

	uint64_t
	TaskSchedulerThread::StatPopScheduleCount()
	{
		return 0;
	}

	void
	TaskSchedulerThread::Run()
	{
		boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
			work(theCtx.get_executor());
		theCtx.run();
	}

}
}

#include "BenchTaskSchedulerTemplate.hpp"

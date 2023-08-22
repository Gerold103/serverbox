#include "BenchIOBoostCore.h"

#include "Bench.h"
#include "mg/box/ThreadFunc.h"

#include <boost/asio.hpp>

namespace mg {
namespace bench {
namespace io {

	static void
	BenchIOTCPClientIOCtxWorker(
		boost::asio::io_context& aCtx)
	{
		boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
			work(aCtx.get_executor());
		aCtx.run();
	}

	BoostIOCore::BoostIOCore()
		: myCtx(nullptr)
	{
	}

	BoostIOCore::~BoostIOCore()
	{
		Stop();
	}

	void
	BoostIOCore::Start(
		uint32_t aThreadCount)
	{
		MG_BOX_ASSERT(myCtx == nullptr);
		myCtx = new boost::asio::io_context();
		myWorkers.reserve(aThreadCount);
		for (uint32_t i = 0; i < aThreadCount; ++i)
		{
			mg::box::Thread *t = new mg::box::ThreadFunc("mgben.io",
				std::bind(BenchIOTCPClientIOCtxWorker, std::ref(*myCtx)));
			myWorkers.push_back(t);
			t->Start();
		}
	}

	void
	BoostIOCore::Stop()
	{
		if (myCtx == nullptr)
			return;
		myCtx->stop();
		for (mg::box::Thread* t : myWorkers)
			t->StopAndDelete();
		myWorkers.clear();
		delete myCtx;
		myCtx = nullptr;
	}

}
}
}

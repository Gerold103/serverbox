#include "BenchIOBoostCore.h"

#include "mg/box/ThreadFunc.h"

#include "Bench.h"

#if MG_BENCH_IO_HAS_BOOST
#include <boost/asio.hpp>
#endif

namespace mg {
namespace bench {

#if MG_BENCH_IO_HAS_BOOST
	static void
	BenchIOTCPClientIOCtxWorker(
		boost::asio::io_context& aCtx)
	{
		boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
			work(aCtx.get_executor());
		aCtx.run();
	}
#endif

	BoostIOCore::BoostIOCore()
		: myCtx(nullptr)
	{
	}

	void
	BoostIOCore::Start(
		uint32_t aThreadCount)
	{
		MG_BOX_ASSERT(myCtx == nullptr);
#if MG_BENCH_IO_HAS_BOOST
		myCtx = new boost::asio::io_context();
		myWorkers.reserve(aThreadCount);
		for (uint32_t i = 0; i < aThreadCount; ++i)
		{
			mg::box::Thread *t = new mg::box::ThreadFunc("mgben.io",
				std::bind(BenchIOTCPClientIOCtxWorker, std::ref(*myCtx)));
			myWorkers.push_back(t);
			t->Start();
		}
#else
		MG_UNUSED(aThreadCount);
		MG_BOX_ASSERT(!"Boost is not supported in this build");
#endif
	}

	BoostIOCore::~BoostIOCore()
	{
		// Stopping is not supported.
		MG_BOX_ASSERT(myCtx == nullptr);
	}

}
}

#pragma once

#include "mg/box/Thread.h"

#include <vector>

F_DECLARE_CLASS(boost, asio, io_context)

namespace mg {
namespace bench {
namespace io {

	class BoostIOCore
	{
	public:
		BoostIOCore();
		~BoostIOCore();

		void Start(
			uint32_t aThreadCount);

		void Stop();

		boost::asio::io_context& GetCtx() { return *myCtx; }

	private:
		boost::asio::io_context* myCtx;
		std::vector<mg::box::Thread*> myWorkers;
	};

}
}
}

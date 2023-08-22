#pragma once

#include "mg/box/Thread.h"

#include <vector>

F_DECLARE_CLASS(boost, asio, io_context)

namespace mg {
namespace bench {

	class BoostIOCore
	{
	public:
		BoostIOCore();
		~BoostIOCore();

		void Start(
			uint32_t aThreadCount);

		boost::asio::io_context& GetCtx() { return *myCtx; }

	private:
		boost::asio::io_context* myCtx;
		std::vector<mg::box::Thread*> myWorkers;
	};

}
}

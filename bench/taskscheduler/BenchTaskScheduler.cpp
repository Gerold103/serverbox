#include "Bench.h"

#include "mg/sch/TaskScheduler.h"

namespace mg {
namespace bench {

	using Task = mg::sch::Task;
	using TaskScheduler = mg::sch::TaskScheduler;
	using TaskSchedulerThread = mg::sch::TaskSchedulerThread;

}
}

#include "BenchTaskSchedulerTemplate.hpp"

#include "Bench.h"

#include "mg/sched/TaskScheduler.h"

namespace mg {
namespace bench {

	using Task = mg::sched::Task;
	using TaskScheduler = mg::sched::TaskScheduler;
	using TaskSchedulerThread = mg::sched::TaskSchedulerThread;

}
}

#include "BenchTaskSchedulerTemplate.hpp"

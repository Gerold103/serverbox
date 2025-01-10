#include "mg/sch/TaskScheduler.h"

#include <iostream>

//
// The most trivial example of TaskScheduler usage. A single shot task which just prints
// something and gets deleted.
//

int
main()
{
	mg::sch::TaskScheduler sched("tst",
		1, // Thread count.
		5  // Subqueue size.
	);
	sched.Post(new mg::sch::Task([&](mg::sch::Task *self) {
		std::cout << "Executed in scheduler!\n";
		delete self;
	}));
	return 0;
}

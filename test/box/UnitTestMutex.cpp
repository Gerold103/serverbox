#include "mg/box/Mutex.h"

#include "mg/box/ThreadFunc.h"
#include "mg/box/Time.h"

#include "UnitTest.h"

#include <vector>

namespace mg {
namespace unittests {
namespace box {

	static void
	UnitTestMutexBasic()
	{
		mg::box::Mutex mutex;
		uint32_t counter = 0;
		const uint32_t threadCount = 10;
		std::vector<mg::box::ThreadFunc*> threads;
		threads.reserve(threadCount);
		for (uint32_t i = 0; i < threadCount; ++i)
		{
			threads.push_back(new mg::box::ThreadFunc("mgtst", [&]() {
				uint64_t deadline = mg::box::GetMilliseconds() + 2000;
				uint64_t yield = 0;
				while (mg::box::GetMilliseconds() < deadline)
				{
					mg::box::MutexLock lock(mutex);
					TEST_CHECK(counter == 0);
					counter++;
					// Also test recursiveness.
					mutex.Lock();
					TEST_CHECK(counter == 1);
					counter--;
					mutex.Unlock();
					TEST_CHECK(counter == 0);
					if (++yield % 1000 == 0)
						mg::box::Sleep(1);
				}
			}));
		}
		for (mg::box::ThreadFunc* f : threads)
			f->Start();
		for (mg::box::ThreadFunc* f : threads)
			delete f;
		TEST_CHECK(counter == 0);
	}

	void
	UnitTestMutex()
	{
		TestSuiteGuard suite("Mutex");

		UnitTestMutexBasic();
	}

}
}
}

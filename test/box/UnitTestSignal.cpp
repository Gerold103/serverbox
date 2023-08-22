#include "mg/box/Signal.h"

#include "mg/box/ThreadFunc.h"

#include "UnitTest.h"

#include <vector>

namespace mg {
namespace unittests {
namespace box {

	static void
	UnitTestSignalBasic()
	{
		mg::box::Signal s;
		TEST_CHECK(!s.Receive());

		s.Send();
		TEST_CHECK(s.Receive());
		TEST_CHECK(!s.Receive());

		s.Send();
		s.ReceiveBlocking();
		TEST_CHECK(!s.Receive());

		s.Send();
		TEST_CHECK(s.ReceiveTimed(1000000));
		TEST_CHECK(!s.Receive());

		s.Send();
		TEST_CHECK(s.ReceiveTimed(0));
		TEST_CHECK(!s.Receive());

		TEST_CHECK(!s.ReceiveTimed(1));
	}

	static void
	UnitTestSignalStressSendAndReceive()
	{
		// The test checks that right after Receive() returns
		// success, it is safe to do with the signal anything.
		// Including deletion. Even if Send() in another thread is
		// not finished yet. Big number of signals is required so
		// as to catch the moment when the Send() thread is
		// interrupted right after setting the new state, but
		// before unlocking the mutex.
		const uint32_t count = 10000000;
		std::vector<mg::box::Signal*> signals;
		signals.reserve(count);
		for (uint32_t i = 0; i < count; ++i)
			signals.push_back(new mg::box::Signal());
		mg::box::ThreadFunc worker([&]() {
			uint32_t count = (uint32_t)signals.size();
			for (uint32_t i = 0; i < count; ++i)
				signals[i]->Send();
		});
		worker.Start();
		uint64_t yield = 0;
		for (uint32_t i = 0; i < count; ++i)
		{
			while (!signals[i]->Receive())
			{
				if (++yield % 10000 == 0)
					mg::box::Sleep(1);
			}
			delete signals[i];
		}
		worker.BlockingStop();
	}

	void
	UnitTestSignal()
	{
		TestSuiteGuard suite("Signal");

		UnitTestSignalBasic();
		UnitTestSignalStressSendAndReceive();
	}

}
}
}

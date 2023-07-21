#include "mg/box/Time.h"

#include "UnitTest.h"

namespace mg {
namespace unittests {
namespace box {
namespace time {

	static void
	UnitTestTimePoint()
	{
		TestCaseGuard guard("TimePoint");

		mg::box::TimePoint point1(0);
		TEST_CHECK(point1.myValue == 0);
		point1.myValue = 1;

		mg::box::TimePoint point2(point1);
		TEST_CHECK(point2.myValue == 1);

		TEST_CHECK(point1 <= point2);
		TEST_CHECK(point2 <= point1);
		TEST_CHECK(point1 <= point1);

		point2.myValue = 2;
		TEST_CHECK(point1 <= point2);
		TEST_CHECK(!(point2 <= point1));
		TEST_CHECK(point2 <= point2);

		point1 = point2;
		TEST_CHECK(point1.myValue == 2);

		TEST_CHECK(mg::box::theTimePointInf.myValue == MG_TIME_INFINITE);
	}

	static void
	UnitTestTimeDuration()
	{
		TestCaseGuard guard("TimeDuration");

		mg::box::TimeDuration d1((int32_t)1);
		TEST_CHECK(d1.myValue == 1);
		d1 = mg::box::TimeDuration((uint32_t)2);
		TEST_CHECK(d1.myValue == 2);
		d1 = mg::box::TimeDuration((int64_t)3);
		TEST_CHECK(d1.myValue == 3);
		d1 = mg::box::TimeDuration((uint64_t)4);
		TEST_CHECK(d1.myValue == 4);

		uint64_t ts = mg::box::GetMilliseconds();
		TEST_CHECK(d1.ToPointFromNow().myValue >= ts);

		d1.myValue = MG_TIME_INFINITE;
		TEST_CHECK(d1.ToPointFromNow().myValue == MG_TIME_INFINITE);

		TEST_CHECK(mg::box::theTimeDurationInf.myValue == MG_TIME_INFINITE);
	}

	static void
	UnitTestTimeLimit()
	{
		TestCaseGuard guard("TimeLimit");

		{
			mg::box::TimeLimit l;
			TEST_CHECK(l.myType == mg::box::TIME_LIMIT_NONE);
			TEST_CHECK(l.myPoint.myValue == 0);
		}
		// Point in the past.
		{
			uint64_t point = 123;
			mg::box::TimeLimit l{mg::box::TimePoint(point)};
			TEST_CHECK(l.myType == mg::box::TIME_LIMIT_POINT);
			TEST_CHECK(l.myPoint.myValue == point);
			TEST_CHECK(l.ToPointFromNow().myValue == point);
			TEST_CHECK(l.ToDurationFromNow().myValue == 0);
		}
		// Infinite point.
		{
			mg::box::TimeLimit l(mg::box::theTimePointInf);
			TEST_CHECK(l.myType == mg::box::TIME_LIMIT_POINT);
			TEST_CHECK(l.myPoint.myValue == MG_TIME_INFINITE);
			TEST_CHECK(l.ToPointFromNow().myValue == MG_TIME_INFINITE);
			TEST_CHECK(l.ToDurationFromNow().myValue == MG_TIME_INFINITE);
		}
		// Point in the future.
		{
			uint64_t offset = 50000;
			uint64_t point = mg::box::GetMilliseconds() + offset;
			mg::box::TimeLimit l{mg::box::TimePoint(point)};
			TEST_CHECK(l.myType == mg::box::TIME_LIMIT_POINT);
			TEST_CHECK(l.myPoint.myValue == point);
			TEST_CHECK(l.ToPointFromNow().myValue == point);
			uint64_t duration = l.ToDurationFromNow().myValue;
			// Divided by two to protect from when the lines above would be
			// stuck due to any reason on a slow PC.
			TEST_CHECK(duration <= offset && duration >= offset / 2);
		}
		// Normal duration.
		{
			uint64_t offset = 50000;
			mg::box::TimeLimit l{mg::box::TimeDuration(offset)};
			TEST_CHECK(l.myType == mg::box::TIME_LIMIT_DURATION);
			TEST_CHECK(l.myDuration.myValue == offset);
			uint64_t ts = mg::box::GetMilliseconds();
			uint64_t point = l.ToPointFromNow().myValue;
			TEST_CHECK(point >= ts + offset && point < ts + offset * 1.5);
		}
		// Infinite duration.
		{
			mg::box::TimeLimit l(mg::box::theTimeDurationInf);
			TEST_CHECK(l.myType == mg::box::TIME_LIMIT_DURATION);
			TEST_CHECK(l.myDuration.myValue == MG_TIME_INFINITE);
			TEST_CHECK(l.ToPointFromNow().myValue == MG_TIME_INFINITE);
			TEST_CHECK(l.ToDurationFromNow().myValue == MG_TIME_INFINITE);
		}
		// Reset.
		{
			mg::box::TimeLimit l;
			l.Reset();
			TEST_CHECK(l.myType == mg::box::TIME_LIMIT_NONE);
			TEST_CHECK(l.myPoint.myValue == 0);

			l = mg::box::TimeDuration(100);
			TEST_CHECK(l.myType == mg::box::TIME_LIMIT_DURATION);
			TEST_CHECK(l.myDuration.myValue == 100);
			l.Reset();
			TEST_CHECK(l.myType == mg::box::TIME_LIMIT_NONE);
			TEST_CHECK(l.myPoint.myValue == 0);

			l = mg::box::TimePoint(1000);
			TEST_CHECK(l.myType == mg::box::TIME_LIMIT_POINT);
			TEST_CHECK(l.myPoint.myValue == 1000);
			l.Reset();
			TEST_CHECK(l.myType == mg::box::TIME_LIMIT_NONE);
			TEST_CHECK(l.myPoint.myValue == 0);
		}
	}
}

	void
	UnitTestTime()
	{
		using namespace time;
		TestSuiteGuard suite("Time");

		UnitTestTimePoint();
		UnitTestTimeDuration();
		UnitTestTimeLimit();
	}

}
}
}

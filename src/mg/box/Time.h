#pragma once

#include "mg/box/Assert.h"

namespace mg {
namespace box {

	enum TimeLimitType
	{
		TIME_LIMIT_NONE,
		TIME_LIMIT_DURATION,
		TIME_LIMIT_POINT,
	};

	struct TimePoint
	{
		explicit constexpr TimePoint(
			uint64_t aValue) : myValue(aValue) {}

		constexpr bool operator<=(
			const TimePoint& aOther) const { return myValue <= aOther.myValue; }

		uint64_t myValue;
	};

	static constexpr TimePoint theTimePointInf{MG_TIME_INFINITE};

	struct TimeDuration
	{
		explicit constexpr TimeDuration(
			int32_t aValue) : myValue(aValue >= 0 ? aValue : 0) {}
		explicit constexpr TimeDuration(
			uint32_t aValue) : myValue(aValue) {}
		explicit constexpr TimeDuration(
			int64_t aValue) : myValue(aValue >= 0 ? aValue : 0) {}
		explicit constexpr TimeDuration(
			uint64_t aValue) : myValue(aValue) {}

		TimePoint ToPointFromNow() const;

		uint64_t myValue;
	};

	static constexpr TimeDuration theTimeDurationInf{MG_TIME_INFINITE};

	struct TimeLimit
	{
		constexpr TimeLimit() : myType(TIME_LIMIT_NONE), myPoint(0) {}
		constexpr TimeLimit(
			const TimePoint& aPoint) : myType(TIME_LIMIT_POINT), myPoint(aPoint) {}
		constexpr TimeLimit(
			const TimeDuration& aDuration) : myType(TIME_LIMIT_DURATION), myDuration(aDuration) {}

		TimePoint ToPointFromNow() const;
		TimeDuration ToDurationFromNow() const;
		void Reset() { myType = TIME_LIMIT_NONE; myPoint.myValue = 0; }

		TimeLimitType myType;
		union
		{
			TimePoint myPoint;
			TimeDuration myDuration;
		};
	};

	// Returns the number of milliseconds since some point in the
	// past. Monotonic, won't go back on clock changes.
	uint64_t GetMilliseconds();

	// Same but with higher precision.
	double GetMillisecondsPrecise();

	// Same but with nanoseconds precision (might be less precise, depending on platform).
	uint64_t GetNanoseconds();

	////////////////////////////////////////////////////////////////////////////

	inline TimePoint
	TimeDuration::ToPointFromNow() const
	{
		uint64_t ts = GetMilliseconds();
		if (ts >= UINT64_MAX - myValue)
			return theTimePointInf;
		return TimePoint(myValue + ts);
	}

	inline TimePoint
	TimeLimit::ToPointFromNow() const
	{
		if (myType == TIME_LIMIT_POINT)
			return myPoint;
		MG_DEV_ASSERT(myType == TIME_LIMIT_DURATION);
		return myDuration.ToPointFromNow();
	}

	inline TimeDuration
	TimeLimit::ToDurationFromNow() const
	{
		if (myType == TIME_LIMIT_DURATION)
			return myDuration;
		MG_DEV_ASSERT(myType == TIME_LIMIT_POINT);
		if (myPoint.myValue == MG_TIME_INFINITE)
			return theTimeDurationInf;
		uint64_t ts = GetMilliseconds();
		if (ts >= myPoint.myValue)
			return TimeDuration(0);
		return TimeDuration(myPoint.myValue - ts);
	}

}
}

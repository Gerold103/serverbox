#pragma once

#include "mg/box/Definitions.h"

#include <string>
#include <vector>

namespace mg {
namespace test {

	class CommandLine
	{
	public:
		CommandLine(
			int aArgc,
			const char* const* aArgv);

		bool IsPresent(
			const char* aName) const;

		const char* GetStr(
			const char* aName) const;

		uint64_t GetU64(
			const char* aName) const;

		uint32_t GetU32(
			const char* aName) const;

	private:
		struct Pair
		{
			std::string myKey;
			std::string myValue;
		};

		const Pair* PrivFind(
			const char* aName) const;

		const Pair& PrivGet(
			const char* aName) const;

		std::vector<Pair> myArgs;
	};

}
}
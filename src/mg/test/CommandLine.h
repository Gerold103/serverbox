#pragma once

#include "mg/box/Definitions.h"

#include <string>
#include <vector>

namespace mg {
namespace tst {

	class CommandLine
	{
	public:
		CommandLine(
			int aArgc,
			const char* const* aArgv);

		bool IsPresent(
			const char* aName) const;
		bool IsTrue(
			const char* aName) const;

		const char* GetStr(
			const char* aName) const;
		bool GetStr(
			const char* aName,
			const char*& aOutRes) const;

		uint64_t GetU64(
			const char* aName) const;
		bool GetU64(
			const char* aName,
			uint64_t& aOutRes) const;

		uint32_t GetU32(
			const char* aName) const;
		bool GetU32(
			const char* aName,
			uint32_t& aOutRes) const;

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

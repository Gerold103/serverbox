#include "CommandLine.h"

#include "mg/box/Assert.h"
#include "mg/box/StringFunctions.h"

namespace mg {
namespace tst {

	CommandLine::CommandLine(
		int aArgc,
		const char* const* aArgv)
	{
		int i = 0;
		while (i < aArgc)
		{
			const char* arg = aArgv[i];
			++i;
			if (arg[0] != '-')
				continue;
			++arg;
			const char* pos = arg;
			MG_BOX_ASSERT_F(*pos != 0, "Argument %d is empty", i);
			while (*pos != 0)
			{
				char c = *pos;
				MG_BOX_ASSERT_F(isalpha(c) || isdigit(c) || c == '-' || c == '_',
					"Argument %d has invalid chars", i);
				++pos;
			}
			uint32_t argi = (uint32_t)myArgs.size();
			myArgs.resize(argi + 1);
			Pair& p = myArgs[argi];
			p.myKey = arg;
			if (i < aArgc)
			{
				p.myValue = aArgv[i];
				++i;
			}
		}
	}

	bool
	CommandLine::IsPresent(
		const char* aName) const
	{
		return PrivFind(aName) != nullptr;
	}

	bool
	CommandLine::IsTrue(
		const char* aName) const
	{
		const Pair* p = PrivFind(aName);
		if (p == nullptr)
			return false;
		if (p->myValue == "0")
			return false;
		return mg::box::Strcasecmp(p->myValue.c_str(), "false") != 0;
	}

	const std::string&
	CommandLine::GetStr(
		const char* aName) const
	{
		return PrivGet(aName).myValue;
	}

	bool
	CommandLine::GetStr(
		const char* aName,
		std::string& aOutRes) const
	{
		const Pair* p = PrivFind(aName);
		if (p == nullptr)
			return false;
		aOutRes = p->myValue;
		return true;
	}

	uint64_t
	CommandLine::GetU64(
		const char* aName) const
	{
		uint64_t res = 0;
		MG_BOX_ASSERT_F(GetU64(aName, res), "Couldn't get arg %s as uint64", aName);
		return res;
	}

	bool
	CommandLine::GetU64(
		const char* aName,
		uint64_t& aOutRes) const
	{
		const Pair* p = PrivFind(aName);
		if (p == nullptr)
			return false;
		MG_BOX_ASSERT_F(mg::box::StringToNumber(p->myValue.c_str(), aOutRes),
			"Couldn't convert arg %s to uint64", aName);
		return true;
	}

	uint32_t
	CommandLine::GetU32(
		const char* aName) const
	{
		uint32_t res = 0;
		MG_BOX_ASSERT_F(GetU32(aName, res), "Couldn't get arg %s as uint32", aName);
		return res;
	}

	bool
	CommandLine::GetU32(
		const char* aName,
		uint32_t& aOutRes) const
	{
		const Pair* p = PrivFind(aName);
		if (p == nullptr)
			return false;
		MG_BOX_ASSERT_F(mg::box::StringToNumber(p->myValue.c_str(), aOutRes),
			"Couldn't convert arg %s to uint32", aName);
		return true;
	}

	uint16_t
	CommandLine::GetU16(
		const char* aName) const
	{
		uint16_t res = 0;
		MG_BOX_ASSERT_F(GetU16(aName, res), "Couldn't get arg %s as uint16", aName);
		return res;
	}

	bool
	CommandLine::GetU16(
		const char* aName,
		uint16_t& aOutRes) const
	{
		const Pair* p = PrivFind(aName);
		if (p == nullptr)
			return false;
		MG_BOX_ASSERT_F(mg::box::StringToNumber(p->myValue.c_str(), aOutRes),
			"Couldn't convert arg %s to uint16", aName);
		return true;
	}

	const CommandLine::Pair*
	CommandLine::PrivFind(
		const char* aName) const
	{
		for (const Pair& p : myArgs)
		{
			if (p.myKey == aName)
				return &p;
		}
		return nullptr;
	}

	const CommandLine::Pair&
	CommandLine::PrivGet(
		const char* aName) const
	{
		const Pair* p = PrivFind(aName);
		MG_BOX_ASSERT_F(p != nullptr, "Couldn't get arg %s", aName);
		return *p;
	}

}
}

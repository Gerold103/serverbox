#include "mg/box/StringFunctions.h"

#include "UnitTest.h"

namespace mg {
namespace unittests {
namespace box {

	static void
	UnitTestStringStrcasecmp()
	{
		TestCaseGuard guard("Strcasecmp()");

		TEST_CHECK(mg::box::Strcasecmp("", "") == 0);
		TEST_CHECK(mg::box::Strcasecmp("aBcDe", "AbCdE") == 0);
		TEST_CHECK(mg::box::Strcasecmp("aBcDe", "Ab") > 0);
		TEST_CHECK(mg::box::Strcasecmp("aBc", "AbCdE") < 0);
		TEST_CHECK(mg::box::Strcasecmp("a", "b") < 0);
		TEST_CHECK(mg::box::Strcasecmp("a", "B") < 0);
		TEST_CHECK(mg::box::Strcasecmp("A", "b") < 0);
		TEST_CHECK(mg::box::Strcasecmp("b", "a") > 0);
		TEST_CHECK(mg::box::Strcasecmp("b", "A") > 0);
		TEST_CHECK(mg::box::Strcasecmp("B", "a") > 0);
	}

	static void
	UnitTestStringTrim()
	{
		TestCaseGuard guard("StringTrim()");

		std::string str;
		mg::box::StringTrim(str);
		TEST_CHECK(str.empty());

		str = "a";
		mg::box::StringTrim(str);
		TEST_CHECK(str == "a");

		str = "a b";
		mg::box::StringTrim(str);
		TEST_CHECK(str == "a b");

		str = " \r \t \n ab c \t \r \n ";
		mg::box::StringTrim(str);
		TEST_CHECK(str == "ab c");
	}

	static void
	UnitTestStringFormat()
	{
		TestCaseGuard guard("StringFormat()");

		TEST_CHECK(mg::box::StringFormat("") == "");
		TEST_CHECK(mg::box::StringFormat("abc") == "abc");
		TEST_CHECK(mg::box::StringFormat(
			"a %d %s b c %u", 1, "str", 2U) == "a 1 str b c 2");
	}

	template <typename NumT, NumT aMax>
	static void
	UnitTestStringToNumberBasicTemplate()
	{
		NumT num = 0;
		TEST_CHECK(!mg::box::StringToNumber("", num));
		TEST_CHECK(!mg::box::StringToNumber(" ", num));
		TEST_CHECK(!mg::box::StringToNumber("abc", num));
		TEST_CHECK(!mg::box::StringToNumber("123abc", num));
		TEST_CHECK(!mg::box::StringToNumber("123  ", num));
		TEST_CHECK(!mg::box::StringToNumber(
			"999999999999999999999999999999999999999999999", num));

		num = 1;
		TEST_CHECK(mg::box::StringToNumber("0", num) && num == 0);
		num = 0;
		TEST_CHECK(mg::box::StringToNumber("1", num) && num == 1);
		num = 0;
		TEST_CHECK(mg::box::StringToNumber("123", num) && num == 123);
		num = 0;
		TEST_CHECK(mg::box::StringToNumber(" \t \n \r 200", num) && num == 200);

		uint64_t max = aMax;
		std::string maxStr;
		if (max < UINT64_MAX)
			maxStr = mg::box::StringFormat("%llu", (unsigned long long)max + 1);
		else
			maxStr = mg::box::StringFormat("%llu0", (unsigned long long)UINT64_MAX);
		num = 0;
		TEST_CHECK(!mg::box::StringToNumber(maxStr.c_str(), num));
	}

	template <typename NumT, NumT aMax>
	static void
	UnitTestStringToNumberUnsignedTemplate(
		const char* aTypeName)
	{
		TestCaseGuard guard("StringToNumber(%s)", aTypeName);

		UnitTestStringToNumberBasicTemplate<NumT, aMax>();
		NumT num = 0;
		TEST_CHECK(!mg::box::StringToNumber("-1", num));
	}

	static void
	UnitTestStringToNumberUnsigned()
	{
		UnitTestStringToNumberUnsignedTemplate<uint16_t, UINT16_MAX>("uint16");
		UnitTestStringToNumberUnsignedTemplate<uint32_t, UINT32_MAX>("uint32");
		UnitTestStringToNumberUnsignedTemplate<uint64_t, UINT64_MAX>("uint64");
	}

	void
	UnitTestString()
	{
		TestSuiteGuard suite("String");

		UnitTestStringStrcasecmp();
		UnitTestStringTrim();
		UnitTestStringFormat();
		UnitTestStringToNumberUnsigned();
	}

}
}
}

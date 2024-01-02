#include "mg/net/URL.h"

#include "UnitTest.h"

namespace mg {
namespace unittests {
namespace net {
namespace url {

	static void
	UnitTestURLBasic()
	{
		TestCaseGuard guard("Basic");

		mg::net::URL url = mg::net::URLParse("127.0.0.1");
		TEST_CHECK(url.myHost == "127.0.0.1");
		TEST_CHECK(url.myProtocol.empty());
		TEST_CHECK(url.myPort == 0);
		TEST_CHECK(url.myTarget.empty());

		url = mg::net::URLParse("127.0.0.1:443");
		TEST_CHECK(url.myHost == "127.0.0.1");
		TEST_CHECK(url.myProtocol.empty());
		TEST_CHECK(url.myPort == 443);
		TEST_CHECK(url.myTarget.empty());

		url = mg::net::URLParse("www.google.com");
		TEST_CHECK(url.myHost == "www.google.com");
		TEST_CHECK(url.myProtocol.empty());
		TEST_CHECK(url.myPort == 0);
		TEST_CHECK(url.myTarget.empty());

		url = mg::net::URLParse("https://www.google.com");
		TEST_CHECK(url.myHost == "www.google.com");
		TEST_CHECK(url.myProtocol == "https");
		TEST_CHECK(url.myPort == 0);
		TEST_CHECK(url.myTarget.empty());

		url = mg::net::URLParse("https://www.google.com:100");
		TEST_CHECK(url.myHost == "www.google.com");
		TEST_CHECK(url.myProtocol == "https");
		TEST_CHECK(url.myPort == 100);
		TEST_CHECK(url.myTarget.empty());

		url = mg::net::URLParse("https://www.google.com/");
		TEST_CHECK(url.myHost == "www.google.com");
		TEST_CHECK(url.myProtocol == "https");
		TEST_CHECK(url.myPort == 0);
		TEST_CHECK(url.myTarget == "/");

		url = mg::net::URLParse("http://www.google.com:200/");
		TEST_CHECK(url.myHost == "www.google.com");
		TEST_CHECK(url.myProtocol == "http");
		TEST_CHECK(url.myPort == 200);
		TEST_CHECK(url.myTarget == "/");

		url = mg::net::URLParse("http://www.google.com/path/?arg1=value1&arg2=value2");
		TEST_CHECK(url.myHost == "www.google.com");
		TEST_CHECK(url.myProtocol == "http");
		TEST_CHECK(url.myPort == 0);
		TEST_CHECK(url.myTarget == "/path/?arg1=value1&arg2=value2");
	}
}

	void
	UnitTestURL()
	{
		using namespace url;
		TestSuiteGuard suite("URL");

		UnitTestURLBasic();
	}

}
}
}

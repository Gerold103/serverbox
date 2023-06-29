#include "mg/net/Host.h"

#include "UnitTest.h"

namespace mg {
namespace unittests {
namespace net {

	static_assert(mg::net::ADDR_FAMILY_IPV4 == AF_INET, "Addr family");
	static_assert(sizeof(mg::net::Host::myAddrIn) == sizeof(struct sockaddr_in), "Addr IPv4 size");
	static_assert(mg::net::ADDR_FAMILY_IPV6 == AF_INET6, "Addr family");
	static_assert(sizeof(mg::net::Host::myAddrIn6) == sizeof(struct sockaddr_in6), "Addr IPv6 size");

	static const char* theIPV6v1 = "4da3:3ba5:c2ae:ec29:fce2:1b68:46db:3caa";
	static const char* theIPV6v2 = "3eb7:96dc:dc72:d9d0:d8da:b5ac:b59c:97e9";

	static void
	UnitTestHostConstructorIPV4()
	{
		TestCaseGuard guard("Constructor(IPV4)");
		// Empty.
		{
			mg::net::Host host;
			TEST_CHECK(!host.IsSet());
			TEST_CHECK(host.myAddr.sa_family == mg::net::ADDR_FAMILY_NONE);
		}
		// From sockaddr.
		{
			mg::net::SockaddrIn addr;
			mg::net::SockaddrInCreate(addr);
			mg::net::Host host((mg::net::Sockaddr*)&addr);
			TEST_CHECK(host.IsSet());
			TEST_CHECK(host.myAddr.sa_family == mg::net::ADDR_FAMILY_IPV4);
		}
		// From string.
		{
			{
				mg::net::Host host("");
				TEST_CHECK(!host.IsSet());
			}
			{
				mg::net::Host host("bad address");
				TEST_CHECK(!host.IsSet());
			}
			{
				mg::net::Host host("127.0.0.1");
				TEST_CHECK(host.IsSet());
				TEST_CHECK(host == mg::net::HostMakeLocalIPV4(0));
			}
			{
				mg::net::Host host("127.0.0.1:1234");
				TEST_CHECK(host.IsSet());
				TEST_CHECK(host == mg::net::HostMakeLocalIPV4(1234));
			}
		}
		// From std::string.
		{
			mg::net::Host host(std::string("127.0.0.1:1234"));
			TEST_CHECK(host.IsSet());
			TEST_CHECK(host == mg::net::HostMakeLocalIPV4(1234));
		}
	}

	static void
	UnitTestHostConstructorIPV6()
	{
		TestCaseGuard guard("Constructor(IPV6)");
		// From sockaddr.
		{
			mg::net::SockaddrIn6 addr;
			mg::net::SockaddrIn6Create(addr);
			mg::net::Host host((mg::net::Sockaddr*)&addr);
			TEST_CHECK(host.IsSet());
			TEST_CHECK(host.myAddr.sa_family == mg::net::ADDR_FAMILY_IPV6);
		}
		// From string.
		{
			{
				mg::net::Host host("::1");
				TEST_CHECK(host.IsSet());
				TEST_CHECK(host == mg::net::HostMakeLocalIPV6(0));
			}
			{
				mg::net::Host host("[::1]:1234");
				TEST_CHECK(host.IsSet());
				TEST_CHECK(host == mg::net::HostMakeLocalIPV6(1234));
			}
		}
	}

	static void
	UnitTestHostSetIPV4()
	{
		TestCaseGuard guard("Set(IPV4)");

		mg::net::Host host;
		mg::net::Host hostipv4("192.168.0.1:12345");
		// From sockaddr.
		host.Set((mg::net::Sockaddr*)&hostipv4.myAddrIn);
		TEST_CHECK(host == hostipv4);

		// From string.
		TEST_CHECK(!host.Set(""));
		TEST_CHECK(!host.Set("bad address"));
		TEST_CHECK(host == hostipv4);

		TEST_CHECK(host.Set("192.168.0.1:12345"));
		TEST_CHECK(host == hostipv4);

		// From std::string.
		TEST_CHECK(!host.Set(std::string("")));
		TEST_CHECK(host == hostipv4);
		TEST_CHECK(host.Set(std::string("192.168.0.1")));
		TEST_CHECK(host.IsEqualNoPort(hostipv4));
		//
		// Various IP addresses.
		//
		// Only IP.
		host.Set("127.0.0.1");
		TEST_CHECK(host.IsIPV4());
		TEST_CHECK(host.ToString() == "127.0.0.1:0");

		TEST_CHECK(host.Set("192.168.0.1"));
		TEST_CHECK(host.IsIPV4());
		TEST_CHECK(host.ToString() == "192.168.0.1:0");

		TEST_CHECK(host.Set("255.255.255.255"));
		TEST_CHECK(host.IsIPV4());
		TEST_CHECK(host.ToString() == "255.255.255.255:0");

		TEST_CHECK(host.Set("0.0.0.0"));
		TEST_CHECK(host.IsIPV4());
		TEST_CHECK(host.ToString() == "0.0.0.0:0");
		TEST_CHECK(host == mg::net::HostMakeAllIPV4(0));

		// IP + port.
		TEST_CHECK(host.Set("127.0.0.1:123"));
		TEST_CHECK(host.IsIPV4());
		TEST_CHECK(host.ToString() == "127.0.0.1:123");

		TEST_CHECK(host.Set("192.168.0.1:4567"));
		TEST_CHECK(host.IsIPV4());
		TEST_CHECK(host.ToString() == "192.168.0.1:4567");

		TEST_CHECK(host.Set("255.255.255.255:0"));
		TEST_CHECK(host.IsIPV4());
		TEST_CHECK(host.ToString() == "255.255.255.255:0");

		TEST_CHECK(host.Set("192.168.0.1:65535"));
		TEST_CHECK(host.IsIPV4());
		TEST_CHECK(host.ToString() == "192.168.0.1:65535");

		TEST_CHECK(host.Set("0.0.0.0:543"));
		TEST_CHECK(host.IsIPV4());
		TEST_CHECK(host.ToString() == "0.0.0.0:543");
		TEST_CHECK(host == mg::net::HostMakeAllIPV4(543));

		// Malformed. Also ensure the errors won't change the original value.
		TEST_CHECK(host.Set("192.168.1.2:456"));
		TEST_CHECK(!host.Set("255.255.255.2555"));
#if !IS_PLATFORM_APPLE
		TEST_CHECK(!host.Set("192.168.000.1"));
		TEST_CHECK(!host.Set("192.168.0000.1"));
#endif
		TEST_CHECK(!host.Set("127.0.0.1:"));
		TEST_CHECK(!host.Set("192.168.0.1:65536"));
		TEST_CHECK(!host.Set("192.168.0.1:-1"));
		TEST_CHECK(!host.Set("bad"));
		TEST_CHECK(!host.Set("127. 0. 0. 1:123"));
		TEST_CHECK(!host.Set(" 127.0.0.1:123"));
		TEST_CHECK(!host.Set("127.0.0.1:123 "));
		// The original value is kept intact.
		TEST_CHECK(host.ToString() == "192.168.1.2:456");
	}

	static void
	UnitTestHostSetIPV6()
	{
		TestCaseGuard guard("Set(IPV6)");

		mg::net::Host host;
		mg::net::Host hostipv6(mg::box::StringFormat("[%s]:12345", theIPV6v1));
		// From sockaddr.
		host.Set((mg::net::Sockaddr*)&hostipv6.myAddrIn6);
		TEST_CHECK(host == hostipv6);

		// From string.
		TEST_CHECK(!host.Set(""));
		TEST_CHECK(!host.Set("bad address"));
		TEST_CHECK(host == hostipv6);

		TEST_CHECK(host.Set(mg::box::StringFormat("[%s]:12345", theIPV6v1)));
		TEST_CHECK(host == hostipv6);

		// From std::string.
		TEST_CHECK(!host.Set(std::string("")));
		TEST_CHECK(host == hostipv6);
		TEST_CHECK(host.Set(std::string(theIPV6v1)));
		TEST_CHECK(host.IsEqualNoPort(hostipv6));
		//
		// Various IP addresses.
		//
		// Only IP.
		host.Set(theIPV6v1);
		TEST_CHECK(host.IsIPV6());
		TEST_CHECK(host.ToString() == mg::box::StringFormat("[%s]:0", theIPV6v1));

		TEST_CHECK(host.Set(theIPV6v2));
		TEST_CHECK(host.IsIPV6());
		TEST_CHECK(host.ToString() == mg::box::StringFormat("[%s]:0", theIPV6v2));

		TEST_CHECK(host.Set("::1"));
		TEST_CHECK(host.IsIPV6());
		TEST_CHECK(host.ToString() == "[::1]:0");

		TEST_CHECK(host.Set("::"));
		TEST_CHECK(host.IsIPV6());
		TEST_CHECK(host.ToString() == "[::]:0");
		TEST_CHECK(host == mg::net::HostMakeAllIPV6(0));

		// Leading zeros in some sections.
		TEST_CHECK(host.Set( "0da3:3ba5:00ae:ec29:fce2:1b68:06db:3caa"));
		TEST_CHECK(host.IsIPV6());
		TEST_CHECK(host.ToString() == "[da3:3ba5:ae:ec29:fce2:1b68:6db:3caa]:0");

		// IP + port.
		TEST_CHECK(host.Set(mg::box::StringFormat("[%s]:123", theIPV6v1)));
		TEST_CHECK(host.IsIPV6());
		TEST_CHECK(host.ToString() == mg::box::StringFormat("[%s]:123", theIPV6v1));

		TEST_CHECK(host.Set(mg::box::StringFormat("[%s]:4567", theIPV6v2)));
		TEST_CHECK(host.IsIPV6());
		TEST_CHECK(host.ToString() == mg::box::StringFormat("[%s]:4567", theIPV6v2));

		TEST_CHECK(host.Set("[::1]:0"));
		TEST_CHECK(host.IsIPV6());
		TEST_CHECK(host.ToString() == "[::1]:0");

		TEST_CHECK(host.Set(mg::box::StringFormat("[%s]:65535", theIPV6v2)));
		TEST_CHECK(host.IsIPV6());
		TEST_CHECK(host.ToString() == mg::box::StringFormat("[%s]:65535", theIPV6v2));

		TEST_CHECK(host.Set("[::]:5632"));
		TEST_CHECK(host.IsIPV6());
		TEST_CHECK(host.ToString() == "[::]:5632");
		TEST_CHECK(host == mg::net::HostMakeAllIPV6(5632));

		// Malformed. Also ensure the errors won't change the original value.
		TEST_CHECK(host.Set(mg::box::StringFormat("[%s]:123", theIPV6v1)));
		//                          Excess digit here V
		TEST_CHECK(!host.Set("4870:7fa2:2613:1169:17cb9:e172:4d2:6cf0"));
		TEST_CHECK(!host.Set("::::::::::::100"));
		TEST_CHECK(!host.Set("100::::::::::::"));
		TEST_CHECK(!host.Set(mg::box::StringFormat("[%s]:", theIPV6v1)));
		TEST_CHECK(!host.Set(mg::box::StringFormat("[%s]:65536", theIPV6v1)));
		TEST_CHECK(!host.Set(mg::box::StringFormat("[%s]:-1", theIPV6v1)));
		TEST_CHECK(!host.Set("bad"));
		TEST_CHECK(!host.Set("bda: 738f:3de1:d416 :daa4:4a76: 1a42:9e41"));
		TEST_CHECK(!host.Set("[bda:738f:3de1:d416:daa4:4a76:1a42:9e41] :123"));
		TEST_CHECK(!host.Set(" [bda:738f:3de1:d416:daa4:4a76:1a42:9e41]:123"));
		TEST_CHECK(!host.Set("[bda:738f:3de1:d416:daa4:4a76:1a42:9e41]:123 "));
		// The original value is kept intact.
		TEST_CHECK(host.ToString() == mg::box::StringFormat("[%s]:123", theIPV6v1));
	}

	static void
	UnitTestHostClear()
	{
		TestCaseGuard guard("Clear()");

		mg::net::Host host;
		TEST_CHECK(!host.IsSet());
		host.Clear();
		TEST_CHECK(!host.IsSet());

		host = mg::net::HostMakeLocalIPV4(123);
		TEST_CHECK(host.IsSet());
		host.Clear();
		TEST_CHECK(!host.IsSet());

		host = mg::net::HostMakeLocalIPV6(123);
		TEST_CHECK(host.IsSet());
		host.Clear();
		TEST_CHECK(!host.IsSet());
	}

	static void
	UnitTestHostSetPortIPV4()
	{
		TestCaseGuard guard("SetPort(IPV4)");

		mg::net::Host host("192.168.0.1:12345");
		mg::net::Host orig = host;
		orig.SetPort(0);

		TEST_CHECK(host.GetPort() == 12345);
		host.SetPort(456);
		TEST_CHECK(host.GetPort() == 456);
 		TEST_CHECK(host.IsEqualNoPort(orig));
	}

	static void
	UnitTestHostSetPortIPV6()
	{
		TestCaseGuard guard("SetPort(IPV6)");

		mg::net::Host host(mg::box::StringFormat("[%s]:12345", theIPV6v1));
		mg::net::Host orig = host;
		orig.SetPort(0);

		TEST_CHECK(host.GetPort() == 12345);
		host.SetPort(456);
		TEST_CHECK(host.GetPort() == 456);
		TEST_CHECK(host.IsEqualNoPort(orig));
	}

	static void
	UnitTestHostGetSockaddrSize()
	{
		TestCaseGuard guard("GetSockaddrSize()");

		TEST_CHECK(mg::net::HostMakeLocalIPV4(123).GetSockaddrSize() ==
			sizeof(mg::net::Host::myAddrIn));
		TEST_CHECK(mg::net::HostMakeLocalIPV6(123).GetSockaddrSize() ==
			sizeof(mg::net::Host::myAddrIn6));
	}

	static void
	UnitTestHostGetPort()
	{
		TestCaseGuard guard("GetPort(IPV4)");

		mg::net::Host host = mg::net::HostMakeLocalIPV4(0);
		TEST_CHECK(host.GetPort() == 0);
		host.SetPort(123);
		TEST_CHECK(host.GetPort() == 123);

		host = mg::net::HostMakeLocalIPV6(0);
		TEST_CHECK(host.GetPort() == 0);
		host.SetPort(123);
		TEST_CHECK(host.GetPort() == 123);
	}

	static void
	UnitTestHostIPVersionCheck()
	{
		TestCaseGuard guard("IP version check");

		mg::net::Host host = mg::net::HostMakeLocalIPV4(0);
		TEST_CHECK(host.IsIPV4());
		TEST_CHECK(!host.IsIPV6());

		host = mg::net::HostMakeLocalIPV6(0);
		TEST_CHECK(!host.IsIPV4());
		TEST_CHECK(host.IsIPV6());
	}

	static void
	UnitTestHostIsSet()
	{
		TestCaseGuard guard("IsSet()");

		mg::net::Host host;
		TEST_CHECK(!host.IsSet());
		TEST_CHECK(host.myAddr.sa_family == mg::net::ADDR_FAMILY_NONE);

		host = mg::net::HostMakeLocalIPV4(0);
		TEST_CHECK(host.IsSet());
		TEST_CHECK(host.myAddr.sa_family == mg::net::ADDR_FAMILY_IPV4);

		host = mg::net::HostMakeLocalIPV6(0);
		TEST_CHECK(host.IsSet());
		TEST_CHECK(host.myAddr.sa_family == mg::net::ADDR_FAMILY_IPV6);
	}

	static void
	UnitTestHostIsEqualNoPortIPV4()
	{
		TestCaseGuard guard("IsEqualNoPort(IPV4)");

		mg::net::Host host1("192.168.0.1:1234");
		mg::net::Host host2(host1);
		TEST_CHECK(host1.IsEqualNoPort(host2));
		TEST_CHECK(host2.IsEqualNoPort(host1));

		host2.SetPort(456);
		TEST_CHECK(host1.IsEqualNoPort(host2));
		TEST_CHECK(host2.IsEqualNoPort(host1));

		host2.Set("192.168.0.2");
		TEST_CHECK(!host1.IsEqualNoPort(host2));
		TEST_CHECK(!host2.IsEqualNoPort(host1));

		// IPV4 vs IPV6.
		host2.Set(theIPV6v1);
		TEST_CHECK(!host1.IsEqualNoPort(host2));
		TEST_CHECK(!host2.IsEqualNoPort(host1));

		// IPV4 vs empty.
		host2.Clear();
		TEST_CHECK(!host1.IsEqualNoPort(host2));
		TEST_CHECK(!host2.IsEqualNoPort(host1));

		// Empty vs empty.
		mg::net::Host empty;
		TEST_CHECK(empty.IsEqualNoPort(host2));
		TEST_CHECK(host2.IsEqualNoPort(empty));
		//
		// Comparison and hash should only use standard fields. Omit padding between them
		// and platform-specific fields.
		//
		mg::net::Host orig = host1;
		TEST_CHECK(host1.IsIPV4());
		memset(&host1.myAddrIn, '#', sizeof(host1.myAddrIn));
		host1.myAddrIn.sin_family = orig.myAddrIn.sin_family;
		host1.myAddrIn.sin_addr = orig.myAddrIn.sin_addr;
		host1.myAddrIn.sin_port = orig.myAddrIn.sin_port;

		host2 = orig;
		memset(&host2.myAddrIn, '?', sizeof(host2.myAddrIn));
		host2.myAddrIn.sin_family = orig.myAddrIn.sin_family;
		host2.myAddrIn.sin_addr = orig.myAddrIn.sin_addr;
		host2.myAddrIn.sin_port = orig.myAddrIn.sin_port;

		TEST_CHECK(host1.IsEqualNoPort(host2));
		TEST_CHECK(host2.IsEqualNoPort(host1));
	}

	static void
	UnitTestHostIsEqualNoPortIPV6()
	{
		TestCaseGuard guard("IsEqualNoPort(IPV6)");

		mg::net::Host host1(mg::box::StringFormat("[%s]:1234", theIPV6v1));
		mg::net::Host host2(host1);
		TEST_CHECK(host1.IsEqualNoPort(host2));
		TEST_CHECK(host2.IsEqualNoPort(host1));

		host2.SetPort(456);
		TEST_CHECK(host1.IsEqualNoPort(host2));
		TEST_CHECK(host2.IsEqualNoPort(host1));

		host2.Set(theIPV6v2);
		TEST_CHECK(!host1.IsEqualNoPort(host2));
		TEST_CHECK(!host2.IsEqualNoPort(host1));

		// IPV6 vs IPV4.
		host2.Set("192.168.0.1");
		TEST_CHECK(!host1.IsEqualNoPort(host2));
		TEST_CHECK(!host2.IsEqualNoPort(host1));

		// IPV6 vs empty.
		host2.Clear();
		TEST_CHECK(!host1.IsEqualNoPort(host2));
		TEST_CHECK(!host2.IsEqualNoPort(host1));

		//
		// Comparison and hash should only use standard fields. Omit padding between them
		// and platform-specific fields.
		//
		mg::net::Host orig = host1;
		TEST_CHECK(host1.IsIPV6());
		memset(&host1.myAddrIn6, '#', sizeof(host1.myAddrIn6));
		host1.myAddrIn6.sin6_family = orig.myAddrIn6.sin6_family;
		host1.myAddrIn6.sin6_addr = orig.myAddrIn6.sin6_addr;
		host1.myAddrIn6.sin6_port = orig.myAddrIn6.sin6_port;

		host2 = orig;
		memset(&host2.myAddrIn6, '?', sizeof(host2.myAddrIn6));
		host2.myAddrIn6.sin6_family = orig.myAddrIn6.sin6_family;
		host2.myAddrIn6.sin6_addr = orig.myAddrIn6.sin6_addr;
		host2.myAddrIn6.sin6_port = orig.myAddrIn6.sin6_port;

		TEST_CHECK(host1.IsEqualNoPort(host2));
		TEST_CHECK(host2.IsEqualNoPort(host1));
	}

	static void
	UnitTestHostToString()
	{
		TestCaseGuard guard("ToString()");

		mg::net::Host host = mg::net::HostMakeLocalIPV4(123);
		TEST_CHECK(host.ToString() == "127.0.0.1:123");

		host.SetPort(0);
		TEST_CHECK(host.ToString() == "127.0.0.1:0");

		host = mg::net::HostMakeLocalIPV6(123);
		TEST_CHECK(host.ToString() == "[::1]:123");

		host.SetPort(0);
		TEST_CHECK(host.ToString() == "[::1]:0");
	}

	static void
	UnitTestHostToStringNoPort()
	{
		TestCaseGuard guard("ToStringNoPort()");

		mg::net::Host host = mg::net::HostMakeLocalIPV4(123);
		TEST_CHECK(host.ToStringNoPort() == "127.0.0.1");

		host.SetPort(0);
		TEST_CHECK(host.ToStringNoPort() == "127.0.0.1");

		host = mg::net::HostMakeLocalIPV6(123);
		TEST_CHECK(host.ToStringNoPort() == "::1");

		host.SetPort(0);
		TEST_CHECK(host.ToStringNoPort() == "::1");
	}

	static void
	UnitTestHostOperatorEqNeIPV4()
	{
		TestCaseGuard guard("operator==/!=(IPV4)");

		mg::net::Host host1("192.168.0.1:1234");
		mg::net::Host host2(host1);
		TEST_CHECK(host1 == host2);
		TEST_CHECK(host2 == host1);
		TEST_CHECK(!(host1 != host2));
		TEST_CHECK(!(host2 != host1));

		host2.SetPort(456);
		TEST_CHECK(host1 != host2);
		TEST_CHECK(host2 != host1);
		TEST_CHECK(!(host1 == host2));
		TEST_CHECK(!(host2 == host1));

		host2.Set("192.168.0.2:1234");
		TEST_CHECK(host1 != host2);
		TEST_CHECK(host2 != host1);
		TEST_CHECK(!(host1 == host2));
		TEST_CHECK(!(host2 == host1));

		// IPV4 vs IPV6.
		host2.Set(theIPV6v1);
		TEST_CHECK(host1 != host2);
		TEST_CHECK(host2 != host1);
		TEST_CHECK(!(host1 == host2));
		TEST_CHECK(!(host2 == host1));

		// IPV4 vs empty.
		host2.Clear();
		TEST_CHECK(host1 != host2);
		TEST_CHECK(host2 != host1);
		TEST_CHECK(!(host1 == host2));
		TEST_CHECK(!(host2 == host1));

		// Empty vs empty.
		mg::net::Host empty;
		TEST_CHECK(empty == host2);
		TEST_CHECK(host2 == empty);
		TEST_CHECK(!(empty != host2));
		TEST_CHECK(!(host2 != empty));
		//
		// Comparison and hash should only use standard fields. Omit padding between them
		// and platform-specific fields.
		//
		mg::net::Host orig = host1;
		TEST_CHECK(host1.IsIPV4());
		memset(&host1.myAddrIn, '#', sizeof(host1.myAddrIn));
		host1.myAddrIn.sin_family = orig.myAddrIn.sin_family;
		host1.myAddrIn.sin_addr = orig.myAddrIn.sin_addr;
		host1.myAddrIn.sin_port = orig.myAddrIn.sin_port;

		host2 = orig;
		memset(&host2.myAddrIn, '?', sizeof(host2.myAddrIn));
		host2.myAddrIn.sin_family = orig.myAddrIn.sin_family;
		host2.myAddrIn.sin_addr = orig.myAddrIn.sin_addr;
		host2.myAddrIn.sin_port = orig.myAddrIn.sin_port;

		TEST_CHECK(host1 == host2);
		TEST_CHECK(host2 == host1);
		TEST_CHECK(!(host1 != host2));
		TEST_CHECK(!(host2 != host1));
	}

	static void
	UnitTestHostOperatorEqNeIPV6()
	{
		TestCaseGuard guard("operator==/!=(IPV6)");

		mg::net::Host host1(mg::box::StringFormat("[%s]:1234", theIPV6v1));
		mg::net::Host host2(host1);
		TEST_CHECK(host1 == host2);
		TEST_CHECK(host2 == host1);
		TEST_CHECK(!(host1 != host2));
		TEST_CHECK(!(host2 != host1));

		host2.SetPort(456);
		TEST_CHECK(host1 != host2);
		TEST_CHECK(host2 != host1);
		TEST_CHECK(!(host1 == host2));
		TEST_CHECK(!(host2 == host1));

		host2.Set(mg::box::StringFormat("[%s]:1234", theIPV6v2));
		TEST_CHECK(host1 != host2);
		TEST_CHECK(host2 != host1);
		TEST_CHECK(!(host1 == host2));
		TEST_CHECK(!(host2 == host1));

		// IPV6 vs IPV4.
		host2.Set("192.168.0.1");
		TEST_CHECK(host1 != host2);
		TEST_CHECK(host2 != host1);
		TEST_CHECK(!(host1 == host2));
		TEST_CHECK(!(host2 == host1));

		// IPV6 vs empty.
		host2.Clear();
		TEST_CHECK(host1 != host2);
		TEST_CHECK(host2 != host1);
		TEST_CHECK(!(host1 == host2));
		TEST_CHECK(!(host2 == host1));
		//
		// Comparison and hash should only use standard fields. Omit padding between them
		// and platform-specific fields.
		//
		mg::net::Host orig = host1;
		TEST_CHECK(host1.IsIPV6());
		memset(&host1.myAddrIn6, '#', sizeof(host1.myAddrIn6));
		host1.myAddrIn6.sin6_family = orig.myAddrIn6.sin6_family;
		host1.myAddrIn6.sin6_addr = orig.myAddrIn6.sin6_addr;
		host1.myAddrIn6.sin6_port = orig.myAddrIn6.sin6_port;

		host2 = orig;
		memset(&host2.myAddrIn6, '?', sizeof(host2.myAddrIn6));
		host2.myAddrIn6.sin6_family = orig.myAddrIn6.sin6_family;
		host2.myAddrIn6.sin6_addr = orig.myAddrIn6.sin6_addr;
		host2.myAddrIn6.sin6_port = orig.myAddrIn6.sin6_port;

		TEST_CHECK(host1 == host2);
		TEST_CHECK(host2 == host1);
		TEST_CHECK(!(host1 != host2));
		TEST_CHECK(!(host2 != host1));
	}

	static void
	UnitTestHostMakeLocal()
	{
		TestCaseGuard guard("HostMakeLocalIPV4/6()");

		TEST_CHECK(mg::net::HostMakeLocalIPV4(123).ToString() == "127.0.0.1:123");
		TEST_CHECK(mg::net::HostMakeLocalIPV6(123).ToString() == "[::1]:123");
	}

	static void
	UnitTestHostMakeAll()
	{
		TestCaseGuard guard("HostMakeAllIPV4/6()");

		TEST_CHECK(mg::net::HostMakeAllIPV4(123).ToString() == "0.0.0.0:123");
		TEST_CHECK(mg::net::HostMakeAllIPV6(123).ToString() == "[::]:123");
	}

	static void
	UnitTestHostEndianess()
	{
		TestCaseGuard guard("Endianess");

		TEST_CHECK(mg::net::NtoHs(mg::net::HtoNs(123)) == 123);
		TEST_CHECK(mg::net::NtoHl(mg::net::HtoNl(123)) == 123);
	}

	static void
	UnitTestHostSockaddrCreate()
	{
		TestCaseGuard guard("Sockaddr create");

		mg::net::SockaddrIn addr4;
		addr4.sin_family = mg::net::ADDR_FAMILY_NONE;
		mg::net::SockaddrInCreate(addr4);
		TEST_CHECK(addr4.sin_family == mg::net::ADDR_FAMILY_IPV4);

		mg::net::SockaddrIn6 addr6;
		addr6.sin6_family = mg::net::ADDR_FAMILY_NONE;
		mg::net::SockaddrIn6Create(addr6);
		TEST_CHECK(addr6.sin6_family == mg::net::ADDR_FAMILY_IPV6);
	}

	void
	UnitTestHost()
	{
		TestSuiteGuard suite("Host");

		UnitTestHostConstructorIPV4();
		UnitTestHostConstructorIPV6();
		UnitTestHostSetIPV4();
		UnitTestHostSetIPV6();
		UnitTestHostClear();
		UnitTestHostSetPortIPV4();
		UnitTestHostSetPortIPV6();
		UnitTestHostGetSockaddrSize();
		UnitTestHostGetPort();
		UnitTestHostIPVersionCheck();
		UnitTestHostIsSet();
		UnitTestHostIsEqualNoPortIPV4();
		UnitTestHostIsEqualNoPortIPV6();
		UnitTestHostToString();
		UnitTestHostToStringNoPort();
		UnitTestHostOperatorEqNeIPV4();
		UnitTestHostOperatorEqNeIPV6();
		UnitTestHostMakeLocal();
		UnitTestHostMakeAll();
		UnitTestHostEndianess();
		UnitTestHostSockaddrCreate();
	}

}
}
}

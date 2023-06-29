#include "mg/net/Host.h"

#include "UnitTest.h"

#define TEST_CHECK MG_BOX_ASSERT

namespace mg {
namespace unittests {

	static const char* theIPV6v1 = "bda:738f:3de1:d416:daa4:4a76:1a42:9e41";
	static const char* theIPV6v2 = "4870:7fa2:2613:1169:7cb9:e172:4d2:6cf0";
	static const char* theIPV6Like4 = "::ffff:192.0.2.128";

	static void
	UnitTestHostBasicIPV4()
	{
		TestCaseGuard guard("Basic IPV4");

		uint16_t port = 12345;
		mg::net::Host host1 = mg::net::HostMakeLocalIPV4(port);
		mg::net::Host host2 = mg::net::HostMakeLocalIPV4(port);
		TEST_CHECK(host1 == host2);
		TEST_CHECK(!(host1 != host2));

		TEST_CHECK(host1.GetAddressFamily() == AF_INET);
		TEST_CHECK(host1.GetSockaddrSize() == sizeof(struct sockaddr_in));
		TEST_CHECK(host1.ToString() == "127.0.0.1:12345");
		TEST_CHECK(host1.IsIPV4());
		TEST_CHECK(!host1.IsIPV6());
		TEST_CHECK(host1.IsSet());
		TEST_CHECK(host1.GetPort() == 12345);
		TEST_CHECK(host1.IsEqualNoPort(host2));

		host1.SetPort(54321);
		TEST_CHECK(host1.GetPort() == 54321);
		TEST_CHECK(host1.IsEqualNoPort(host2));

		host1 = mg::net::HostMakeAllIPV4(port);
		TEST_CHECK(!host1.IsEqualNoPort(host2));
		TEST_CHECK(host1 != host2);
		TEST_CHECK(!(host1 == host2));

		host1.Clear();
		TEST_CHECK(!host1.IsSet());

		//
		// Comparison and hash should only use standard fields. Omit padding between them
		// and platform-specific fields.
		//
		mg::net::Host host3 = mg::net::HostMakeLocalIPV4(123);
		memset(&host1.myAddrStorage, '#', sizeof(host1.myAddrStorage));
		host1.myAddrIn.sin_family = host3.myAddrIn.sin_family;
		host1.myAddrIn.sin_addr = host3.myAddrIn.sin_addr;
		host1.myAddrIn.sin_port = host3.myAddrIn.sin_port;

		memset(&host2.myAddrStorage, '?', sizeof(host2.myAddrStorage));
		host2.myAddr.sa_family = mg::net::ADDR_FAMILY_NONE;
		host2.myAddrIn.sin_family = host3.myAddrIn.sin_family;
		host2.myAddrIn.sin_addr = host3.myAddrIn.sin_addr;
		host2.myAddrIn.sin_port = host3.myAddrIn.sin_port;

		TEST_CHECK(host1 == host2);
	}

	void
	UnitTestHostBasicIPV6()
	{
		TestCaseGuard guard("Basic IPV6");

		uint16_t port = 12345;
		mg::net::Host host1 = mg::net::HostMakeLocalIPV6(port);
		mg::net::Host host2 = mg::net::HostMakeLocalIPV6(port);
		TEST_CHECK(host1 == host2);
		TEST_CHECK(!(host1 != host2));

		TEST_CHECK(host1.GetAddressFamily() == AF_INET6);
		TEST_CHECK(host1.GetSockaddrSize() == sizeof(struct sockaddr_in6));
		TEST_CHECK(host1.ToString() == "[::1]:12345");
		TEST_CHECK(host1.IsIPV6());
		TEST_CHECK(!host1.IsIPV4());
		TEST_CHECK(host1.IsSet());
		TEST_CHECK(host1.GetPort() == 12345);
		TEST_CHECK(host1.IsEqualNoPort(host2));

		host1.SetPort(54321);
		TEST_CHECK(host1.GetPort() == 54321);
		TEST_CHECK(host1.IsEqualNoPort(host2));

		host1 = mg::net::HostMakeAllIPV6(port);
		TEST_CHECK(!host1.IsEqualNoPort(host2));
		TEST_CHECK(host1 != host2);
		TEST_CHECK(!(host1 == host2));

		host1.Clear();
		TEST_CHECK(!host1.IsSet());
	}

	static void
	UnitTestHostSetStringIPV4()
	{
		TestCaseGuard guard("Set string IPV4");

		// Only IP.
		mg::net::Host host("127.0.0.1");
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

		// Malformed. Also ensure the errors won't change the
		// original value.
		TEST_CHECK(host.Set("192.168.1.2:456"));
		TEST_CHECK(!host.Set("255.255.255.2555"));
		TEST_CHECK(!host.Set("192.168.000.1"));
		TEST_CHECK(!host.Set("192.168.0000.1"));
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
	UnitTestHostSetStringIPV6()
	{
		TestCaseGuard guard("Set string IPV6");

		// Only IP.
		mg::net::Host host(theIPV6v1);
		TEST_CHECK(host.IsIPV6());
		TEST_CHECK(host.ToString() == mg::box::StringFormat("[%s]:0", theIPV6v1));

		TEST_CHECK(host.Set(theIPV6v2));
		TEST_CHECK(host.IsIPV6());
		TEST_CHECK(host.ToString() == mg::box::StringFormat("[%s]:0", theIPV6v2));

		TEST_CHECK(host.Set("::1"));
		TEST_CHECK(host.IsIPV6());
		TEST_CHECK(host.ToString() == "[::1]:0");

		TEST_CHECK(host.Set(theIPV6Like4));
		TEST_CHECK(host.IsIPV6());
		TEST_CHECK(host.ToString() == mg::box::StringFormat("[%s]:0", theIPV6Like4));

		TEST_CHECK(host.Set("::"));
		TEST_CHECK(host.IsIPV6());
		TEST_CHECK(host.ToString() == "[::]:0");
		TEST_CHECK(host == mg::net::HostMakeAllIPV6(0));

		// Leading zeros in some sections.
		TEST_CHECK(host.Set( "0bda:738f:3de1:d416:daa4:0a76:1a42:9e41"));
		TEST_CHECK(host.IsIPV6());
		TEST_CHECK(host.ToString() == "[bda:738f:3de1:d416:daa4:a76:1a42:9e41]:0");

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

		TEST_CHECK(host.Set(mg::box::StringFormat("[%s]:8910", theIPV6Like4)));
		TEST_CHECK(host.IsIPV6());
		TEST_CHECK(host.ToString() == mg::box::StringFormat("[%s]:8910", theIPV6Like4));

		TEST_CHECK(host.Set(mg::box::StringFormat("[%s]:65535", theIPV6v2)));
		TEST_CHECK(host.IsIPV6());
		TEST_CHECK(host.ToString() == mg::box::StringFormat("[%s]:65535", theIPV6v2));

		TEST_CHECK(host.Set("[::]:5632"));
		TEST_CHECK(host.IsIPV6());
		TEST_CHECK(host.ToString() == "[::]:5632");
		TEST_CHECK(host == mg::net::HostMakeAllIPV6(5632));

		// Malformed. Also ensure the errors won't change the
		// original value.
		TEST_CHECK(host.Set(mg::box::StringFormat("[%s]:123", theIPV6v1)));
		//                            Excess digit here V
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
		TestCaseGuard guard("Clear");

		mg::net::Host host;
		TEST_CHECK(!host.IsSet());
		host.Clear();
		TEST_CHECK(!host.IsSet());

		host.Set("127.0.0.1:123");
		TEST_CHECK(host.ToString() == "127.0.0.1:123");
		TEST_CHECK(host.IsSet());
		TEST_CHECK(host.IsIPV4());
		host.Clear();
		TEST_CHECK(!host.IsSet());

		host.Set(mg::box::StringFormat("[%s]:123", theIPV6v1));
		TEST_CHECK(host.ToString() == mg::box::StringFormat("[%s]:123", theIPV6v1));
		TEST_CHECK(host.IsSet());
		TEST_CHECK(host.IsIPV6());
		host.Clear();
		TEST_CHECK(!host.IsSet());
	}

	static void
	UnitTestHostSetPortIPV4()
	{
		TestCaseGuard guard("Set port IPV4");

		mg::net::Host host("192.168.0.1");
		host.SetPort(12345);
		TEST_CHECK(host.ToString() == "192.168.0.1:12345");
		host.SetPort(0);
		TEST_CHECK(host.ToString() == "192.168.0.1:0");
		host.SetPort(456);
		TEST_CHECK(host.ToString() == "192.168.0.1:456");
	}

	static void
	UnitTestHostSetPortIPV6()
	{
		TestCaseGuard guard("Set port IPV6");

		mg::net::Host host(theIPV6v1);
		host.SetPort(12345);
		TEST_CHECK(host.ToString() == mg::box::StringFormat("[%s]:12345", theIPV6v1));
		host.SetPort(0);
		TEST_CHECK(host.ToString() == mg::box::StringFormat("[%s]:0", theIPV6v1));
		host.SetPort(456);
		TEST_CHECK(host.ToString() == mg::box::StringFormat("[%s]:456", theIPV6v1));
	}

	static void
	UnitTestHostGetPlatformDetails()
	{
		TestCaseGuard guard("Get platform details");

		mg::net::Host host;
		TEST_CHECK(host.myAddr.sa_family == mg::net::ADDR_FAMILY_NONE);

		host.Set("192.168.0.1:12345");
		TEST_CHECK(host.GetSockaddr() == (mg::net::Sockaddr*)&host.myAddrIn);
		TEST_CHECK(host.GetAddressFamily() == mg::net::ADDR_FAMILY_IPV4);
		static_assert(mg::net::ADDR_FAMILY_IPV4 == AF_INET, "Addr family");
		TEST_CHECK(host.GetSockaddrSize() == sizeof(host.myAddrIn));
		static_assert(sizeof(host.myAddrIn) == sizeof(struct sockaddr_in), "Addr IPv4 size");

		host.Set(mg::box::StringFormat("[%s]:456", theIPV6v1));
		TEST_CHECK(host.GetSockaddr() == (mg::net::Sockaddr*)&host.myAddrIn6);
		TEST_CHECK(host.GetAddressFamily() == mg::net::ADDR_FAMILY_IPV6);
		static_assert(mg::net::ADDR_FAMILY_IPV6 == AF_INET6, "Addr family");
		TEST_CHECK(host.GetSockaddrSize() == sizeof(host.myAddrIn6));
		static_assert(sizeof(host.myAddrIn6) == sizeof(struct sockaddr_in6), "Addr IPv6 size");
	}

	static void
	UnitTestHostToStringIPV4()
	{
		TestCaseGuard guard("ToString() IPV4");

		mg::net::Host host;
		TEST_CHECK(host.ToString() == "none");
		TEST_CHECK(host.ToStringNoPort() == "none");

		host = mg::net::HostMakeLocalIPV4(0);
		TEST_CHECK(host.ToString() == "127.0.0.1:0");
		TEST_CHECK(host.ToStringNoPort() == "127.0.0.1");

		host.SetPort(123);
		TEST_CHECK(host.ToString() == "127.0.0.1:123");
		TEST_CHECK(host.ToStringNoPort() == "127.0.0.1");

		host.Set("192.168.0.1");
		TEST_CHECK(host.ToString() == "192.168.0.1:0");
		TEST_CHECK(host.ToStringNoPort() == "192.168.0.1");

		host.SetPort(456);
		TEST_CHECK(host.ToString() == "192.168.0.1:456");
		TEST_CHECK(host.ToStringNoPort() == "192.168.0.1");

		host = mg::net::HostMakeAllIPV4(789);
		TEST_CHECK(host.ToString() == "0.0.0.0:789");
		TEST_CHECK(host.ToStringNoPort() == "0.0.0.0");
	}

	static void
	UnitTestHostToStringIPV6()
	{
		TestCaseGuard guard("ToString() IPV6");

		mg::net::Host host;
		TEST_CHECK(host.ToString() == "none");
		TEST_CHECK(host.ToStringNoPort() == "none");

		host = mg::net::HostMakeLocalIPV6(0);
		TEST_CHECK(host.ToString() == "[::1]:0");
		TEST_CHECK(host.ToStringNoPort() == "::1");

		host.SetPort(123);
		TEST_CHECK(host.ToString() == "[::1]:123");
		TEST_CHECK(host.ToStringNoPort() == "::1");

		host.Set(theIPV6v1);
		TEST_CHECK(host.ToString() == mg::box::StringFormat("[%s]:0", theIPV6v1));
		TEST_CHECK(host.ToStringNoPort() == theIPV6v1);

		host.SetPort(456);
		TEST_CHECK(host.ToString() == mg::box::StringFormat("[%s]:456", theIPV6v1));
		TEST_CHECK(host.ToStringNoPort() == theIPV6v1);

		host = mg::net::HostMakeAllIPV6(789);
		TEST_CHECK(host.ToString() == "[::]:789");
		TEST_CHECK(host.ToStringNoPort() == "::");
	}

	static void
	UnitTestHostIsIPAddress()
	{
		TestCaseGuard guard("HostIsIPAddress()");

		TEST_CHECK(mg::net::HostIsIPAddress("192.168.0.1"));
		TEST_CHECK(!mg::net::HostIsIPAddress("192.168.0.1:1234"));
		TEST_CHECK(!mg::net::HostIsIPAddress("http://192.168.0.1"));

		TEST_CHECK(mg::net::HostIsIPAddress(
			"2001:db8:3333:4444:5555:6666:7777:8888"));
		TEST_CHECK(!mg::net::HostIsIPAddress(
			"[2001:db8:3333:4444:5555:6666:7777:8888]"));
		TEST_CHECK(!mg::net::HostIsIPAddress(
			"[2001:db8:3333:4444:5555:6666:7777:8888]:1234"));
		TEST_CHECK(!mg::net::HostIsIPAddress(
			"http://2001:db8:3333:4444:5555:6666:7777:8888"));
		TEST_CHECK(!mg::net::HostIsIPAddress(
			"http://[2001:db8:3333:4444:5555:6666:7777:8888]:1234"));

		TEST_CHECK(!mg::net::HostIsIPAddress("bad address"));
	}

	void
	UnitTestHost()
	{
		TestSuiteGuard suite("Host");

		UnitTestHostBasicIPV4();
		UnitTestHostBasicIPV6();
		UnitTestHostSetStringIPV4();
		UnitTestHostSetStringIPV6();
		UnitTestHostClear();
		UnitTestHostSetPortIPV4();
		UnitTestHostSetPortIPV6();
		UnitTestHostGetPlatformDetails();
		UnitTestHostToStringIPV4();
		UnitTestHostToStringIPV6();
		UnitTestHostIsIPAddress();
	}

}
}

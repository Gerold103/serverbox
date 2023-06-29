#include "Host.h"

#if IS_PLATFORM_POSIX
#include <arpa/inet.h>
#else
#include <ws2tcpip.h>
#endif

namespace mg {
namespace net {

	static int InAddrCmp(
		const InAddr& aAddr1,
		const InAddr& aAddr2);

#if IS_IPV6_SUPPORTED
	static int In6AddrCmp(
		const In6Addr& aAddr1,
		const In6Addr& aAddr2);
#endif

	//////////////////////////////////////////////////////////////

	Host::Host(
		const Sockaddr* aSockaddr)
	{
		if (aSockaddr == nullptr)
			Clear();
		else
			Set(aSockaddr);
	}

	void
	Host::Set(
		const Sockaddr* aSockaddr)
	{
		if (aSockaddr->sa_family == ADDR_FAMILY_IPV4)
			myAddrIn = *(const SockaddrIn*)aSockaddr;
#if IS_IPV6_SUPPORTED
		else if (aSockaddr->sa_family == ADDR_FAMILY_IPV6)
			myAddrIn6 = *(const SockaddrIn6*)aSockaddr;
#endif
		else
			MG_BOX_ASSERT(!"Unsupported address family");
	}

	bool
	Host::Set(
		const char* aHost)
	{
#if IS_IPV6_SUPPORTED
		if (*aHost == '[')
		{
			// This must be "[ipv6]" or "[ipv6]:port".
			const char* ipStr = ++aHost;
			while (*aHost != ']')
			{
				if (*aHost == 0)
					return false;
				++aHost;
			}
			uint64_t ipLen = aHost - ipStr;
			if (ipLen > HOST_INET6_STRLEN)
				return false;
			// Skip ']'.
			++aHost;

			// Extract IP from inside of [...].
			In6Addr addr6;
			memset(&addr6, 0, sizeof(addr6));
			char buf[HOST_INET6_STRLEN + 1];
			memcpy(buf, ipStr, ipLen);
			buf[ipLen] = 0;
			if (InetPtoN(ADDR_FAMILY_IPV6, buf, &addr6) != 1)
				return false;

			// Optional port suffix.
			uint16_t port = 0;
			if (*aHost != 0)
			{
				if (*aHost != ':')
					return false;
				// Skip ':'.
				++aHost;
				if (!mg::box::StringToNumber(aHost, port))
					return false;
			}
			SockaddrIn6Create(myAddrIn6);
			myAddrIn6.sin6_addr = addr6;
			myAddrIn6.sin6_port = HtoNs(port);
			return true;
		}
#endif
		// It is either "ipv6" or "ipv4:port" or "ipv4".
		InAddr addr4;
		memset(&addr4, 0, sizeof(addr4));
		if (InetPtoN(ADDR_FAMILY_IPV4, aHost, &addr4) == 1)
		{
			SockaddrInCreate(myAddrIn);
			myAddrIn.sin_addr = addr4;
			return true;
		}
#if IS_IPV6_SUPPORTED
		// It is either "ipv6" or "ipv4:port".
		In6Addr addr6;
		memset(&addr6, 0, sizeof(addr6));
		if (InetPtoN(ADDR_FAMILY_IPV6, aHost, &addr6) == 1)
		{
			SockaddrIn6Create(myAddrIn6);
			myAddrIn6.sin6_addr = addr6;
			return true;
		}
#endif
		// It is "ipv4:port".
		const char* ipStr = aHost;
		while (*aHost != ':')
		{
			if (*aHost == 0)
				return false;
			++aHost;
		}
		uint64_t ipLen = aHost - ipStr;
		if (ipLen > HOST_INET_STRLEN)
			return false;
		// Skip ':'.
		++aHost;

		// Extract IP separated from port.
		char buf[HOST_INET_STRLEN + 1];
		memcpy(buf, ipStr, ipLen);
		buf[ipLen] = 0;
		if (InetPtoN(ADDR_FAMILY_IPV4, buf, &addr4) != 1)
			return false;

		uint16_t port = 0;
		if (!mg::box::StringToNumber(aHost, port))
			return false;
		SockaddrInCreate(myAddrIn);
		myAddrIn.sin_addr = addr4;
		myAddrIn.sin_port = HtoNs(port);
		return true;
	}

	void
	Host::SetPort(
		uint16_t aPort)
	{
		if (IsIPV4())
			myAddrIn.sin_port = HtoNs(aPort);
#if IS_IPV6_SUPPORTED
		else if (IsIPV6())
			myAddrIn6.sin6_port = HtoNs(aPort);
#endif
		else
			MG_BOX_ASSERT(!"Unsupported address family");
	}

	uint32_t
	Host::GetSockaddrSize() const
	{
		if (IsIPV4())
			return sizeof(SockaddrIn);
#if IS_IPV6_SUPPORTED
		if (IsIPV6())
			return sizeof(SockaddrIn6);
#endif
		MG_BOX_ASSERT(!"Unsupported address family");
		return 0;
	}

	uint16_t
	Host::GetPort() const
	{
		if (IsIPV4())
			return NtoHs(myAddrIn.sin_port);
#if IS_IPV6_SUPPORTED
		else if (IsIPV6())
			return NtoHs(myAddrIn6.sin6_port);
#endif
		else
			MG_BOX_ASSERT(!"Unsupported address family");
		return 0;
	}

	bool
	Host::IsEqualNoPort(
		const Host& aOther) const
	{
		if (myAddr.sa_family != aOther.myAddr.sa_family)
			return false;
		switch(myAddr.sa_family)
		{
		case ADDR_FAMILY_NONE:
			return true;
		case ADDR_FAMILY_IPV4:
			return InAddrCmp(myAddrIn.sin_addr, aOther.myAddrIn.sin_addr) == 0;
#if IS_IPV6_SUPPORTED
		case ADDR_FAMILY_IPV6:
			return In6AddrCmp(myAddrIn6.sin6_addr, aOther.myAddrIn6.sin6_addr) == 0;
#endif
		default:
			MG_BOX_ASSERT(!"Unsupported address family");
		}
		return false;
	}

	std::string
	Host::ToString() const
	{
		switch (myAddr.sa_family)
		{
		case ADDR_FAMILY_NONE:
			return "none";
		case ADDR_FAMILY_IPV4:
		{
			char ipStr[HOST_INET_STRLEN + 1];
			InetNtoP(ADDR_FAMILY_IPV4, &myAddrIn.sin_addr, ipStr, HOST_INET_STRLEN);
			return mg::box::StringFormat("%s:%u", ipStr, NtoHs(myAddrIn.sin_port));
		}
#if IS_IPV6_SUPPORTED
		case ADDR_FAMILY_IPV6:
		{
			char ipStr[HOST_INET6_STRLEN + 1];
			InetNtoP(ADDR_FAMILY_IPV6, &myAddrIn6.sin6_addr, ipStr, HOST_INET6_STRLEN);
			return mg::box::StringFormat("[%s]:%u", ipStr, NtoHs(myAddrIn6.sin6_port));
		}
#endif
		default:
			MG_BOX_ASSERT(!"Unsupported address family");
			return {};
		}
	}

	std::string
	Host::ToStringNoPort() const
	{
		switch (myAddr.sa_family)
		{
		case ADDR_FAMILY_NONE:
			return "none";
		case ADDR_FAMILY_IPV4:
		{
			char ipStr[HOST_INET_STRLEN + 1];
			InetNtoP(ADDR_FAMILY_IPV4, &myAddrIn.sin_addr, ipStr, HOST_INET_STRLEN);
			return std::string(ipStr);
		}
#if IS_IPV6_SUPPORTED
		case ADDR_FAMILY_IPV6:
		{
			char ipStr[HOST_INET6_STRLEN + 1];
			InetNtoP(ADDR_FAMILY_IPV6, &myAddrIn6.sin6_addr, ipStr, HOST_INET6_STRLEN);
			return std::string(ipStr);
		}
#endif
		default:
			MG_BOX_ASSERT(!"Unsupported address family");
			return {};
		}
	}

	bool
	Host::operator==(
		const Host& aRhs) const
	{
		if (myAddr.sa_family != aRhs.myAddr.sa_family)
			return false;
		// Can't memcmp the entire sockaddr object, because 1) it can contain padding
		// between the members with garbage bytes, 2) it can contain non-standard
		// platform-specific fields unrelated to 'ip:port' pair.
		switch (myAddr.sa_family)
		{
		case ADDR_FAMILY_NONE:
			return true;
		case ADDR_FAMILY_IPV4:
			return InAddrCmp(myAddrIn.sin_addr, aRhs.myAddrIn.sin_addr) == 0 &&
				myAddrIn.sin_port == aRhs.myAddrIn.sin_port;
#if IS_IPV6_SUPPORTED
		case ADDR_FAMILY_IPV6:
			return In6AddrCmp(myAddrIn6.sin6_addr, aRhs.myAddrIn6.sin6_addr) == 0 &&
				myAddrIn6.sin6_port == aRhs.myAddrIn6.sin6_port;
#endif
		default:
			MG_BOX_ASSERT(false);
			return false;
		}
	}

	//////////////////////////////////////////////////////////////

	int
	InetPtoN(
		SockAddrFamily aFamily,
		const char* aStr,
		void* aOut)
	{
		return inet_pton(aFamily, aStr, aOut);
	}

	const char*
	InetNtoP(
		SockAddrFamily aFamily,
		const void* aAddr,
		char* aOut,
		uint32_t aSize)
	{
		return inet_ntop(aFamily, aAddr, aOut, aSize);
	}

	uint16_t
	NtoHs(
		uint16_t aValue)
	{
		return ntohs(aValue);
	}

	uint32_t
	NtoHl(
		uint32_t aValue)
	{
		return ntohl(aValue);
	}

	uint16_t
	HtoNs(
		uint16_t aValue)
	{
		return htons(aValue);
	}

	uint32_t
	HtoNl(
		uint32_t aValue)
	{
		return htonl(aValue);
	}

	void
	SockaddrInCreate(
		SockaddrIn& aAddr)
	{
		memset(&aAddr, 0, sizeof(aAddr));
		aAddr.sin_family = ADDR_FAMILY_IPV4;
#if IS_ADDR_SIN_LEN_SUPPORTED
		aAddr.sin_len = sizeof(aAddr);
#endif
	}

#if IS_IPV6_SUPPORTED
	void
	SockaddrIn6Create(
		SockaddrIn6& aAddr)
	{
		memset(&aAddr, 0, sizeof(aAddr));
		aAddr.sin6_family = ADDR_FAMILY_IPV6;
#if IS_ADDR_SIN_LEN_SUPPORTED
		aAddr.sin6_len = sizeof(aAddr);
#endif
	}
#endif

	bool
	HostIsIPAddress(
		const char* aStr)
	{
		InAddr addr4;
		int rc = InetPtoN(ADDR_FAMILY_IPV4, aStr, &addr4);
		MG_BOX_ASSERT(rc >= 0);
		if (rc == 1)
			return true;
#if IS_IPV6_SUPPORTED
		In6Addr addr6;
		rc = InetPtoN(ADDR_FAMILY_IPV6, aStr, &addr6);
		MG_BOX_ASSERT(rc >= 0);
		return rc == 1;
#else
		return false;
#endif
	}

	Host
	HostMakeLocalIPV4(
		uint16_t aPort)
	{
		SockaddrIn addr;
		SockaddrInCreate(addr);
		addr.sin_addr.s_addr = HtoNl((uint32_t)HOST_INADDR_LOOPBACK);
		addr.sin_port = HtoNs(aPort);
		return Host((Sockaddr*)&addr);
	}

	Host
	HostMakeLocalIPV6(
		uint16_t aPort)
	{
#if !IS_IPV6_SUPPORTED
		MG_UNUSED(aPort);
		MG_BOX_ASSERT(!"Not supported");
		return Host();
#else
		SockaddrIn6 addr;
		SockaddrIn6Create(addr);
		addr.sin6_addr = HOST_IN6ADDR_LOOPBACK;
		addr.sin6_port = HtoNs(aPort);
		return Host((Sockaddr*)&addr);
#endif
	}

	Host
	HostMakeAllIPV4(
		uint16_t aPort)
	{
		SockaddrIn addr;
		SockaddrInCreate(addr);
		addr.sin_addr.s_addr = HtoNl((uint32_t)HOST_INADDR_ANY);
		addr.sin_port = HtoNs(aPort);
		return Host((Sockaddr*)&addr);
	}

	Host
	HostMakeAllIPV6(
		uint16_t aPort)
	{
#if !IS_IPV6_SUPPORTED
		MG_UNUSED(aPort);
		MG_BOX_ASSERT(!"Not supported");
		return Host();
#else
		SockaddrIn6 addr;
		SockaddrIn6Create(addr);
		addr.sin6_addr = HOST_IN6ADDR_ANY;
		addr.sin6_port = HtoNs(aPort);
		return Host((Sockaddr*)&addr);
#endif
	}

	//////////////////////////////////////////////////////////////

	static int
	InAddrCmp(
		const InAddr& aAddr1,
		const InAddr& aAddr2)
	{
		static_assert(sizeof(aAddr1) == 4, "sin_addr contains only IP");
		return memcmp(&aAddr1, &aAddr2, sizeof(aAddr1));
	}

#if IS_IPV6_SUPPORTED
	static int
	In6AddrCmp(
		const In6Addr& aAddr1,
		const In6Addr& aAddr2)
	{
		static_assert(sizeof(aAddr1) == 16, "sin6_addr contains only IP");
		return memcmp(&aAddr1, &aAddr2, sizeof(aAddr1));
	}
#endif

}
}

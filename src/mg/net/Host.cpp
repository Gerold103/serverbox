#include "Host.h"

#if IS_PLATFORM_UNIX
#include <arpa/inet.h>
#else
#include <ws2tcpip.h>
#endif

#define MG_HOST_INADDR_LOOPBACK INADDR_LOOPBACK
#define MG_HOST_INADDR_ANY INADDR_ANY

#define MG_HOST_IN6ADDR_LOOPBACK in6addr_loopback
#define MG_HOST_IN6ADDR_ANY in6addr_any

namespace mg {
namespace net {

	using InAddr = in_addr;
	using In6Addr = in6_addr;

	// +6 for ':<port>' (max ':65535').
	static constexpr uint32_t theHostIPV4Len = INET_ADDRSTRLEN + 6;
	// +6 for ':<port>' (max ':65535').
	// +2 for [] around IP.
	static constexpr uint32_t theHostIPV6Len = INET6_ADDRSTRLEN + 6 + 2;

	static int InAddrCmp(
		const InAddr& aAddr1,
		const InAddr& aAddr2);

	static int In6AddrCmp(
		const In6Addr& aAddr1,
		const In6Addr& aAddr2);

	static int InetPtoN(
		SockAddrFamily aFamily,
		const char* aStr,
		void* aOut);

	static const char* InetNtoP(
		SockAddrFamily aFamily,
		const void* aAddr,
		char* aOut,
		uint32_t aSize);

	//////////////////////////////////////////////////////////////////////////////////////

	void
	Host::Set(
		const Sockaddr* aSockaddr)
	{
		if (aSockaddr->sa_family == ADDR_FAMILY_IPV4)
			myAddrIn = *(const SockaddrIn*)aSockaddr;
		else if (aSockaddr->sa_family == ADDR_FAMILY_IPV6)
			myAddrIn6 = *(const SockaddrIn6*)aSockaddr;
		else
			MG_BOX_ASSERT(!"Unsupported address family");
	}

	bool
	Host::Set(
		const char* aHost)
	{
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
			if (ipLen > theHostIPV6Len)
				return false;
			// Skip ']'.
			++aHost;

			// Extract IP from inside of [...].
			In6Addr addr6;
			memset(&addr6, 0, sizeof(addr6));
			char buf[theHostIPV6Len + 1];
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
		// It is either "ipv6" or "ipv4:port" or "ipv4".
		InAddr addr4;
		memset(&addr4, 0, sizeof(addr4));
		if (InetPtoN(ADDR_FAMILY_IPV4, aHost, &addr4) == 1)
		{
			SockaddrInCreate(myAddrIn);
			myAddrIn.sin_addr = addr4;
			return true;
		}
		// It is either "ipv6" or "ipv4:port".
		In6Addr addr6;
		memset(&addr6, 0, sizeof(addr6));
		if (InetPtoN(ADDR_FAMILY_IPV6, aHost, &addr6) == 1)
		{
			SockaddrIn6Create(myAddrIn6);
			myAddrIn6.sin6_addr = addr6;
			return true;
		}
		// It is "ipv4:port".
		const char* ipStr = aHost;
		while (*aHost != ':')
		{
			if (*aHost == 0)
				return false;
			++aHost;
		}
		uint64_t ipLen = aHost - ipStr;
		if (ipLen > theHostIPV4Len)
			return false;
		// Skip ':'.
		++aHost;

		// Extract IP separated from port.
		char buf[theHostIPV4Len + 1];
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
		else if (IsIPV6())
			myAddrIn6.sin6_port = HtoNs(aPort);
		else
			MG_BOX_ASSERT(!"Unsupported address family");
	}

	SocklenT
	Host::GetSockaddrSize() const
	{
		if (IsIPV4())
			return sizeof(SockaddrIn);
		if (IsIPV6())
			return sizeof(SockaddrIn6);
		MG_BOX_ASSERT(!"Unsupported address family");
		return 0;
	}

	uint16_t
	Host::GetPort() const
	{
		if (IsIPV4())
			return NtoHs(myAddrIn.sin_port);
		if (IsIPV6())
			return NtoHs(myAddrIn6.sin6_port);
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
		case ADDR_FAMILY_IPV6:
			return In6AddrCmp(myAddrIn6.sin6_addr, aOther.myAddrIn6.sin6_addr) == 0;
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
		{
			return "none";
		}
		case ADDR_FAMILY_IPV4:
		{
			char ipStr[theHostIPV4Len + 1];
			MG_BOX_ASSERT(InetNtoP(ADDR_FAMILY_IPV4, &myAddrIn.sin_addr, ipStr, theHostIPV4Len) != nullptr);
			return mg::box::StringFormat("%s:%u", ipStr, NtoHs(myAddrIn.sin_port));
		}
		case ADDR_FAMILY_IPV6:
		{
			char ipStr[theHostIPV6Len + 1];
			MG_BOX_ASSERT(InetNtoP(ADDR_FAMILY_IPV6, &myAddrIn6.sin6_addr, ipStr, theHostIPV6Len) != nullptr);
			return mg::box::StringFormat("[%s]:%u", ipStr, NtoHs(myAddrIn6.sin6_port));
		}
		default:
		{
			MG_BOX_ASSERT(!"Unsupported address family");
			return {};
		}
		}
	}

	std::string
	Host::ToStringNoPort() const
	{
		switch (myAddr.sa_family)
		{
		case ADDR_FAMILY_NONE:
		{
			return "none";
		}
		case ADDR_FAMILY_IPV4:
		{
			char ipStr[theHostIPV4Len + 1];
			MG_BOX_ASSERT(InetNtoP(ADDR_FAMILY_IPV4, &myAddrIn.sin_addr, ipStr, theHostIPV4Len) != nullptr);
			return std::string(ipStr);
		}
		case ADDR_FAMILY_IPV6:
		{
			char ipStr[theHostIPV6Len + 1];
			MG_BOX_ASSERT(InetNtoP(ADDR_FAMILY_IPV6, &myAddrIn6.sin6_addr, ipStr, theHostIPV6Len) != nullptr);
			return std::string(ipStr);
		}
		default:
		{
			MG_BOX_ASSERT(!"Unsupported address family");
			return {};
		}
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
		case ADDR_FAMILY_IPV6:
			return In6AddrCmp(myAddrIn6.sin6_addr, aRhs.myAddrIn6.sin6_addr) == 0 &&
				myAddrIn6.sin6_port == aRhs.myAddrIn6.sin6_port;
		default:
			MG_BOX_ASSERT(!"Unsupported address family");
			return false;
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////

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
	}

	void
	SockaddrIn6Create(
		SockaddrIn6& aAddr)
	{
		memset(&aAddr, 0, sizeof(aAddr));
		aAddr.sin6_family = ADDR_FAMILY_IPV6;
	}

	Host
	HostMakeLocalIPV4(
		uint16_t aPort)
	{
		SockaddrIn addr;
		SockaddrInCreate(addr);
		addr.sin_addr.s_addr = HtoNl((uint32_t)MG_HOST_INADDR_LOOPBACK);
		addr.sin_port = HtoNs(aPort);
		return Host((Sockaddr*)&addr);
	}

	Host
	HostMakeLocalIPV6(
		uint16_t aPort)
	{
		SockaddrIn6 addr;
		SockaddrIn6Create(addr);
		addr.sin6_addr = MG_HOST_IN6ADDR_LOOPBACK;
		addr.sin6_port = HtoNs(aPort);
		return Host((Sockaddr*)&addr);
	}

	Host
	HostMakeAllIPV4(
		uint16_t aPort)
	{
		SockaddrIn addr;
		SockaddrInCreate(addr);
		addr.sin_addr.s_addr = HtoNl((uint32_t)MG_HOST_INADDR_ANY);
		addr.sin_port = HtoNs(aPort);
		return Host((Sockaddr*)&addr);
	}

	Host
	HostMakeAllIPV6(
		uint16_t aPort)
	{
		SockaddrIn6 addr;
		SockaddrIn6Create(addr);
		addr.sin6_addr = MG_HOST_IN6ADDR_ANY;
		addr.sin6_port = HtoNs(aPort);
		return Host((Sockaddr*)&addr);
	}

	//////////////////////////////////////////////////////////////////////////////////////

	static int
	InAddrCmp(
		const InAddr& aAddr1,
		const InAddr& aAddr2)
	{
		static_assert(sizeof(aAddr1) == 4, "sin_addr contains only IP");
		return memcmp(&aAddr1, &aAddr2, sizeof(aAddr1));
	}

	static int
	In6AddrCmp(
		const In6Addr& aAddr1,
		const In6Addr& aAddr2)
	{
		static_assert(sizeof(aAddr1) == 16, "sin6_addr contains only IP");
		return memcmp(&aAddr1, &aAddr2, sizeof(aAddr1));
	}

	static int
	InetPtoN(
		SockAddrFamily aFamily,
		const char* aStr,
		void* aOut)
	{
		return inet_pton(aFamily, aStr, aOut);
	}

	static const char*
	InetNtoP(
		SockAddrFamily aFamily,
		const void* aAddr,
		char* aOut,
		uint32_t aSize)
	{
		return inet_ntop(aFamily, aAddr, aOut, aSize);
	}

}
}

#pragma once

#include "mg/box/Assert.h"
#include "mg/box/StringFunctions.h"

#if IS_PLATFORM_WIN
#include <ws2def.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif

namespace mg {
namespace net {

	using Sockaddr = sockaddr;
	using SockaddrIn = sockaddr_in;
	using SockaddrIn6 = sockaddr_in6;
	using SockAddrFamily = int;
	using SocklenT = socklen_t;

	static constexpr SockAddrFamily ADDR_FAMILY_NONE = 0;

	static constexpr SockAddrFamily ADDR_FAMILY_IPV4 = AF_INET;
	static_assert(ADDR_FAMILY_IPV4 != ADDR_FAMILY_NONE, "ipv4 != none");

	static constexpr SockAddrFamily ADDR_FAMILY_IPV6 = AF_INET6;
	static_assert(ADDR_FAMILY_IPV6 != ADDR_FAMILY_NONE, "ipv6 != none");

	class Host
	{
	public:
		Host() { Clear(); }
		Host(
			const Sockaddr* aSockaddr) { Set(aSockaddr); }
		Host(
			const char* aHost) { if (!Set(aHost)) Clear(); }
		Host(
			const std::string& aHost) : Host(aHost.c_str()) {}

		void Set(
			const Sockaddr* aSockaddr);
		// Works with port number. Doesn't work with domain names.
		bool Set(
			const char* aHost);
		bool Set(
			const std::string& aHost) { return Set(aHost.c_str()); }
		void Clear() { memset(&myAddr, 0, sizeof(myAddr)); }
		void SetPort(
			uint16_t aPort);

		SocklenT GetSockaddrSize() const;
		uint16_t GetPort() const;

		bool IsIPV4() const { return myAddr.sa_family == ADDR_FAMILY_IPV4; }
		bool IsIPV6() const { return myAddr.sa_family == ADDR_FAMILY_IPV6; }
		bool IsSet() const { return myAddr.sa_family != ADDR_FAMILY_NONE; }
		bool IsEqualNoPort(
			const Host& aOther) const;

		std::string ToString() const;
		std::string ToStringNoPort() const;

		bool operator==(
			const Host& aRhs) const;
		bool operator!=(
			const Host& aRhs) const { return !(*this == aRhs); }

		union
		{
			Sockaddr myAddr;
			SockaddrIn myAddrIn;
			SockaddrIn6 myAddrIn6;
		};
	};

	//////////////////////////////////////////////////////////////////////////////////////

	// Localhost IPV4.
	Host HostMakeLocalIPV4(
		uint16_t aPort);
	// Localhost IPV6.
	Host HostMakeLocalIPV6(
		uint16_t aPort);
	// All network interfaces IPV4.
	Host HostMakeAllIPV4(
		uint16_t aPort);
	// All network interfaces IPV6.
	Host HostMakeAllIPV6(
		uint16_t aPort);

	uint16_t NtoHs(
		uint16_t aValue);
	uint32_t NtoHl(
		uint32_t aValue);
	uint16_t HtoNs(
		uint16_t aValue);
	uint32_t HtoNl(
		uint32_t aValue);

	void SockaddrInCreate(
		SockaddrIn& aAddr);
	void SockaddrIn6Create(
		SockaddrIn6& aAddr);

}
}

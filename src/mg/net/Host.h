#pragma once

#include "mg/box/Assert.h"
#include "mg/box/StringFunctions.h"
#include "mg/box/Util.h"

#include <string>

#if IS_PLATFORM_WIN
#include <ws2def.h>
#include <ws2ipdef.h>
#else
#include <netinet/in.h>
#endif

namespace mg {
namespace net {

#if IS_PLATFORM_POSIX || IS_PLATFORM_WIN || IS_PLATFORM_APPLE
	#define IS_IPV6_SUPPORTED 1
	#define IS_ADDR_SIN_LEN_SUPPORTED 0

	using Sockaddr = struct ::sockaddr;
	using SockaddrIn = struct ::sockaddr_in;
	using SockaddrIn6 = struct ::sockaddr_in6;
	using SockaddrStorage = struct ::sockaddr_storage;
	using SockAddrFamily = int;
	using InAddr = struct ::in_addr;
	using In6Addr = struct ::in6_addr;

	#define HOST_INADDR_LOOPBACK INADDR_LOOPBACK
	#define HOST_INADDR_ANY INADDR_ANY
	// +6 for ':<port>' (max ':65535').
	#define HOST_INET_STRLEN (INET_ADDRSTRLEN + 6)

	#define HOST_IN6ADDR_LOOPBACK in6addr_loopback
	#define HOST_IN6ADDR_ANY in6addr_any
	// +6 for ':<port>' (max ':65535').
	// +2 for [] around IP.
	#define HOST_INET6_STRLEN (INET6_ADDRSTRLEN + 6 + 2)

	#define HOST_MAX_STRLEN HOST_INET6_STRLEN

	constexpr SockAddrFamily ADDR_FAMILY_NONE = 0;
	constexpr SockAddrFamily ADDR_FAMILY_IPV4 = AF_INET;
	constexpr SockAddrFamily ADDR_FAMILY_IPV6 = AF_INET6;
#else
	// Add here definitions for another platform.
#endif

	enum HostType
	{
		HOST_TYPE_NONE,
		HOST_TYPE_IPV4,
		HOST_TYPE_IPV6,
	};

	class Host
	{
	public:
		Host(
			const Sockaddr* aSockaddr);
		Host();
		explicit Host(
			const char* aHost);

		void Set(
			const Sockaddr* aSockaddr);
		// Works with port number. Doesn't work with domain names.
		bool Set(
			const char* aHost);
		bool Set(
			const std::string& aHost);
		void Clear();
		void SetPort(
			uint16_t aPort);

		const Sockaddr* GetSockaddr() const;
		uint32_t GetSockaddrSize() const;
		SockAddrFamily GetAddressFamily() const;
		uint16_t GetPort() const;

		bool IsIPV4() const;
		bool IsIPV6() const;
		bool IsSet() const;
		bool IsEqualNoPort(
			const Host& aOther) const;

		std::string ToString() const;
		std::string ToStringNoPort() const;

		bool operator==(
			const Host& aRhs) const;
		bool operator!=(
			const Host& aRhs) const;

		union
		{
			Sockaddr myAddr;
			SockaddrIn myAddrIn;
#if IS_IPV6_SUPPORTED
			SockaddrIn6 myAddrIn6;
#endif
			SockaddrStorage myAddrStorage;
		};
	};

	//////////////////////////////////////////////////////////////////////////////////////

	// Whether the string contains either an IPv4 or an IPv6 address. Note, port is not a
	// part of IP. <ip:port> format is not accepted as a valid IP address.
	bool HostIsIPAddress(
		const char* aStr);

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

	int InetPtoN(
		SockAddrFamily aFamily,
		const char* aStr,
		void* aOut);

	const char* InetNtoP(
		SockAddrFamily aFamily,
		const void* aAddr,
		char* aOut,
		uint32_t aSize);

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

#if IS_IPV6_SUPPORTED
	void SockaddrIn6Create(
		SockaddrIn6& aAddr);
#endif

	//////////////////////////////////////////////////////////////

	inline
	Host::Host()
	{
		Clear();
	}

	inline
	Host::Host(
		const char* aHost)
	{
		if (!Set(aHost))
			Clear();
	}

	inline bool
	Host::Set(
		const std::string& aHost)
	{
		return Set(aHost.c_str());
	}

	inline void
	Host::Clear()
	{
		memset(&myAddrStorage, 0, sizeof(myAddrStorage));
	}

	inline const Sockaddr*
	Host::GetSockaddr() const
	{
		MG_BOX_ASSERT(myAddr.sa_family != ADDR_FAMILY_NONE);
		return &myAddr;
	}

	inline SockAddrFamily
	Host::GetAddressFamily() const
	{
		MG_BOX_ASSERT(myAddr.sa_family != ADDR_FAMILY_NONE);
		return myAddr.sa_family;
	}

	inline bool
	Host::IsIPV4() const
	{
		return myAddr.sa_family == ADDR_FAMILY_IPV4;
	}

	inline bool
	Host::IsIPV6() const
	{
#if IS_IPV6_SUPPORTED
		return myAddr.sa_family == ADDR_FAMILY_IPV6;
#else
		return false;
#endif
	}

	inline bool
	Host::IsSet() const
	{
		return myAddr.sa_family != ADDR_FAMILY_NONE;
	}

	inline bool
	Host::operator!=(
		const Host& aRhs) const
	{
		return !(*this == aRhs);
	}

}
}

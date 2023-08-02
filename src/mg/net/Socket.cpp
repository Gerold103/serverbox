#include "Socket.h"

namespace mg {
namespace net {

	bool
	SocketBindAny(
		Socket aSock,
		SockAddrFamily aAddrFamily,
		mg::box::Error::Ptr& aOutErr)
	{
		Host host;
		switch(aAddrFamily)
		{
		case ADDR_FAMILY_IPV4:
			host = mg::net::HostMakeAllIPV4(0);
			break;
		case ADDR_FAMILY_IPV6:
			host = mg::net::HostMakeAllIPV6(0);
			break;
		default:
			MG_BOX_ASSERT(!"Unknown address family");
			break;
		}
		return SocketBind(aSock, host, aOutErr);
	}

}
}

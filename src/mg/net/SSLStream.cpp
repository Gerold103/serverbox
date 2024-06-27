#include "SSLStream.h"

#include "mg/net/SSLContext.h"
#include "mg/net/SSLStreamOpenSSL.h"

namespace mg {
namespace net {

	SSLStream::SSLStream(
		SSLContext* aCtx)
		: myImpl(aCtx->myImpl)
	{
	}

	SSLStream::~SSLStream() = default;

}
}

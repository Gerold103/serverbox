#include "SSLContext.h"

namespace mg {
namespace net {

	SSLContext::SSLContext(
		bool aIsServer)
		: myImpl(SSLContextBase::New(aIsServer))
	{
	}

	SSLContext::~SSLContext()
	{
		myImpl->Delete();
	}

}
}

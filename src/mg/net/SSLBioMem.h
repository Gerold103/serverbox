#pragma once

#include "mg/net/Buffer.h"
#include "mg/net/SSLContext.h"

#include <openssl/bio.h>

namespace mg {
namespace net {

	// This is a storage where SSL filter can store input and output data. But SSL library
	// can't operate on serverbox objects directly. For that the app is expected to
	// provide a proxy object.
	//
	// In OpenSSL it is BIO struct. Application can "inherit" it with such methods so they
	// would operate on serverbox storage under the hood.
	//
	// In mbedTLS there is API mbedtls_ssl_set_bio() which allows to proxy raw IO
	// per-sslcontext via callbacks.
	BIO* OpenSSLWrapBioMem(
		BufferStream& aStorage);

}
}

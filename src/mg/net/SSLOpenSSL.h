#pragma once

#include "mg/net/SSL.h"

#include <openssl/opensslv.h>

#if defined(OPENSSL_VERSION_PREREQ)
#if OPENSSL_VERSION_PREREQ(3, 0)
#define MG_OPENSSL_VERSION_GE_3_0 1
#endif
#endif

#ifndef MG_OPENSSL_VERSION_GE_3_0
#define MG_OPENSSL_VERSION_GE_3_0 0
#endif

// EVP key comparison function has only appeared in 3.0.
#define MG_OPENSSL_CTX_HAS_COMPARISON MG_OPENSSL_VERSION_GE_3_0

namespace mg {
namespace net {

	void OpenSSLInit();

	void OpenSSLDestroy();

	// Use for codes returned from various SSL functions. They are marked as "happened in
	// serverbox" then when the result is converted to a string.
	uint32_t OpenSSLPackError(
		int aCode);

	const char* OpenSSLErrorToString(
		uint32_t aCode);

	const char* OpenSSLCipherToString(
		SSLCipher aCipher);

	SSLCipher OpenSSLCipherFromString(
		const char* aName);

	int OpenSSLVersionToInt(
		SSLVersion aVersion);

	SSLVersion OpenSSLVersionFromInt(
		int aVersion);

}
}
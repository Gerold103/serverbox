#pragma once

#include "mg/box/Error.h"

namespace mg {
namespace net {

	enum SSLVersion
	{
		SSL_VERSION_TLSv1,
		SSL_VERSION_TLSv1_1,
		SSL_VERSION_TLSv1_2,
		SSL_VERSION_TLSv1_3,
		// Select the highest compatible version.
		SSL_VERSION_ANY,
		SSL_VERSION_DEFAULT = SSL_VERSION_TLSv1_2,
		SSL_VERSION_NONE,
	};

	enum SSLTrust
	{
		// Hardest verification.
		SSL_TRUST_STRICT = 0,
		// Accept self-signed certs, allow certain weak ciphers.
		SSL_TRUST_RELAXED = 1,
		// Accept untrusted certs.
		SSL_TRUST_UNSECURE = 2,
		// Cert is ignored, handshake proceeds regardless of any validation errors.
		SSL_TRUST_BYPASS_VERIFICATION = 3,
		SSL_TRUST_DEFAULT = SSL_TRUST_STRICT,
	};

	enum SSLCipher
	{
		// TLS 1.2
		SSL_CIPHER_ECDHE_ECDSA_AES128_GCM_SHA256,
		SSL_CIPHER_ECDHE_ECDSA_AES256_GCM_SHA384,
		// TLS 1.3
		SSL_CIPHER_TLS_AES_128_GCM_SHA256,
		SSL_CIPHER_TLS_AES_256_GCM_SHA384,
		SSL_CIPHER_TLS_CHACHA20_POLY1305_SHA256,

		SSL_CIPHER_NONE,
	};

	const char* SSLErrorToString(
		uint32_t aCode);

	mg::box::ErrorPtrRaised ErrorRaiseSSL(
		uint32_t aCode);

	mg::box::ErrorPtrRaised ErrorRaiseSSL(
		uint32_t aCode,
		const char* aComment);

	MG_STRFORMAT_PRINTF(2, 3)
	mg::box::ErrorPtrRaised ErrorRaiseFormatSSL(
		uint32_t aCode,
		const char* aCommentFormat,
		...);

}
}

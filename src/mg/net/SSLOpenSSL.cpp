#include "mg/net/SSLOpenSSL.h"

#include "mg/box/StringFunctions.h"
#include "mg/net/SSLContextOpenSSL.h"

#include <algorithm>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

// OpenSSL headers define those macros and mess with the STL functions std::min/max.
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace mg {
namespace net {

namespace {
	// Match serverbox cipher ID to OpenSSL cipher name.
	struct OpenSSLCipherDef
	{
		SSLCipher myMgID;
		const char* myLibName;
	};

	// Match serverbox version ID to OpenSSL version ID.
	struct OpenSSLVersionDef
	{
		SSLVersion myMgID;
		int myLibID;
	};
}

	static const OpenSSLCipherDef theOpenSSLCipherDefs[] = {
		{SSL_CIPHER_ECDHE_ECDSA_AES128_GCM_SHA256, "ECDHE-ECDSA-AES128-GCM-SHA256"},
		{SSL_CIPHER_ECDHE_ECDSA_AES256_GCM_SHA384, "ECDHE-ECDSA-AES256-GCM-SHA384"},
		{SSL_CIPHER_TLS_AES_128_GCM_SHA256, "TLS_AES_128_GCM_SHA256"},
		{SSL_CIPHER_TLS_AES_256_GCM_SHA384, "TLS_AES_256_GCM_SHA384"},
		{SSL_CIPHER_TLS_CHACHA20_POLY1305_SHA256, "TLS_CHACHA20_POLY1305_SHA256"},
	};

	static const OpenSSLVersionDef theOpenSSLVersionDefs[] = {
		{SSL_VERSION_TLSv1, TLS1_VERSION},
		{SSL_VERSION_TLSv1_1, TLS1_1_VERSION},
		{SSL_VERSION_TLSv1_2, TLS1_2_VERSION},
		{SSL_VERSION_TLSv1_3, TLS1_3_VERSION},
	};

	static void* OpenSSLMemMalloc(
		size_t aSize,
		const char* aFile,
		int aLine);

	static void* OpenSSLMemRealloc(
		void* aPtr,
		size_t aNewSize,
		const char* aFile,
		int aLine);

	static void OpenSSLMemFree(
		void* aPtr,
		const char* aFile,
		int aLine);

	static int OpenSSLCreateErrors();

	static int OpenSSLGetErrorID();

	//////////////////////////////////////////////////////////////////////////////////////

	void
	OpenSSLInit()
	{
		CRYPTO_set_mem_functions(&OpenSSLMemMalloc, &OpenSSLMemRealloc, &OpenSSLMemFree);
		OPENSSL_init_ssl(OPENSSL_INIT_ADD_ALL_CIPHERS |
			OPENSSL_INIT_ADD_ALL_DIGESTS |
			OPENSSL_INIT_LOAD_SSL_STRINGS |
			OPENSSL_INIT_LOAD_CRYPTO_STRINGS |
			OPENSSL_INIT_NO_ATEXIT,
			nullptr);
	}

	void
	OpenSSLDestroy()
	{
		OPENSSL_cleanup();
	}

	//////////////////////////////////////////////////////////////////////////////////////

	uint32_t
	OpenSSLPackError(
		int aCode)
	{
		return ERR_PACK(OpenSSLGetErrorID(), 0, aCode);
	}

	const char*
	OpenSSLErrorToString(
		uint32_t aCode)
	{
		return ERR_reason_error_string(aCode);
	}

	const char*
	OpenSSLCipherToString(
		SSLCipher aCipher)
	{
		for (const OpenSSLCipherDef& def : theOpenSSLCipherDefs)
		{
			if (aCipher == def.myMgID)
				return def.myLibName;
		}
		return nullptr;
	}

	SSLCipher
	OpenSSLCipherFromString(
		const char* aName)
	{
		if (aName == nullptr)
			return SSL_CIPHER_NONE;
		for (const OpenSSLCipherDef& def : theOpenSSLCipherDefs)
		{
			if (mg::box::Strcmp(aName, def.myLibName) == 0)
				return def.myMgID;
		}
		return SSL_CIPHER_NONE;
	}

	int
	OpenSSLVersionToInt(
		SSLVersion aVersion)
	{
		for (const OpenSSLVersionDef& def : theOpenSSLVersionDefs)
		{
			if (def.myMgID == aVersion)
				return def.myLibID;
		}
		MG_BOX_ASSERT(false);
		return -1;
	}

	SSLVersion
	OpenSSLVersionFromInt(
		int aVersion)
	{
		for (const OpenSSLVersionDef& def : theOpenSSLVersionDefs)
		{
			if (def.myLibID == aVersion)
				return def.myMgID;
		}
		MG_BOX_ASSERT(false);
		return SSL_VERSION_NONE;
	}

	//////////////////////////////////////////////////////////////////////////////////////

	static void*
	OpenSSLMemMalloc(
		size_t aSize,
		const char* /* aFile */,
		int /* aLine */)
	{
		uint8_t* result = new uint8_t[aSize + sizeof(size_t)];
		*(size_t*)result = aSize;
		return result + sizeof(size_t);
	}

	static void*
	OpenSSLMemRealloc(
		void* aPtr,
		size_t aNewSize,
		const char* aFile,
		int aLine)
	{
		if (aPtr == nullptr)
			return OpenSSLMemMalloc(aNewSize, aFile, aLine);

		uint8_t* origPtr = (uint8_t*)aPtr - sizeof(size_t);
		const size_t oldSize = *(size_t*)origPtr;
		// XXX: why not check if the old size is enough before doing an actual realloc?
		void* newPtr = OpenSSLMemMalloc(aNewSize, aFile, aLine);
		memcpy(newPtr, aPtr, std::min(oldSize, aNewSize));
		delete[] origPtr;
		return newPtr;
	}

	static void
	OpenSSLMemFree(
		void* aPtr,
		const char* /* aFile */,
		int /* aLine */)
	{
		if (aPtr != nullptr)
			delete[]((uint8_t*)aPtr - sizeof(size_t));
	}

	static int
	OpenSSLCreateErrors()
	{
		// OpenSSL errors are packed as {lib ID, func, reason}. This is the lib ID used
		// for errors raised from serverbox.
		int id = ERR_get_next_error_library();
		// This actually seems necessary. Default error strings even for built-in errors
		// report nothing. For instance, SSL_ERROR_SYSCALL is by default 'DH lib'.
		static ERR_STRING_DATA errorReasons[] =
		{
			{OpenSSLPackError(SSL_ERROR_NONE), "ssl_error_none"},
			{OpenSSLPackError(SSL_ERROR_SSL), "ssl_error_ssl"},
			{OpenSSLPackError(SSL_ERROR_WANT_READ), "ssl_error_want_read"},
			{OpenSSLPackError(SSL_ERROR_WANT_WRITE), "ssl_error_want_write"},
			{OpenSSLPackError(SSL_ERROR_WANT_X509_LOOKUP), "ssl_error_want_x509_lookup"},
			{OpenSSLPackError(SSL_ERROR_SYSCALL), "ssl_error_syscall"},
			{OpenSSLPackError(SSL_ERROR_ZERO_RETURN), "ssl_error_zero_return"},
			{OpenSSLPackError(SSL_ERROR_WANT_CONNECT), "ssl_error_want_connect"},
			{OpenSSLPackError(SSL_ERROR_WANT_ACCEPT), "ssl_error_want_accept"},
			{OpenSSLPackError(SSL_ERROR_WANT_ASYNC), "ssl_error_want_async"},
			{OpenSSLPackError(SSL_ERROR_WANT_ASYNC_JOB), "ssl_error_want_async_job"},
			{OpenSSLPackError(SSL_ERROR_WANT_CLIENT_HELLO_CB), "ssl_error_want_client_hello_cb"},
			{0, nullptr}
		};
		if (ERR_reason_error_string(errorReasons[0].error) == nullptr)
			ERR_load_strings(id, errorReasons);
		return id;
	}

	static int
	OpenSSLGetErrorID()
	{
		static int id = OpenSSLCreateErrors();
		return id;
	}

}
}

#pragma once

#include "mg/net/SSL.h"
#include "mg/net/SSLOpenSSL.h"

#include <openssl/conf.h>
#include <openssl/ssl.h>
#include <vector>

namespace mg {
namespace net {

	class SSLStreamOpenSSL;

	class SSLContextOpenSSL
	{
	private:
		SSLContextOpenSSL(
			bool aIsServer);
		SSLContextOpenSSL(
			const SSLContextOpenSSL&) = delete;
		~SSLContextOpenSSL();

	public:
		static SSLContextOpenSSL* New(
			bool aIsServer) { return new SSLContextOpenSSL(aIsServer); }

		void Delete();

	public:
		bool AddRemoteCertFile(
			const char* aFile);

		bool AddRemoteCert(
			const void* aData,
			uint32_t aSize);

		bool AddRemoteCertFromSystem();

		bool AddLocalCertFile(
			const char* aCertFile,
			const char* aKeyFile,
			const char* aPassword);

		bool AddLocalCert(
			const void* aCertData,
			uint32_t aCertSize,
			const void* aKeyData,
			uint32_t aKeySize);

		bool AddCipher(
			SSLCipher aCipher);

		bool SetTrust(
			SSLTrust aTrust);

		bool SetMinVersion(
			SSLVersion aVersion);

		bool SetMaxVersion(
			SSLVersion aVersion);

		bool IsServer() const { return myIsServer; }

#if MG_OPENSSL_CTX_HAS_COMPARISON
		bool operator==(
			const SSLContextOpenSSL& aOther) const;
#endif

		void PrivOn_SSL_CTX_Delete();

	private:
		SSL_CTX* myConfig;
		SSLTrust myTrust;
		bool myIsServer;
		bool myHasLocalCert;
		bool myIsDeleted;
		std::string myPassword;
		std::vector<SSLCipher> myCiphers;

		friend SSLStreamOpenSSL;
	};

	const SSLContextOpenSSL* OpenSSL_SSL_ToContext(
		const SSL* aSSL);

}
}

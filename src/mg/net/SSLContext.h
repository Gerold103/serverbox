#pragma once

#include "mg/net/SSLContextOpenSSL.h"

namespace mg {
namespace net {

	class SSLStream;
	using SSLContextBase = SSLContextOpenSSL;

	//////////////////////////////////////////////////////////////////////////////////////

	// SSL configuration data from which SSL connections/streams can be spawned.
	class SSLContext
	{
	public:
		SHARED_PTR_API(SSLContext, myRef)

	public:
		bool AddRemoteCertFile(
			const char* aFile) { return myImpl->AddRemoteCertFile(aFile); }

		bool AddRemoteCert(
			const void* aData,
			uint32_t aSize) { return myImpl->AddRemoteCert(aData, aSize); }

		bool AddRemoteCertFromSystem() { return myImpl->AddRemoteCertFromSystem(); }

		bool AddLocalCertFile(
			const char* aCertFile,
			const char* aKeyFile,
			const char* aPassword)
			{ return myImpl->AddLocalCertFile(aCertFile, aKeyFile, aPassword); }

		bool AddLocalCert(
			const void* aCertData,
			uint32_t aCertSize,
			const void* aKeyData,
			uint32_t aKeySize)
			{ return myImpl->AddLocalCert(aCertData, aCertSize, aKeyData, aKeySize); }

		bool AddCipher(
			SSLCipher aCipher) { return myImpl->AddCipher(aCipher); }

		bool SetTrust(
			SSLTrust aTrust) { return myImpl->SetTrust(aTrust); }

		bool SetMinVersion(
			SSLVersion aVersion) { return myImpl->SetMinVersion(aVersion); }

		bool SetMaxVersion(
			SSLVersion aVersion) { return myImpl->SetMaxVersion(aVersion); }

		bool IsServer() const { return myImpl->IsServer(); }

#if MG_OPENSSL_CTX_HAS_COMPARISON
		bool operator==(
			const SSLContext& aOther) const { return *myImpl == *aOther.myImpl; }
		bool operator!=(
			const SSLContext& aOther) const { return !(*this == aOther); }
#endif

	private:
		SSLContext(
			bool aIsServer);
		SSLContext(
			const SSLContext&) = delete;
		SSLContext& operator=(
			const SSLContext&) const = delete;
		~SSLContext();

		SSLContextBase* myImpl;
		mg::box::RefCount myRef;

		friend SSLStream;
	};

}
}

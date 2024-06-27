#include "SSLContextOpenSSL.h"

#include "mg/box/Log.h"
#include "mg/box/StringFunctions.h"
#include "mg/net/SSLOpenSSL.h"

#include <algorithm>
#include <openssl/err.h>
#include <openssl/evp.h>
#if IS_PLATFORM_WIN
#include <wincrypt.h>
#endif

namespace mg {
namespace net {

	static int OpenSSLVerifyCertStrict(
		int aIsVerified,
		X509_STORE_CTX* aStoreCtx);

	static int OpenSSLVerifyCertRelaxed(
		int aIsVerified,
		X509_STORE_CTX*	aStoreCtx);

	static int OpenSSLVerifyCertUnsecure(
		int aIsVerified,
		X509_STORE_CTX* aStoreCtx);

	static int OpenSSLLoadPassword(
		char* aOutBuffer,
		int aBufferSize,
		int aRWFlag,
		void* aUserData);

	static bool OpenSSLVerifyCert(
		const char* aFile,
		bool aIsAllowedRSA);

	static bool OpenSSLImportSystemCerts(
		SSL_CTX* aConfig);

	static X509* OpenSSLCertLoadPEM(
		const char* aData,
		uint32_t aSize);

	static X509* OpenSSLCertLoadDER(
		const void* aData,
		uint32_t aSize);

#if MG_OPENSSL_CTX_HAS_COMPARISON

	static bool OpenSSLCertChainAreEqual(
		const STACK_OF(X509_OBJECT)* aStack1,
		const STACK_OF(X509_OBJECT)* aStack2);

	static bool OpenSSLCipherChainAreEqual(
		const STACK_OF(SSL_CIPHER)* aStack1,
		const STACK_OF(SSL_CIPHER)* aStack2);

#endif

	static bool OpenSSLVersionDoesSupportCipher(
		SSLVersion aVersion,
		SSLCipher aCipher);

	static void OpenSSL_SSL_CTX_IndexContextFree(
		void* aParent,
		void* aPtr,
		CRYPTO_EX_DATA* aAd,
		int aIndex,
		long aArgl,
		void* aArgp);

	static int OpenSSLGetContextIndex();

	//////////////////////////////////////////////////////////////////////////////////////

	const SSLContextOpenSSL*
	OpenSSL_SSL_ToContext(
		const SSL* aSSL)
	{
		const SSL_CTX* sslCtx = SSL_get_SSL_CTX(aSSL);
		int idx = OpenSSLGetContextIndex();
		return (const SSLContextOpenSSL*)SSL_CTX_get_ex_data(sslCtx, idx);
	}

	//////////////////////////////////////////////////////////////////////////////////////

	SSLContextOpenSSL::SSLContextOpenSSL(
		bool aIsServer)
		: myConfig(nullptr)
		, myTrust(SSL_TRUST_DEFAULT)
		, myIsServer(aIsServer)
		, myHasLocalCert(false)
		, myIsDeleted(false)
	{
		if (aIsServer)
			myConfig = SSL_CTX_new(TLS_server_method());
		else
			myConfig = SSL_CTX_new(TLS_client_method());
		MG_BOX_ASSERT(myConfig != nullptr);

		SSL_CTX_set_security_level(myConfig, 0);
		// The object's lifetime is tied to SSL_CTX via its reference counter. SSL_CTX is
		// kept alive by SSLs along with this object. Even if the object's owner
		// unreferences it and won't have any more refs in own code, the object still
		// lives until all SSLs are deleted. It allows to use object's members in various
		// callbacks which are activated by SSLs.
		SSL_CTX_set_ex_data(myConfig, OpenSSLGetContextIndex(), this);
	}

	SSLContextOpenSSL::~SSLContextOpenSSL()
	{
		// It should be called from SSL_CTX destructor whose members are already invalid
		// by now. It should be nullified then. It also protects from attempts to call
		// 'delete' on the object in user's code.
		MG_BOX_ASSERT(myConfig == nullptr);
		MG_BOX_ASSERT(myIsDeleted);
	}

	void
	SSLContextOpenSSL::Delete()
	{
		MG_BOX_ASSERT(!myIsDeleted);
		myIsDeleted = true;
		// Context's lifetime is tied to the base SSL_CTX object via an 'ex data' index.
		SSL_CTX_free(myConfig);
	}

	bool
	SSLContextOpenSSL::AddRemoteCertFile(
		const char* aFile)
	{
		constexpr bool yesRSA = true;
		constexpr bool noRSA = false;
		bool ok = false;
		switch (myTrust)
		{
		case SSL_TRUST_STRICT:
			ok = OpenSSLVerifyCert(aFile, noRSA);
			break;
		case SSL_TRUST_RELAXED:
			ok = OpenSSLVerifyCert(aFile, yesRSA);
			break;
		case SSL_TRUST_UNSECURE:
			ok = OpenSSLVerifyCert(aFile, yesRSA);
			break;
		default:
			MG_BOX_ASSERT(false);
			break;
		}
		if (!ok)
			return false;
		ERR_clear_error();
		return SSL_CTX_load_verify_locations(myConfig, aFile, nullptr) != 0;
	}

	bool
	SSLContextOpenSSL::AddRemoteCert(
		const void* aData,
		uint32_t aSize)
	{
		// A useful practice taken from mbedtls - allow both PEM and DER, detect them
		// automatically. The caller then doesn't need to care about a format.
		X509* cert = OpenSSLCertLoadDER(aData, aSize);
		if (cert == nullptr)
		{
			cert = OpenSSLCertLoadPEM((const char*)aData, aSize);
			if (cert == nullptr)
				return false;
		}

		EVP_PKEY* pubKey = X509_get_pubkey(cert);
		int pubKeyType = EVP_PKEY_base_id(pubKey);

		MG_BOX_ASSERT(pubKey != nullptr);
		MG_BOX_ASSERT(pubKeyType == EVP_PKEY_EC);

		EVP_PKEY_free(pubKey);

		X509_STORE* store = SSL_CTX_get_cert_store(myConfig);
		int rc = X509_STORE_add_cert(store, cert);
		X509_free(cert);
		return rc == 1;
	}

	bool
	SSLContextOpenSSL::AddRemoteCertFromSystem()
	{
		return OpenSSLImportSystemCerts(myConfig);
	}

	bool
	SSLContextOpenSSL::AddLocalCertFile(
		const char* aCertFile,
		const char* aKeyFile,
		const char* aPassword)
	{
		MG_BOX_ASSERT(!myHasLocalCert);
		const bool yesRSA = true;
		const bool noRSA = false;
		bool ok = false;
		switch (myTrust)
		{
		case SSL_TRUST_STRICT:
			ok = OpenSSLVerifyCert(aCertFile, noRSA);
			break;
		case SSL_TRUST_RELAXED:
			ok = OpenSSLVerifyCert(aCertFile, yesRSA);
			break;
		case SSL_TRUST_BYPASS_VERIFICATION:
			ok = OpenSSLVerifyCert(aCertFile, yesRSA);
			break;
		default:
			MG_BOX_ASSERT(false);
			break;
		}
		if (!ok)
			return false;
		// OpenSSL docs do not say how long the passwords used for private keys loading
		// should stay valid. That forces to store them all the time while SSL_CTX is
		// valid.
		myPassword = aPassword;
		SSL_CTX_set_default_passwd_cb(myConfig, OpenSSLLoadPassword);
		SSL_CTX_set_default_passwd_cb_userdata(myConfig, (void*)myPassword.c_str());

		if (SSL_CTX_use_certificate_file(myConfig, aCertFile, SSL_FILETYPE_PEM) == 0)
			return false;
		if (SSL_CTX_use_PrivateKey_file(myConfig, aKeyFile, SSL_FILETYPE_PEM) == 0)
			return false;
		if (SSL_CTX_check_private_key(myConfig) != 1)
			return false;
		myHasLocalCert = true;
		return true;
	}

	bool
	SSLContextOpenSSL::AddLocalCert(
		const void* aCertData,
		uint32_t aCertSize,
		const void* aKeyData,
		uint32_t aKeySize)
	{
		MG_BOX_ASSERT(!myHasLocalCert);
		MG_BOX_ASSERT(aCertData != nullptr);
		MG_BOX_ASSERT(aKeyData != nullptr);
		bool ok = false;
		EVP_PKEY* pubKey = nullptr;
		EVP_PKEY* privKey = nullptr;
		int privKeyType;
		int pubKeyType;
		X509* cert = d2i_X509(nullptr, (const unsigned char**)&aCertData, aCertSize);
		if (cert == nullptr)
		{
			MG_LOG_ERROR("mgnet.ssl.add_local_cert", "couldn't load a cert");
			goto finish;
		}

		pubKey = X509_get_pubkey(cert);
		MG_BOX_ASSERT(pubKey != nullptr);
		pubKeyType = EVP_PKEY_base_id(pubKey);
		switch (myTrust)
		{
		case SSL_TRUST_STRICT:
			ok = pubKeyType == EVP_PKEY_EC;
			break;
		case SSL_TRUST_RELAXED:
		case SSL_TRUST_BYPASS_VERIFICATION:
			ok = pubKeyType == EVP_PKEY_EC || pubKeyType == EVP_PKEY_RSA;
			break;
		default:
			MG_BOX_ASSERT(false);
			break;
		}
		if (!ok)
		{
			MG_LOG_ERROR("mgnet.ssl.add_local_cert", "not allowed key type");
			goto finish;
		}
		ok = false;

		privKey = d2i_PrivateKey(EVP_PKEY_EC, nullptr,
			(const unsigned char**)&aKeyData, aKeySize);
		if (privKey == nullptr)
		{
			MG_LOG_ERROR("mgnet.ssl.add_local_cert", "couldn't load a private key");
			goto finish;
		}
		privKeyType = EVP_PKEY_base_id(privKey);
		MG_BOX_ASSERT(privKeyType == EVP_PKEY_EC);

		ok = SSL_CTX_use_certificate(myConfig, cert) != 0 &&
			SSL_CTX_use_PrivateKey(myConfig, privKey) != 0 &&
			SSL_CTX_check_private_key(myConfig) != 0;
		if (!ok) {
			MG_LOG_ERROR("mgnet.ssl.add_local_cert",
				"couldn't use provided cert and key");
			goto finish;
		}
		myHasLocalCert = true;
finish:
		if (privKey != nullptr)
			EVP_PKEY_free(privKey);
		if (pubKey != nullptr)
			EVP_PKEY_free(pubKey);
		if (cert != nullptr)
			X509_free(cert);
		return ok;
	}

	bool
	SSLContextOpenSSL::AddCipher(
		SSLCipher aCipher)
	{
		if (std::find(myCiphers.begin(), myCiphers.end(), aCipher) != myCiphers.end())
			return true;

		// OpenSSL API has separate cipher set functions for <= TLSv1.2 and >= TLSv1.3.
		// One solution would be to just call both with all ciphers together and see if
		// any succeeded. But it won't work. TLSv1.2 setter simply skips not suitable
		// ciphers. But TLSv1.3 setter fails the entire list if just one didn't work. Need
		// to set them separately.
		SSLVersion version;
		if (OpenSSLVersionDoesSupportCipher(SSL_VERSION_TLSv1_3, aCipher))
			version = SSL_VERSION_TLSv1_3;
		else
			version = SSL_VERSION_TLSv1_2;

		std::vector<SSLCipher> ciphList;
		for (SSLCipher ciph : myCiphers)
		{
			if (OpenSSLVersionDoesSupportCipher(version, ciph))
				ciphList.push_back(ciph);
		}
		// Add to the end - order matters.
		ciphList.push_back(aCipher);

		std::string ciphsStr;
		const char* delim = "";
		for (SSLCipher ciph : ciphList)
		{
			ciphsStr += delim;
			ciphsStr += OpenSSLCipherToString(ciph);
			delim = ":";
		}
		int rc;
		if (version == SSL_VERSION_TLSv1_2)
			rc = SSL_CTX_set_cipher_list(myConfig, ciphsStr.c_str());
		else
			rc = SSL_CTX_set_ciphersuites(myConfig, ciphsStr.c_str());
		if (rc != 1)
			return false;
		myCiphers.push_back(aCipher);
		return true;
	}

	bool
	SSLContextOpenSSL::SetTrust(
		SSLTrust aTrust)
	{
		if (aTrust == SSL_TRUST_STRICT)
		{
			SSL_CTX_set_verify(myConfig,
				SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
				OpenSSLVerifyCertStrict);
		}
		else if (aTrust == SSL_TRUST_RELAXED)
		{
			SSL_CTX_set_verify(myConfig,
				SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
				OpenSSLVerifyCertRelaxed);
		}
		else if (aTrust == SSL_TRUST_UNSECURE)
		{
			SSL_CTX_set_verify(myConfig,
				SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
				OpenSSLVerifyCertUnsecure);
		}
		else if (aTrust == SSL_TRUST_BYPASS_VERIFICATION)
		{
			SSL_CTX_set_verify(myConfig, SSL_VERIFY_NONE, nullptr);
		}
		else
		{
			MG_BOX_ASSERT(false);
		}
		myTrust = aTrust;
		return true;
	}

	bool
	SSLContextOpenSSL::SetMinVersion(
		SSLVersion aVersion)
	{
		int ver;
		if (aVersion == SSL_VERSION_ANY)
			ver = TLS1_VERSION;
		else
			ver = OpenSSLVersionToInt(aVersion);
		if (SSL_CTX_set_min_proto_version(myConfig, ver) != 1)
			return false;
		// OpenSSL API allows to set an impossible version range. Need to correct it
		// manually, otherwise all new sessions will break immediately.
		int maxVer = SSL_CTX_get_max_proto_version(myConfig);
		if (maxVer != 0 && maxVer < ver)
			return SSL_CTX_set_max_proto_version(myConfig, ver) == 1;
		return true;
	}

	bool
	SSLContextOpenSSL::SetMaxVersion(
		SSLVersion aVersion)
	{
		int ver;
		if (aVersion == SSL_VERSION_ANY)
			ver = TLS1_3_VERSION;
		else
			ver = OpenSSLVersionToInt(aVersion);
		if (SSL_CTX_set_max_proto_version(myConfig, ver) != 1)
			return false;
		// OpenSSL API allows to set an impossible version range. Need to correct it
		// manually, otherwise all new sessions will break immediately.
		int minVer = SSL_CTX_get_min_proto_version(myConfig);
		if (minVer != 0 && minVer > ver)
			return SSL_CTX_set_min_proto_version(myConfig, ver) == 1;
		return true;
	}

#if MG_OPENSSL_CTX_HAS_COMPARISON

	bool
	SSLContextOpenSSL::operator==(
		const SSLContextOpenSSL& aOther) const
	{
		// The function does not cover all the possible SSL_CTX methods. For example,
		// SSL_CTX_get0_chain_certs() in all tests always returned an empty result.
		// Perhaps, it works only if previously SSL_CTX_add0_chain_cert() was
		// called, but it is never done in serverbox. There can be more SSL_CTX attributes
		// not covered here and not used in serverbox so far.

		if (myTrust != aOther.myTrust)
			return false;
		if (myCiphers != aOther.myCiphers)
			return false;
		if (myIsServer != aOther.myIsServer)
			return false;

		const SSL_CTX* ctx1 = myConfig;
		const SSL_CTX* ctx2 = aOther.myConfig;

		if (SSL_CTX_get_min_proto_version((SSL_CTX*)ctx1) !=
			SSL_CTX_get_min_proto_version((SSL_CTX*)ctx2))
			return false;
		if (SSL_CTX_get_max_proto_version((SSL_CTX*)ctx1) !=
			SSL_CTX_get_max_proto_version((SSL_CTX*)ctx2))
			return false;

		X509_STORE* store1 = SSL_CTX_get_cert_store(ctx1);
		X509_STORE* store2 = SSL_CTX_get_cert_store(ctx2);
		STACK_OF(X509_OBJECT)* objs1 = X509_STORE_get0_objects(store1);
		STACK_OF(X509_OBJECT)* objs2 = X509_STORE_get0_objects(store2);
		if (!OpenSSLCertChainAreEqual(objs1, objs2))
			return false;

		// The currently used certificate somewhy not always is present in the object
		// stack checked above.
		X509* cert1 = SSL_CTX_get0_certificate(ctx1);
		X509* cert2 = SSL_CTX_get0_certificate(ctx2);
		if ((cert1 == nullptr) != (cert2 == nullptr))
			return false;
		if (cert1 != nullptr && X509_cmp(cert1, cert2) != 0)
			return false;

		// Ciphers are different, for instance, when turn on or do not turn on ECDHE. The
		// stacks do not need freeing. It is not said in the documentation, but the
		// experiments have shown that.
		STACK_OF(SSL_CIPHER)* ciphs1 = SSL_CTX_get_ciphers(ctx1);
		STACK_OF(SSL_CIPHER)* ciphs2 = SSL_CTX_get_ciphers(ctx2);
		if (!OpenSSLCipherChainAreEqual(ciphs1, ciphs2))
			return false;

		EVP_PKEY* key1 = SSL_CTX_get0_privatekey(myConfig);
		EVP_PKEY* key2 = SSL_CTX_get0_privatekey(aOther.myConfig);
		if ((key1 == nullptr) != (key2 == nullptr))
			return false;
		// Key cmp returns 1 when equal, on the contrary with x509 cmp. The API is just
		// inconsistent.
		if (key1 != nullptr && EVP_PKEY_eq(key1, key2) != 1)
			return false;

		if (SSL_CTX_get_verify_mode(ctx1) != SSL_CTX_get_verify_mode(ctx2))
			return false;
		if (SSL_CTX_get_ssl_method(ctx1) != SSL_CTX_get_ssl_method(ctx2))
			return false;
		if (SSL_CTX_get_verify_callback(ctx1) != SSL_CTX_get_verify_callback(ctx2))
			return false;

		return true;
	}

#endif

	void
	SSLContextOpenSSL::PrivOn_SSL_CTX_Delete()
	{
		MG_BOX_ASSERT(myConfig != nullptr);
		// SSL_CTX is already deleted by now. Nullify it for sanity checks.
		myConfig = nullptr;
		delete this;
	}

	//////////////////////////////////////////////////////////////////////////////////////

	static int
	OpenSSLVerifyCertStrict(
		int aIsVerified,
		X509_STORE_CTX* aStoreCtx)
	{
		if (aIsVerified)
			return aIsVerified;
		int err = X509_STORE_CTX_get_error(aStoreCtx);
		if (err != X509_V_OK)
		{
			MG_LOG_ERROR("mgnet.ssl.verify_strict", "error: %s",
				X509_verify_cert_error_string(err));
			return 0;
		}
		return 1;
	}

	static int
	OpenSSLVerifyCertRelaxed(
		int aIsVerified,
		X509_STORE_CTX*	aStoreCtx)
	{
		if (aIsVerified)
			return aIsVerified;
		int err = X509_STORE_CTX_get_error(aStoreCtx);
		if (err != X509_V_OK && err != X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN)
		{
			MG_LOG_ERROR("mgnet.ssl.verify_relaxed", "error: %s",
				X509_verify_cert_error_string(err));
			return 0;
		}
		return 1;
	}

	static int
	OpenSSLVerifyCertUnsecure(
		int aIsVerified,
		X509_STORE_CTX*	aStoreCtx)
	{
		if (aIsVerified)
			return aIsVerified;

		int err = X509_STORE_CTX_get_error(aStoreCtx);
		if(	err != X509_V_OK &&
			err != X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY &&
			err != X509_V_ERR_CERT_UNTRUSTED)
		{
			MG_LOG_ERROR("mgnet.ssl.verify_unsecure", "error: %s",
				X509_verify_cert_error_string(err));
			return 0;
		}
		return 1;
	}

	static int
	OpenSSLLoadPassword(
		char* aOutBuffer,
		int aBufferSize,
		int /* aRWFlag */,
		void* aUserData)
	{
		uint32_t len = mg::box::Strlen((const char*)aUserData);
		MG_BOX_ASSERT(len < (uint32_t)aBufferSize);
		memcpy(aOutBuffer, aUserData, len);
		aOutBuffer[len] = 0;
		return (int)len;
	}

	static bool
	OpenSSLVerifyCert(
		const char* aFile,
		bool aIsAllowedRSA)
	{
		X509* x509 = nullptr;
		EVP_PKEY* pubKey = nullptr;
		bool ok = false;
		int pubKeyType;
		FILE* f = fopen(aFile, "r");
		if (f == nullptr)
		{
			MG_LOG_ERROR("mgnet.ssl.verify_cert", "failed to open %s: %s", aFile,
				mg::box::ErrorRaiseErrno()->myMessage.c_str());
			goto finish;
		}

		PEM_read_X509(f, &x509, nullptr, nullptr);
		if (x509 == nullptr)
		{
			MG_LOG_ERROR("mgnet.ssl.verify_cert", "couldn't read cert from %s",
				aFile);
			goto finish;
		}

		pubKey = X509_get_pubkey(x509);
		if (pubKey == nullptr)
		{
			MG_LOG_ERROR("mgnet.ssl.verify_cert", "no pubkey in %s", aFile);
			goto finish;
		}

		pubKeyType = EVP_PKEY_base_id(pubKey);
		if (aIsAllowedRSA)
			ok = pubKeyType == EVP_PKEY_EC || pubKeyType == EVP_PKEY_RSA;
		else
			ok = pubKeyType == EVP_PKEY_EC;
		if (!ok)
		{
			MG_LOG_ERROR("mgnet.ssl.verify_cert", "not allowed key type");
			goto finish;
		}

		ok = true;
finish:
		if (f != nullptr)
			fclose(f);
		if (pubKey != nullptr)
			EVP_PKEY_free(pubKey);
		if (x509 != nullptr)
			X509_free(x509);
		return ok;
	}

#if IS_PLATFORM_WIN
	static void
	OpenSSLImportCertStore(
		const char* aName,
		SSL_CTX* aConfig)
	{
		HANDLE sysStore = CertOpenSystemStoreA(NULL, aName);
		if (sysStore == nullptr)
			return;
		X509_STORE* store = SSL_CTX_get_cert_store(aConfig);
		MG_BOX_ASSERT(store != nullptr);

		PCCERT_CONTEXT cert = nullptr;
		while ((cert = CertEnumCertificatesInStore(sysStore, cert)) != nullptr)
		{
			if (CertVerifyTimeValidity(nullptr, cert->pCertInfo) != 0)
				continue;

			const unsigned char* certData = cert->pbCertEncoded;
			X509* x509 = d2i_X509(nullptr, &certData, cert->cbCertEncoded);
			if (x509 == nullptr)
				continue;

			X509_STORE_add_cert(store, x509);
			X509_free(x509);
		}
		CertCloseStore(sysStore, 0);
	}
#endif

	static bool
	OpenSSLImportSystemCerts(
		SSL_CTX* aConfig)
	{
#if IS_PLATFORM_WIN
		OpenSSLImportCertStore("ROOT", aConfig);
		OpenSSLImportCertStore("CA", aConfig);
#elif IS_PLATFORM_APPLE
		MG_UNUSED(aConfig);
		MG_BOX_ASSERT(!"Not implemented");
#else
		SSL_CTX_load_verify_locations(aConfig, nullptr, "/etc/ssl/certs");
#endif
		return true;
	}

	static X509*
	OpenSSLCertLoadPEM(
		const char* aData,
		uint32_t aSize)
	{
		BIO *bio = BIO_new(BIO_s_mem());
		if (BIO_write(bio, (const void*) aData, aSize) == 0)
		{
			int ec = ERR_get_error();
			char buf[256];
			ERR_error_string(ec, buf);
			BIO_free(bio);
			MG_BOX_ASSERT_F(false, "Failed to write PEM to bio. Error: %s", buf);
			return nullptr;
		}
		X509* cert = PEM_read_bio_X509(bio, nullptr, 0, nullptr);
		BIO_free(bio);
		return cert;
	}

	static X509*
	OpenSSLCertLoadDER(
		const void* aData,
		uint32_t aSize)
	{
		return d2i_X509(nullptr, (const unsigned char**)&aData, aSize);
	}

#if MG_OPENSSL_CTX_HAS_COMPARISON

	static bool
	OpenSSLCertChainAreEqual(
		const STACK_OF(X509_OBJECT)* aStack1,
		const STACK_OF(X509_OBJECT)* aStack2)
	{
		if ((aStack1 == nullptr) != (aStack2 == nullptr))
			return false;
		if (aStack1 == nullptr)
			return true;
		int cnt1 = sk_X509_OBJECT_num(aStack1);
		int cnt2 = sk_X509_OBJECT_num(aStack2);
		if (cnt1 != cnt2)
			return false;
		for (int i = 0; i < cnt1; ++i)
		{
			X509_OBJECT* obj1 = sk_X509_OBJECT_value(aStack1, i);
			X509_OBJECT* obj2 = sk_X509_OBJECT_value(aStack2, i);
			const X509* cert1 = X509_OBJECT_get0_X509(obj1);
			const X509* cert2 = X509_OBJECT_get0_X509(obj2);
			if ((cert1 == nullptr) != (cert2 == nullptr))
				return false;
			if (cert1 == nullptr)
				continue;
			if (X509_cmp(cert1, cert2) != 0)
				return false;

			const X509_CRL* crl1 = X509_OBJECT_get0_X509_CRL(obj1);
			const X509_CRL* crl2 = X509_OBJECT_get0_X509_CRL(obj2);
			if ((crl1 == nullptr) != (crl2 == nullptr))
				return false;
			if (crl1 == nullptr)
				continue;
			if (X509_CRL_cmp(crl1, crl2) != 0)
				return false;
		}
		return true;
	}

	static bool
	OpenSSLCipherChainAreEqual(
		const STACK_OF(SSL_CIPHER)* aStack1,
		const STACK_OF(SSL_CIPHER)* aStack2)
	{
		if ((aStack1 == nullptr) != (aStack2 == nullptr))
			return false;
		if (aStack1 == nullptr)
			return true;
		int cnt1 = sk_SSL_CIPHER_num(aStack1);
		int cnt2 = sk_SSL_CIPHER_num(aStack2);
		if (cnt1 != cnt2)
			return false;
		// The documentation says it is max 128 bytes. But the limit is not defined in the
		// header anywhere. To be on the safe side - use a bigger buffer.
		const int descSize = 256;
		char desc1[descSize];
		char desc2[descSize];
		for (int i = 0; i < cnt1; ++i)
		{
			const SSL_CIPHER* obj1 = sk_SSL_CIPHER_value(aStack1, i);
			const SSL_CIPHER* obj2 = sk_SSL_CIPHER_value(aStack2, i);
			MG_BOX_ASSERT(SSL_CIPHER_description(obj1, desc1, descSize) != nullptr);
			MG_BOX_ASSERT(SSL_CIPHER_description(obj2, desc2, descSize) != nullptr);
			if (mg::box::Strcmp(desc1, desc2) != 0)
				return false;
		}
		return true;
	}

#endif

	static bool
	OpenSSLVersionDoesSupportCipher(
		SSLVersion aVersion,
		SSLCipher aCipher)
	{
		switch (aCipher)
		{
		case SSL_CIPHER_ECDHE_ECDSA_AES128_GCM_SHA256:
		case SSL_CIPHER_ECDHE_ECDSA_AES256_GCM_SHA384:
			return aVersion == SSL_VERSION_TLSv1_2;
		case SSL_CIPHER_TLS_AES_128_GCM_SHA256:
		case SSL_CIPHER_TLS_AES_256_GCM_SHA384:
		case SSL_CIPHER_TLS_CHACHA20_POLY1305_SHA256:
			return aVersion == SSL_VERSION_TLSv1_3;
		default:
			MG_BOX_ASSERT(false);
			break;
		}
		return false;
	}

	static void
	OpenSSL_SSL_CTX_IndexContextFree(
		void* /* aParent */,
		void* aPtr,
		CRYPTO_EX_DATA* /* aAd */,
		int /* aIndex */,
		long /* aArgl */,
		void* /*aArgp */)
	{
		// Index free is called for all contexts and all their registered indexes, even if
		// they were never actually set. It happens for SSL_CTX objects created internally
		// by libraries like curl inside of the same process.
		if (aPtr != nullptr)
			((SSLContextOpenSSL*)aPtr)->PrivOn_SSL_CTX_Delete();
	}

	static int
	OpenSSLGetContextIndex()
	{
		// OpenSSL SSL_CTX is stored in serverbox SSL context. But their lifetime is tied
		// together. Each SSL_CTX inside stores a pointer to its owner - serverbox
		// context. In OpenSSL such payload storage works via 'ex data' indexes.
		// Essentially, each SSL_CTX has an array like this:
		//
		//     [0, 0, ..., sbox ctx, 0, ...]
		//
		// The index where serverbox ctx is stored is defined globally the same for all
		// SSL_CTX objects.
		//
		// This allows to store data needed for SSL_CTX callbacks in the serverbox
		// context. For instance passwords, or something else in the future.
		static int idx = SSL_CTX_get_ex_new_index(0, nullptr, nullptr, nullptr,
			OpenSSL_SSL_CTX_IndexContextFree);

		return idx;
	}

}
}

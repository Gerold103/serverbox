#include "mg/net/SSL.h"

#include "mg/box/StringFunctions.h"
#include "mg/net/SSLBioMem.h"
#include "mg/net/SSLContext.h"
#include "mg/net/SSLStream.h"

#include "UnitTest.h"
#include "UnitTestSSLCerts.h"

namespace mg {
namespace unittests {
namespace net {
namespace ssl {

	static uint64_t
	UnitTestSSLBufferListSize(
		const mg::net::Buffer* aHead)
	{
		uint64_t res = 0;
		while (aHead != nullptr)
		{
			res += aHead->myPos;
			aHead = aHead->myNext.GetPointer();
		}
		return res;
	}

	// Generic helpers to produce more various contexts //////////////////////////////////

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewServerLocalTemplate(
		mg::net::SSLTrust aTrust,
		const uint8_t* aCert,
		uint32_t aCertLen,
		const uint8_t* aKey,
		uint32_t aKeyLen)
	{
		mg::net::SSLContext::Ptr ssl = mg::net::SSLContext::NewShared(true);
		ssl->SetTrust(aTrust);
		TEST_CHECK(ssl->AddLocalCert(aCert, aCertLen, aKey, aKeyLen));
		return ssl;
	}

#if MG_OPENSSL_CTX_HAS_COMPARISON

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewServerRemoteV3Template(
		mg::net::SSLTrust aTrust,
		const uint8_t* aCert,
		uint32_t aCertLen,
		const uint8_t* aKey,
		uint32_t aKeyLen)
	{
		mg::net::SSLContext::Ptr ssl = UnitTestSSLNewServerLocalTemplate(
			aTrust, aCert, aCertLen, aKey, aKeyLen);
		TEST_CHECK(ssl->AddRemoteCert(theUnitTestCACert3, theUnitTestCACert3Size));
		return ssl;
	}

#endif

	static void
	UnitTesTSSLAddCommonCiphers(mg::net::SSLContext* ssl)
	{
		// TLSv1.2
		TEST_CHECK(ssl->AddCipher(mg::net::SSL_CIPHER_ECDHE_ECDSA_AES256_GCM_SHA384));
		// TLSv1.3
		TEST_CHECK(ssl->AddCipher(mg::net::SSL_CIPHER_TLS_AES_256_GCM_SHA384));
	}

	// Final versions ////////////////////////////////////////////////////////////////////

#if MG_OPENSSL_CTX_HAS_COMPARISON

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewServerEmptyStrict()
	{
		mg::net::SSLContext::Ptr ssl = mg::net::SSLContext::NewShared(true);
		ssl->SetTrust(mg::net::SSL_TRUST_STRICT);
		return ssl;
	}

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewServerEmptyStrictWithMinVersion()
	{
		mg::net::SSLContext::Ptr ssl = mg::net::SSLContext::NewShared(true);
		ssl->SetTrust(mg::net::SSL_TRUST_STRICT);
		ssl->SetMinVersion(mg::net::SSL_VERSION_TLSv1_2);
		return ssl;
	}

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewServerEmptyStrictCipher()
	{
		mg::net::SSLContext::Ptr ssl = mg::net::SSLContext::NewShared(true);
		ssl->SetTrust(mg::net::SSL_TRUST_STRICT);
		UnitTesTSSLAddCommonCiphers(ssl.GetPointer());
		return ssl;
	}

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewServerEmptyRelaxed()
	{
		mg::net::SSLContext::Ptr ssl = mg::net::SSLContext::NewShared(true);
		ssl->SetTrust(mg::net::SSL_TRUST_RELAXED);
		return ssl;
	}

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewServerEmptyRelaxedCipher()
	{
		mg::net::SSLContext::Ptr ssl = mg::net::SSLContext::NewShared(true);
		ssl->SetTrust(mg::net::SSL_TRUST_RELAXED);
		UnitTesTSSLAddCommonCiphers(ssl.GetPointer());
		return ssl;
	}

#endif

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewServerLocalV1Bypass()
	{
		return UnitTestSSLNewServerLocalTemplate(
			mg::net::SSL_TRUST_BYPASS_VERIFICATION,
			theUnitTestCert1, theUnitTestCert1Size,
			theUnitTestKey1, theUnitTestKey1Size);
	}

#if MG_OPENSSL_CTX_HAS_COMPARISON

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewServerLocalV1Strict()
	{
		return UnitTestSSLNewServerLocalTemplate(
			mg::net::SSL_TRUST_STRICT,
			theUnitTestCert1, theUnitTestCert1Size,
			theUnitTestKey1, theUnitTestKey1Size);
	}

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewServerLocalV2Bypass()
	{
		return UnitTestSSLNewServerLocalTemplate(
			mg::net::SSL_TRUST_BYPASS_VERIFICATION,
			theUnitTestCert2, theUnitTestCert2Size,
			theUnitTestKey2, theUnitTestKey2Size);
	}

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewServerLocalV2Strict()
	{
		return UnitTestSSLNewServerLocalTemplate(
			mg::net::SSL_TRUST_STRICT,
			theUnitTestCert2, theUnitTestCert2Size,
			theUnitTestKey2, theUnitTestKey2Size);
	}

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewServerLocalV3Cert1Bypass()
	{
		return UnitTestSSLNewServerLocalTemplate(
			mg::net::SSL_TRUST_BYPASS_VERIFICATION,
			theUnitTestCert31, theUnitTestCert31Size,
			theUnitTestKey3, theUnitTestKey3Size);
	}

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewServerLocalV3Cert2Strict()
	{
		return UnitTestSSLNewServerLocalTemplate(
			mg::net::SSL_TRUST_STRICT,
			theUnitTestCert31, theUnitTestCert31Size,
			theUnitTestKey3, theUnitTestKey3Size);
	}

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewServerRemoteV3StrictCert1()
	{
		return UnitTestSSLNewServerRemoteV3Template(
			mg::net::SSL_TRUST_STRICT,
			theUnitTestCert31, theUnitTestCert31Size,
			theUnitTestKey3, theUnitTestKey3Size);
	}

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewServerRemoteV3RelaxedCert1()
	{
		return UnitTestSSLNewServerRemoteV3Template(
			mg::net::SSL_TRUST_RELAXED,
			theUnitTestCert31, theUnitTestCert31Size,
			theUnitTestKey3, theUnitTestKey3Size);
	}

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewServerRemoteV3StrictCert2()
	{
		return UnitTestSSLNewServerRemoteV3Template(
			mg::net::SSL_TRUST_STRICT,
			theUnitTestCert32, theUnitTestCert32Size,
			theUnitTestKey3, theUnitTestKey3Size);
	}

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewServerRemoteV3RelaxedCert2()
	{
		return UnitTestSSLNewServerRemoteV3Template(
			mg::net::SSL_TRUST_RELAXED,
			theUnitTestCert32, theUnitTestCert32Size,
			theUnitTestKey3, theUnitTestKey3Size);
	}

#endif

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewClientBypass()
	{
		mg::net::SSLContext::Ptr ssl = mg::net::SSLContext::NewShared(false);
		ssl->SetTrust(mg::net::SSL_TRUST_BYPASS_VERIFICATION);
		ssl->SetMinVersion(mg::net::SSL_VERSION_TLSv1_2);
		return ssl;
	}

#if MG_OPENSSL_CTX_HAS_COMPARISON

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewClientStrict()
	{
		mg::net::SSLContext::Ptr ssl = mg::net::SSLContext::NewShared(false);
		ssl->SetTrust(mg::net::SSL_TRUST_STRICT);
		ssl->SetMinVersion(mg::net::SSL_VERSION_TLSv1_2);
		return ssl;
	}

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewClientStrictOldTLS()
	{
		mg::net::SSLContext::Ptr ssl = mg::net::SSLContext::NewShared(false);
		ssl->SetTrust(mg::net::SSL_TRUST_STRICT);
		ssl->SetMinVersion(mg::net::SSL_VERSION_TLSv1);
		return ssl;
	}

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewClientRelaxed()
	{
		mg::net::SSLContext::Ptr ssl = mg::net::SSLContext::NewShared(false);
		ssl->SetTrust(mg::net::SSL_TRUST_RELAXED);
		ssl->SetMinVersion(mg::net::SSL_VERSION_TLSv1_2);
		return ssl;
	}

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewClientRelaxedOldTLS()
	{
		mg::net::SSLContext::Ptr ssl = mg::net::SSLContext::NewShared(false);
		ssl->SetTrust(mg::net::SSL_TRUST_RELAXED);
		ssl->SetMinVersion(mg::net::SSL_VERSION_TLSv1);
		return ssl;
	}

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewClientStrictWithCA1()
	{
		mg::net::SSLContext::Ptr ssl = UnitTestSSLNewClientStrict();
		ssl->AddRemoteCert(theUnitTestCACert3Pem, theUnitTestCACert3PemSize);
		return ssl;
	}

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewClientStrictWithCA1Cipher()
	{
		mg::net::SSLContext::Ptr ssl = UnitTestSSLNewClientStrict();
		UnitTesTSSLAddCommonCiphers(ssl.GetPointer());
		ssl->AddRemoteCert(theUnitTestCACert3Pem, theUnitTestCACert3PemSize);
		return ssl;
	}

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewClientStrictWithCA1OldTLS()
	{
		mg::net::SSLContext::Ptr ssl = UnitTestSSLNewClientStrictOldTLS();
		ssl->AddRemoteCert(theUnitTestCACert3Pem, theUnitTestCACert3PemSize);
		return ssl;
	}

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewClientRelaxedWithCA1()
	{
		mg::net::SSLContext::Ptr ssl = UnitTestSSLNewClientRelaxed();
		ssl->AddRemoteCert(theUnitTestCACert3Pem, theUnitTestCACert3PemSize);
		return ssl;
	}

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewClientRelaxedWithCA1Cipher()
	{
		mg::net::SSLContext::Ptr ssl = UnitTestSSLNewClientRelaxed();
		UnitTesTSSLAddCommonCiphers(ssl.GetPointer());
		ssl->AddRemoteCert(theUnitTestCACert3Pem, theUnitTestCACert3PemSize);
		return ssl;
	}

	static mg::net::SSLContext::Ptr
	UnitTestSSLNewClientRelaxedWithCA1OldTLS()
	{
		mg::net::SSLContext::Ptr ssl = UnitTestSSLNewClientRelaxedOldTLS();
		ssl->AddRemoteCert(theUnitTestCACert3Pem, theUnitTestCACert3PemSize);
		return ssl;
	}

#endif

	//////////////////////////////////////////////////////////////////////////////////////

	static void
	UnitTestSSLBioMemOpenSSLBasic()
	{
		const uint32_t bsize = mg::net::theBufferCopySize;
		uint8_t rbuf[bsize];
		mg::net::BufferStream stream;
		BIO* bio = mg::net::OpenSSLWrapBioMem(stream);
		//
		// BIO_write_ex.
		//
		size_t byteCount = 0;
		BIO_set_retry_read(bio);
		BIO_set_retry_write(bio);
		TEST_CHECK(BIO_write_ex(bio, "123456", 6, &byteCount) == 1);
		TEST_CHECK(byteCount == 6);
		TEST_CHECK(stream.GetReadSize() == 6);
		stream.ReadData(rbuf, 6);
		TEST_CHECK(memcmp(rbuf, "123456", 6) == 0);
		TEST_CHECK(!BIO_should_retry(bio));
		//
		// BIO_write.
		//
		BIO_set_retry_read(bio);
		BIO_set_retry_write(bio);
		TEST_CHECK(BIO_write(bio, "78910", 5) == 5);
		memset(rbuf, 0, bsize);
		TEST_CHECK(stream.GetReadSize() == 5);
		stream.ReadData(rbuf, 5);
		TEST_CHECK(memcmp(rbuf, "78910", 5) == 0);
		TEST_CHECK(!BIO_should_retry(bio));
		//
		// BIO_read_ex.
		//
		stream.WriteCopy("1234", 4);
		byteCount = 0;
		BIO_set_retry_read(bio);
		BIO_set_retry_write(bio);
		memset(rbuf, 0, bsize);
		TEST_CHECK(BIO_read_ex(bio, rbuf, bsize, &byteCount) == 1);
		TEST_CHECK(byteCount == 4);
		TEST_CHECK(memcmp(rbuf, "1234", 4) == 0);
		TEST_CHECK(stream.GetReadSize() == 0);
		TEST_CHECK(!BIO_should_retry(bio));
		TEST_CHECK(BIO_read_ex(bio, rbuf, bsize, &byteCount) == 0);
		TEST_CHECK(byteCount == 0);
		TEST_CHECK(BIO_should_retry(bio) && BIO_should_read(bio));
		//
		// BIO_read.
		//
		stream.WriteCopy("1234567", 7);
		BIO_set_retry_read(bio);
		BIO_set_retry_write(bio);
		memset(rbuf, 0, bsize);
		TEST_CHECK(BIO_read(bio, rbuf, bsize) == 7);
		TEST_CHECK(memcmp(rbuf, "1234567", 7) == 0);
		TEST_CHECK(stream.GetReadSize() == 0);
		TEST_CHECK(!BIO_should_retry(bio));

		TEST_CHECK(BIO_read(bio, rbuf, bsize) == 0);
		TEST_CHECK(BIO_should_retry(bio) && BIO_should_read(bio));
		//
		// BIO_flush.
		//
		TEST_CHECK(BIO_flush(bio) == 1);

		BIO_free(bio);
	}

	static void
	UnitTestSSLBioMemOpenSSLConsistencyOne(
		BIO* aBio)
	{
		uint8_t rbuf[mg::net::theBufferCopySize];
		//
		// BIO_write.
		//
		// Empty.
		BIO_clear_retry_flags(aBio);
		TEST_CHECK(BIO_write(aBio, "", 0) == 0);
		TEST_CHECK(BIO_get_retry_flags(aBio) == 0);

		// Non-empty.
		BIO_clear_retry_flags(aBio);
		TEST_CHECK(BIO_write(aBio, "12345", 5) == 5);
		TEST_CHECK(BIO_get_retry_flags(aBio) == 0);
		memset(rbuf, 0, mg::net::theBufferCopySize);
		TEST_CHECK(BIO_read(aBio, rbuf, mg::net::theBufferCopySize) == 5);
		TEST_CHECK(memcmp(rbuf, "12345", 5) == 0);

		//
		// BIO_write_ex.
		//
		// Empty.
		size_t byteCount = 0;
		BIO_clear_retry_flags(aBio);
		// OpenSSL doc says writing 0 bytes is not an error, but BIO_s_mem violates that.
		// Can't check for an exact return code then.
		TEST_CHECK(BIO_write_ex(aBio, "", 0, &byteCount) >= 0);
		TEST_CHECK(byteCount == 0);
		TEST_CHECK(BIO_get_retry_flags(aBio) == 0);

		// Non-empty.
		BIO_clear_retry_flags(aBio);
		TEST_CHECK(BIO_write_ex(aBio, "12345", 5, &byteCount) == 1);
		TEST_CHECK(byteCount == 5);
		TEST_CHECK(BIO_get_retry_flags(aBio) == 0);
		memset(rbuf, 0, mg::net::theBufferCopySize);
		TEST_CHECK(BIO_read(aBio, rbuf, mg::net::theBufferCopySize) == 5);
		TEST_CHECK(memcmp(rbuf, "12345", 5) == 0);

		//
		// BIO_read.
		//
		// BIO_s_mem doesn't implement read_ex and fails reading from an empty buffer
		// critically with -1. But read_ex in serverbox can't return -1. Hence can't check
		// for an exact error code. Can only ensure it was not successful and check
		// retry-flags.
		int readFailEofBorder = 0;
		// Empty from empty.
		BIO_clear_retry_flags(aBio);
		TEST_CHECK(BIO_read(aBio, rbuf, 0) <= readFailEofBorder);
		TEST_CHECK(BIO_should_retry(aBio) && BIO_should_read(aBio));

		// Empty from non-empty. Somewhy it is another type of failure - no retry flags
		// are set.
		TEST_CHECK(BIO_write(aBio, "123", 3) == 3);
		BIO_clear_retry_flags(aBio);
		TEST_CHECK(BIO_read(aBio, rbuf, 0) == 0);
		TEST_CHECK(BIO_get_retry_flags(aBio) == 0);
		// Consume old data.
		TEST_CHECK(BIO_read(aBio, rbuf, mg::net::theBufferCopySize) == 3);

		// Non-empty from empty.
		BIO_clear_retry_flags(aBio);
		TEST_CHECK(BIO_read(aBio, rbuf, mg::net::theBufferCopySize) <= readFailEofBorder);
		TEST_CHECK(BIO_should_retry(aBio) && BIO_should_read(aBio));

		// Non-empty from non-empty.
		TEST_CHECK(BIO_write(aBio, "123", 3) == 3);
		BIO_clear_retry_flags(aBio);
		TEST_CHECK(BIO_read(aBio, rbuf, mg::net::theBufferCopySize) == 3);
		TEST_CHECK(memcmp(rbuf, "123", 3) == 0);
		TEST_CHECK(BIO_get_retry_flags(aBio) == 0);

		//
		// BIO_read_ex.
		//
		// Empty from empty.
		BIO_clear_retry_flags(aBio);
		byteCount = 0;
		TEST_CHECK(BIO_read_ex(aBio, rbuf, 0, &byteCount) == 0);
		TEST_CHECK(byteCount == 0);
		TEST_CHECK(BIO_should_retry(aBio) && BIO_should_read(aBio));

		// Empty from non-empty. Somewhy it is another type of failure - no retry flags
		// are set.
		TEST_CHECK(BIO_write(aBio, "123", 3) == 3);
		BIO_clear_retry_flags(aBio);
		TEST_CHECK(BIO_read_ex(aBio, rbuf, 0, &byteCount) == 0);
		TEST_CHECK(byteCount == 0);
		TEST_CHECK(BIO_get_retry_flags(aBio) == 0);
		// Consume old data.
		TEST_CHECK(BIO_read(aBio, rbuf, mg::net::theBufferCopySize) == 3);

		// Non-empty from empty.
		BIO_clear_retry_flags(aBio);
		TEST_CHECK(BIO_read_ex(aBio, rbuf, mg::net::theBufferCopySize, &byteCount) == 0);
		TEST_CHECK(byteCount == 0);
		TEST_CHECK(BIO_should_retry(aBio) && BIO_should_read(aBio));

		// Non-empty from non-empty.
		TEST_CHECK(BIO_write(aBio, "123", 3) == 3);
		BIO_clear_retry_flags(aBio);
		TEST_CHECK(BIO_read_ex(aBio, rbuf, mg::net::theBufferCopySize, &byteCount) == 1);
		TEST_CHECK(byteCount == 3);
		TEST_CHECK(memcmp(rbuf, "123", 3) == 0);
		TEST_CHECK(BIO_get_retry_flags(aBio) == 0);
	}

	//
	// Ensure SSLBioMem is a subset of BIO_s_mem.
	//
	static void
	UnitTestSSLBioMemOpenSSLConsistency()
	{
		mg::net::BufferStream stream;
		BIO* sbio = mg::net::OpenSSLWrapBioMem(stream);
		BIO* mbio = BIO_new(BIO_s_mem());

		UnitTestSSLBioMemOpenSSLConsistencyOne(mbio);
		UnitTestSSLBioMemOpenSSLConsistencyOne(sbio);

		BIO_free(mbio);
		BIO_free(sbio);
	}

 	//////////////////////////////////////////////////////////////////////////////////////

	static void
	UnitTestSSLCompare()
	{
#if MG_OPENSSL_CTX_HAS_COMPARISON
		mg::net::SSLContext::Ptr ssls[] = {
			UnitTestSSLNewServerEmptyStrict(),
			UnitTestSSLNewServerEmptyStrictWithMinVersion(),
			UnitTestSSLNewServerEmptyStrictCipher(),
			UnitTestSSLNewServerEmptyRelaxed(),
			UnitTestSSLNewServerEmptyRelaxedCipher(),
			UnitTestSSLNewServerLocalV1Bypass(),
			UnitTestSSLNewServerLocalV1Strict(),
			UnitTestSSLNewServerLocalV2Bypass(),
			UnitTestSSLNewServerLocalV2Strict(),
			UnitTestSSLNewServerLocalV3Cert1Bypass(),
			UnitTestSSLNewServerLocalV3Cert2Strict(),
			UnitTestSSLNewServerRemoteV3StrictCert2(),
			UnitTestSSLNewServerRemoteV3RelaxedCert2(),
			UnitTestSSLNewServerRemoteV3StrictCert1(),
			UnitTestSSLNewServerRemoteV3RelaxedCert1(),
			UnitTestSSLNewClientStrict(),
			UnitTestSSLNewClientStrictOldTLS(),
			UnitTestSSLNewClientRelaxed(),
			UnitTestSSLNewClientRelaxedOldTLS(),
			UnitTestSSLNewClientStrictWithCA1(),
			UnitTestSSLNewClientStrictWithCA1Cipher(),
			UnitTestSSLNewClientStrictWithCA1OldTLS(),
			UnitTestSSLNewClientRelaxedWithCA1(),
			UnitTestSSLNewClientRelaxedWithCA1Cipher(),
			UnitTestSSLNewClientRelaxedWithCA1OldTLS(),
		};
		const int count = sizeof(ssls) / sizeof(ssls[0]);
		for (int i = 0; i < count; ++i)
		{
			mg::net::SSLContext* ssl1 = ssls[i].GetPointer();
			for (int j = 0; j < count; ++j)
			{
				mg::net::SSLContext* ssl2 = ssls[j].GetPointer();
				TEST_CHECK((*ssl1 == *ssl2) == (i == j));
				TEST_CHECK((*ssl1 != *ssl2) == (i != j));
			}
		}
#endif
	}

	static bool
	UnitTestSSLSend(
		mg::net::SSLStream* aSrc,
		mg::net::SSLStream* aDst)
	{
		const char* data = "123456789";
		uint64_t size = mg::box::Strlen(data) + 1;
		aSrc->AppendAppInputCopy(data, size);
		uint64_t received = 0;
		mg::net::Buffer::Ptr head;
		while (received < size)
		{
			uint64_t byteCount1 = aSrc->PopNetOutput(head);
			if (aSrc->HasError())
				return false;
			TEST_CHECK(UnitTestSSLBufferListSize(head.GetPointer()) == byteCount1);

			aDst->AppendNetInputRef(std::move(head));
			if (aDst->HasError())
				return false;

			byteCount1 = aSrc->PopAppOutput(head);
			if (aSrc->HasError())
				return false;
			TEST_CHECK(UnitTestSSLBufferListSize(head.GetPointer()) == byteCount1);

			byteCount1 = aDst->PopAppOutput(head);
			if (aDst->HasError())
				return false;
			TEST_CHECK(UnitTestSSLBufferListSize(head.GetPointer()) == byteCount1);
			received += byteCount1;

			byteCount1 = aDst->PopNetOutput(head);
			if (aDst->HasError())
				return false;
			TEST_CHECK(UnitTestSSLBufferListSize(head.GetPointer()) == byteCount1);

			aSrc->AppendNetInputRef(std::move(head));
			if (aSrc->HasError())
				return false;
		}
		TEST_CHECK(received == size);
		return true;
	}

	static void
	UnitTestSSLCheckSessionMeta(
		const mg::net::SSLStream& aFilter1,
		const mg::net::SSLStream& aFilter2,
		mg::net::SSLVersion aVersion,
		mg::net::SSLCipher aCipher)
	{
		TEST_CHECK(aFilter1.GetVersion() == aVersion);
		if (aCipher != mg::net::SSL_CIPHER_NONE)
			TEST_CHECK(aFilter1.GetCipher() == aCipher);
		TEST_CHECK(aFilter1.GetVersion() == aFilter2.GetVersion());
		TEST_CHECK(aFilter1.GetCipher() == aFilter2.GetCipher());
	}

	static void
	UnitTestSSLFinalizeShutdown(
		mg::net::SSLStream& aFilter1,
		mg::net::SSLStream& aFilter2)
	{
		mg::net::Buffer::Ptr head;
		uint64_t rc;
		while (!aFilter1.IsClosed() || !aFilter2.IsClosed())
		{
			rc = aFilter1.PopNetOutput(head);
			TEST_CHECK(!aFilter1.HasError());
			aFilter2.AppendNetInputRef(std::move(head));
			TEST_CHECK(!aFilter2.HasError());

			rc = aFilter1.PopAppOutput(head);
			TEST_CHECK(!aFilter1.HasError());
			TEST_CHECK(rc == 0);

			rc = aFilter2.PopAppOutput(head);
			TEST_CHECK(!aFilter2.HasError());
			TEST_CHECK(rc == 0);

			rc = aFilter2.PopNetOutput(head);
			TEST_CHECK(!aFilter2.HasError());
			aFilter1.AppendNetInputRef(std::move(head));
			TEST_CHECK(!aFilter1.HasError());
		}
	}

	static void
	UnitTestSSLInteract(
		mg::net::SSLStream& aFilter1,
		mg::net::SSLStream& aFilter2)
	{
		TEST_CHECK(UnitTestSSLSend(&aFilter1, &aFilter2));
		TEST_CHECK(UnitTestSSLSend(&aFilter2, &aFilter1));
	}

	static void
	UnitTestSSLInteract(
		mg::net::SSLContext* aCtx1,
		mg::net::SSLContext* aCtx2)
	{
		mg::net::SSLStream filter1(aCtx1);
		filter1.Connect();
		mg::net::SSLStream filter2(aCtx2);
		filter2.Connect();
		UnitTestSSLInteract(filter1, filter2);
	}

	static void
	UnitTestSSLInteract(
		mg::net::SSLContext* aCtx1,
		mg::net::SSLContext* aCtx2,
		mg::net::SSLVersion aVersion,
		mg::net::SSLCipher aCipher)
	{
		mg::net::SSLStream filter1(aCtx1);
		filter1.Connect();
		mg::net::SSLStream filter2(aCtx2);
		filter2.Connect();
		UnitTestSSLInteract(filter1, filter2);
		UnitTestSSLCheckSessionMeta(filter1, filter2, aVersion, aCipher);
	}

	static void
	UnitTestSSLMethods()
	{
		// Output to network layer appends to the list. Not resets it.
		mg::net::SSLContext::Ptr serverCtx = mg::net::SSLContext::NewShared(true);
		serverCtx->SetTrust(mg::net::SSL_TRUST_BYPASS_VERIFICATION);
		TEST_CHECK(serverCtx->AddLocalCert(
			theUnitTestCert1, theUnitTestCert1Size,
			theUnitTestKey1, theUnitTestKey1Size));
		mg::net::SSLStream server(serverCtx);
		server.Connect();

		mg::net::SSLContext::Ptr clientCtx = mg::net::SSLContext::NewShared(false);
		clientCtx->SetTrust(mg::net::SSL_TRUST_BYPASS_VERIFICATION);
		mg::net::SSLStream client(clientCtx);
		client.Connect();

		UnitTestSSLInteract(server, client);

		for (int i = 0; i < 2; ++i)
		{
			client.AppendAppInputCopy("123", 3);
			mg::net::Buffer::Ptr head;
			uint64_t size = client.PopNetOutput(head);
			TEST_CHECK(size > 0);
			TEST_CHECK(size == UnitTestSSLBufferListSize(head.GetPointer()));
		}
	}

	static void
	UnitTestSSLStreamInteraction()
	{
		// Normal situation - server has a CA-signed cert, accepts all clients. The client
		// makes proper checks.
		{
			mg::net::SSLContext::Ptr serverCtx =  mg::net::SSLContext::NewShared(true);
			serverCtx->SetTrust(mg::net::SSL_TRUST_BYPASS_VERIFICATION);
			TEST_CHECK(serverCtx->AddLocalCert(
				theUnitTestCert31, theUnitTestCert31Size,
				theUnitTestKey3, theUnitTestKey3Size));

			mg::net::SSLContext::Ptr clientCtx = mg::net::SSLContext::NewShared(false);
			clientCtx->SetTrust(mg::net::SSL_TRUST_STRICT);
			clientCtx->AddRemoteCert(theUnitTestCACert3Pem, theUnitTestCACert3PemSize);

			UnitTestSSLInteract(serverCtx.GetPointer(), clientCtx.GetPointer());
		}
		// Custom cipher.
		{
			mg::net::SSLContext::Ptr serverCtx = mg::net::SSLContext::NewShared(true);
			serverCtx->SetTrust(mg::net::SSL_TRUST_BYPASS_VERIFICATION);
			TEST_CHECK(serverCtx->AddLocalCert(
				theUnitTestCert31, theUnitTestCert31Size,
				theUnitTestKey3, theUnitTestKey3Size));
			UnitTesTSSLAddCommonCiphers(serverCtx.GetPointer());

			mg::net::SSLContext::Ptr clientCtx = mg::net::SSLContext::NewShared(false);
			clientCtx->SetTrust(mg::net::SSL_TRUST_STRICT);
			clientCtx->AddRemoteCert(theUnitTestCACert3Pem, theUnitTestCACert3PemSize);
			UnitTesTSSLAddCommonCiphers(clientCtx.GetPointer());

			UnitTestSSLInteract(serverCtx.GetPointer(), clientCtx.GetPointer());
		}
		// Custom cipher on server side.
		{
			mg::net::SSLContext::Ptr serverCtx = mg::net::SSLContext::NewShared(true);
			serverCtx->SetTrust(mg::net::SSL_TRUST_BYPASS_VERIFICATION);
			TEST_CHECK(serverCtx->AddLocalCert(
				theUnitTestCert31, theUnitTestCert31Size,
				theUnitTestKey3, theUnitTestKey3Size));
			UnitTesTSSLAddCommonCiphers(serverCtx.GetPointer());

			mg::net::SSLContext::Ptr clientCtx = mg::net::SSLContext::NewShared(false);
			clientCtx->SetTrust(mg::net::SSL_TRUST_STRICT);
			clientCtx->AddRemoteCert(theUnitTestCACert3Pem, theUnitTestCACert3PemSize);

			UnitTestSSLInteract(serverCtx.GetPointer(), clientCtx.GetPointer());
		}
		// Custom cipher on client side.
		{
			mg::net::SSLContext::Ptr serverCtx = mg::net::SSLContext::NewShared(true);
			serverCtx->SetTrust(mg::net::SSL_TRUST_BYPASS_VERIFICATION);
			TEST_CHECK(serverCtx->AddLocalCert(
				theUnitTestCert31, theUnitTestCert31Size,
				theUnitTestKey3, theUnitTestKey3Size));

			mg::net::SSLContext::Ptr clientCtx = mg::net::SSLContext::NewShared(false);
			clientCtx->SetTrust(mg::net::SSL_TRUST_STRICT);
			clientCtx->AddRemoteCert(theUnitTestCACert3Pem, theUnitTestCACert3PemSize);
			UnitTesTSSLAddCommonCiphers(clientCtx.GetPointer());

			UnitTestSSLInteract(serverCtx.GetPointer(), clientCtx.GetPointer());
		}
		// Client fails server cert validation.
		{
			mg::net::SSLContext::Ptr serverCtx = mg::net::SSLContext::NewShared(true);
			serverCtx->SetTrust(mg::net::SSL_TRUST_BYPASS_VERIFICATION);
			TEST_CHECK(serverCtx->AddLocalCert(
				theUnitTestCert31, theUnitTestCert31Size,
				theUnitTestKey3, theUnitTestKey3Size));

			mg::net::SSLContext::Ptr clientCtx = mg::net::SSLContext::NewShared(false);
			clientCtx->SetTrust(mg::net::SSL_TRUST_STRICT);
			TEST_CHECK(clientCtx->AddRemoteCert(theUnitTestCert1, theUnitTestCert1Size));

			mg::net::SSLStream server(serverCtx);
			server.Connect();
			mg::net::SSLStream client(clientCtx);
			client.Connect();
			TEST_CHECK(!UnitTestSSLSend(&client, &server));
			TEST_CHECK(client.HasError() && !server.HasError());
		}
		// Server validates client cert and vice versa.
		{
			mg::net::SSLContext::Ptr serverCtx = mg::net::SSLContext::NewShared(true);
			serverCtx->SetTrust(mg::net::SSL_TRUST_STRICT);
			TEST_CHECK(serverCtx->AddLocalCert(
				theUnitTestCert31, theUnitTestCert31Size,
				theUnitTestKey3, theUnitTestKey3Size));
			TEST_CHECK(serverCtx->AddRemoteCert(
				theUnitTestCert1, theUnitTestCert1Size));

			mg::net::SSLContext::Ptr clientCtx = mg::net::SSLContext::NewShared(false);
			clientCtx->SetTrust(mg::net::SSL_TRUST_STRICT);
			TEST_CHECK(clientCtx->AddLocalCert(
				theUnitTestCert1, theUnitTestCert1Size,
				theUnitTestKey1, theUnitTestKey1Size));
			TEST_CHECK(clientCtx->AddRemoteCert(
				theUnitTestCACert3, theUnitTestCACert3Size));

			UnitTestSSLInteract(serverCtx.GetPointer(), clientCtx.GetPointer());
		}
		// Server fails client cert validation.
		{
			mg::net::SSLContext::Ptr serverCtx = mg::net::SSLContext::NewShared(true);
			serverCtx->SetTrust(mg::net::SSL_TRUST_STRICT);
			TEST_CHECK(serverCtx->AddLocalCert(
				theUnitTestCert31, theUnitTestCert31Size,
				theUnitTestKey3, theUnitTestKey3Size));
			TEST_CHECK(serverCtx->AddRemoteCert(
				theUnitTestCert1, theUnitTestCert1Size));

			mg::net::SSLContext::Ptr clientCtx = mg::net::SSLContext::NewShared(false);
			clientCtx->SetTrust(mg::net::SSL_TRUST_STRICT);
			TEST_CHECK(clientCtx->AddLocalCert(
				theUnitTestCert2, theUnitTestCert2Size,
				theUnitTestKey2, theUnitTestKey2Size));
			TEST_CHECK(clientCtx->AddRemoteCert(
				theUnitTestCACert3, theUnitTestCACert3Size));

			mg::net::SSLStream server(serverCtx);
			server.Connect();
			mg::net::SSLStream client(clientCtx);
			client.Connect();
			TEST_CHECK(!UnitTestSSLSend(&client, &server));
			TEST_CHECK(!client.HasError() && server.HasError());
		}
		// Filter fails on corrupted data.
		{
			mg::net::SSLContext::Ptr serverCtx = mg::net::SSLContext::NewShared(true);
			serverCtx->SetTrust(mg::net::SSL_TRUST_BYPASS_VERIFICATION);
			TEST_CHECK(serverCtx->AddLocalCert(
				theUnitTestCert31, theUnitTestCert31Size,
				theUnitTestKey3, theUnitTestKey3Size));

			mg::net::SSLContext::Ptr clientCtx = mg::net::SSLContext::NewShared(false);
			clientCtx->SetTrust(mg::net::SSL_TRUST_STRICT);
			clientCtx->AddRemoteCert(theUnitTestCACert3Pem, theUnitTestCACert3PemSize);

			mg::net::SSLStream server(serverCtx);
			server.Connect();
			mg::net::SSLStream client(clientCtx);
			client.Connect();
			UnitTestSSLInteract(server, client);

			client.AppendNetInputCopy("bad data from network", 21);
			mg::net::Buffer::Ptr head;
			TEST_CHECK(client.PopAppOutput(head) == 0);
			TEST_CHECK(client.HasError() && client.GetError() != 0);
		}
		// Filter fails on an untrusted certificate in strict mode.
		{
			mg::net::SSLContext::Ptr serverCtx = mg::net::SSLContext::NewShared(true);
			serverCtx->SetTrust(mg::net::SSL_TRUST_BYPASS_VERIFICATION);
			TEST_CHECK(serverCtx->AddLocalCert(
				theUnitTestCert1, theUnitTestCert1Size,
				theUnitTestKey1, theUnitTestKey1Size));

			mg::net::SSLContext::Ptr clientCtx = mg::net::SSLContext::NewShared(false);
			clientCtx->SetTrust(mg::net::SSL_TRUST_STRICT);
			TEST_CHECK(clientCtx->AddRemoteCert(theUnitTestCert2, theUnitTestCert2Size));

			mg::net::SSLStream server(serverCtx);
			server.Connect();
			mg::net::SSLStream client(clientCtx);
			client.Connect();
			TEST_CHECK(!UnitTestSSLSend(&client, &server));
			TEST_CHECK(client.HasError() && !server.HasError());
		}
		// Fail when no certificate is specified, even in unsecure mode.
		{
			mg::net::SSLContext::Ptr serverCtx = mg::net::SSLContext::NewShared(true);
			serverCtx->SetTrust(mg::net::SSL_TRUST_BYPASS_VERIFICATION);
			TEST_CHECK(serverCtx->AddLocalCert(
				theUnitTestCert1, theUnitTestCert1Size,
				theUnitTestKey1, theUnitTestKey1Size));

			mg::net::SSLContext::Ptr clientCtx = mg::net::SSLContext::NewShared(false);
			clientCtx->SetTrust(mg::net::SSL_TRUST_UNSECURE);

			mg::net::SSLStream server(serverCtx);
			server.Connect();
			mg::net::SSLStream client(clientCtx);
			client.Connect();
			TEST_CHECK(!UnitTestSSLSend(&client, &server));
			TEST_CHECK(client.HasError() && !server.HasError());
		}
		// Works when no certificate is specified and it is not needed.
		{
			mg::net::SSLContext::Ptr serverCtx = mg::net::SSLContext::NewShared(true);
			serverCtx->SetTrust(mg::net::SSL_TRUST_BYPASS_VERIFICATION);
			TEST_CHECK(serverCtx->AddLocalCert(
				theUnitTestCert1, theUnitTestCert1Size,
				theUnitTestKey1, theUnitTestKey1Size));

			mg::net::SSLContext::Ptr clientCtx = mg::net::SSLContext::NewShared(false);
			clientCtx->SetTrust(mg::net::SSL_TRUST_BYPASS_VERIFICATION);

			UnitTestSSLInteract(serverCtx.GetPointer(), clientCtx.GetPointer());
		}
	}

	static void
	UnitTestSSLStreamEnable()
	{
		mg::net::SSLContext::Ptr serverCtx = mg::net::SSLContext::NewShared(true);
		serverCtx->SetTrust(mg::net::SSL_TRUST_BYPASS_VERIFICATION);
		TEST_CHECK(serverCtx->AddLocalCert(
			theUnitTestCert1, theUnitTestCert1Size,
			theUnitTestKey1, theUnitTestKey1Size));

		mg::net::SSLContext::Ptr clientCtx = mg::net::SSLContext::NewShared(false);
		clientCtx->SetTrust(mg::net::SSL_TRUST_BYPASS_VERIFICATION);

		// Basic test.
		{
			mg::net::SSLStream server(serverCtx);
			mg::net::SSLStream client(clientCtx);
			UnitTestSSLInteract(serverCtx.GetPointer(), clientCtx.GetPointer());
			TEST_CHECK(!server.IsEncrypted());
			TEST_CHECK(!client.IsEncrypted());

			server.Connect();
			TEST_CHECK(server.IsEncrypted());
			client.Connect();
			TEST_CHECK(client.IsEncrypted());
			UnitTestSSLInteract(serverCtx.GetPointer(), clientCtx.GetPointer());
		}
		// Disabled filter is able to return app-data right after it gets net-data.
		{
			mg::net::SSLStream server(serverCtx);
			mg::net::SSLStream client(clientCtx);

			mg::net::Buffer::Ptr head;
			client.AppendAppInputCopy("1234", 4);
			uint64_t byteCount = client.PopNetOutput(head);
			TEST_CHECK(head->myPos == 4);
			TEST_CHECK(memcmp(head->myWData, "1234", 4) == 0);
			TEST_CHECK(byteCount == 4);

			server.AppendNetInputRef(std::move(head));
			byteCount = server.PopAppOutput(head);
			TEST_CHECK(head->myPos == 4);
			TEST_CHECK(memcmp(head->myRData, "1234", 4) == 0);
			TEST_CHECK(byteCount == 4);

			client.Connect();
			server.Connect();
			UnitTestSSLInteract(serverCtx.GetPointer(), clientCtx.GetPointer());
		}
		// Encryption is enabled with non-empty network input in the filter. It shouldn't
		// happen normally, but the API still allows that.
		{
			mg::net::SSLStream server(serverCtx);
			mg::net::SSLStream client(clientCtx);

			mg::net::Buffer::Ptr head;
			client.AppendAppInputCopy("1234", 4);
			uint64_t byteCount = client.PopNetOutput(head);
			TEST_CHECK(head->myPos == 4);
			TEST_CHECK(memcmp(head->myWData, "1234", 4) == 0);
			TEST_CHECK(byteCount == 4);

			server.AppendNetInputRef(std::move(head));
			byteCount = server.PopAppOutput(head);
			TEST_CHECK(head->myPos == 4);
			TEST_CHECK(memcmp(head->myRData, "1234", 4) == 0);
			TEST_CHECK(byteCount == 4);

			client.Connect();
			byteCount = client.PopNetOutput(head);
			TEST_CHECK(byteCount > 0);
			TEST_CHECK(head->myPos > 0);
			server.AppendNetInputRef(std::move(head));
			// Enable when already have first bytes of SSL
			// handshake in the network input.
			server.Connect();

			UnitTestSSLInteract(serverCtx.GetPointer(), clientCtx.GetPointer());
		}
	}

	static void
	UnitTestSSLBadCertsAndKeys()
	{
		mg::net::SSLContext::Ptr ctx;
		bool isServer = true;
		const mg::net::SSLTrust trust = mg::net::SSL_TRUST_STRICT;

		//
		// Bad remote certs.
		//
		ctx = mg::net::SSLContext::NewShared(isServer);
		ctx->SetTrust(trust);
		TEST_CHECK(!ctx->AddRemoteCertFile("unit_test_ssl_not_existing.pem"));

		ctx = mg::net::SSLContext::NewShared(isServer);
		ctx->SetTrust(trust);
		TEST_CHECK(!ctx->AddRemoteCert("bad data", 8));

		//
		// Bad local certs.
		//
		ctx = mg::net::SSLContext::NewShared(isServer);
		ctx->SetTrust(trust);
		TEST_CHECK(!ctx->AddLocalCertFile("unit_test_ssl_not_existing.pem",
			"unit_test_ssl_not_existing.pem", "bad_pass"));

		ctx = mg::net::SSLContext::NewShared(isServer);
		ctx->SetTrust(trust);
		TEST_CHECK(!ctx->AddLocalCert("bad_cert", 8, "bad_key", 7));

		// Only key is bad.
		ctx = mg::net::SSLContext::NewShared(isServer);
		ctx->SetTrust(trust);
		TEST_CHECK(!ctx->AddLocalCert(
			theUnitTestCert1, theUnitTestCert1Size, "bad_key", 7));

		// Only cert is bad.
		ctx = mg::net::SSLContext::NewShared(isServer);
		ctx->SetTrust(trust);
		TEST_CHECK(!ctx->AddLocalCert(
			"bad_cert", 8, theUnitTestKey1, theUnitTestKey1Size));
	}

	static void
	UnitTestSSLError()
	{
		// Client fails server cert validation. Check if the error code can be translated
		// to string.
		{
			mg::net::SSLContext::Ptr serverCtx = mg::net::SSLContext::NewShared(true);
			serverCtx->SetTrust(mg::net::SSL_TRUST_BYPASS_VERIFICATION);
			TEST_CHECK(serverCtx->AddLocalCert(
				theUnitTestCert31, theUnitTestCert31Size,
				theUnitTestKey3, theUnitTestKey3Size));

			mg::net::SSLContext::Ptr clientCtx = mg::net::SSLContext::NewShared(false);
			clientCtx->SetTrust(mg::net::SSL_TRUST_STRICT);
			TEST_CHECK(clientCtx->AddRemoteCert(theUnitTestCert1, theUnitTestCert1Size));

			mg::net::SSLStream server(serverCtx);
			server.Connect();
			mg::net::SSLStream client(clientCtx);
			client.Connect();
			TEST_CHECK(!UnitTestSSLSend(&client, &server));
			TEST_CHECK(client.HasError() && !server.HasError());

			uint32_t err = client.GetError();
			const char* msg = mg::net::SSLErrorToString(err);
			TEST_CHECK(mg::box::Strcmp(msg, "certificate verify failed") == 0);
		}
		// Server cert is expired.
		{
			mg::net::SSLContext::Ptr serverCtx = mg::net::SSLContext::NewShared(true);
			serverCtx->SetTrust(mg::net::SSL_TRUST_BYPASS_VERIFICATION);
			TEST_CHECK(serverCtx->AddLocalCert(
				theUnitTestCert33Expired, theUnitTestCert33ExpiredSize,
				theUnitTestKey3, theUnitTestKey3Size));

			mg::net::SSLContext::Ptr clientCtx = mg::net::SSLContext::NewShared(false);
			clientCtx->SetTrust(mg::net::SSL_TRUST_STRICT);
			TEST_CHECK(clientCtx->AddRemoteCert(
				theUnitTestCACert3, theUnitTestCACert3Size));

			mg::net::SSLStream server(serverCtx);
			server.Connect();
			mg::net::SSLStream client(clientCtx);
			client.Connect();
			TEST_CHECK(!UnitTestSSLSend(&client, &server));
			TEST_CHECK(client.HasError() && !server.HasError());

			uint32_t err = client.GetError();
			const char* msg = mg::net::SSLErrorToString(err);
			TEST_CHECK(mg::box::Strcmp(msg, "certificate verify failed") == 0);
		}
	}

	static void
	UnitTestSSLExternal_SSL_CTX()
	{
		// There is a global index allocated in every SSL_CTX for SSLContextOpenSSL
		// pointer. It is filled with a real value for SSL_CTXs created by
		// SSLContextOpenSSL, but it is null in the others. For instance, created by other
		// modules such as curl. Then the index destructor should ignore null.
		SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
		SSL_CTX_free(ctx);
	}

	static void
	UnitTestSSLCipherAndVersionCheck()
	{
		// Same cipher and version.
		{
			mg::net::SSLContext::Ptr serverCtx = UnitTestSSLNewServerLocalV1Bypass();
			TEST_CHECK(serverCtx->SetMaxVersion(mg::net::SSL_VERSION_TLSv1_2));
			TEST_CHECK(serverCtx->AddCipher(
				mg::net::SSL_CIPHER_ECDHE_ECDSA_AES128_GCM_SHA256));

			mg::net::SSLContext::Ptr clientCtx = UnitTestSSLNewClientBypass();
			TEST_CHECK(clientCtx->SetMaxVersion(mg::net::SSL_VERSION_TLSv1_2));
			TEST_CHECK(clientCtx->AddCipher(
				mg::net::SSL_CIPHER_ECDHE_ECDSA_AES128_GCM_SHA256));

			UnitTestSSLInteract(serverCtx.GetPointer(), clientCtx.GetPointer(),
				mg::net::SSL_VERSION_TLSv1_2,
				mg::net::SSL_CIPHER_ECDHE_ECDSA_AES128_GCM_SHA256);
		}
		// One version is higher - select minimal.
		{
			mg::net::SSLContext::Ptr serverCtx = UnitTestSSLNewServerLocalV1Bypass();
			TEST_CHECK(serverCtx->SetMaxVersion(mg::net::SSL_VERSION_TLSv1_3));
			TEST_CHECK(serverCtx->AddCipher(
				mg::net::SSL_CIPHER_ECDHE_ECDSA_AES256_GCM_SHA384));

			mg::net::SSLContext::Ptr clientCtx = UnitTestSSLNewClientBypass();
			TEST_CHECK(clientCtx->SetMaxVersion(mg::net::SSL_VERSION_TLSv1_2));
			TEST_CHECK(clientCtx->AddCipher(
				mg::net::SSL_CIPHER_ECDHE_ECDSA_AES256_GCM_SHA384));

			UnitTestSSLInteract(serverCtx.GetPointer(), clientCtx.GetPointer(),
				mg::net::SSL_VERSION_TLSv1_2,
				mg::net::SSL_CIPHER_ECDHE_ECDSA_AES256_GCM_SHA384);
		}
		// Version is selected before cipher. So if the ciphers do not match for a higher
		// version, it won't break.
		{
			mg::net::SSLContext::Ptr serverCtx = UnitTestSSLNewServerLocalV1Bypass();
			TEST_CHECK(serverCtx->SetMaxVersion(mg::net::SSL_VERSION_TLSv1_3));
			// TLSv1.2.
			TEST_CHECK(serverCtx->AddCipher(
				mg::net::SSL_CIPHER_ECDHE_ECDSA_AES256_GCM_SHA384));
			// TLSv1.3.
			TEST_CHECK(serverCtx->AddCipher(
				mg::net::SSL_CIPHER_TLS_AES_128_GCM_SHA256));

			mg::net::SSLContext::Ptr clientCtx = UnitTestSSLNewClientBypass();
			TEST_CHECK(clientCtx->SetMaxVersion(mg::net::SSL_VERSION_TLSv1_2));
			// TLSv1.2.
			TEST_CHECK(clientCtx->AddCipher(
				mg::net::SSL_CIPHER_ECDHE_ECDSA_AES256_GCM_SHA384));
			// TLSv1.3 - different from the server.
			TEST_CHECK(clientCtx->AddCipher(
				mg::net::SSL_CIPHER_TLS_AES_256_GCM_SHA384));

			UnitTestSSLInteract(serverCtx.GetPointer(), clientCtx.GetPointer(),
				mg::net::SSL_VERSION_TLSv1_2,
				mg::net::SSL_CIPHER_ECDHE_ECDSA_AES256_GCM_SHA384);
		}
		// If version is selected and does not have suitable cipher, it breaks. Even if
		// another version has cipher match.
		{
			mg::net::SSLContext::Ptr serverCtx = UnitTestSSLNewServerLocalV1Bypass();
			TEST_CHECK(serverCtx->SetMaxVersion(mg::net::SSL_VERSION_TLSv1_3));
			// TLSv1.3.
			TEST_CHECK(serverCtx->AddCipher(
				mg::net::SSL_CIPHER_TLS_CHACHA20_POLY1305_SHA256));
			// TLSv1.2.
			TEST_CHECK(serverCtx->AddCipher(
				mg::net::SSL_CIPHER_ECDHE_ECDSA_AES128_GCM_SHA256));

			mg::net::SSLContext::Ptr clientCtx = UnitTestSSLNewClientBypass();
			TEST_CHECK(clientCtx->SetMaxVersion(mg::net::SSL_VERSION_TLSv1_3));
			// TLSv1.3 - different from the server.
			TEST_CHECK(clientCtx->AddCipher(
				mg::net::SSL_CIPHER_TLS_AES_256_GCM_SHA384));
			// TLSv1.2.
			TEST_CHECK(clientCtx->AddCipher(
				mg::net::SSL_CIPHER_ECDHE_ECDSA_AES128_GCM_SHA256));

			mg::net::SSLStream server(serverCtx);
			server.Connect();
			mg::net::SSLStream client(clientCtx);
			client.Connect();
			TEST_CHECK(!UnitTestSSLSend(&server, &client));
			TEST_CHECK(server.HasError() && !client.HasError());
		}
		// Version mismatch fails.
		{
			mg::net::SSLContext::Ptr serverCtx = UnitTestSSLNewServerLocalV1Bypass();
			TEST_CHECK(serverCtx->SetMinVersion(mg::net::SSL_VERSION_TLSv1_3));

			mg::net::SSLContext::Ptr clientCtx = UnitTestSSLNewClientBypass();
			TEST_CHECK(clientCtx->SetMaxVersion(mg::net::SSL_VERSION_TLSv1_2));

			mg::net::SSLStream server(serverCtx);
			server.Connect();
			mg::net::SSLStream client(clientCtx);
			client.Connect();
			TEST_CHECK(!UnitTestSSLSend(&server, &client));
			TEST_CHECK(server.HasError() && !client.HasError());
		}
		// Cipher mismatch fails in TLSv1.2.
		{
			mg::net::SSLContext::Ptr serverCtx = UnitTestSSLNewServerLocalV1Bypass();
			TEST_CHECK(serverCtx->SetMaxVersion(mg::net::SSL_VERSION_TLSv1_2));
			TEST_CHECK(serverCtx->AddCipher(
				mg::net::SSL_CIPHER_ECDHE_ECDSA_AES128_GCM_SHA256));

			mg::net::SSLContext::Ptr clientCtx = UnitTestSSLNewClientBypass();
			TEST_CHECK(clientCtx->SetMaxVersion(mg::net::SSL_VERSION_TLSv1_2));
			TEST_CHECK(clientCtx->AddCipher(
				mg::net::SSL_CIPHER_ECDHE_ECDSA_AES256_GCM_SHA384));

			mg::net::SSLStream server(serverCtx);
			server.Connect();
			mg::net::SSLStream client(clientCtx);
			client.Connect();
			TEST_CHECK(!UnitTestSSLSend(&server, &client));
			TEST_CHECK(server.HasError() && !client.HasError());
		}
		// Cipher mismatch fails in TLSv1.3.
		{
			mg::net::SSLContext::Ptr serverCtx = UnitTestSSLNewServerLocalV1Bypass();
			TEST_CHECK(serverCtx->SetMaxVersion(mg::net::SSL_VERSION_TLSv1_3));
			TEST_CHECK(serverCtx->AddCipher(
				mg::net::SSL_CIPHER_TLS_AES_128_GCM_SHA256));

			mg::net::SSLContext::Ptr clientCtx = UnitTestSSLNewClientBypass();
			TEST_CHECK(clientCtx->SetMaxVersion(mg::net::SSL_VERSION_TLSv1_3));
			TEST_CHECK(clientCtx->AddCipher(
				mg::net::SSL_CIPHER_TLS_AES_256_GCM_SHA384));

			mg::net::SSLStream server(serverCtx);
			server.Connect();
			mg::net::SSLStream client(clientCtx);
			client.Connect();
			TEST_CHECK(!UnitTestSSLSend(&server, &client));
			TEST_CHECK(server.HasError() && !client.HasError());
		}
		// TLSv1.0 works.
		{
			mg::net::SSLContext::Ptr serverCtx = UnitTestSSLNewServerLocalV1Bypass();
			TEST_CHECK(serverCtx->SetMaxVersion(mg::net::SSL_VERSION_TLSv1));

			mg::net::SSLContext::Ptr clientCtx = UnitTestSSLNewClientBypass();
			TEST_CHECK(clientCtx->SetMaxVersion(mg::net::SSL_VERSION_TLSv1));

			UnitTestSSLInteract(serverCtx.GetPointer(), clientCtx.GetPointer(),
				mg::net::SSL_VERSION_TLSv1, mg::net::SSL_CIPHER_NONE);
		}
		// TLSv1.1 works.
		{
			mg::net::SSLContext::Ptr serverCtx = UnitTestSSLNewServerLocalV1Bypass();
			TEST_CHECK(serverCtx->SetMaxVersion(mg::net::SSL_VERSION_TLSv1_1));

			mg::net::SSLContext::Ptr clientCtx = UnitTestSSLNewClientBypass();
			TEST_CHECK(clientCtx->SetMaxVersion(mg::net::SSL_VERSION_TLSv1_1));

			UnitTestSSLInteract(serverCtx.GetPointer(), clientCtx.GetPointer(),
				mg::net::SSL_VERSION_TLSv1_1, mg::net::SSL_CIPHER_NONE);
		}
		// TLS 1.0 and 1.1 fail if try to use a cipher supported only in 1.2.
		{
			mg::net::SSLContext::Ptr serverCtx = UnitTestSSLNewServerLocalV1Bypass();
			TEST_CHECK(serverCtx->SetMaxVersion(mg::net::SSL_VERSION_TLSv1));
			TEST_CHECK(serverCtx->AddCipher(
				mg::net::SSL_CIPHER_ECDHE_ECDSA_AES256_GCM_SHA384));

			mg::net::SSLContext::Ptr clientCtx = UnitTestSSLNewClientBypass();
			TEST_CHECK(clientCtx->SetMaxVersion(mg::net::SSL_VERSION_TLSv1));
			TEST_CHECK(clientCtx->AddCipher(
				mg::net::SSL_CIPHER_ECDHE_ECDSA_AES256_GCM_SHA384));

			mg::net::SSLStream server(serverCtx);
			server.Connect();
			mg::net::SSLStream client(clientCtx);
			client.Connect();
			TEST_CHECK(!UnitTestSSLSend(&server, &client));
			TEST_CHECK(server.HasError() && !client.HasError());
		}
		// Conflicting version range min > max is auto-fixed (it needs to be, otherwise
		// OpenSSL can break even if set just max or min alone).
		{
			mg::net::SSLContext::Ptr serverCtx1 = UnitTestSSLNewServerLocalV1Bypass();
			TEST_CHECK(serverCtx1->SetMaxVersion(mg::net::SSL_VERSION_TLSv1_1));
			TEST_CHECK(serverCtx1->SetMinVersion(mg::net::SSL_VERSION_TLSv1_3));

			mg::net::SSLContext::Ptr serverCtx2 = UnitTestSSLNewServerLocalV1Bypass();
			TEST_CHECK(serverCtx2->SetMinVersion(mg::net::SSL_VERSION_TLSv1_3));
			TEST_CHECK(serverCtx2->SetMaxVersion(mg::net::SSL_VERSION_TLSv1_1));

			mg::net::SSLContext::Ptr clientCtx = UnitTestSSLNewClientBypass();
			TEST_CHECK(clientCtx->SetMaxVersion(mg::net::SSL_VERSION_ANY));
			TEST_CHECK(clientCtx->SetMinVersion(mg::net::SSL_VERSION_TLSv1));

			UnitTestSSLInteract(serverCtx1.GetPointer(), clientCtx.GetPointer(),
				mg::net::SSL_VERSION_TLSv1_3, mg::net::SSL_CIPHER_NONE);
			UnitTestSSLInteract(serverCtx2.GetPointer(), clientCtx.GetPointer(),
				mg::net::SSL_VERSION_TLSv1_1, mg::net::SSL_CIPHER_NONE);
		}
		// Cipher order matters for TLSv1.3.
		{
			mg::net::SSLContext::Ptr serverCtx = UnitTestSSLNewServerLocalV1Bypass();

			mg::net::SSLContext::Ptr clientCtx1 = UnitTestSSLNewClientBypass();
			// Add some TLSv1.2 noise to see if it won't break the order.
			TEST_CHECK(clientCtx1->AddCipher(
				mg::net::SSL_CIPHER_ECDHE_ECDSA_AES256_GCM_SHA384));
			TEST_CHECK(clientCtx1->AddCipher(
				mg::net::SSL_CIPHER_TLS_AES_128_GCM_SHA256));
			TEST_CHECK(clientCtx1->AddCipher(
				mg::net::SSL_CIPHER_TLS_AES_256_GCM_SHA384));

			mg::net::SSLContext::Ptr clientCtx2 = UnitTestSSLNewClientBypass();
			// Add some TLSv1.2 noise to see if it won't break the order.
			TEST_CHECK(clientCtx2->AddCipher(
				mg::net::SSL_CIPHER_ECDHE_ECDSA_AES256_GCM_SHA384));
			TEST_CHECK(clientCtx2->AddCipher(
				mg::net::SSL_CIPHER_TLS_AES_256_GCM_SHA384));
			TEST_CHECK(clientCtx2->AddCipher(
				mg::net::SSL_CIPHER_TLS_AES_128_GCM_SHA256));

			UnitTestSSLInteract(serverCtx.GetPointer(), clientCtx1.GetPointer(),
				mg::net::SSL_VERSION_TLSv1_3,
				mg::net::SSL_CIPHER_TLS_AES_128_GCM_SHA256);

			UnitTestSSLInteract(serverCtx.GetPointer(), clientCtx2.GetPointer(),
				mg::net::SSL_VERSION_TLSv1_3,
				mg::net::SSL_CIPHER_TLS_AES_256_GCM_SHA384);
		}
		// Cipher order matters for TLSv1.2.
		{
			mg::net::SSLContext::Ptr serverCtx = UnitTestSSLNewServerLocalV1Bypass();

			mg::net::SSLContext::Ptr clientCtx1 = UnitTestSSLNewClientBypass();
			clientCtx1->SetMaxVersion(mg::net::SSL_VERSION_TLSv1_2);
			// Add some TLSv1.3 noise to see if it won't break the order.
			TEST_CHECK(clientCtx1->AddCipher(
				mg::net::SSL_CIPHER_TLS_AES_256_GCM_SHA384));
			TEST_CHECK(clientCtx1->AddCipher(
				mg::net::SSL_CIPHER_ECDHE_ECDSA_AES128_GCM_SHA256));
			TEST_CHECK(clientCtx1->AddCipher(
				mg::net::SSL_CIPHER_ECDHE_ECDSA_AES256_GCM_SHA384));

			mg::net::SSLContext::Ptr clientCtx2 = UnitTestSSLNewClientBypass();
			clientCtx2->SetMaxVersion(mg::net::SSL_VERSION_TLSv1_2);
			// Add some TLSv1.3 noise to see if it won't break the order.
			TEST_CHECK(clientCtx2->AddCipher(
				mg::net::SSL_CIPHER_TLS_AES_256_GCM_SHA384));
			TEST_CHECK(clientCtx1->AddCipher(
				mg::net::SSL_CIPHER_ECDHE_ECDSA_AES256_GCM_SHA384));
			TEST_CHECK(clientCtx1->AddCipher(
				mg::net::SSL_CIPHER_ECDHE_ECDSA_AES128_GCM_SHA256));

			UnitTestSSLInteract(serverCtx.GetPointer(), clientCtx1.GetPointer(),
				mg::net::SSL_VERSION_TLSv1_2,
				mg::net::SSL_CIPHER_ECDHE_ECDSA_AES128_GCM_SHA256);

			UnitTestSSLInteract(serverCtx.GetPointer(), clientCtx2.GetPointer(),
				mg::net::SSL_VERSION_TLSv1_2,
				mg::net::SSL_CIPHER_ECDHE_ECDSA_AES256_GCM_SHA384);
		}
	}

	static void
	UnitTestSSLHostName()
	{
		mg::net::SSLVersion versions[] = {
			mg::net::SSL_VERSION_TLSv1,
			mg::net::SSL_VERSION_TLSv1_1,
			mg::net::SSL_VERSION_TLSv1_2,
			mg::net::SSL_VERSION_TLSv1_3,
			mg::net::SSL_VERSION_ANY,
		};
		size_t count = sizeof(versions) / sizeof(versions[0]);
		for (size_t verInt = 0; verInt < count; ++verInt)
		{
			if (verInt == mg::net::SSL_VERSION_NONE)
				continue;
			mg::net::SSLVersion ver = (mg::net::SSLVersion)verInt;
			mg::net::SSLContext::Ptr serverCtx =  mg::net::SSLContext::NewShared(true);
			serverCtx->SetTrust(mg::net::SSL_TRUST_BYPASS_VERIFICATION);
			TEST_CHECK(serverCtx->AddLocalCert(
				theUnitTestCert31, theUnitTestCert31Size,
				theUnitTestKey3, theUnitTestKey3Size));
			serverCtx->SetMinVersion(ver);

			mg::net::SSLContext::Ptr clientCtx = mg::net::SSLContext::NewShared(false);
			clientCtx->SetTrust(mg::net::SSL_TRUST_STRICT);
			clientCtx->AddRemoteCert(theUnitTestCACert3Pem, theUnitTestCACert3PemSize);
			clientCtx->SetMinVersion(ver);

			mg::net::SSLStream server(serverCtx);
			TEST_CHECK(server.GetHostName().empty());
			server.Connect();
			mg::net::SSLStream client(clientCtx);
			client.SetHostName("testserver");
			TEST_CHECK(client.GetHostName() == "testserver");
			client.Connect();

			auto checkNameWhenConnected = [&]() {
				if (server.IsConnected())
					TEST_CHECK(server.GetHostName() == "testserver");
			};
			while (!server.IsConnected() && !client.IsConnected())
			{
				mg::net::Buffer::Ptr head;
				server.PopNetOutput(head);
				checkNameWhenConnected();
				client.AppendNetInputRef(std::move(head));
				client.Update();

				client.PopNetOutput(head);
				server.AppendNetInputRef(std::move(head));
				checkNameWhenConnected();
				server.Update();
				checkNameWhenConnected();

				TEST_CHECK(!server.HasError() && !client.HasError());
			}
			checkNameWhenConnected();
			UnitTestSSLInteract(server, client);
		}
	}

	static void
	UnitTestSSLShutdown()
	{
		mg::net::SSLContext::Ptr serverCtx = mg::net::SSLContext::NewShared(true);
		serverCtx->SetTrust(mg::net::SSL_TRUST_BYPASS_VERIFICATION);
		TEST_CHECK(serverCtx->AddLocalCert(
			theUnitTestCert1, theUnitTestCert1Size,
			theUnitTestKey1, theUnitTestKey1Size));
		mg::net::SSLStream server(serverCtx);
		server.Connect();

		mg::net::SSLContext::Ptr clientCtx = mg::net::SSLContext::NewShared(false);
		clientCtx->SetTrust(mg::net::SSL_TRUST_BYPASS_VERIFICATION);
		mg::net::SSLStream client(clientCtx);
		client.Connect();

		// Normal shutdown.
		{
			mg::net::SSLStream filter1(serverCtx);
			filter1.Connect();
			mg::net::SSLStream filter2(clientCtx);
			filter2.Connect();
			UnitTestSSLInteract(filter1, filter2);

			filter1.Shutdown();
			// Stays marked encrypted even after shutdown. Once encrypted - always
			// stays so.
			TEST_CHECK(filter1.IsEncrypted());
			UnitTestSSLFinalizeShutdown(filter1, filter2);

			// After shutdown data sending won't work.
			TEST_CHECK(!filter1.IsConnected());
			TEST_CHECK(!filter2.IsConnected());
			filter1.AppendAppInputCopy("123", 3);
			mg::net::Buffer::Ptr head;
			int64_t rc = filter1.PopNetOutput(head);
			TEST_CHECK(!filter1.HasError());
			TEST_CHECK(rc == 0);
			TEST_CHECK(!head.IsSet());
			TEST_CHECK(filter1.IsEncrypted());
		}
		// Break shutdown with invalid data.
		{
			mg::net::SSLStream filter1(serverCtx);
			filter1.Connect();
			mg::net::SSLStream filter2(clientCtx);
			filter2.Connect();
			UnitTestSSLInteract(filter1, filter2);

			filter1.Shutdown();
			filter1.AppendNetInputCopy("bad data", 8);
			TEST_CHECK(!filter1.Update());
			TEST_CHECK(filter1.HasError());
		}
		// Send unencrypted data during shutdown. It gets dropped when shutdown starts.
		{
			mg::net::SSLStream filter1(serverCtx);
			filter1.Connect();
			mg::net::SSLStream filter2(clientCtx);
			filter2.Connect();
			UnitTestSSLInteract(filter1, filter2);

			filter1.Shutdown();
			filter1.AppendAppInputCopy("data", 4);
			UnitTestSSLFinalizeShutdown(filter1, filter2);
			mg::net::Buffer::Ptr head;
			TEST_CHECK(filter1.PopAppOutput(head) == 0);
			TEST_CHECK(filter2.PopAppOutput(head) == 0);
		}
		// Shutdown a new stream. Data gets discarded then.
		{
			mg::net::SSLStream filter1(serverCtx);
			filter1.AppendAppInputCopy("data", 4);
			filter1.Shutdown();
			TEST_CHECK(filter1.IsClosed());
			mg::net::Buffer::Ptr head;
			TEST_CHECK(filter1.PopNetOutput(head) == 0);
		}
		// Shutdown while being closed remotely.
		{
			mg::net::SSLStream filter1(serverCtx);
			filter1.Connect();
			mg::net::SSLStream filter2(clientCtx);
			filter2.Connect();
			UnitTestSSLInteract(filter1, filter2);

			filter1.Shutdown();
			mg::net::Buffer::Ptr head;
			int64_t rc = filter1.PopNetOutput(head);
			TEST_CHECK(rc > 0);
			filter2.AppendNetInputRef(std::move(head));
			TEST_CHECK(filter2.PopAppOutput(head) == 0);
			TEST_CHECK(filter2.IsClosingOrClosed());
			TEST_CHECK(!filter2.IsClosed());
			filter2.Shutdown();
			UnitTestSSLFinalizeShutdown(filter1, filter2);
		}
	}

}

	void
	UnitTestSSL()
	{
		using namespace ssl;
		TestSuiteGuard suite("SSL");

		UnitTestSSLBioMemOpenSSLBasic();
		UnitTestSSLBioMemOpenSSLConsistency();
		UnitTestSSLCompare();
		UnitTestSSLMethods();
		UnitTestSSLStreamInteraction();
		UnitTestSSLStreamEnable();
		UnitTestSSLBadCertsAndKeys();
		UnitTestSSLError();
		UnitTestSSLExternal_SSL_CTX();
		UnitTestSSLCipherAndVersionCheck();
		UnitTestSSLHostName();
		UnitTestSSLShutdown();
	}

}
}
}

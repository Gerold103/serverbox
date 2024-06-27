#include "SSLBioMem.h"

#include "mg/net/SSLContextOpenSSL.h"

namespace mg {
namespace net {

	static int
	SSLBioMemOpenSSLWriteEx(
		BIO* aBase,
		const char* aData,
		size_t aInSize,
		size_t* aOutSize)
	{
		BIO_clear_retry_flags(aBase);
		BufferStream* bio = (BufferStream*)BIO_get_data(aBase);
		bio->WriteCopy(aData, aInSize);
		*aOutSize = aInSize;
		return 1;
	}

	static int
	SSLBioMemOpenSSLReadEx(
		BIO* aBase,
		char* aDst,
		size_t aInSize,
		size_t* aOutSize)
	{
		BIO_clear_retry_flags(aBase);
		BufferStream* bio = (BufferStream*)BIO_get_data(aBase);
		uint64_t size = bio->GetReadSize();
		if (size == 0)
		{
			BIO_set_retry_read(aBase);
			*aOutSize = 0;
			return 0;
		}
		if (aInSize == 0)
		{
			*aOutSize = 0;
			return 0;
		}
		if (size < aInSize)
			aInSize = size;
		bio->ReadData(aDst, aInSize);
		*aOutSize = aInSize;
		return 1;
	}

	static long
	SSLBioMemOpenSSLCtrl(
		BIO* /*aBase*/,
		int aCmd,
		long /*aNum*/,
		void* /*aPtr*/)
	{
		// No need for other commands yet. SSL either doesn't use the others or ignores
		// their result anyway.
		return aCmd == BIO_CTRL_FLUSH;
	}

	static int
	SSLBioMemOpenSSLDestroy(
		BIO* /*aBase*/)
	{
		return 1;
	}

	static BIO_METHOD*
	SSLBioMemOpenSSLNewMethod()
	{
		int idx = BIO_get_new_index();
		MG_BOX_ASSERT(idx != -1);
		BIO_METHOD* method = BIO_meth_new(idx, "bio_openssl");
		MG_BOX_ASSERT(method != nullptr);
		bool ok = true;
		// No plain read/write are set. OpenSSL uses either _ex or plain callbacks.
		ok = ok && BIO_meth_set_write_ex(method, SSLBioMemOpenSSLWriteEx) == 1;
		ok = ok && BIO_meth_set_read_ex(method, SSLBioMemOpenSSLReadEx) == 1;
		ok = ok && BIO_meth_set_ctrl(method, SSLBioMemOpenSSLCtrl) == 1;
		ok = ok && BIO_meth_set_destroy(method, SSLBioMemOpenSSLDestroy) == 1;
		MG_BOX_ASSERT(ok);
		return method;
	}

	BIO*
	OpenSSLWrapBioMem(
		BufferStream& aStorage)
	{
		// Destroy is called when the bio is deleted.
		// Init OpenSSL before creating the method, because the
		// creation might need it.
		static BIO_METHOD* methodSingleton = SSLBioMemOpenSSLNewMethod();
		BIO* res = BIO_new(methodSingleton);
		BIO_set_data(res, &aStorage);
		return res;
	}

}
}

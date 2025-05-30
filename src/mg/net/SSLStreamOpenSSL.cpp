#include "SSLStreamOpenSSL.h"

#include "mg/net/SSLOpenSSL.h"
#include "mg/net/SSLContextOpenSSL.h"

#include <openssl/err.h>

namespace mg {
namespace net {

	static void OpenSSLPrepareToCall();

	SSLStreamOpenSSL::SSLStreamOpenSSL(
		SSLContextOpenSSL* aCtx)
		: mySSL(SSL_new(aCtx->myConfig))
		, myError(0)
		, myState(SSL_STREAM_OPENSSL_STATE_NEW)
	{
		MG_BOX_ASSERT(mySSL != NULL);
		if (aCtx->IsServer())
			SSL_set_accept_state(mySSL);
		else
			SSL_set_connect_state(mySSL);
	}

	SSLStreamOpenSSL::~SSLStreamOpenSSL()
	{
		if (myState != SSL_STREAM_OPENSSL_STATE_CLOSING &&
			myState != SSL_STREAM_OPENSSL_STATE_CLOSED)
		{
			SSL_shutdown(mySSL);
		}
		SSL_free(mySSL);
	}

	bool
	SSLStreamOpenSSL::Update()
	{
		if (PrivUpdate())
			return true;
		return myError == 0;
	}

	void
	SSLStreamOpenSSL::Connect()
	{
		MG_BOX_ASSERT(myState == SSL_STREAM_OPENSSL_STATE_NEW);
		MG_BOX_ASSERT(myAppInput.IsEmpty());
		// Treat the rest of already received but not yet read data as encrypted. It looks
		// logical. The client could send a command like 'enable encryption' and then
		// encrypted data. It can't be handled that way given that the sockets usually
		// read all the data from SSL filter at once, not message by message and then
		// parse it, but anyway.
		PrivSetState(SSL_STREAM_OPENSSL_STATE_CONNECTING);
		SSL_set_bio(mySSL, OpenSSLWrapBioMem(myNetInput), OpenSSLWrapBioMem(myNetOutput));
	}

	void
	SSLStreamOpenSSL::Shutdown()
	{
		if (myState == SSL_STREAM_OPENSSL_STATE_NEW)
		{
			// Non-encrypted data. Don't leak it after shutdown.
			myNetOutput.Clear();
			PrivSetState(SSL_STREAM_OPENSSL_STATE_CLOSED);
			return;
		}
		if (myState == SSL_STREAM_OPENSSL_STATE_CLOSING)
			return;
		MG_BOX_ASSERT(
			myState == SSL_STREAM_OPENSSL_STATE_CONNECTING ||
			myState == SSL_STREAM_OPENSSL_STATE_CONNECTED);
		int rc = SSL_shutdown(mySSL);
		if (rc == 0)
		{
			PrivSetState(SSL_STREAM_OPENSSL_STATE_CLOSING);
			return;
		}
		if (rc == 1)
		{
			PrivSetState(SSL_STREAM_OPENSSL_STATE_CLOSED);
			return;
		}
		PrivCheckError(rc);
	}

	bool
	SSLStreamOpenSSL::SetHostName(
		const char* aName)
	{
		const SSLContextOpenSSL* ctx = OpenSSL_SSL_ToContext(mySSL);
		MG_BOX_ASSERT(!ctx->IsServer());
		return SSL_set_tlsext_host_name(mySSL, aName) == 1;
	}

	std::string
	SSLStreamOpenSSL::GetHostName() const
	{
		const char* name = SSL_get_servername(mySSL, TLSEXT_NAMETYPE_host_name);
		if (name == NULL)
			return {};
		return name;
	}

	void
	SSLStreamOpenSSL::AppendAppInputCopy(
		const void* aData,
		uint64_t aSize)
	{
		int rc;
		if (!PrivUpdate())
		{
			MG_BOX_ASSERT(myState != SSL_STREAM_OPENSSL_STATE_CONNECTED);
			if (myState == SSL_STREAM_OPENSSL_STATE_NEW)
			{
				myNetOutput.WriteCopy(aData, aSize);
				return;
			}
			if (myState == SSL_STREAM_OPENSSL_STATE_CLOSED ||
				myState == SSL_STREAM_OPENSSL_STATE_CLOSING)
				return;
			MG_BOX_ASSERT(myState == SSL_STREAM_OPENSSL_STATE_CONNECTING);
			goto fallback;
		}
		if (!myAppInput.IsEmpty())
			goto fallback;
		rc = PrivAppWrite(aData, aSize);
		if (rc < 0)
			return;
		if (rc == 0)
			goto fallback;
		return;

	fallback:
		myAppInput.WriteCopy(aData, aSize);
	}

	uint64_t
	SSLStreamOpenSSL::PopAppOutput(
		Buffer::Ptr& aOutHead)
	{
		if (!PrivUpdate())
		{
			if (myState == SSL_STREAM_OPENSSL_STATE_NEW)
			{
				uint64_t res = myNetInput.GetReadSize();
				aOutHead = myNetInput.PopData();
				return res;
			}
			return 0;
		}
		OpenSSLPrepareToCall();
		uint64_t byteCount = 0;
		Buffer::Ptr head;
		Buffer* tail = nullptr;
		// Need a cached buffer because can't tell whether SSL has any data until try to
		// read. But then need a place to read into. If nothing is read, then the buffer
		// is not deleted but reused later. An alternative is to read into a buffer on the
		// stack but it would require further copying into the list then.
		Buffer* next = PrivGetCachedBuf();
		int rc;
		while ((rc = SSL_read(mySSL, next->myWData, next->myCapacity - next->myPos)) > 0)
		{
			next->myPos += rc;
			byteCount += (uint64_t)rc;
			if (!head.IsSet())
				head = std::move(myCachedBuf);
			else
				tail->myNext = std::move(myCachedBuf);
			tail = next;
			next = PrivGetCachedBuf();
		}
		if (!PrivCheckError(rc))
			return 0;
		aOutHead = std::move(head);
		return byteCount;
	}

	SSLVersion
	SSLStreamOpenSSL::GetVersion() const
	{
		MG_BOX_ASSERT(myState == SSL_STREAM_OPENSSL_STATE_CONNECTED);
		return OpenSSLVersionFromInt(SSL_version(mySSL));
	}

	SSLCipher
	SSLStreamOpenSSL::GetCipher() const
	{
		MG_BOX_ASSERT(myState == SSL_STREAM_OPENSSL_STATE_CONNECTED);
		return OpenSSLCipherFromString(SSL_get_cipher(mySSL));
	}

	bool
	SSLStreamOpenSSL::PrivUpdate()
	{
		if (myState == SSL_STREAM_OPENSSL_STATE_CONNECTED)
			return PrivFlushAppInput();
		if (myState == SSL_STREAM_OPENSSL_STATE_NEW)
			return false;
		if (myState == SSL_STREAM_OPENSSL_STATE_CLOSING)
		{
			MG_BOX_ASSERT(myAppInput.IsEmpty());
			int rc = SSL_shutdown(mySSL);
			if (rc == 1)
				PrivSetState(SSL_STREAM_OPENSSL_STATE_CLOSED);
			else if (rc < 0)
				PrivCheckError(rc);
			else
				MG_BOX_ASSERT(rc == 0);
			return false;
		}
		if (myState == SSL_STREAM_OPENSSL_STATE_CLOSED)
			return false;
		MG_BOX_ASSERT(myError == 0);
		OpenSSLPrepareToCall();
		// It is called in other SSL functions internally as well. But here it is done
		// explicitly to notice the handshake end and react to it.
		int rc = SSL_do_handshake(mySSL);
		if (rc != 1)
			return PrivCheckError(rc);
		PrivSetState(SSL_STREAM_OPENSSL_STATE_CONNECTED);
		return true;
	}

	bool
	SSLStreamOpenSSL::PrivFlushAppInput()
	{
		while (myAppInput.GetReadSize() > 0)
		{
			const Buffer* pos = myAppInput.GetReadPos();
			const uint8_t* buf = pos->myRData;
			uint64_t size = pos->myPos;
			MG_BOX_ASSERT(size != 0);
			int rc = PrivAppWrite(buf, size);
			if (rc < 0)
				return false;
			if (rc == 0)
				return true;
			myAppInput.SkipData(size);
		}
		return true;
	}

	int
	SSLStreamOpenSSL::PrivAppWrite(
		const void* aData,
		uint64_t aSize)
	{
		OpenSSLPrepareToCall();
		size_t outSize = 0;
		int rc = SSL_write_ex(mySSL, aData, aSize, &outSize);
		if (rc != 1)
		{
			if (!PrivCheckError(rc))
				return -1;
			return 0;
		}
		// Partial writes are not possible unless configured so, which is not the case.
		MG_BOX_ASSERT(outSize == aSize);
		return 1;
	}

	void
	SSLStreamOpenSSL::PrivClose(
		uint32_t aErr)
	{
		MG_BOX_ASSERT(aErr != 0);
		if (myState == SSL_STREAM_OPENSSL_STATE_CLOSED)
			return;
		if (myError == 0)
			myError = aErr;
		if (myState == SSL_STREAM_OPENSSL_STATE_CLOSING)
		{
			MG_BOX_ASSERT(myAppInput.IsEmpty());
			PrivSetState(SSL_STREAM_OPENSSL_STATE_CLOSED);
			return;
		}
		int rc = SSL_shutdown(mySSL);
		if (rc == 0)
			PrivSetState(SSL_STREAM_OPENSSL_STATE_CLOSING);
		else
			PrivSetState(SSL_STREAM_OPENSSL_STATE_CLOSED);
	}

	bool
	SSLStreamOpenSSL::PrivCheckError(
		int aRetCode)
	{
		int err = SSL_get_error(mySSL, aRetCode);
		if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
			return true;
		if (err == SSL_ERROR_ZERO_RETURN)
		{
			PrivSetState(SSL_STREAM_OPENSSL_STATE_CLOSING);
			return true;
		}
		if (err == SSL_ERROR_SSL)
			PrivClose(ERR_get_error());
		else
			PrivClose(OpenSSLPackError(err));
		return false;
	}

	void
	SSLStreamOpenSSL::PrivSetState(
		SSLStreamOpenSSLState aState)
	{
		MG_BOX_ASSERT(myState != aState);
		if (myState == SSL_STREAM_OPENSSL_STATE_CLOSING ||
			myState == SSL_STREAM_OPENSSL_STATE_CLOSED)
		{
			MG_BOX_ASSERT(myAppInput.IsEmpty());
		}
		if (aState == SSL_STREAM_OPENSSL_STATE_CLOSING ||
			aState == SSL_STREAM_OPENSSL_STATE_CLOSED)
		{
			myAppInput.Clear();
		}
		myState = aState;
	}

	Buffer*
	SSLStreamOpenSSL::PrivGetCachedBuf()
	{
		Buffer* res = myCachedBuf.GetPointer();
		if (res != nullptr)
			return res;
		return (myCachedBuf = BufferCopy::NewShared()).GetPointer();
	}

	static void inline
	OpenSSLPrepareToCall()
	{
		// OpensSSL says that getting an error is not reliable unless the error queue is
		// cleared before any SSL I/O call. Can be omitted if an error is not possible, or
		// is handled by an assertion, or just isn't checked.
		ERR_clear_error();
	}

}
}

#pragma once

#include "mg/net/SSL.h"
#include "mg/net/SSLBioMem.h"

#include <openssl/ssl.h>

namespace mg {
namespace net {

	class SSLContextOpenSSL;

	enum SSLStreamOpenSSLState
	{
		SSL_STREAM_OPENSSL_STATE_NEW,
		SSL_STREAM_OPENSSL_STATE_CONNECTING,
		SSL_STREAM_OPENSSL_STATE_CONNECTED,
		SSL_STREAM_OPENSSL_STATE_CLOSING,
		SSL_STREAM_OPENSSL_STATE_CLOSED,
	};

	class SSLStreamOpenSSL
	{
	public:
		SSLStreamOpenSSL(
			SSLContextOpenSSL* aCtx);

		~SSLStreamOpenSSL();

		bool Update();
		void Connect();
		void Shutdown();
		bool SetHostName(
			const char* aName);
		std::string GetHostName() const;

		void AppendNetInputCopy(
			const void* aData,
			uint64_t aSize) { myNetInput.WriteCopy(aData, aSize); }

		void AppendNetInputRef(
			Buffer::Ptr&& aHead) { myNetInput.WriteRef(std::move(aHead)); }

		uint64_t PopNetOutput(
			Buffer::Ptr& aOutHead);

		void AppendAppInputCopy(
			const void* aData,
			uint64_t aSize);

		uint64_t PopAppOutput(
			Buffer::Ptr& aOutHead);

		bool HasError() const { return myError != 0; }
		bool IsConnected() const { return myState == SSL_STREAM_OPENSSL_STATE_CONNECTED; }
		bool IsClosed() const { return myState == SSL_STREAM_OPENSSL_STATE_CLOSED; }
		bool IsClosingOrClosed() const {
			return myState >= SSL_STREAM_OPENSSL_STATE_CLOSING; }

		SSLVersion GetVersion() const;
		SSLCipher GetCipher() const;
		uint32_t GetError() const { return myError; }

	private:
		bool PrivUpdate();
		bool PrivFlushAppInput();
		int PrivAppWrite(
			const void* aData,
			uint64_t aSize);
		void PrivClose(
			uint32_t aErr);
		bool PrivCheckError(
			int aRetCode);
		void PrivSetState(
			SSLStreamOpenSSLState aState);

		mg::net::Buffer* PrivGetCachedBuf();

		SSL* mySSL;
		BufferStream myNetInput;
		BufferStream myNetOutput;
		BufferStream myAppInput;
		Buffer::Ptr myCachedBuf;
		uint32_t myError;
		SSLStreamOpenSSLState myState;
	};

	inline uint64_t
	SSLStreamOpenSSL::PopNetOutput(
		Buffer::Ptr& aOutHead)
	{
		PrivUpdate();
		uint64_t res = myNetOutput.GetReadSize();
		aOutHead = myNetOutput.PopData();
		return res;
	}

}
}

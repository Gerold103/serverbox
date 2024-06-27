#pragma once

#include "mg/net/SSL.h"
#include "mg/net/SSLBioMem.h"

#include <openssl/ssl.h>

namespace mg {
namespace net {

	class SSLContextOpenSSL;

	class SSLStreamOpenSSL
	{
	public:
		SSLStreamOpenSSL(
			SSLContextOpenSSL* aCtx);

		~SSLStreamOpenSSL();

		bool Update();
		void Connect();
		bool SetHostName(
			const char* aName);

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
		bool IsConnected() const { return myIsConnected; }
		bool IsEnabled() const { return myIsEnabled; }

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

		mg::net::Buffer* PrivGetCachedBuf();

		SSL* mySSL;
		BufferStream myNetInput;
		BufferStream myNetOutput;
		BufferStream myAppInput;
		Buffer::Ptr myCachedBuf;
		uint32_t myError;
		bool myIsConnected;
		bool myIsEnabled;
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

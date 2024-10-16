#pragma once

#include "mg/net/SSL.h"
#include "mg/net/SSLStreamOpenSSL.h"

namespace mg {
namespace net {

	class SSLContext;
	using SSLStreamBase = SSLStreamOpenSSL;

	class SSLStream
	{
	public:
		SSLStream(
			SSLContext* aCtx);
		SSLStream(
			SSLContext::Ptr& aCtx) : SSLStream(aCtx.GetPointer()) {}
		SSLStream(
			const SSLStream&) = delete;
		~SSLStream();

		SSLStream& operator=(
			const SSLStream&) = delete;

		bool Update() { return myImpl.Update(); }
		void Connect() { myImpl.Connect(); }
		void Shutdown() { myImpl.Shutdown(); }
		bool SetHostName(
			const char* aName) { return myImpl.SetHostName(aName); }
		std::string GetHostName() const { return myImpl.GetHostName(); }

		void AppendNetInputCopy(
			const void* aData,
			uint64_t aSize) { myImpl.AppendNetInputCopy(aData, aSize); }

		void AppendNetInputRef(
			Buffer::Ptr&& aHead) { myImpl.AppendNetInputRef(std::move(aHead)); }

		uint64_t PopNetOutput(
			Buffer::Ptr& aOutHead) { return myImpl.PopNetOutput(aOutHead); }

		void AppendAppInputCopy(
			const void* aData,
			uint64_t aSize) { myImpl.AppendAppInputCopy(aData, aSize); }

		uint64_t PopAppOutput(
			Buffer::Ptr& aOutHead) { return myImpl.PopAppOutput(aOutHead); }

		bool HasError() const { return myImpl.HasError(); }
		bool IsConnected() const { return myImpl.IsConnected(); }
		bool IsClosed() const { return myImpl.IsClosed(); }
		bool IsClosingOrClosed() const { return myImpl.IsClosingOrClosed(); }
		bool IsEncrypted() const { return myImpl.IsEncrypted(); }

		SSLVersion GetVersion() const { return myImpl.GetVersion(); }
		SSLCipher GetCipher() const { return myImpl.GetCipher(); }
		uint32_t GetError() const { return myImpl.GetError(); }

	private:
		SSLStreamBase myImpl;
	};


}
}

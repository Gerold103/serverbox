#pragma once

#include "mg/aio/IOTask.h"

#include "mg/box/Mutex.h"
#include "mg/box/SharedPtr.h"

#include "mg/net/Host.h"

namespace mg {
namespace aio {

	class IOCore;

	enum TCPServerState
	{
		TCP_SERVER_STATE_NEW,
		TCP_SERVER_STATE_BOUND,
		TCP_SERVER_STATE_LISTENING,
		TCP_SERVER_STATE_CLOSING,
		TCP_SERVER_STATE_CLOSED,
	};

	class TCPServerSubscription
	{
	public:
		virtual void OnAccept(
			mg::net::Socket aSock,
			const mg::net::Host& aHost) = 0;
		virtual void OnClose() = 0;
	};

	class TCPServer
		: private IOSubscription
	{
	public:
		SHARED_PTR_RE_API(TCPServer)

		bool Bind(
			const mg::net::Host& aHost,
			mg::box::Error::Ptr& aOutErr);

		uint16_t GetPort() const;

		bool Listen(
			IOCore& aCore,
			TCPServerSubscription* aSub,
			mg::box::Error::Ptr& aOutErr);

		void Close();
		bool IsClosed() const;

	private:
		TCPServer();
		~TCPServer() override;

		void OnEvent(
			const IOArgs& aArgs) override;

		mutable mg::box::Mutex myMutex;
		TCPServerState myState;
		IOTask myTask;
		IOEvent myAcceptEvent;
		TCPServerSubscription* mySub;
		IOServerSocket* myBoundSocket;
	};

}
}

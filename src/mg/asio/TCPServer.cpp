#include "TCPServer.h"

namespace mg {
namespace asio {

	bool
	TCPServer::Bind(
		uint16_t aPort,
		mg::box::Error::Ptr& aOutErr)
	{
		MG_DEV_ASSERT(myState == TCP_SERVER_STATE_NEW);
		IOServerSocket* sock = SocketBind(aPort, aOutErr);
		if (sock == nullptr)
			return false;
		myState = TCP_SERVER_STATE_BOUND;
		myBoundSocket = sock;
		return true;
	}

	bool
	TCPServer::Listen(
		IOCore& aCore,
		TCPServerSubscription* aSub,
		mg::box::Error::Ptr& aOutErr)
	{
		mg::box::MutexLock lock(myMutex);
		MG_DEV_ASSERT(myState == TCP_SERVER_STATE_BOUND);
		// Move the ownership to IOCore.
		IOServerSocket* sock = myBoundSocket;
		myBoundSocket = nullptr;

		if (!SocketListen(myBoundSocket, INT_MAX, aOutErr))
		{
			myBoundSocket = sock;
			return false;
		}
		MG_DEV_ASSERT(mySub == nullptr);
		mySub = aSub;
		myState = TCP_SERVER_STATE_LISTENING;
		myTask.Post(aCore, sock, this);
		// Immediately start serving.
		myTask.Wakeup();
		return true;
	}

	void
	TCPServer::Close()
	{
		mg::box::MutexLock lock(myMutex);
		if (myState == TCP_SERVER_STATE_LISTENING)
		{
			myState = TCP_SERVER_STATE_CLOSING;
			myTask.Close();
			return;
		}
		myState = TCP_SERVER_STATE_CLOSED;
	}

	bool
	TCPServer::IsClosed() const
	{
		mg::box::MutexLock lock(myMutex);
		return myState == TCP_SERVER_STATE_CLOSED;
	}

	TCPServer::TCPServer()
		: myState(TCP_SERVER_STATE_NEW)
		, mySub(nullptr)
		, myBoundSocket(nullptr)
	{
	}

	TCPServer::~TCPServer()
	{
		mg::box::MutexLock lock(myMutex);
		MG_DEV_ASSERT(myState == TCP_SERVER_STATE_CLOSED);
		delete myBoundSocket;
		myBoundSocket = nullptr;
	}

	void
	TCPServer::OnEvent(
		const IOArgs& aArgs)
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		if (myTask.IsClosed())
		{
			myMutex.Lock();
			MG_BOX_ASSERT(myState == TCP_SERVER_STATE_CLOSING &&
				"TCPServer is killed externally");
			myState = TCP_SERVER_STATE_CLOSED;
			myMutex.Unlock();

			// Move the sub out of the server. The latter might be deleted by
			// the sub in the OnClose() callback.
			TCPServerSubscription* sub = mySub;
			mySub = nullptr;
			sub->OnClose();
			return;
		}
		mg::box::Error::Ptr err;
		if (!myTask.ProcessArgs(aArgs, err))
		{
			MG_BOX_ASSERT_F(false, "TCPServer error: %s",
				err->myMessage.c_str());
		}
		mg::net::Host peerHost;
		mg::net::Socket peerSock = myTask.Accept(myAcceptEvent, peerHost);
		if (peerSock == mg::net::theInvalidSocket)
		{
			// Check if started an async accept. Couldn't accept right away.
			if (myAcceptEvent.IsLocked())
				return;
			MG_BOX_ASSERT_F(!myAcceptEvent.IsError(),
				"TCPServer accept error: %s",
				mg::box::ErrorCodeMessage(myAcceptEvent.GetError()));
			// Otherwise the accepted client could be closed remotely before
			// being accepted. In that case the returned event is just empty.
			MG_DEV_ASSERT(myAcceptEvent.IsEmpty());
			myTask.Reschedule();
			return;
		}
		// On success Accept() did consume the event already.
		MG_DEV_ASSERT(myAcceptEvent.IsEmpty());
		mySub->OnAccept(peerSock, peerHost);
	}

}
}

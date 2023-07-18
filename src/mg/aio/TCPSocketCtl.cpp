#include "TCPSocketCtl.h"

namespace mg {
namespace aio {

	TCPSocketCtlConnectParams::TCPSocketCtlConnectParams()
		: mySocket(mg::net::theInvalidSocket)
		, myHost(nullptr)
	{
	}

	TCPSocketCtl::TCPSocketCtl(
		IoCoreTask* aTask)
		: myHasShutdown(false)
		, myHasConnect(false)
		, myConnectIsInProgress(false)
		, myConnectSocket(mg::net::theInvalidSocket)
		, myHasAttach(false)
		, myTask(aTask)
	{
	}

	TCPSocketCtl::~TCPSocketCtl()
	{
		if (myHasConnect)
			PrivEndConnect();
		if (myHasAttach)
			PrivEndAttach();
	}

	void
	TCPSocketCtl::MergeFrom(
		TCPSocketCtl* aSrc)
	{
		MG_DEV_ASSERT(myTask == aSrc->myTask);
		if (aSrc->myHasShutdown)
		{
			myHasShutdown = true;
			aSrc->myHasShutdown = false;
		}
		if (aSrc->myHasAttach)
		{
			MG_DEV_ASSERT(!myHasAttach);
			myHasAttach = true;
			myAttachSocket = aSrc->myAttachSocket;
			aSrc->myAttachSocket = mg::net::theInvalidSocket;
			aSrc->myHasAttach = false;
		}
		if (aSrc->myHasConnect)
		{
			MG_DEV_ASSERT(!myHasConnect);
			MG_DEV_ASSERT(!aSrc->myConnectIsInProgress);
			myHasConnect = true;
			myConnectHost = aSrc->myConnectHost;
			myConnectSocket = aSrc->myConnectSocket;
			aSrc->myHasConnect = false;
			aSrc->myConnectHost.Clear();
			aSrc->myConnectSocket = mg::net::theInvalidSocket;
		}
		delete aSrc;
	}

	void 
	TCPSocketCtl::AddConnect(
		const TCPSocketCtlConnectParams& aParams)
	{
		PrivStartConnect();
		myConnectSocket = aParams.mySocket;
		myConnectHost = *aParams.myHost;
	}

	void
	TCPSocketCtl::AddAttach(
		mg::net::Socket aSocket)
	{
		PrivStartAttach();
		myAttachSocket = aSocket;
	}

	void
	TCPSocketCtl::AddShutdown()
	{
		myHasShutdown = true;
	}

	bool
	TCPSocketCtl::IsIdle() const
	{
		return !myHasShutdown && !myHasConnect && !myHasAttach;
	}

	bool
	TCPSocketCtl::HasShutdown() const
	{
		return myHasShutdown;
	}

	bool
	TCPSocketCtl::DoShutdown(
		mg::box::Error::Ptr& aOutErr)
	{
		MG_DEV_ASSERT(myHasShutdown);
		mg::net::Socket sock = myTask->GetSocket();
		// Shutdown on a non-connected socket simply does not work. Wait to kill it later.
		if (myHasConnect)
			return true;
		
		myHasShutdown = false;
		// Might be an empty task without a socket. Then nothing to shutdown.
		if (sock == mg::net::theInvalidSocket)
			return true;

		return SocketShutdown(sock, aOutErr);
	}

	bool
	TCPSocketCtl::HasConnect() const
	{
		return myHasConnect;
	}

	bool
	TCPSocketCtl::DoConnect(
		mg::box::Error::Ptr& aOutErr)
	{
		MG_DEV_ASSERT(myHasConnect);
		MG_DEV_ASSERT(!myHasAttach);
		myConnectIsInProgress = true;

		bool ok;
		IOEvent& event = myConnectEvent;
		const mg::network::Host& host = myConnectHost;
		if (myTask->HasSocket())
			ok = myTask->ConnectUpdate(event);
		else if (myConnectSocket != mg::net::theInvalidSocket)
			ok = myTask->ConnectStart(myConnectSocket, host, event);
		else
			ok = myTask->ConnectStart(host, event);
		// In case of success the socket belongs to IOCore. Can't close it directly
		// anymore.
		if (ok)
			myConnectSocket = mg::net::theInvalidSocket;

		if (myConnectEvent.IsLocked())
			return true;

		// Remember error before clearing the connect process. Just in case the event is
		// invalidated at clear.
		mg::box::ErrorCode errCode = mg::box::ERR_NONE;
		if (!ok)
			errCode = myConnectEvent.GetError();
		PrivEndConnect();
		if (!ok)
		{
			aOutErr = mg::box::ErrorRaise(errCode);
			return false;
		}
		if (myHasShutdown)
		{
			// Shutdown is already waiting. Reschedule the task to do another round of ctl
			// messages processing, and make the socket finally meet its destiny.
			myTask->Reschedule();
		}
		return true;
	}

	bool
	TCPSocketCtl::HasAttach() const
	{
		return myHasAttach;
	}

	void
	TCPSocketCtl::DoAttach()
	{
		MG_DEV_ASSERT(myHasAttach);
		MG_DEV_ASSERT(!myHasConnect);
		myHasAttach = false;
		myAttachSocket = mg::net::theInvalidSocket;
		myTask.AttachSocket(myAttachSocket);
	}

	void
	TCPSocketCtl::PrivStartConnect()
	{
		MG_DEV_ASSERT(!myHasConnect);
		MG_DEV_ASSERT(!myHasAttach);
		// Connect or attach are mutually exclusive and are always a first ctl. Shutdown
		// couldn't be requested before them.
		MG_DEV_ASSERT(!myHasShutdown);
		MG_DEV_ASSERT(myConnectSocket == mg::net::theInvalidSocket);
		MG_DEV_ASSERT(!myConnectIsInProgress);
		myHasConnect = true;
	}

	void
	TCPSocketCtl::PrivEndConnect()
	{
		MG_DEV_ASSERT(myHasConnect);
		if (myConnectSocket != mg::net::theInvalidSocket)
		{
			mg::net::SocketClose(myConnectSocket);
			myConnectSocket = mg::net::theInvalidSocket;
		}
		myConnectIsInProgress = false;
		myHasConnect = false;
	}

	void
	TCPSocketCtl::PrivStartAttach()
	{
		MG_DEV_ASSERT(!myHasConnect);
		MG_DEV_ASSERT(!myHasAttach);
		// Connect or attach are mutually exclusive and are always a first ctl. Shutdown
		// couldn't be requested before them.
		MG_DEV_ASSERT(!myHasShutdown);
		myHasAttach = true;
	}

	void
	TCPSocketCtl::PrivEndAttach()
	{
		MG_DEV_ASSERT(myHasAttach);
		mg::net::SocketClose(myAttachSocket);
		myAttachSocket = mg::net::theInvalidSocket;
		myHasAttach = false;
	}

}
}

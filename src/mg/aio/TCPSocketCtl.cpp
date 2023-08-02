#include "TCPSocketCtl.h"

#include "mg/box/Mutex.h"
#include "mg/net/DomainToIP.h"
#include "mg/net/URL.h"

namespace mg {
namespace aio {

	struct TCPSocketCtlDomainRequest
	{
		TCPSocketCtlDomainRequest(
			IOTask* aTask,
			const char* aDomain,
			uint16_t aPort,
			mg::net::SockAddrFamily aAddrFamily);

		// True - finished, false - not finished.
		bool Update();
		void Cancel();

		void PrivOnDomainResolve(
			const char* aDomain,
			const std::vector<mg::net::DomainEndpoint>& aEndpoints,
			mg::box::Error* aError);

		IOTask* myTask;
		mg::box::Error::Ptr myError;
		const uint16_t myPort;
		const std::string myDomain;
		const mg::net::SockAddrFamily myAddrFamily;
		mg::net::Host myHost;
		mg::net::DomainToIPRequest myRequest;
		bool myIsSent;
		mg::box::Mutex myMutex;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	TCPSocketHandshake::TCPSocketHandshake(
		const TCPSocketHandshakeCallback& aCallback)
		: myCallback(aCallback)
	{
	}

	bool
	TCPSocketHandshake::Update()
	{
		return myCallback(this);
	}

	//////////////////////////////////////////////////////////////////////////////////////

	TCPSocketCtlConnectParams::TCPSocketCtlConnectParams()
		: mySocket(mg::net::theInvalidSocket)
		, myAddrFamily(mg::net::ADDR_FAMILY_NONE)
	{
	}

	//////////////////////////////////////////////////////////////////////////////////////

	TCPSocketCtl::TCPSocketCtl(
		IOTask* aTask)
		: myHasShutdown(false)
		, myHasConnect(false)
		, myConnectIsStarted(false)
		, myConnectDomain(nullptr)
		, myConnectSocket(mg::net::theInvalidSocket)
		, myConnectStartDeadline(0)
		, myHasAttach(false)
		, myAttachSocket(mg::net::theInvalidSocket)
		, myHasHandshake(false)
		, myHandshake(nullptr)
		, myTask(aTask)
	{
	}

	TCPSocketCtl::~TCPSocketCtl()
	{
		if (myHasConnect)
			PrivEndConnect();
		if (myHasAttach)
			PrivEndAttach();
		PrivEndHandshake();
	}

	void
	TCPSocketCtl::MergeFrom(
		TCPSocketCtl* aSrc)
	{
		// Handshake is always added first. Can't be merged from a non-first ctl.
		MG_DEV_ASSERT(!aSrc->myHasHandshake);
		MG_DEV_ASSERT(aSrc->myHandshake == nullptr);
		MG_DEV_ASSERT(myTask == aSrc->myTask);
		if (aSrc->myHasShutdown)
		{
			// Shutdown can be requested multiple times.
			myHasShutdown = true;
			aSrc->myHasShutdown = false;
		}
		if (aSrc->myHasAttach)
		{
			// Can attach only one socket.
			MG_DEV_ASSERT(!myHasAttach);
			myHasAttach = true;
			myAttachSocket = aSrc->myAttachSocket;
			aSrc->myAttachSocket = mg::net::theInvalidSocket;
			aSrc->myHasAttach = false;
		}
		if (aSrc->myHasConnect)
		{
			// Can connect only once (until the socket is closed and re-opened).
			MG_DEV_ASSERT(!myHasConnect);
			MG_DEV_ASSERT(!aSrc->myConnectIsStarted);
			myHasConnect = true;
			myConnectHost = aSrc->myConnectHost;
			myConnectDomain = aSrc->myConnectDomain;
			myConnectSocket = aSrc->myConnectSocket;
			myConnectDelay = aSrc->myConnectDelay;
			aSrc->myHasConnect = false;
			aSrc->myConnectHost.Clear();
			aSrc->myConnectDomain = nullptr;
			aSrc->myConnectSocket = mg::net::theInvalidSocket;
			aSrc->myConnectDelay.Reset();
		}
		delete aSrc;
	}

	void 
	TCPSocketCtl::AddConnect(
		const TCPSocketCtlConnectParams& aParams)
	{
		// Connect or attach are mutually exclusive and are always the first ctl. Shutdown
		// couldn't be requested before them.
		MG_DEV_ASSERT(!myHasConnect);
		MG_DEV_ASSERT(!myHasAttach);
		MG_DEV_ASSERT(!myHasShutdown);
		MG_DEV_ASSERT(myConnectSocket == mg::net::theInvalidSocket);
		MG_DEV_ASSERT(myConnectDomain == nullptr);
		MG_DEV_ASSERT(!myConnectIsStarted);
		myHasConnect = true;
		myConnectSocket = aParams.mySocket;
		myConnectDelay = aParams.myDelay;
		mg::net::URL url = mg::net::URLParse(aParams.myEndpoint);
		if (myConnectHost.Set(url.myHost))
		{
			myConnectHost.SetPort(url.myPort);
		}
		else
		{
			myConnectDomain = new TCPSocketCtlDomainRequest(
				myTask, url.myHost.c_str(), url.myPort, aParams.myAddrFamily);
		}
	}

	void
	TCPSocketCtl::AddAttach(
		mg::net::Socket aSocket)
	{
		// Connect or attach are mutually exclusive and are always the first ctl. Shutdown
		// couldn't be requested before them.
		MG_DEV_ASSERT(!myHasConnect);
		MG_DEV_ASSERT(!myHasAttach);
		MG_DEV_ASSERT(!myHasShutdown);
		myHasAttach = true;
		myAttachSocket = aSocket;
	}

	void
	TCPSocketCtl::AddHandshake(
		TCPSocketHandshake* aHandshake)
	{
		// Needs to be added before all the other steps, especially before connect/attach.
		// Otherwise it might be too late.
		MG_DEV_ASSERT(!myHasAttach);
		MG_DEV_ASSERT(!myHasConnect);
		MG_DEV_ASSERT(!myHasHandshake);
		MG_DEV_ASSERT(!myHasShutdown);
		MG_DEV_ASSERT(myHandshake == nullptr);
		// Handshake presence flag is set later automatically at a specific stage and is
		/// mandatory. It just ends right away if no handshake handler was provided.
		myHandshake = aHandshake;
	}

	void
	TCPSocketCtl::AddShutdown()
	{
		myHasShutdown = true;
	}

	bool
	TCPSocketCtl::IsIdle() const
	{
		return !myHasShutdown && !myHasConnect && !myHasAttach &&
			!myHasHandshake && myHandshake == nullptr;
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
		// Shutdown on a non-connected socket simply does not work. Wait to kill it later.
		if (myHasConnect)
			return true;
		
		mg::net::Socket sock = myTask->GetSocket();
		myHasShutdown = false;
		// Might be an empty task without a socket. Then nothing to shutdown.
		if (sock == mg::net::theInvalidSocket)
			return true;

		return mg::net::SocketShutdown(sock, aOutErr);
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
		if (!myConnectIsStarted)
		{
			myConnectIsStarted = true;
			if (myConnectDelay.myType != mg::box::TIME_LIMIT_NONE)
				myConnectStartDeadline = myConnectDelay.ToPointFromNow().myValue;
			else
				myConnectStartDeadline = 0;
		}
		if (myConnectStartDeadline != 0)
		{
			if (mg::box::GetMilliseconds() < myConnectStartDeadline)
			{
				myTask->SetDeadline(myConnectStartDeadline);
				return true;
			}
			myConnectStartDeadline = 0;
		}
		if (myConnectDomain != nullptr)
		{
			MG_DEV_ASSERT(!myConnectHost.IsSet());
			if (!myConnectDomain->Update())
				return true;
			aOutErr = std::move(myConnectDomain->myError);
			myConnectHost = myConnectDomain->myHost;
			delete myConnectDomain;
			myConnectDomain = nullptr;
			if (aOutErr.IsSet())
			{
				PrivEndConnect();
				return false;
			}
		}
		bool ok;
		IOEvent& event = myConnectEvent;
		const mg::net::Host& host = myConnectHost;
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

		if (event.IsLocked())
			return true;

		// Remember error before clearing the connect process. Just in case the event is
		// invalidated at clear.
		if (!ok)
			aOutErr = mg::box::ErrorRaise(event.PopError(), "connect");
		else
			MG_BOX_ASSERT(event.PopBytes() == 0);
		PrivEndConnect();
		if (!ok)
			return false;
		PrivStartHandshake();
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
		myTask->AttachSocket(myAttachSocket);
		myAttachSocket = mg::net::theInvalidSocket;
		PrivStartHandshake();
	}

	bool
	TCPSocketCtl::HasHandshake() const
	{
		return myHasHandshake;
	}

	void
	TCPSocketCtl::DoHandshake()
	{
		MG_DEV_ASSERT(!myHasConnect);
		MG_DEV_ASSERT(!myHasAttach);
		MG_DEV_ASSERT(myHasHandshake);
		// Handshake might be optional. In that case it simply ends right away.
		if (myHandshake == nullptr || myHandshake->Update())
			PrivEndHandshake();
	}

	void
	TCPSocketCtl::PrivEndConnect()
	{
		MG_DEV_ASSERT(myHasConnect);
		if (myConnectDomain != nullptr)
		{
			myConnectDomain->Cancel();
			myConnectDomain = nullptr;
		}
		if (myConnectSocket != mg::net::theInvalidSocket)
		{
			mg::net::SocketClose(myConnectSocket);
			myConnectSocket = mg::net::theInvalidSocket;
		}
		myConnectIsStarted = false;
		myHasConnect = false;
	}

	void
	TCPSocketCtl::PrivEndAttach()
	{
		MG_DEV_ASSERT(myHasAttach);
		mg::net::SocketClose(myAttachSocket);
		myAttachSocket = mg::net::theInvalidSocket;
		myHasAttach = false;
	}

	void
	TCPSocketCtl::PrivStartHandshake()
	{
		MG_DEV_ASSERT(!myHasConnect);
		MG_DEV_ASSERT(!myHasAttach);
		MG_DEV_ASSERT(!myHasHandshake);
		myHasHandshake = true;
	}

	void
	TCPSocketCtl::PrivEndHandshake()
	{
		delete myHandshake;
		myHandshake = nullptr;
		myHasHandshake = false;
	}

	//////////////////////////////////////////////////////////////////////////////////////

	TCPSocketCtlDomainRequest::TCPSocketCtlDomainRequest(
		IOTask* aTask,
		const char* aDomain,
		uint16_t aPort,
		mg::net::SockAddrFamily aAddrFamily)
		: myTask(aTask)
		, myPort(aPort)
		, myDomain(aDomain)
		, myAddrFamily(aAddrFamily)
		, myIsSent(false)
	{
		MG_DEV_ASSERT(aDomain != nullptr);
	}

	bool
	TCPSocketCtlDomainRequest::Update()
	{
		if (!myIsSent)
		{
			myIsSent = true;
			mg::net::DomainToIPAsync(myRequest, myDomain.c_str(),
				mg::box::theTimePointInf, std::bind(
					&TCPSocketCtlDomainRequest::PrivOnDomainResolve, this,
					std::placeholders::_1, std::placeholders::_2,
					std::placeholders::_3));
			return false;
		}
		mg::box::MutexLock lock(myMutex);
		// Task is dropped when resolve is done.
		return myTask == nullptr;
	}

	void
	TCPSocketCtlDomainRequest::Cancel()
	{
		if (!myIsSent)
		{
			delete this;
			return;
		}
		myMutex.Lock();
		if (myTask == nullptr)
		{
			myMutex.Unlock();
			delete this;
			return;
		}
		// Can't delete. The request is running. Drop the task to let it know it must
		// delete itself when done.
		myTask = nullptr;
		mg::net::DomainToIPCancel(myRequest);
		myMutex.Unlock();
	}

	void
	TCPSocketCtlDomainRequest::PrivOnDomainResolve(
		const char* /* aDomain */,
		const std::vector<mg::net::DomainEndpoint>& aEndpoints,
		mg::box::Error* aError)
	{
		if (aError != nullptr)
		{
			myError.Set(aError);
		}
		else
		{
			if (myAddrFamily == mg::net::ADDR_FAMILY_NONE)
			{
				myHost = aEndpoints.front().myHost;
			}
			else
			{
				for (const mg::net::DomainEndpoint& de : aEndpoints)
				{
					if (de.myHost.myAddr.sa_family != myAddrFamily)
						continue;
					myHost = de.myHost;
					break;
				}
				if (!myHost.IsSet())
				{
					myError.Set(mg::box::ErrorRaise(mg::box::ERR_NET_ADDR_NOT_AVAIL,
						"address with the given addr family is not found"));
				}
			}
			if (myHost.IsSet())
				myHost.SetPort(myPort);
			else
				MG_BOX_ASSERT(myError.IsSet());
		}

		myMutex.Lock();
		if (myTask != nullptr)
		{
			myTask->PostWakeup();
			myTask = nullptr;
			myMutex.Unlock();
			return;
		}
		myMutex.Unlock();
		// Too late, already canceled.
		delete this;
	}

}
}

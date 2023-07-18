// ProjectFilter(Network)
#include "stdafx.h"
#include "TCPSocketCtl.h"

#include "mg/common/Error.h"

#include "mg/serverbox/Resolver.h"
#include "mg/serverbox/SocketUtils.h"

namespace mg {
namespace serverbox {

	struct TCPSocketCtlResolveRequest final
		: public mg::serverbox::Resolver::Callback
	{
		TCPSocketCtlResolveRequest(
			IoCoreTask* aTask,
			const char* aHost,
			uint16 aPort)
			: myTask(aTask)
			, myError(0)
			, myPort(aPort)
			, myIsStarted(false)
			, myHostStr(aHost)
		{
			MG_COMMON_ASSERT(aHost != nullptr);
		}

		// True - finished, false - not finished.
		bool
		Update()
		{
			mg::common::MutexLock lock(myLock);
			if (!myIsStarted)
			{
				myIsStarted = true;
				Resolver::GetInstance().Resolve(myHostStr, this);
				return false;
			}
			// Task is dropped when resolve is done.
			return myTask == nullptr;
		}

		void
		Cancel()
		{
			myLock.Lock();
			if (myTask == nullptr || !myIsStarted)
			{
				myLock.Unlock();
				delete this;
			}
			else
			{
				// Can't delete. The request is running in the
				// resolver thread. Drop the task to let it know
				// it must delete itself when done.
				myTask = nullptr;
				myLock.Unlock();
			}
		}

		void
		OnResolveComplete(
			const char*,
			struct sockaddr* aAddr,
			size_t) override
		{
			if (aAddr == nullptr)
			{
				myError = GetLastError();
				MG_COMMON_ASSERT(myError != 0);
			}
			else
			{
				myHost.Set(aAddr);
				myHost.SetPort(myPort);
			}

			myLock.Lock();
			if (myTask != nullptr)
			{
				IoCore::GetInstance().WakeupTask(myTask);
				myTask = nullptr;
				myLock.Unlock();
			}
			else
			{
				myLock.Unlock();
				// Too late, already canceled.
				delete this;
			}
		}

		IoCoreTask* myTask;
		uint32 myError;
		uint16 myPort;
		bool myIsStarted;
		mg::common::HybridString<32> myHostStr;
		mg::network::Host myHost;
		// The task is manipulated under a lock, because the
		// resolver tries to signal it, but it is not allowed in
		// case the task is already closed. Before close the
		// resolve request must be canceled.
		mg::common::Mutex myLock;
	};

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

	TCPSocketCtlConnectParams::TCPSocketCtlConnectParams()
		: mySocket(INVALID_SOCKET_VALUE)
		, myAddr(nullptr)
		, myHost(nullptr)
		, myPort(0)
		, myDelay(0)
	{
	}

	TCPSocketCtl::TCPSocketCtl(
		IoCoreTask* aTask)
		: myHasShutdown(false)
		, myHasConnect(false)
		, myConnectIsStarted(false)
		, myConnectResolve(nullptr)
		, myConnectSocket(INVALID_SOCKET_VALUE)
		, myHasAttach(false)
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
		// Handshake is always added first. Never is supposed to
		// be merged from a later ctl.
		MG_COMMON_ASSERT(!aSrc->myHasHandshake);
		MG_COMMON_ASSERT(aSrc->myHandshake == nullptr);
		MG_COMMON_ASSERT(myTask == aSrc->myTask);
		if (aSrc->myHasShutdown)
		{
			myHasShutdown = true;
			aSrc->myHasShutdown = false;
		}
		if (aSrc->myHasAttach)
		{
			MG_COMMON_ASSERT(!myHasAttach);
			myHasAttach = true;
			myAttachSocket = aSrc->myAttachSocket;
			aSrc->myAttachSocket = INVALID_SOCKET_VALUE;
			aSrc->myHasAttach = false;
		}
		if (aSrc->myHasConnect)
		{
			MG_COMMON_ASSERT(!myHasConnect);
			MG_COMMON_ASSERT(!aSrc->myConnectIsStarted);
			myHasConnect = true;
			myConnectHost = aSrc->myConnectHost;
			myConnectResolve = aSrc->myConnectResolve;
			myConnectSocket = aSrc->myConnectSocket;
			myConnectDelay = aSrc->myConnectDelay;
			aSrc->myHasConnect = false;
			aSrc->myConnectHost.Clear();
			aSrc->myConnectResolve = nullptr;
			aSrc->myConnectSocket = INVALID_SOCKET_VALUE;
			aSrc->myConnectDelay = 0;
		}
		delete aSrc;
	}

	void 
	TCPSocketCtl::AddConnect(
		const TCPSocketCtlConnectParams& aParams)
	{
		PrivStartConnect();
		myConnectSocket = aParams.mySocket;
		myConnectDelay = aParams.myDelay;
		if (aParams.myAddr != nullptr)
		{
			myConnectHost = *aParams.myAddr;
		}
		else
		{
			myConnectResolve = new TCPSocketCtlResolveRequest(
				myTask, aParams.myHost, aParams.myPort
			);
		}
	}

	void
	TCPSocketCtl::AddAttach(
		Socket aSocket)
	{
		PrivStartAttach();
		myAttachSocket = aSocket;
	}

	void
	TCPSocketCtl::AddHandshake(
		TCPSocketHandshake* aHandshake)
	{
		// Needs to be added before all the other steps,
		// especially before connect/attach. Otherwise it might be
		// too late.
		MG_COMMON_ASSERT(!myHasAttach);
		MG_COMMON_ASSERT(!myHasConnect);
		MG_COMMON_ASSERT(!myHasHandshake);
		MG_COMMON_ASSERT(!myHasShutdown);
		MG_COMMON_ASSERT(myHandshake == nullptr);
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
	TCPSocketCtl::DoShutdown()
	{
		MG_COMMON_ASSERT(myHasShutdown);
		uint32 err;
		Socket sock = myTask->GetSocket();
		// Shutdown on a non-connected socket simply does not
		// work. Wait to kill it later.
		if (myHasConnect)
			return true;
		
		myHasShutdown = false;
		// Might be an empty task without a socket. Then nothing
		// to shutdown.
		if (sock == INVALID_SOCKET_VALUE)
			return true;

		bool ok = SocketUtils::ShutdownBoth(sock, err);
		if (!ok)
			SetLastError(err);
		return ok;
	}

	bool
	TCPSocketCtl::HasConnect() const
	{
		return myHasConnect;
	}

	bool
	TCPSocketCtl::DoConnect()
	{
		MG_COMMON_ASSERT(myHasConnect);
		MG_COMMON_ASSERT(!myHasAttach);
		uint32 err;
		if (!myConnectIsStarted)
		{
			myConnectIsStarted = true;
			if (myConnectDelay != 0)
				myConnectDeadline = mg::common::GetMilliseconds() + myConnectDelay;
			else
				myConnectDeadline = 0;
		}
		if (myConnectDeadline != 0)
		{
			if (mg::common::GetMilliseconds() < myConnectDeadline)
			{
				myTask->SetDeadline(myConnectDeadline);
				return true;
			}
			myConnectDeadline = 0;
		}

		if (myConnectResolve != nullptr)
		{
			MG_COMMON_ASSERT(!myConnectHost.IsSet());
			if (!myConnectResolve->Update())
				return true;
			err = myConnectResolve->myError;
			myConnectHost = myConnectResolve->myHost;
			delete myConnectResolve;
			myConnectResolve = nullptr;
			if (err != 0)
			{
				PrivEndConnect();
				SetLastError(err);
				return false;
			}
		}

		bool ok;
		IoCore& core = IoCore::GetInstance();
		IoCoreEvent& event = myConnectEvent;
		const mg::network::Host& host = myConnectHost;
		if (myTask->HasSocket())
			ok = myTask->ConnectUpdate(host, event);
		else if (myConnectSocket != INVALID_SOCKET_VALUE)
			ok = myTask->ConnectStart(core, myConnectSocket, host, event);
		else
			ok = myTask->ConnectStart(core, host, event);
		// In case of success the socket belongs to IoCore. Can't
		// close it directly anymore.
		if (ok)
			myConnectSocket = INVALID_SOCKET_VALUE;

		if (myConnectEvent.IsLocked())
			return true;

		// Remember error before clearing the connect process.
		// Just in case the event is invalidated at clear.
		if (!ok)
			err = myConnectEvent.GetError();
		else
			err = 0;
		PrivEndConnect();
		if (!ok)
		{
			SetLastError(err);
			return false;
		}
		PrivStartHandshake();
		if (myHasShutdown)
		{
			// Shutdown is already waiting. Reschedule the task to
			// do another round of ctl messages processing, and
			// make the socket finally meet its destiny.
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
		MG_COMMON_ASSERT(myHasAttach);
		MG_COMMON_ASSERT(!myHasConnect);
		myHasAttach = false;
		IoCore::GetInstance().AttachSocket(myAttachSocket, myTask);
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
		MG_COMMON_ASSERT(!myHasConnect);
		MG_COMMON_ASSERT(!myHasAttach);
		MG_COMMON_ASSERT(myHasHandshake);
		// Handshake might be optional. In that case it simply
		// ends right away.
		if (myHandshake == nullptr || myHandshake->Update())
			PrivEndHandshake();
	}

	void
	TCPSocketCtl::PrivStartConnect()
	{
		MG_COMMON_ASSERT(!myHasConnect);
		MG_COMMON_ASSERT(!myHasAttach);
		// Connect or attach are mutually exclusive and are always
		// a first ctl. Shutdown couldn't be requested before
		// them.
		MG_COMMON_ASSERT(!myHasShutdown);
		MG_COMMON_ASSERT(myConnectSocket == INVALID_SOCKET_VALUE);
		MG_COMMON_ASSERT(myConnectResolve == nullptr);
		MG_COMMON_ASSERT(!myConnectIsStarted);
		myHasConnect = true;
	}

	void
	TCPSocketCtl::PrivEndConnect()
	{
		MG_COMMON_ASSERT(myHasConnect);
		if (myConnectResolve != nullptr)
		{
			myConnectResolve->Cancel();
			myConnectResolve = nullptr;
		}
		if (myConnectSocket != INVALID_SOCKET_VALUE)
		{
			SocketUtils::Close(myConnectSocket);
			myConnectSocket = INVALID_SOCKET_VALUE;
		}
		myConnectIsStarted = false;
		myHasConnect = false;
	}

	void
	TCPSocketCtl::PrivStartAttach()
	{
		MG_COMMON_ASSERT(!myHasConnect);
		MG_COMMON_ASSERT(!myHasAttach);
		// Connect or attach are mutually exclusive and are always
		// a first ctl. Shutdown couldn't be requested before
		// them.
		MG_COMMON_ASSERT(!myHasShutdown);
		myHasAttach = true;
	}

	void
	TCPSocketCtl::PrivEndAttach()
	{
		MG_COMMON_ASSERT(myHasAttach);
		SocketUtils::Close(myAttachSocket);
		myAttachSocket = INVALID_SOCKET_VALUE;
		myHasAttach = false;
	}

	void
	TCPSocketCtl::PrivStartHandshake()
	{
		MG_COMMON_ASSERT(!myHasConnect);
		MG_COMMON_ASSERT(!myHasAttach);
		MG_COMMON_ASSERT(!myHasHandshake);
		myHasHandshake = true;
	}

	void
	TCPSocketCtl::PrivEndHandshake()
	{
		delete myHandshake;
		myHandshake = nullptr;
		myHasHandshake = false;
	}

} // serverbox
} // mg

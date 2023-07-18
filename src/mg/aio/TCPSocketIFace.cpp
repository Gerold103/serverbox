// ProjectFilter(Network)
#include "stdafx.h"
#include "mg/serverbox/TCPSocketBase.h"

#include "mg/common/Error.h"

#include "mg/serverbox/SocketUtils.h"
#include "mg/serverbox/TCPSocketCtl.h"
#include "mg/serverbox/TCPSocketListener.h"

namespace mg {
namespace serverbox {

	TCPSocketConnectParams::TCPSocketConnectParams()
		: mySocket(INVALID_SOCKET_VALUE)
		, myAddr(nullptr)
		, myHost(nullptr)
		, myPort(0)
		, myDelay(0)
	{
	}

	TCPSocketBase::TCPSocketBase()
		: myState(NEW)
		, myWasHandshakeDone(false)
		, myIsRunning(false)
		, myIsReadyToStart(false)
		, myListener(nullptr)
		, myFrontCtl(nullptr)
		, myCtl(nullptr)
	{
		PrivRef();
		PrivDisableIO();
	}

	void
	TCPSocketBase::Delete()
	{
		PostClose();
		PrivUnref();
	}

	void
	TCPSocketBase::PostConnect(
		const mg::network::Host& aHost,
		TCPSocketListener* aListener)
	{
		TCPSocketConnectParams params;
		params.myAddr = &aHost;
		PostConnect(params, aListener);
	}

	void
	TCPSocketBase::PostConnect(
		Socket aSocket,
		const mg::network::Host& aHost,
		TCPSocketListener* aListener)
	{
		MG_COMMON_ASSERT(aSocket != INVALID_SOCKET_VALUE);
		TCPSocketConnectParams params;
		params.mySocket = aSocket;
		params.myAddr = &aHost;
		PostConnect(params, aListener);
	}

	void
	TCPSocketBase::PostConnect(
		const char* aHost,
		uint16 aPort,
		TCPSocketListener* aListener)
	{
		TCPSocketConnectParams params;
		params.myHost = aHost;
		params.myPort = aPort;
		PostConnect(params, aListener);
	}

	void
	TCPSocketBase::PostConnect(
		Socket aSocket,
		const char* aHost,
		uint16 aPort,
		TCPSocketListener* aListener)
	{
		MG_COMMON_ASSERT(aSocket != INVALID_SOCKET_VALUE);
		TCPSocketConnectParams params;
		params.mySocket = aSocket;
		params.myHost = aHost;
		params.myPort = aPort;
		PostConnect(params, aListener);
	}

	void
	TCPSocketBase::PostConnect(
		const TCPSocketConnectParams& aParams,
		TCPSocketListener* aListener)
	{
		IoCore& core = IoCore::GetInstance();

		mg::common::MutexLock lock(myLock);
		MG_COMMON_ASSERT(myState == CLOSED || myState == NEW);
		MG_COMMON_ASSERT(myIsReadyToStart);
		MG_COMMON_ASSERT(!myIsRunning);
		myIsReadyToStart = false;
		myIsRunning = true;
		core.PostTask(&myTask, this);

		myListener = aListener;
		myState = CONNECTING;

		TCPSocketCtlConnectParams params;
		params.mySocket = aParams.mySocket;
		params.myAddr = aParams.myAddr;
		params.myHost = aParams.myHost;
		params.myPort = aParams.myPort;
		params.myDelay = aParams.myDelay;
		if (myFrontCtl == nullptr)
			myFrontCtl = new TCPSocketCtl(&myTask);
		myFrontCtl->AddConnect(params);
		core.WakeupTask(&myTask);
	}

	void
	TCPSocketBase::PostWrap(
		Socket aSocket,
		TCPSocketListener* aListener)
	{
		IoCore& core = IoCore::GetInstance();

		mg::common::MutexLock lock(myLock);
		MG_COMMON_ASSERT(myState == CLOSED || myState == NEW);
		MG_COMMON_ASSERT(myIsReadyToStart);
		MG_COMMON_ASSERT(!myIsRunning);
		myIsReadyToStart = false;
		myIsRunning = true;
		core.PostTask(&myTask, this);

		myListener = aListener;
		myState = CONNECTING;
		if (myFrontCtl == nullptr)
			myFrontCtl = new TCPSocketCtl(&myTask);
		myFrontCtl->AddAttach(aSocket);
		core.WakeupTask(&myTask);
	}

	void
	TCPSocketBase::PostTask(
		TCPSocketListener* aListener)
	{
		IoCore& core = IoCore::GetInstance();

		mg::common::MutexLock lock(myLock);
		MG_COMMON_ASSERT(myState == CLOSED || myState == NEW);
		MG_COMMON_ASSERT(myIsReadyToStart);
		MG_COMMON_ASSERT(!myIsRunning);
		myIsReadyToStart = false;
		myIsRunning = true;
		core.PostTask(&myTask, this);

		myListener = aListener;
		myState = EMPTY;
	}

	bool
	TCPSocketBase::PostSend(
		mg::network::WriteBuffer* aHead)
	{
		mg::network::WriteBuffer* tail = aHead;
		mg::network::WriteBuffer* next;
		while ((next = tail->myNextBuffer.GetPointer()) != nullptr)
			tail = next;
		return PostSend(aHead, tail);
	}

	bool
	TCPSocketBase::PostSend(
		mg::network::WriteBuffer* aHead,
		mg::network::WriteBuffer* aTail)
	{
		if (myTask.IsInWorkerNow())
			return Send(aHead, aTail);

		myLock.Lock();
		bool wasEmpty = myFrontSendQueue.IsEmpty();
		myFrontSendQueue.AppendList(aHead, aTail);
		myLock.Unlock();

		if (wasEmpty)
			IoCore::GetInstance().WakeupTask(&myTask);
		return true;
	}

	void
	TCPSocketBase::PostWakeup()
	{
		IoCore::GetInstance().WakeupTask(&myTask);
	}

	void
	TCPSocketBase::PostClose()
	{
		mg::common::MutexLock lock(myLock);
		if (myState >= CLOSING)
			return;
		if (myState == NEW)
		{
			myState = CLOSED;
			return;
		}
		myState = CLOSING;
		IoCore::GetInstance().CloseTask(&myTask, false);
	}

	void
	TCPSocketBase::PostShutdown()
	{
		mg::common::MutexLock lock(myLock);
		if (myState >= CLOSING)
			return;
		MG_COMMON_ASSERT(myState != NEW);
		if (myFrontCtl == nullptr)
			myFrontCtl = new TCPSocketCtl(&myTask);
		myFrontCtl->AddShutdown();
		IoCore::GetInstance().WakeupTask(&myTask);
	}

	bool
	TCPSocketBase::IsClosed()
	{
		mg::common::MutexLock lock(myLock);
		return myState == CLOSED;
	}

	bool
	TCPSocketBase::IsConnected()
	{
		mg::common::MutexLock lock(myLock);
		return myState == CONNECTED;
	}

	bool
	TCPSocketBase::IsInWorkerNow() const
	{
		return myTask.IsInWorkerNow();
	}

	bool
	TCPSocketBase::Send(
		mg::network::WriteBuffer* aHead)
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		if (myTask.IsClosed())
			return false;
		mySendQueue.AppendList(aHead);
		return true;
	}

	bool
	TCPSocketBase::Send(
		mg::network::WriteBuffer* aHead,
		mg::network::WriteBuffer* aTail)
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		if (myTask.IsClosed())
			return false;
		mySendQueue.AppendList(aHead, aTail);
		return true;
	}

	void
	TCPSocketBase::Connect(
		const TCPSocketConnectParams& aParams)
	{
		TCPSocketCtlConnectParams params;
		params.mySocket = aParams.mySocket;
		params.myAddr = aParams.myAddr;
		params.myHost = aParams.myHost;
		params.myPort = aParams.myPort;
		params.myDelay = aParams.myDelay;

		mg::common::MutexLock lock(myLock);
		// CLOSING can happen if an external thread made
		// PostClose() which wasn't yet delivered. But CLOSED is
		// not acceptable - it means OnClose() was done and the
		// owner then must not issue new Connect() on the closed
		// socket.
		// CLOSING is just ignored because anyway soon the owner
		// will get OnClose() and therefore will notice the
		// connection failure. Returning an error here wouldn't
		// make sense as it can be bypassed by making PostClose()
		// just after this Connect() anyway.
		if (myState == CLOSING)
			return;
		MG_COMMON_ASSERT(myState == EMPTY);
		MG_COMMON_ASSERT(myIsRunning);
		myState = CONNECTING;
		if (myFrontCtl == nullptr)
			myFrontCtl = new TCPSocketCtl(&myTask);
		myFrontCtl->AddConnect(params);

		myTask.Reschedule();
	}

	bool
	TCPSocketBase::SetKeepAlive(
		bool aEnable,
		uint32 aTimeout)
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		uint32 err;
		Socket sock = myTask.GetSocket();
		MG_DEV_ASSERT(sock != INVALID_SOCKET_VALUE);
		if (mg::serverbox::SocketUtils::SetKeepAlive(sock, aEnable, aTimeout, err))
			return true;
		SetLastError(err);
		return false;
	}

	bool
	TCPSocketBase::SetNoDelay(
		bool aEnable)
	{
		uint32 err;
		Socket sock = myTask.GetSocket();
		MG_DEV_ASSERT(sock != INVALID_SOCKET_VALUE);
		if (mg::serverbox::SocketUtils::SetNoDelay(sock, aEnable, err))
			return true;
		SetLastError(err);
		return false;
	}

	void
	TCPSocketBase::ProtOpen(
		uint32 aRecvSize)
	{
		MG_COMMON_ASSERT(aRecvSize > 0);
		MG_COMMON_ASSERT(myRecvQueue.IsEmpty());
		MG_COMMON_ASSERT(!myWasHandshakeDone);

		myLock.Lock();
		MG_COMMON_ASSERT(myState == NEW || myState == CLOSED);
		MG_COMMON_ASSERT(!myIsRunning);
		MG_COMMON_ASSERT(!myIsReadyToStart);
		myIsReadyToStart = true;
		myLock.Unlock();

		myRecvQueue.AppendBytes(aRecvSize);
	}

	void
	TCPSocketBase::ProtPostHandshake(
		TCPSocketHandshake* aHandshake)
	{
		// It should be done from the child class Open() method.
		// Adding the handshake step after connect is already
		// posted would be too late.
		MG_COMMON_ASSERT(myState == NEW || myState == CLOSED);
		MG_COMMON_ASSERT(myFrontCtl == nullptr);
		MG_COMMON_ASSERT(myIsReadyToStart);
		MG_COMMON_ASSERT(!myIsRunning);
		MG_COMMON_ASSERT(!myWasHandshakeDone);
		myFrontCtl = new TCPSocketCtl(&myTask);
		myFrontCtl->AddHandshake(aHandshake);
	}

	bool
	TCPSocketBase::ProtWasHandshakeDone() const
	{
		MG_COMMON_ASSERT(myTask.IsInWorkerNow());
		return myWasHandshakeDone;
	}

	void
	TCPSocketBase::ProtOnWakeup()
	{
		myListener->OnWakeup();
		PrivCtl();
	}

	void
	TCPSocketBase::ProtCloseError(
		mg::common::Error* aError)
	{
		MG_COMMON_ASSERT(myTask.IsInWorkerNow());
		if (!PrivIsCompromised())
			myListener->OnError(aError);
		ProtClose();
	}

	void
	TCPSocketBase::ProtClose()
	{
		MG_COMMON_ASSERT(myTask.IsInWorkerNow());
		MG_COMMON_ASSERT(myIsRunning);
		myLock.Lock();
		MG_COMMON_ASSERT(myState != NEW);
		if (myTask.IsClosed())
		{
			if (myState == CLOSED)
				return;

			// Diligently clear all the members to make them like
			// new. It is needed in case the socket will be reused
			// after the closure ends.
			myFrontSendQueue.Clear();
			myWasHandshakeDone = false;
			myIsRunning = false;
			myState = CLOSED;
			delete myFrontCtl;
			myFrontCtl = nullptr;
			myLock.Unlock();

			mySendQueue.Clear();
			myRecvQueue.Clear();
			PrivDisableIO();
			delete myCtl;
			myCtl = nullptr;
			// Save the listener on the stack in case the socket
			// is deleted in OnClose().
			TCPSocketListener* listener = myListener;
			myListener = nullptr;

			listener->OnClose();
			return;
		}
		if (myState < CLOSING)
			myState = CLOSING;
		else
			MG_COMMON_ASSERT(myState == CLOSING);
		myLock.Unlock();

		// Close the task even if close is already started. IoCore
		// allows that. Otherwise it could happen that close would
		// be posted manually, then an error would happen and skip
		// the close because status is already "closing". But then
		// ctl might be never called again. So nobody would make
		// the actual close-task call. Better do it >= 1 times
		// than 0.
		IoCore::GetInstance().CloseTask(&myTask, false);
	}

	void
	TCPSocketBase::ProtOnSend(
		uint32 aByteCount)
	{
		MG_COMMON_ASSERT(myTask.IsInWorkerNow());
		myListener->OnSend(aByteCount);
	}

	void
	TCPSocketBase::ProtOnSendError(
		mg::common::Error* aError)
	{
		MG_COMMON_ASSERT(myTask.IsInWorkerNow());
		if (!PrivIsCompromised())
		{
			myListener->OnSendError(aError);
			myListener->OnError(aError);
		}
		ProtClose();
		mySendEvent.Lock();
	}

	void
	TCPSocketBase::ProtOnRecv(
		mg::network::WriteBuffer* aHead,
		mg::network::WriteBuffer* aTail,
		uint32 aByteCount)
	{
		MG_COMMON_ASSERT(myTask.IsInWorkerNow());
		myListener->OnRecv(aHead, aTail, aByteCount);
	}

	void
	TCPSocketBase::ProtOnRecvError(
		mg::common::Error* aError)
	{
		MG_COMMON_ASSERT(myTask.IsInWorkerNow());
		if (!PrivIsCompromised())
		{
			myListener->OnRecvError(aError);
			myListener->OnError(aError);
		}
		ProtClose();
		myRecvEvent.Lock();
	}

	TCPSocketBase::~TCPSocketBase()
	{
		mg::common::MutexLock lock(myLock);
		// If it was connected, it was referenced by IoCore.
		// Therefore couldn't be deleted before closed.
		MG_COMMON_ASSERT(myState == CLOSED);
		delete myCtl;
		myCtl = nullptr;
		delete myFrontCtl;
		myFrontCtl = nullptr;
	}

	void
	TCPSocketBase::PrivConnectAbort(
		mg::common::Error* aError)
	{
		MG_COMMON_ASSERT(myTask.IsInWorkerNow());
		if (!PrivIsCompromised())
		{
			myListener->OnConnectError(aError);
			myListener->OnError(aError);
		}
		ProtClose();
	}

	void
	TCPSocketBase::PrivHandshakeStart()
	{
		MG_COMMON_ASSERT(myTask.IsInWorkerNow());
		MG_COMMON_ASSERT(myCtl != nullptr && myCtl->HasHandshake());
		myLock.Lock();
		if (myState == CONNECTING)
			myState = HANDSHAKE;
		else
			MG_COMMON_ASSERT(myState == CLOSING);
		myLock.Unlock();
	}

	void
	TCPSocketBase::PrivHandshakeCommit()
	{
		MG_COMMON_ASSERT(myTask.IsInWorkerNow());
		MG_COMMON_ASSERT(!myWasHandshakeDone);
		myLock.Lock();
		if (myState == HANDSHAKE)
			myState = CONNECTED;
		else
			MG_COMMON_ASSERT(myState == CLOSING);
		myLock.Unlock();

		myListener->OnConnect();
		myWasHandshakeDone = true;
	}

	bool
	TCPSocketBase::PrivIsCompromised()
	{
		MG_COMMON_ASSERT(myTask.IsInWorkerNow());
		mg::common::MutexLock lock(myLock);
		return myState >= CLOSING;
	}

	void
	TCPSocketBase::PrivCtl()
	{
		MG_COMMON_ASSERT(myTask.IsInWorkerNow());
		myLock.Lock();
		if (myFrontCtl == nullptr && myCtl == nullptr)
			goto end;

		if (myFrontCtl != nullptr)
		{
			if (myCtl != nullptr)
				myCtl->MergeFrom(myFrontCtl);
			else
				myCtl = myFrontCtl;
			myFrontCtl = nullptr;
		}

		// Unlock because the external code can only change the
		// front ctl. No need to keep the lock while internal ctl
		// works. And it can be slow as its commands do system
		// calls.
		myLock.Unlock();

		if (myCtl->HasShutdown() && !myCtl->DoShutdown())
			ProtCloseError(mg::common::ErrorRaiseSys("shutdown"));

		if (myCtl->HasAttach())
		{
			myCtl->DoAttach();
			PrivHandshakeStart();
			PrivEnableIO();
		}

		if (myCtl->HasConnect())
		{
			if (!myCtl->DoConnect())
			{
				// Failed connect terminates the operation. Should
				// not still be present.
				MG_COMMON_ASSERT(!myCtl->HasConnect());
				PrivConnectAbort(mg::common::ErrorRaiseSys("connect"));
			}
			else if (!myCtl->HasConnect())
			{
				// Ctl connect was updated without errors and is
				// not present anymore? - then it is successfully
				// finished.
				PrivHandshakeStart();
				PrivEnableIO();
				myTask.Reschedule();
			}
			// Otherwise connect is updated and not finished -
			// still in progress, nothing to do. Wait for more
			// events.
		}

		if (myCtl->HasHandshake())
		{
			myCtl->DoHandshake();
			if (!myCtl->HasHandshake())
			{
				PrivHandshakeCommit();
				myTask.Reschedule();
			}
		}

		if (myCtl->IsIdle())
		{
			delete myCtl;
			myCtl = nullptr;
		}

		myLock.Lock();
		// No need to check front ctl again. If new messages
		// appeared, they will signal the task, and it will be
		// rescheduled.
	end:
		mySendQueue.AppendList(mg::common::Move(myFrontSendQueue));
		myLock.Unlock();
	}

	void
	TCPSocketBase::PrivEnableIO()
	{
		MG_COMMON_ASSERT(myTask.IsInWorkerNow());
		mySendEvent.Unlock();
		myRecvEvent.Unlock();
	}

	void
	TCPSocketBase::PrivDisableIO()
	{
		MG_COMMON_ASSERT(!myIsRunning);
		mySendEvent.Reset();
		mySendEvent.Lock();
		myRecvEvent.Reset();
		myRecvEvent.Lock();
	}

} // serverbox
} // mg

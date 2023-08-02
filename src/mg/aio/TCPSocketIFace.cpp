#include "TCPSocketIFace.h"

#include "mg/aio/TCPSocketCtl.h"
#include "mg/aio/TCPSocketSubscription.h"

namespace mg {
namespace aio {

	TCPSocketConnectParams::TCPSocketConnectParams()
		: mySocket(mg::net::theInvalidSocket)
		, myAddrFamily(mg::net::ADDR_FAMILY_NONE)
	{
	}

	TCPSocketIFace::TCPSocketIFace(
		IOCore& aCore)
		: myRecvSize(0)
		, myTask(aCore)
		, myState(TCP_SOCKET_STATE_NEW)
		, myWasHandshakeDone(false)
		, myIsRunning(false)
		, myIsReadyToStart(false)
		, mySub(nullptr)
		, myFrontRecvSize(0)
		, myFrontCtl(nullptr)
		, myCtl(nullptr)
	{
		PrivDisableIO();
	}

	void
	TCPSocketIFace::Delete()
	{
		PostClose();
		PrivUnref();
	}

	void
	TCPSocketIFace::PostConnect(
		const TCPSocketConnectParams& aParams,
		TCPSocketSubscription* aSub)
	{
		TCPSocketCtlConnectParams params;
		params.mySocket = aParams.mySocket;
		params.myEndpoint = aParams.myEndpoint;
		params.myAddrFamily = aParams.myAddrFamily;
		params.myDelay = aParams.myDelay;

		mg::box::MutexLock lock(myMutex);
		MG_DEV_ASSERT(myState == TCP_SOCKET_STATE_CLOSED ||
			myState == TCP_SOCKET_STATE_NEW);
		MG_DEV_ASSERT(myIsReadyToStart);
		MG_DEV_ASSERT(!myIsRunning);
		myIsReadyToStart = false;
		myIsRunning = true;
		mySub = aSub;
		myState = TCP_SOCKET_STATE_CONNECTING;
		if (myFrontCtl == nullptr)
			myFrontCtl = new TCPSocketCtl(&myTask);
		myFrontCtl->AddConnect(params);
		myTask.Post(this);
		myTask.PostWakeup();
	}

	void
	TCPSocketIFace::PostWrap(
		mg::net::Socket aSocket,
		TCPSocketSubscription* aSub)
	{
		mg::box::MutexLock lock(myMutex);
		MG_DEV_ASSERT(myState == TCP_SOCKET_STATE_CLOSED ||
			myState == TCP_SOCKET_STATE_NEW);
		MG_DEV_ASSERT(myIsReadyToStart);
		MG_DEV_ASSERT(!myIsRunning);
		myIsReadyToStart = false;
		myIsRunning = true;
		mySub = aSub;
		myState = TCP_SOCKET_STATE_CONNECTING;
		if (myFrontCtl == nullptr)
			myFrontCtl = new TCPSocketCtl(&myTask);
		myFrontCtl->AddAttach(aSocket);
		myTask.Post(this);
		myTask.PostWakeup();
	}

	void
	TCPSocketIFace::PostTask(
		TCPSocketSubscription* aSub)
	{
		mg::box::MutexLock lock(myMutex);
		MG_DEV_ASSERT(myState == TCP_SOCKET_STATE_CLOSED ||
			myState == TCP_SOCKET_STATE_NEW);
		MG_DEV_ASSERT(myIsReadyToStart);
		MG_DEV_ASSERT(!myIsRunning);
		myIsReadyToStart = false;
		myIsRunning = true;
		mySub = aSub;
		myState = TCP_SOCKET_STATE_EMPTY;
		myTask.Post(this);
	}

	void
	TCPSocketIFace::PostSendMove(
		mg::net::BufferLink* aHead)
	{
		if (myTask.IsInWorkerNow())
			return SendMove(aHead);

		myMutex.Lock();
		bool wasEmpty = myFrontSendQueue.IsEmpty();
		myFrontSendQueue.AppendMove(aHead);
		myMutex.Unlock();

		if (wasEmpty)
			myTask.PostWakeup();
	}

	void
	TCPSocketIFace::PostSendRef(
		mg::net::Buffer* aHead)
	{
		PostSendMove(new mg::net::BufferLink(aHead));
	}

	void
	TCPSocketIFace::PostSendRef(
		mg::net::Buffer::Ptr&& aHead)
	{
		PostSendMove(new mg::net::BufferLink(std::move(aHead)));
	}

	void
	TCPSocketIFace::PostSendRef(
		const void* aData,
		uint64_t aSize)
	{
		PostSendRef(mg::net::BuffersRef(aData, aSize));
	}

	void
	TCPSocketIFace::PostSendCopy(
		const mg::net::BufferLink* aHead)
	{
		PostSendRef(mg::net::BuffersCopy(aHead));
	}

	void
	TCPSocketIFace::PostSendCopy(
		const mg::net::Buffer* aHead)
	{
		PostSendRef(mg::net::BuffersCopy(aHead));
	}

	void
	TCPSocketIFace::PostSendCopy(
		const void* aData,
		uint64_t aSize)
	{
		PostSendRef(mg::net::BuffersCopy(aData, aSize));
	}

	void
	TCPSocketIFace::PostRecv(
		uint64_t aSize)
	{
		if (myTask.IsInWorkerNow())
			return Recv(aSize);
		if (myFrontRecvSize.FetchAddRelaxed(aSize) == 0)
			PostWakeup();
	}

	void
	TCPSocketIFace::PostWakeup()
	{
		myTask.PostWakeup();
	}

	void
	TCPSocketIFace::PostClose()
	{
		mg::box::MutexLock lock(myMutex);
		if (myState >= TCP_SOCKET_STATE_CLOSING)
			return;
		if (myState == TCP_SOCKET_STATE_NEW)
		{
			myState = TCP_SOCKET_STATE_CLOSED;
			return;
		}
		myState = TCP_SOCKET_STATE_CLOSING;
		myTask.PostClose();
	}

	void
	TCPSocketIFace::PostShutdown()
	{
		mg::box::MutexLock lock(myMutex);
		if (myState >= TCP_SOCKET_STATE_CLOSING)
			return;
		MG_DEV_ASSERT(myState != TCP_SOCKET_STATE_NEW);
		if (myFrontCtl == nullptr)
			myFrontCtl = new TCPSocketCtl(&myTask);
		myFrontCtl->AddShutdown();
		myTask.PostWakeup();
	}

	bool
	TCPSocketIFace::IsClosed() const
	{
		mg::box::MutexLock lock(myMutex);
		return myState == TCP_SOCKET_STATE_CLOSED;
	}

	bool
	TCPSocketIFace::IsConnected() const
	{
		mg::box::MutexLock lock(myMutex);
		return myState == TCP_SOCKET_STATE_CONNECTED;
	}

	bool
	TCPSocketIFace::IsInWorkerNow() const
	{
		return myTask.IsInWorkerNow();
	}

	void
	TCPSocketIFace::SendMove(
		mg::net::BufferLink* aHead)
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		if (myTask.IsClosed())
		{
			delete aHead;
			return;
		}
		mySendQueue.AppendMove(aHead);
	}

	void
	TCPSocketIFace::SendRef(
		mg::net::Buffer* aHead)
	{
		SendMove(new mg::net::BufferLink(aHead));
	}

	void
	TCPSocketIFace::SendRef(
		mg::net::Buffer::Ptr&& aHead)
	{
		SendMove(new mg::net::BufferLink(std::move(aHead)));
	}

	void
	TCPSocketIFace::SendRef(
		const void* aData,
		uint64_t aSize)
	{
		SendRef(mg::net::BuffersRef(aData, aSize));
	}

	void
	TCPSocketIFace::SendCopy(
		const mg::net::BufferLink* aHead)
	{
		SendRef(mg::net::BuffersCopy(aHead));
	}

	void
	TCPSocketIFace::SendCopy(
		const mg::net::Buffer* aHead)
	{
		SendRef(mg::net::BuffersCopy(aHead));
	}

	void
	TCPSocketIFace::SendCopy(
		const void* aData,
		uint64_t aSize)
	{
		SendRef(mg::net::BuffersCopy(aData, aSize));
	}

	void
	TCPSocketIFace::Recv(
		uint64_t aSize)
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		myRecvSize += aSize;
	}

	void
	TCPSocketIFace::Connect(
		const TCPSocketConnectParams& aParams)
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		TCPSocketCtlConnectParams params;
		params.mySocket = aParams.mySocket;
		params.myEndpoint = aParams.myEndpoint;
		params.myAddrFamily = aParams.myAddrFamily;
		params.myDelay = aParams.myDelay;

		myMutex.Lock();
		// CLOSING can happen if an external thread made PostClose() which wasn't yet
		// delivered. But CLOSED is not acceptable - it means OnClose() was done and the
		// owner then must not issue new Connect() on the closed socket without opening it
		// again.
		//
		// CLOSING is just ignored because anyway soon the owner will get OnClose() and
		// therefore will notice the connection failure. Returning an error here wouldn't
		// make sense as it can be bypassed by making PostClose() in another thread just
		// after this Connect() anyway.
		if (myState == TCP_SOCKET_STATE_CLOSING)
		{
			myMutex.Unlock();
			return;
		}
		MG_DEV_ASSERT(myState == TCP_SOCKET_STATE_EMPTY);
		MG_DEV_ASSERT(myIsRunning);
		myState = TCP_SOCKET_STATE_CONNECTING;
		if (myFrontCtl == nullptr)
			myFrontCtl = new TCPSocketCtl(&myTask);
		myFrontCtl->AddConnect(params);
		myMutex.Unlock();

		myTask.Reschedule();
	}

	bool
	TCPSocketIFace::SetKeepAlive(
		bool aValue,
		uint32_t aTimeout,
		mg::box::Error::Ptr& aOutErr)
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		mg::net::Socket sock = myTask.GetSocket();
		MG_DEV_ASSERT(sock != mg::net::theInvalidSocket);
		return mg::net::SocketSetKeepAlive(sock, aValue, aTimeout, aOutErr);
	}

	bool
	TCPSocketIFace::SetNoDelay(
		bool aValue,
		mg::box::Error::Ptr& aOutErr)
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		mg::net::Socket sock = myTask.GetSocket();
		MG_DEV_ASSERT(sock != mg::net::theInvalidSocket);
		return mg::net::SocketSetNoDelay(sock, aValue, aOutErr);
	}

	void
	TCPSocketIFace::ProtOpen()
	{
		mg::box::MutexLock lock(myMutex);
		MG_DEV_ASSERT(myRecvQueue.IsEmpty());
		MG_DEV_ASSERT(myRecvSize == 0);
		MG_DEV_ASSERT(!myWasHandshakeDone);
		MG_DEV_ASSERT(myState == TCP_SOCKET_STATE_NEW ||
			myState == TCP_SOCKET_STATE_CLOSED);
		MG_DEV_ASSERT(!myIsRunning);
		MG_DEV_ASSERT(!myIsReadyToStart);
		myIsReadyToStart = true;
	}

	void
	TCPSocketIFace::ProtPostHandshake(
		TCPSocketHandshake* aHandshake)
	{
		mg::box::MutexLock lock(myMutex);
		// It should be done from the child class Open() method. Adding the handshake step
		// after connect is already posted would be too late.
		MG_DEV_ASSERT(myState == TCP_SOCKET_STATE_NEW ||
			myState == TCP_SOCKET_STATE_CLOSED);
		MG_DEV_ASSERT(myFrontCtl == nullptr);
		MG_DEV_ASSERT(myIsReadyToStart);
		MG_DEV_ASSERT(!myIsRunning);
		MG_DEV_ASSERT(!myWasHandshakeDone);
		myFrontCtl = new TCPSocketCtl(&myTask);
		myFrontCtl->AddHandshake(aHandshake);
	}

	bool
	TCPSocketIFace::ProtWasHandshakeDone() const
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		return myWasHandshakeDone;
	}

	void
	TCPSocketIFace::ProtOnEvent()
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		mySub->OnEvent();
		PrivCtl();
	}

	void
	TCPSocketIFace::ProtClose()
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		MG_DEV_ASSERT(myIsRunning);

		myMutex.Lock();
		MG_DEV_ASSERT(myState != TCP_SOCKET_STATE_NEW);
		if (myTask.IsClosed())
		{
			if (myState == TCP_SOCKET_STATE_CLOSED)
			{
				myMutex.Unlock();
				return;
			}
			// Diligently clear all the members to make them like new. It is needed in
			// case the socket will be reopened after the closure ends.
			myFrontSendQueue.Clear();
			myFrontRecvSize.StoreRelaxed(0);
			myWasHandshakeDone = false;
			myIsRunning = false;
			myState = TCP_SOCKET_STATE_CLOSED;
			delete myFrontCtl;
			myFrontCtl = nullptr;
			// Save the sub on the stack in case the socket is deleted in OnClose().
			TCPSocketSubscription* sub = mySub;
			mySub = nullptr;
			myMutex.Unlock();

			// Members private to the worker thread. Can clear them without a mutex lock.
			mySendQueue.Clear();
			myRecvQueue.Clear();
			myRecvSize = 0;
			PrivDisableIO();
			delete myCtl;
			myCtl = nullptr;

			sub->OnClose();
			return;
		}
		if (myState < TCP_SOCKET_STATE_CLOSING)
			myState = TCP_SOCKET_STATE_CLOSING;
		else
			MG_DEV_ASSERT(myState == TCP_SOCKET_STATE_CLOSING);
		myMutex.Unlock();
		// Close the task even if close is already started. IOCore allows that. Easier
		// than caring about branching here.
		myTask.PostClose();
	}

	void
	TCPSocketIFace::ProtCloseError(
		mg::box::Error* aError)
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		if (!PrivIsCompromised())
			mySub->OnError(aError);
		ProtClose();
	}

	void
	TCPSocketIFace::ProtOnSend(
		uint32_t aByteCount)
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		mySub->OnSend(aByteCount);
	}

	void
	TCPSocketIFace::ProtOnSendError(
		mg::box::Error* aError)
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		if (!PrivIsCompromised())
		{
			mySub->OnSendError(aError);
			mySub->OnError(aError);
		}
		ProtClose();
		mySendEvent.Lock();
	}

	void
	TCPSocketIFace::ProtOnRecv(
		mg::net::BufferReadStream& aStream)
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		mySub->OnRecv(aStream);
	}

	void
	TCPSocketIFace::ProtOnRecvError(
		mg::box::Error* aError)
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		if (!PrivIsCompromised())
		{
			mySub->OnRecvError(aError);
			mySub->OnError(aError);
		}
		ProtClose();
		myRecvEvent.Lock();
	}

	TCPSocketIFace::~TCPSocketIFace()
	{
		mg::box::MutexLock lock(myMutex);
		// If it was connected, it was referenced by IOCore. Therefore couldn't be deleted
		// before closed.
		MG_DEV_ASSERT(myState == TCP_SOCKET_STATE_CLOSED);
		delete myCtl;
		myCtl = nullptr;
		delete myFrontCtl;
		myFrontCtl = nullptr;
	}

	void
	TCPSocketIFace::PrivConnectAbort(
		mg::box::Error* aError)
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		if (!PrivIsCompromised())
		{
			mySub->OnConnectError(aError);
			mySub->OnError(aError);
		}
		ProtClose();
	}

	void
	TCPSocketIFace::PrivHandshakeStart()
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		MG_DEV_ASSERT(!myWasHandshakeDone);
		MG_DEV_ASSERT(myCtl != nullptr && myCtl->HasHandshake());

		mg::box::MutexLock lock(myMutex);
		if (myState == TCP_SOCKET_STATE_CONNECTING)
			myState = TCP_SOCKET_STATE_HANDSHAKE;
		else
			MG_DEV_ASSERT(myState == TCP_SOCKET_STATE_CLOSING);
	}

	void
	TCPSocketIFace::PrivHandshakeCommit()
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		MG_DEV_ASSERT(!myWasHandshakeDone);
		myMutex.Lock();
		if (myState == TCP_SOCKET_STATE_HANDSHAKE)
			myState = TCP_SOCKET_STATE_CONNECTED;
		else
			MG_DEV_ASSERT(myState == TCP_SOCKET_STATE_CLOSING);
		myMutex.Unlock();

		myWasHandshakeDone = true;
		mySub->OnConnect();
	}

	bool
	TCPSocketIFace::PrivIsCompromised() const
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		mg::box::MutexLock lock(myMutex);
		return myState >= TCP_SOCKET_STATE_CLOSING;
	}

	void
	TCPSocketIFace::PrivCtl()
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		myMutex.Lock();
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
		// Unlock because the external code can only change the front ctl. No need to keep
		// the lock while internal ctl works. And it can be slow as its commands do system
		// calls.
		myMutex.Unlock();

		if (myCtl->HasShutdown())
		{
			mg::box::Error::Ptr err;
			if (!myCtl->DoShutdown(err))
				ProtCloseError(mg::box::ErrorRaise(err, "shutdown"));
		}
		if (myCtl->HasAttach())
		{
			myCtl->DoAttach();
			PrivHandshakeStart();
			PrivEnableIO();
		}
		if (myCtl->HasConnect())
		{
			mg::box::Error::Ptr err;
			if (!myCtl->DoConnect(err))
			{
				// Failed connect terminates the operation. Should not still be present.
				MG_DEV_ASSERT(!myCtl->HasConnect());
				PrivConnectAbort(mg::box::ErrorRaise(err, "connect"));
			}
			else if (!myCtl->HasConnect())
			{
				// Ctl connect was updated without errors and is not present anymore? -
				// then it is successfully finished.
				PrivHandshakeStart();
				PrivEnableIO();
				myTask.Reschedule();
			}
			// Otherwise connect is updated and not finished - still in progress, nothing
			// to do. Wait for more events.
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

		myMutex.Lock();
		// No need to check front ctl again. If new messages appeared, they will wake the
		// task up, and it will be rescheduled.
	end:
		mySendQueue.Append(std::move(myFrontSendQueue));
		myMutex.Unlock();

		Recv(myFrontRecvSize.ExchangeRelaxed(0));
	}

	void
	TCPSocketIFace::PrivEnableIO()
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		mySendEvent.Unlock();
		myRecvEvent.Unlock();
	}

	void
	TCPSocketIFace::PrivDisableIO()
	{
		MG_DEV_ASSERT(!myIsRunning);
		mySendEvent.Reset();
		mySendEvent.Lock();
		myRecvEvent.Reset();
		myRecvEvent.Lock();
	}

}
}

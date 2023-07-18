#pragma once

#include "mg/aio/IOTask.h"

namespace mg {
namespace aio {

	struct TCPSocketSubscription;
	class TCPSocketCtl;

	enum TCPSocketState
	{
		NEW,
		EMPTY,
		CONNECTING,
		CONNECTED,
		CLOSING,
		CLOSED,
	};

	struct TCPSocketConnectParams
	{
		TCPSocketConnectParams();

		// Optional socket to connect from. It might be specified when the owner added
		// some options to it.
		//
		// XXX: it might make sense to add here socket options to install inside of the
		// CTL. Then the socket wouldn't be needed. The problem is what to do if some
		// options are critical to install and some are not?
		mg::net::Socket mySocket;
		const mg::net::Host* myHost;
	};

	// Base class for TCP-like sockets. It provides the TCP connection state machine along
	// with methods and members necessary for all child sockets.  Killer-feature of the
	// class is in not having any virtual methods in the public API. So its usage costs as
	// much as usage of any of its children, no overhead.
	class TCPSocketIFace
		: public IOSubscription
	{
	public:
		TCPSocketIFace();

		//////////////////////////////////////////////////////////////////////////////////
		// Thread-safe functions, can be called from any thread.
		//

		void Delete();

		void PostConnect(
			const mg::net::Host& aHost,
			TCPSocketSubscription* aSub);

		void PostConnect(
			mg::net::Socket aSocket,
			const mg::net::Host& aHost,
			TCPSocketSubscription* aSub);

		// A generic connect if nothing above is enough.
		void PostConnect(
			const TCPSocketConnectParams& aParams,
			TCPSocketSubscription* aSub);

		// Wrap a functional connected socket.
		void PostWrap(
			mg::net::Socket aSocket,
			TCPSocketSubscription* aSub);

		// Post as a task. Wakeup() and Close() operations start working right away even
		// before a real socket is added, which makes it possible to use the object like a
		// TaskScheduler's task. Helpful when need to do something between reconnects.
		// Connect can be done later from a worker thread.
		void PostTask(
			TCPSocketSubscription* aSub);

		// The sent buffers ownership is taken by the socket. They can't be used after the
		// post. Or can be kept referenced somewhere, but can't be changed anyhow.
		bool PostSendRef(
			mg::net::Buffer* aHead);
		bool PostSendRef(
			const void* aData,
			uint64_t aSize);
		// The entire buffer list is compactly copied. Can free the buffers are the post.
		bool PostSendCopy(
			const mg::net::Buffer* aHead);
		bool PostSendCopy(
			const void* aData,
			uint64_t aSize);

		void PostWakeup();
		void PostClose();
		void PostShutdown();

		bool IsClosed() const;
		bool IsConnected() const;
		bool IsInWorkerNow() const;

		//////////////////////////////////////////////////////////////////////////////////
		// Not thread-safe functions. Can only be called from an IO worker thread. From
		// the callbacks invoked in the listener. But these functions are preferable as
		// they are much faster.
		//

		bool SendRef(
			mg::net::Buffer* aHead);
		bool SendRef(
			const void* aData,
			uint64_t aSize);
		bool SendCopy(
			const mg::net::Buffer* aHead);
		bool SendCopy(
			const void* aData,
			uint64_t aSize);

		void SetDeadline(
			uint64_t aDeadline);

		void Reschedule();

		void Connect(
			const TCPSocketConnectParams& aParams);

		bool IsExpired() const;

		// Socket options. The internal socket must not be exposed by value to prevent its
		// mis-usage.

		bool SetKeepAlive(
			bool aEnable,
			uint32_t aTimeout = 0);
		bool SetNoDelay(
			bool aEnable);

	protected:
		void ProtOpen(
			uint32_t aRecvSize);

		void ProtOnWakeup();

		void ProtClose();
		void ProtCloseError(
			mg::box::Error* aError);

		void ProtOnSend(
			uint32_t aByteCount);
		void ProtOnSendError(
			mg::box::Error* aError);

		void ProtOnRecv(
			mg::net::Buffer* aHead,
			mg::net::Buffer* aTail,
			uint32_t aByteCount);
		void ProtOnRecvError(
			mg::box::Error* aError);

		~TCPSocketIFace() override;

		mg::net::BufferLinkList mySendQueue;
		mg::net::BufferLinkList myRecvQueue;
		IoCoreEvent mySendEvent;
		IoCoreEvent myRecvEvent;
		IoCoreTask myTask;

	private:
		void PrivConnectAbort(
			mg::common::Error* aError);
		void PrivHandshakeStart();
		void PrivHandshakeCommit();

		// Compromised means the socket is already in a faulty
		// state. Either it was closed explicitly, or met an error
		// and the error callbacks were invoked. Anyway all
		// subsequent errors may not be relevant to the initial
		// error and should not be raised then.
		bool PrivIsCompromised();

		void PrivCtl();

		void PrivEnableIO();

		void PrivDisableIO();

		mg::common::Mutex myLock;
		TCPSocketState myState;
		bool myWasHandshakeDone;
		// The task is inside IoCore.
		bool myIsRunning;
		// The task can be posted into IoCore.
		bool myIsReadyToStart;
		TCPSocketSubscription* myListener;
		// Front queue is for pushing new buffers from external
		// threads, it is protected with a lock. Usually is not
		// needed if the listener does IO only from worker
		// threads.
		mg::net::WriteBufferList myFrontSendQueue;
		// Control messages are rare. 1 or 2 per socket lifetime.
		// Hence allocated on demand and freed right after usage.
		TCPSocketCtl* myFrontCtl;
		TCPSocketCtl* myCtl;
	};

	inline void
	TCPSocketIFace::SetDeadline(
		uint64 aDeadline)
	{
		MG_COMMON_ASSERT(myTask.IsInWorkerNow());
		myTask.SetDeadline(aDeadline);
	}

	inline bool
	TCPSocketIFace::IsExpired() const
	{
		MG_COMMON_ASSERT(myTask.IsInWorkerNow());
		return myTask.IsExpired();
	}

	inline void
	TCPSocketIFace::Reschedule()
	{
		myTask.Reschedule();
	}

} // serverbox
} // mg

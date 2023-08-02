#pragma once

#include "mg/aio/IOTask.h"
#include "mg/box/Time.h"
#include "mg/net/Buffer.h"

namespace mg {
namespace aio {

	struct TCPSocketSubscription;
	class TCPSocketCtl;
	class TCPSocketHandshake;

	enum TCPSocketState
	{
		TCP_SOCKET_STATE_NEW,
		TCP_SOCKET_STATE_EMPTY,
		TCP_SOCKET_STATE_CONNECTING,
		TCP_SOCKET_STATE_HANDSHAKE,
		TCP_SOCKET_STATE_CONNECTED,
		TCP_SOCKET_STATE_CLOSING,
		TCP_SOCKET_STATE_CLOSED,
	};

	struct TCPSocketConnectParams
	{
		TCPSocketConnectParams();

		// Optional socket to connect from. It might be specified when the owner added
		// some options to it.
		//
		// XXX: it might make sense to add here socket options to install inside of the
		// ctl. Then the socket wouldn't be needed. The problem is what to do if some
		// options are critical to install and some are not?
		mg::net::Socket mySocket;
		std::string myEndpoint;
		// When a domain name is specified, the field allows to enforce selection of one
		// specific address family among available options when the domain is resolved
		// into one or multiple IPs. When specified but no IPs with the chosen family are
		// found, the connect fails. Even if there were other families available.
		mg::net::SockAddrFamily myAddrFamily;
		// Delay is very handy for reconnects. Normally a user doesn't want to reconnect
		// instantly because a too aggressive reconnect loop could consume notable amount
		// of CPU time. Better do it, for example, once per tens of milliseconds at least.
		// No-delay reconnect also works but usually is a bad idea.
		mg::box::TimeLimit myDelay;
	};

	// Base class for TCP-based sockets. It provides the TCP connection state machine
	// along with methods and members necessary for all child classes. One of the
	// important features of the class is in not having any virtual methods in the public
	// API. So its usage costs as much as usage of any of its children explicitly, no
	// overhead.
	class TCPSocketIFace
		: public IOSubscription
	{
	public:
		TCPSocketIFace(
			IOCore& aCore);

		//
		// Thread-safe functions, can be called from any thread.
		//

		void Delete();

		void PostConnect(
			const TCPSocketConnectParams& aParams,
			TCPSocketSubscription* aSub);

		// Wrap an already functioning connected socket.
		void PostWrap(
			mg::net::Socket aSocket,
			TCPSocketSubscription* aSub);

		// Post as a task. Wakeup() and Close() operations start working right away even
		// before a real socket is added, which makes it possible to use the object like a
		// TaskScheduler's task. Helpful when need to do something between reconnects.
		// Connect can be done later from a worker thread. Or not done at all.
		void PostTask(
			TCPSocketSubscription* aSub);

		// * Send 'move' - data ownership is taken by the socket.
		// * Send 'ref' - original object is referenced. Shouldn't be changed or deleted
		//     until the sending is complete.
		// * Send 'copy' - the data is fully copied. The original object can be deleted or
		//     changed or anything.

		void PostSendMove(
			mg::net::BufferLink* aHead);

		void PostSendRef(
			mg::net::Buffer* aHead);
		void PostSendRef(
			mg::net::Buffer::Ptr&& aHead);
		void PostSendRef(
			const void* aData,
			uint64_t aSize);

		void PostSendCopy(
			const mg::net::BufferLink* aHead);
		void PostSendCopy(
			const mg::net::Buffer* aHead);
		void PostSendCopy(
			const void* aData,
			uint64_t aSize);

		// Add the given size to the next receive buffer's size. If there is no receive in
		// progress right now, then it will be started. Otherwise the accumulated size
		// will be used only for the next receive. The resulting OnRecv() might return
		// less or a bit more data than requested.
		void PostRecv(
			uint64_t aSize);

		void PostWakeup();
		void PostClose();
		void PostShutdown();

		bool IsClosed() const;
		bool IsConnected() const;
		bool IsInWorkerNow() const;

		//
		// Not thread-safe functions. Can only be called from an IO worker thread. From
		// the callbacks invoked in the listener. But these functions are preferable as
		// they are much faster.
		//

		void SendMove(
			mg::net::BufferLink* aHead);

		void SendRef(
			mg::net::Buffer* aHead);
		void SendRef(
			mg::net::Buffer::Ptr&& aHead);
		void SendRef(
			const void* aData,
			uint64_t aSize);

		void SendCopy(
			const mg::net::BufferLink* aHead);
		void SendCopy(
			const mg::net::Buffer* aHead);
		void SendCopy(
			const void* aData,
			uint64_t aSize);

		void Recv(
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
			bool aValue,
			uint32_t aTimeout,
			mg::box::Error::Ptr& aOutErr);

		bool SetNoDelay(
			bool aValue,
			mg::box::Error::Ptr& aOutErr);

	protected:
		void ProtOpen();

		// An optional handshake step helps not to store the handshake context in the
		// actual socket class and is updated only after the raw connection is
		// established. Hence no need to worry about when to start the handshake in child
		// class' code.
		void ProtPostHandshake(
			TCPSocketHandshake* aHandshake);

		bool ProtWasHandshakeDone() const;

		void ProtOnEvent();

		void ProtClose();
		void ProtCloseError(
			mg::box::Error* aError);

		void ProtOnSend(
			uint32_t aByteCount);
		void ProtOnSendError(
			mg::box::Error* aError);

		void ProtOnRecv(
			mg::net::BufferReadStream& aStream);
		void ProtOnRecvError(
			mg::box::Error* aError);

		~TCPSocketIFace() override;

		mg::net::BufferLinkList mySendQueue;
		mg::net::BufferStream myRecvQueue;
		uint64_t myRecvSize;
		IOEvent mySendEvent;
		IOEvent myRecvEvent;
		IOTask myTask;

	private:
		void PrivConnectAbort(
			mg::box::Error* aError);

		void PrivHandshakeStart();
		void PrivHandshakeCommit();

		// Compromised means the socket is already in a faulty state. Either it was closed
		// explicitly, or met an error and the error callbacks were invoked. Anyway all
		// subsequent errors may be irrelevant to the initial error and should not be
		// raised then.
		bool PrivIsCompromised() const;

		void PrivCtl();
		void PrivEnableIO();
		void PrivDisableIO();

		mutable mg::box::Mutex myMutex;
		TCPSocketState myState;
		bool myWasHandshakeDone;
		// The task is inside IOCore.
		bool myIsRunning;
		// The task can be posted into IOCore.
		bool myIsReadyToStart;
		TCPSocketSubscription* mySub;
		// Front queue is for pushing new buffers from external threads, it is protected
		// with a lock. Usually is not needed if the listener does IO only from the IO
		// worker thread.
		mg::net::BufferLinkList myFrontSendQueue;
		mg::box::AtomicU64 myFrontRecvSize;
		// Control messages are rare. 1 or 2 per socket lifetime. Hence allocated on
		// demand and free right after usage.
		TCPSocketCtl* myFrontCtl;
		TCPSocketCtl* myCtl;
	};

	inline void
	TCPSocketIFace::SetDeadline(
		uint64_t aDeadline)
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		myTask.SetDeadline(aDeadline);
	}

	inline bool
	TCPSocketIFace::IsExpired() const
	{
		MG_DEV_ASSERT(myTask.IsInWorkerNow());
		return myTask.IsExpired();
	}

	inline void
	TCPSocketIFace::Reschedule()
	{
		myTask.Reschedule();
	}

}
}

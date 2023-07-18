// ProjectFilter(Network)
#pragma once

#include "mg/network/platform/HostPlatform.h"
#include "mg/network/WriteBufferList.h"

#include "mg/serverbox/IoCore.h"

F_DECLARE_CLASS(mg, common, Error);

namespace mg {
namespace serverbox {

	struct TCPSocketListener;
	class TCPSocketCtl;
	class TCPSocketHandshake;

	enum TCPSocketState
	{
		NEW,
		EMPTY,
		CONNECTING,
		HANDSHAKE,
		CONNECTED,
		CLOSING,
		CLOSED,
	};

	struct TCPSocketConnectParams
	{
		TCPSocketConnectParams();

		// Optional socket to connect from. It might be specified
		// when the owner added some options to it.
		//
		// XXX: it might make sense to add here socket options to
		// install inside of the CTL. Then the socket wouldn't be
		// needed. The problem is what to do if some options are
		// critical to install and some are not?
		Socket mySocket;
		// Either a known IP address must be specified or a string
		// host to resolve into an IP automatically.
		const mg::network::Host* myAddr;
		const char* myHost;
		uint16 myPort;
		// Do not start connecting until this delay passes. This
		// is especially useful as a reconnect back-off. To
		// prevent busy-loop reconnect when it fails immediately
		// due to network issues.
		uint32 myDelay;
	};

	// Base class for TCP-like sockets. It provides the TCP
	// connection state machine along with methods and members
	// necessary for all child sockets.
	// Killer-feature of the class is in not having any virtual
	// methods in the public API. So its usage costs as much as
	// usage of any of its children, no overhead.
	class TCPSocketBase
		: public IIoCoreListener
	{
	public:
		TCPSocketBase();

		//
		// Thread-safe functions, can be called from any thread.
		//

		void Delete();

		void PostConnect(
			const mg::network::Host& aHost,
			TCPSocketListener* aListener);

		void PostConnect(
			Socket aSocket,
			const mg::network::Host& aHost,
			TCPSocketListener* aListener);

		// Slower than the direct connect, but understands domain
		// names.
		void PostConnect(
			const char* aHost,
			uint16 aPort,
			TCPSocketListener* aListener);

		void PostConnect(
			Socket aSocket,
			const char* aHost,
			uint16 aPort,
			TCPSocketListener* aListener);

		// A generic connect if nothing above is enough.
		void PostConnect(
			const TCPSocketConnectParams& aParams,
			TCPSocketListener* aListener);

		// Wrap a functional connected socket.
		void PostWrap(
			Socket aSocket,
			TCPSocketListener* aListener);

		// Post as a task. Wakeup() and Close() operations start
		// working right away even before a real socket is added,
		// which makes it possible to use the object like a
		// TaskScheduler's task. Helpful when need to do something
		// between reconnects. Connect can be done later from a
		// worker thread.
		void PostTask(
			TCPSocketListener* aListener);

		// The sent buffers ownership is taken by the socket. They
		// can't be used after the post. Or can be kept referenced
		// somewhere, but can't be changed anyhow. Including their
		// 'myNext' pointer, because internally they are stored in
		// a list.
		bool PostSend(
			mg::network::WriteBuffer* aHead);

		bool PostSend(
			mg::network::WriteBuffer* aHead,
			mg::network::WriteBuffer* aTail);

		void PostWakeup();

		void PostClose();

		void PostShutdown();

		bool IsClosed();

		bool IsConnected();

		bool IsInWorkerNow() const;

		//
		// Not thread-safe functions. Can only be called from an
		// IO worker thread. From the callbacks invoked in the
		// listener. But these functions are preferable as they
		// are much faster.
		//

		bool Send(
			mg::network::WriteBuffer* aHead);

		bool Send(
			mg::network::WriteBuffer* aHead,
			mg::network::WriteBuffer* aTail);

		void SetDeadline(
			uint64 aDeadline);

		void Reschedule();

		void Connect(
			const TCPSocketConnectParams& aParams);

		bool IsExpired() const;

		// Socket options. The internal socket must not be exposed
		// by value to prevent its mis-usage.

		bool SetKeepAlive(
			bool aEnable,
			uint32 aTimeout = 0);

		bool SetNoDelay(
			bool aEnable);

	protected:
		void ProtOpen(
			uint32 aRecvSize);

		// An optional handshake step helps not to store the
		// handshake context in the actual socket class and is
		// updated only after the raw connection is established.
		// Hence no need to worry about when to start the
		// handshake in child class' code.
		void ProtPostHandshake(
			TCPSocketHandshake* aHandshake);

		bool ProtWasHandshakeDone() const;

		void ProtOnWakeup();

		void ProtCloseError(
			mg::common::Error* aError);

		void ProtClose();

		void ProtOnSend(
			uint32 aByteCount);

		void ProtOnSendError(
			mg::common::Error* aError);

		void ProtOnRecv(
			mg::network::WriteBuffer* aHead,
			mg::network::WriteBuffer* aTail,
			uint32 aByteCount);

		void ProtOnRecvError(
			mg::common::Error* aError);

		~TCPSocketBase() override;

		mg::network::WriteBufferList mySendQueue;
		mg::network::WriteBufferList myRecvQueue;
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
		TCPSocketListener* myListener;
		// Front queue is for pushing new buffers from external
		// threads, it is protected with a lock. Usually is not
		// needed if the listener does IO only from worker
		// threads.
		mg::network::WriteBufferList myFrontSendQueue;
		// Control messages are rare. 1 or 2 per socket lifetime.
		// Hence allocated on demand and freed right after usage.
		TCPSocketCtl* myFrontCtl;
		TCPSocketCtl* myCtl;
	};

	inline void
	TCPSocketBase::SetDeadline(
		uint64 aDeadline)
	{
		MG_COMMON_ASSERT(myTask.IsInWorkerNow());
		myTask.SetDeadline(aDeadline);
	}

	inline bool
	TCPSocketBase::IsExpired() const
	{
		MG_COMMON_ASSERT(myTask.IsInWorkerNow());
		return myTask.IsExpired();
	}

	inline void
	TCPSocketBase::Reschedule()
	{
		myTask.Reschedule();
	}

} // serverbox
} // mg

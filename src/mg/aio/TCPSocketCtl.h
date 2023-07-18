// ProjectFilter(Network)
#pragma once

#include "mg/common/Callback.h"

#include "mg/network/platform/HostPlatform.h"

#include "mg/serverbox/IoCore.h"

namespace mg {
namespace serverbox {

	struct TCPSocketCtlResolveRequest;
	class TCPSocketHandshake;

	struct TCPSocketCtlConnectParams
	{
		TCPSocketCtlConnectParams();

		Socket mySocket;
		const mg::network::Host* myAddr;
		const char* myHost;
		uint16 myPort;
		uint32 myDelay;
	};

	using TCPSocketHandshakeCallback = mg::common::Callback<bool(TCPSocketHandshake*)>;

	// The handshake is not just a callback, because the latter
	// can bind only a very limited number of bytes and doesn't
	// have a destructor (= can't call 'delete' on bound
	// pointers). For big context (like ProofOfWork) the socket
	// class is supposed to inherit the handshake class and put
	// all members there. For a small context can use the
	// callback alone, without inheritance.
	class TCPSocketHandshake
	{
	public:
		TCPSocketHandshake(
			const TCPSocketHandshakeCallback& aCallback);

		virtual ~TCPSocketHandshake() = default;

		bool Update();

	private:
		TCPSocketHandshakeCallback myCallback;
	};

	// TCP socket control message. It is an internal helper, not a
	// part of the public API. It is a transport unit for commands
	// about how to change socket state.
	class TCPSocketCtl
	{
	public:
		TCPSocketCtl(
			IoCoreTask* aTask);

		~TCPSocketCtl();

		// It is assumed the owner has one front ctl message, and
		// one being in progress in an IO worker. Front messages
		// must be merged into the internal one. A queue of
		// messages can't be used, because some ctls like shutdown
		// may affect the others. And because it would waste more
		// memory.
		void MergeFrom(
			TCPSocketCtl* aSrc);

		//
		// One control message can carry multiple commands.
		//

		void AddConnect(
			const TCPSocketCtlConnectParams& aParams);

		// Attach to an already connected socket. It is useful
		// when TCPSocket is used to wrap accepted clients.
		void AddAttach(
			Socket aSocket);

		// An optional handshake step. It is activated after a raw
		// connection is established.
		void AddHandshake(
			TCPSocketHandshake* aHandshake);

		void AddShutdown();

		//
		// For use inside of an IO worker.
		//

		bool IsIdle() const;

		bool HasShutdown() const;

		// In case of fail installs the global last error.
		bool DoShutdown();

		bool HasConnect() const;

		// In case of fail installs the global last error.
		bool DoConnect();

		bool HasAttach() const;

		void DoAttach();

		bool HasHandshake() const;

		void DoHandshake();

	private:
		void PrivStartConnect();

		void PrivEndConnect();

		void PrivStartAttach();

		void PrivEndAttach();

		void PrivStartHandshake();

		void PrivEndHandshake();

		bool myHasShutdown;

		bool myHasConnect;
		bool myConnectIsStarted;
		mg::network::Host myConnectHost;
		IoCoreEvent myConnectEvent;
		TCPSocketCtlResolveRequest* myConnectResolve;
		Socket myConnectSocket;
		uint32 myConnectDelay;
		uint64 myConnectDeadline;

		bool myHasAttach;
		Socket myAttachSocket;

		bool myHasHandshake;
		TCPSocketHandshake* myHandshake;

		IoCoreTask* myTask;
	};
}
}

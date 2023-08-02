#pragma once

#include "mg/aio/IOTask.h"
#include "mg/box/Time.h"

#include <functional>

namespace mg {
namespace aio {

	class TCPSocketHandshake;
	struct TCPSocketCtlDomainRequest;

	struct TCPSocketCtlConnectParams
	{
		TCPSocketCtlConnectParams();

		mg::net::Socket mySocket;
		std::string myEndpoint;
		mg::net::SockAddrFamily myAddrFamily;
		mg::box::TimeLimit myDelay;
	};

	using TCPSocketHandshakeCallback = std::function<bool(TCPSocketHandshake*)>;

	// The handshake is not just a callback, because the latter doesn't have a destructor
	// (= can't call 'delete' on bound pointers). Having a class gives more control,
	// although people can just use the default one with a callback too.
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

	// TCP socket control message. It is an internal helper, not a part of the public API.
	// It is a transport unit for commands about how to change the socket state.
	class TCPSocketCtl
	{
	public:
		TCPSocketCtl(
			IOTask* aTask);
		~TCPSocketCtl();

		// It is assumed the owner has one front ctl message, and one being in progress in
		// an IO worker. Front messages must be merged into the internal one. A queue of
		// messages can't be used, because some ctls like shutdown may affect the others.
		// And because it would waste more memory.
		void MergeFrom(
			TCPSocketCtl* aSrc);

		//
		// One control message can carry multiple commands.
		//

		void AddConnect(
			const TCPSocketCtlConnectParams& aParams);

		// Attach to an already connected socket. It is useful when TCPSocket is used to
		// wrap remotely accepted clients.
		void AddAttach(
			mg::net::Socket aSocket);

		// An optional handshake step. It is activated after a raw TCP connection is
		// established. Can be used for SSL, for custom protocols, for any kinds of
		// initial communications.
		void AddHandshake(
			TCPSocketHandshake* aHandshake);

		void AddShutdown();

		//
		// For use inside of an IO worker.
		//

		bool IsIdle() const;

		bool HasShutdown() const;
		bool DoShutdown(
			mg::box::Error::Ptr& aOutErr);

		bool HasConnect() const;
		bool DoConnect(
			mg::box::Error::Ptr& aOutErr);

		bool HasAttach() const;
		void DoAttach();

		bool HasHandshake() const;
		void DoHandshake();

	private:
		void PrivEndConnect();

		void PrivEndAttach();

		void PrivStartHandshake();
		void PrivEndHandshake();

		bool myHasShutdown;

		bool myHasConnect;
		bool myConnectIsStarted;
		mg::net::Host myConnectHost;
		IOEvent myConnectEvent;
		TCPSocketCtlDomainRequest* myConnectDomain;
		mg::net::Socket myConnectSocket;
		mg::box::TimeLimit myConnectDelay;
		uint64_t myConnectStartDeadline;

		bool myHasAttach;
		mg::net::Socket myAttachSocket;

		bool myHasHandshake;
		TCPSocketHandshake* myHandshake;

		IOTask* myTask;
	};

}
}

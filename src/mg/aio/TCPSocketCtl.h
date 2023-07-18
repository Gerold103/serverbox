#pragma once

#include "mg/aio/IOTask.h"

namespace mg {
namespace aio {

	struct TCPSocketCtlConnectParams
	{
		TCPSocketCtlConnectParams();

		mg::net::Socket mySocket;
		const mg::net::Host* myHost;
	};

	// TCP socket control message. It is an internal helper, not a part of the public API.
	// It is a transport unit for commands about how to change socket state.
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
		// wrap accepted clients.
		void AddAttach(
			mg::net::Socket aSocket);

		void AddShutdown();

		//
		// For use inside of an IO worker.
		//

		bool IsIdle() const;

		bool HasShutdown() const;

		// In case of fail installs the global last error.
		bool DoShutdown(
			mg::box::Error::Ptr& aOutErr);

		bool HasConnect() const;

		// In case of fail installs the global last error.
		bool DoConnect(
			mg::box::Error::Ptr& aOutErr);

		bool HasAttach() const;

		void DoAttach();

	private:
		void PrivStartConnect();

		void PrivEndConnect();

		void PrivStartAttach();

		void PrivEndAttach();

		bool myHasShutdown;

		bool myHasConnect;
		bool myConnectIsInProgress;
		mg::net::Host myConnectHost;
		IOEvent myConnectEvent;
		mg::net::Socket myConnectSocket;

		bool myHasAttach;
		Socket myAttachSocket;

		IOTask* myTask;
	};
}
}

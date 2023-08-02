#pragma once

#include "mg/box/Error.h"

#include "mg/net/Socket.h"

namespace mg {
namespace sio {

	enum TCPServerState
	{
		TCP_SERVER_STATE_CLOSED,
		TCP_SERVER_STATE_BOUND,
		TCP_SERVER_STATE_LISTENING,
	};

	class TCPServer
	{
	public:
		TCPServer();
		~TCPServer();

		bool Bind(
			const mg::net::Host& aHost,
			mg::box::Error::Ptr& aOutErr);

		uint16_t GetPort() const;

		bool Listen(
			uint32_t aBacklog,
			mg::box::Error::Ptr& aOutErr);

		mg::net::Socket Accept(
			mg::net::Host& aOutPeer,
			mg::box::Error::Ptr& aOutErr);

		void Close();

	private:
		TCPServerState myState;
		mg::net::Socket mySocket;
	};

}
}

// ProjectFilter(Network)
#include "stdafx.h"

#include "mg/serverbox/TCPSocketListener.h"

namespace mg {
namespace serverbox {

	void
	TCPSocketListener::OnConnect()
	{
	}

	void
	TCPSocketListener::OnConnectError(
		mg::common::Error*)
	{
	}

	void
	TCPSocketListener::OnRecv(
		mg::network::WriteBuffer*,
		mg::network::WriteBuffer*,
		uint32)
	{
	}

	void
	TCPSocketListener::OnRecvError(
		mg::common::Error*)
	{
	}

	void
	TCPSocketListener::OnSend(
		uint32)
	{
	}

	void
	TCPSocketListener::OnSendError(
		mg::common::Error*)
	{
	}

	void
	TCPSocketListener::OnError(
		mg::common::Error*)
	{
	}

	void
	TCPSocketListener::OnClose()
	{
	}

	void
	TCPSocketListener::OnWakeup()
	{
	}

} // serverbox
} // mg

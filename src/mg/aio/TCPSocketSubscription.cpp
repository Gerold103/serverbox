#include "TCPSocketSubscription.h"

namespace mg {
namespace aio {

	void
	TCPSocketSubscription::OnConnect()
	{
	}

	void
	TCPSocketSubscription::OnConnectError(
		mg::box::Error*)
	{
	}

	void
	TCPSocketSubscription::OnRecv(
		mg::net::BufferReadStream&)
	{
	}

	void
	TCPSocketSubscription::OnRecvError(
		mg::box::Error*)
	{
	}

	void
	TCPSocketSubscription::OnSend(
		uint32_t)
	{
	}

	void
	TCPSocketSubscription::OnSendError(
		mg::box::Error*)
	{
	}

	void
	TCPSocketSubscription::OnError(
		mg::box::Error*)
	{
	}

	void
	TCPSocketSubscription::OnClose()
	{
	}

	void
	TCPSocketSubscription::OnEvent()
	{
	}

}
}

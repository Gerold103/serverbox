#pragma once

#include "mg/box/Definitions.h"

F_DECLARE_CLASS(mg, box, Error);
F_DECLARE_CLASS(mg, net, WriteBuffer);

namespace mg {
namespace aio {

	// All the callbacks are called from an IO worker thread.
	struct TCPSocketSubscription
	{
		virtual ~TCPSocketSubscription() = default;

		virtual void OnConnect();
		virtual void OnConnectError(
			mg::box::Error* aError);

		virtual void OnRecv(
			mg::net::Buffer* aHead,
			mg::net::Buffer* aTail,
			uint32_t aByteCount);
		virtual void OnRecvError(
			mg::box::Error* aError);

		virtual void OnSend(
			uint32_t aByteCount);
		virtual void OnSendError(
			mg::box::Error* aError);

		virtual void OnError(
			mg::box::Error* aError);
		virtual void OnClose();
		virtual void OnWakeup();
	};

}
}
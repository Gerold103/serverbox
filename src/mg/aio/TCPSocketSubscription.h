// ProjectFilter(Network)
#pragma once

#include "mg/common/Types.h"

F_DECLARE_CLASS(mg, common, Error);
F_DECLARE_CLASS(mg, network, WriteBuffer);

namespace mg {
namespace serverbox {

	// All the callbacks are called from an IO worker thread.
	struct TCPSocketListener
	{
		virtual ~TCPSocketListener() = default;

		virtual void OnConnect();

		virtual void OnConnectError(
			mg::common::Error* aError);

		virtual void OnRecv(
			mg::network::WriteBuffer* aHead,
			mg::network::WriteBuffer* aTail,
			uint32 aByteCount);

		virtual void OnRecvError(
			mg::common::Error* aError);

		virtual void OnSend(
			uint32 aByteCount);

		virtual void OnSendError(
			mg::common::Error* aError);

		virtual void OnError(
			mg::common::Error* aError);

		virtual void OnClose();

		virtual void OnWakeup();
	};

} // serverbox
} // mg

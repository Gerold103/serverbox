#pragma once

#include "mg/aio/TCPSocketIFace.h"

namespace mg {
namespace aio {

	struct TCPSocketParams
	{
		TCPSocketParams();

		// No parameters yet.
	};

	// Event-oriented asynchronous TCP socket.
	class TCPSocket
		: public TCPSocketIFace
	{
	public:
		TCPSocket(
			IOCore& aCore);

		void Open(
			const TCPSocketParams& aParams);

	private:
		~TCPSocket() override = default;

		void OnEvent(
			const IOArgs& aArgs) override;

		void PrivSend();
		void PrivSendAbort(
			mg::box::Error* aError);
		void PrivSendCommit(
			uint32_t aByteCount);
		bool PrivSendEventConsume();

		void PrivRecv();
		void PrivRecvAbort(
			mg::box::Error* aError);
		void PrivRecvCommit(
			uint32_t aByteCount);
		bool PrivRecvEventConsume();

		// Offset in the first buffer for sending. It is > 0 when a whole buffer couldn't
		// be sent in one IO operation.
		uint32_t mySendOffset;
	};
}
}

// ProjectFilter(Network)
#pragma once

#include "mg/serverbox/TCPSocketBase.h"

namespace mg {
namespace serverbox {

	struct TCPSocketParams
	{
		TCPSocketParams();

		// Receive size is the number of bytes to allocate for
		// reading from the socket. The number is restored
		// automatically when some data is received and returned
		// to the listener.
		uint32 myRecvSize;
	};

	// Event-oriented asynchronous TCP socket. It is similar to
	// clientbox raw TCP socket, but does not require polling.
	// It is preferable for everything not running on the client.
	class TCPSocket
		: public TCPSocketBase
	{
	public:
		void Open(
			const TCPSocketParams& aParams);

	private:
		void Execute(
			const IoCoreArgs& aArgs) override;

		void PrivSend();
		void PrivSendAbort(
			mg::common::Error* aError);
		void PrivSendCommit(
			uint32 aByteCount);
		bool PrivSendEventConsume();

		void PrivRecv();
		void PrivRecvAbort(
			mg::common::Error* aError);
		void PrivRecvCommit(
			uint32 aByteCount);
		bool PrivRecvEventConsume();

		// Offset in the first buffer for sending. It is > 0 when
		// a whole buffer couldn't be sent in one IO operation.
		uint32 mySendOffset;
	};
}
}

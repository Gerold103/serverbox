#pragma once

#include "mg/aio/TCPSocketIFace.h"

F_DECLARE_CLASS(mg, net, SSLContext)
F_DECLARE_CLASS(mg, net, SSLStream)

namespace mg {
namespace aio {

	struct SSLSocketParams
	{
		SSLSocketParams();

		mg::net::SSLContext* mySSL;
		// Hostname is used for SNI (Server Name Indication) when one server might host
		// multiple domains with different certificates. The server can enable it and even
		// enforce SNI. But usually it is not obligatory. It must be a domain name. IP
		// address will be either ignored or even rejected by the server, most likely.
		const char* myHostName;
		bool myDoEncrypt;
	};

	// Event-oriented asynchronous SSL socket based on TCP protocol.
	class SSLSocket
		: public TCPSocketIFace
	{
	public:
		SSLSocket(
			IOCore& aCore);

		void Open(
			const SSLSocketParams& aParams);

		// Empty on client. And on server it would return the name requested by the
		// client-peer. Or empty if nothing is requested.
		std::string GetHostName() const;

		void Encrypt();

	private:
		~SSLSocket() override;

		void OnEvent(
			const IOArgs& aArgs) override;

		bool PrivHandshake();

		void PrivCipherPopulate();
		void PrivCipherFetch();

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

		// SSL socket, on the contrary with TCP-one, has 2 pairs of queues, not one.
		//
		// - One is the regular send/recv queue pair. Send-queue contains user data
		//     which needs to be sent. Recv-queue contains user-data for delivering to
		//     the user.
		//
		// - Second is a pair of encrypted queues. The actual underlying socket operates
		//     on encrypted data, so it can't just take raw user data. Instead, the user
		//     data goes like this:
		//                   +-----------------------------------------+
		//     send_queue ---+-> <encrypt> -> enc_send_queue -> socket |
		//                   |                                         |
		//     recv_queue <--+-- <decrypt> <- enc_recv_queue <- socket |
		//                   +-----------------------------------------+
		//     The TCP socket interface still operates on the regular queues, but they
		//     are being used not directly by the socket. SSLSocket proxies them through
		//     its own encryption queues.
		//
		// The socket owner sees data as 'sent' as soon as it gets into the send_queue.
		// This simple hack provides the same level of robustness as normal sending
		// (because write()/send() also returns as soon as copied all into the kernel).
		//
		// Also it enables to report sent data sizes transparently and matching what the
		// user gives to SSLSocket. Owner of the socket won't see more data sent than they
		// actually tried to send due to SSL encryption inflating the data. Those details
		// are all hidden.
		//
		// This, for example, means that from what the base interface returns and what
		// passes to callbacks, it is not even possible to say - is it SSL or TCP inside.
		//
		// Also having enc_send_queue separated gives a back-pressure to the SSLStream.
		// If user sends too much, it won't all be given to SSLStream at once. Instead, it
		// is fed in portions. So as a single feed would be too long and expensive.
		mg::net::BufferStream myEncSendQueue;
		mg::net::BufferStream myEncRecvQueue;
		mg::net::SSLStream* mySSL;
		// Number of bytes moved to the encrypted queue, but not yet reported to the
		// socket owner. Main purpose having it as a part of the socket object is to
		// handle the on-fly turn-on of the encryption. When it happens, all the pending
		// data is flushed to the encrypted queue, but can't be reported as 'sent' right
		// away. Because it would require to call the user's callbacks right during the
		// flush inside Encrypt() method. It would be a bad incident to start invoking
		// user callbacks right in the user-called methods.
		uint64_t mySentSize;
	};

}
}

#pragma once

#include "mg/aio/TCPSocketIFace.h"
#include "mg/net/SSLStream.h"

namespace mg {
namespace aio {

	struct SSLSocketParams
	{
		SSLSocketParams();

		mg::net::SSLContext* mySSL;
		// Hostname is used for SNI (Server Name Indication) when
		// one server might host multiple domains with different
		// certificates. The server can enable it and even enforce
		// SNI. But usually it is not obligatory.
		// It must be a domain name. IP address will be either
		// ignored or even rejected by the server, most likely.
		const char* myHostName;
		bool myDoHandshake;
	};

	// Event-oriented asynchronous SSL socket. It is similar to
	// clientbox raw SSL socket, but does not require polling.
	// It is preferable for everything not running on the client.
	class SSLSocket
		: public TCPSocketIFace
	{
	public:
		SSLSocket(
			IOCore& aCore);

		void Open(
			const SSLSocketParams& aParams);

		void Handshake();

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

		// SSL socket, on the contrary with TCP-one, has 3 send
		// queues.
		//
		// - Front send queue: raw appdata collected from
		//   external threads;
		//
		// - Back send queue: raw appdata gathered from the front
		//   queue and from requests from inside of an IO worker
		//   thread;
		//
		// - Encrypted queue: SSL encoded data for actual sending.
		//
		// The socket owner sees data as 'sent' as soon as it gets
		// into the third queue. This simple hack provides the
		// same level of robustness as normal sending (because it
		// also returns as soon as copied all inside of the
		// kernel).
		//
		// But on the other hand allows to report sent data sizes
		// transparently. Owner of the socket won't see more sent
		// data than he actually tried to send due to SSL
		// encryption inflating the data.
		//
		// This, for example, means that from what the base
		// interface returns and what passes to callbacks, it is
		// not even possible to say - is it SSL or TCP inside.
		//
		// The third queue is populated gradually, always not
		// bigger than a certain limit. That allows not to report
		// all appdata as 'sent' immediately. Rate of data
		// encryption is the same as of sending.
		//
		// This is a back pressure in case the socket owner
		// decides to send more data as soon as the previous
		// portion is sent.
		mg::net::BufferStream myEncSendQueue;
		mg::net::BufferStream myEncRecvQueue;
		mg::net::SSLStream* mySSL;
		// Number of bytes moved to the encrypted queue, but not
		// yet reported to the socket owner. Main purpose having
		// it as a part of the socket object is to handle the
		// on-fly turn-on of the encryption. When it happens, all
		// the pending data is flushed to the encrypted queue,
		// but can't be reported as 'sent' right away. Because is
		// invoked directly by the owner. It would be a bad
		// pattern to invoke user callbacks from the explicitly
		// called methods.
		uint64_t mySentSize;
	};

}
}

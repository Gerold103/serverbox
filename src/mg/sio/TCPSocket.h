#pragma once

#include "mg/box/Error.h"

#include "mg/net/Buffer.h"
#include "mg/net/Socket.h"

namespace mg {
namespace sio {

	enum TCPSocketState
	{
		TCP_SOCKET_STATE_CLOSED,
		TCP_SOCKET_STATE_CONNECTING,
		TCP_SOCKET_STATE_CONNECTED,
	};

	class TCPSocket
	{
	public:
		TCPSocket();
		~TCPSocket();

		bool Connect(
			const mg::net::Host& aHost,
			mg::box::Error::Ptr& aOutErr);
		void Wrap(
			mg::net::Socket aSock);
		bool Update(
			mg::box::Error::Ptr& aOutErr);
		void Close();

		bool IsConnecting() const;
		bool IsConnected() const;
		bool IsClosed() const;

		void SendRef(
			const mg::net::Buffer* aHead);
		void SendRef(
			const void* aData,
			uint64_t aSize);
		void SendCopy(
			const void* aData,
			uint64_t aSize);

		int64_t Recv(
			mg::net::Buffer* aHead,
			mg::box::Error::Ptr& aOutErr);
		int64_t Recv(
			void* aData,
			uint64_t aSize,
			mg::box::Error::Ptr& aOutErr);

	private:
		bool PrivUpdateSend(
			mg::box::Error::Ptr& aOutErr);
		bool PrivUpdateConnect(
			mg::box::Error::Ptr& aOutErr);

		TCPSocketState myState;
		mg::net::Socket mySocket;
		mg::net::BufferLinkList myOutput;
		uint32_t mySendOffset;
	};

}
}

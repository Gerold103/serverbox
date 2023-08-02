#pragma once

#include "mg/net/Socket.h"

F_DECLARE_STRUCT(mg, box, IOVec)
F_DECLARE_CLASS(mg, net, Buffer)
F_DECLARE_CLASS(mg, net, BufferLinkList)

namespace mg {
namespace sio {

	mg::net::Socket SocketCreate(
		mg::net::SockAddrFamily aAddrFamily,
		mg::net::TransportProtocol aProtocol,
		mg::box::Error::Ptr& aOutErr);

	mg::net::Socket SocketAccept(
		mg::net::Socket aServer,
		mg::net::Host& aOutPeer,
		mg::box::Error::Ptr& aOutErr);

	bool SocketListen(
		mg::net::Socket aSock,
		uint32_t aBacklog,
		mg::box::Error::Ptr& aOutErr);

	bool SocketConnectStart(
		mg::net::Socket aSock,
		const mg::net::Host& aHost,
		mg::box::Error::Ptr& aOutErr);

	bool SocketConnectUpdate(
		mg::net::Socket aSock,
		mg::box::Error::Ptr& aOutErr);

	int64_t SocketSend(
		mg::net::Socket aSock,
		const void* aData,
		uint64_t aSize,
		mg::box::Error::Ptr& aOutErr);

	int64_t SocketSend(
		mg::net::Socket aSock,
		const mg::box::IOVec* aBuffers,
		uint32_t aBufferCount,
		mg::box::Error::Ptr& aOutErr);

	int64_t SocketSend(
		mg::net::Socket aSock,
		const mg::net::Buffer* aHead,
		uint32_t aByteOffset,
		mg::box::Error::Ptr& aOutErr);

	int64_t SocketSend(
		mg::net::Socket aSock,
		const mg::net::BufferLinkList& aList,
		uint32_t aByteOffset,
		mg::box::Error::Ptr& aOutErr);

	int64_t SocketRecv(
		mg::net::Socket aSock,
		void* aData,
		uint64_t aSize,
		mg::box::Error::Ptr& aOutErr);

	int64_t SocketRecv(
		mg::net::Socket aSock,
		mg::box::IOVec* aBuffers,
		uint32_t aBufferCount,
		mg::box::Error::Ptr& aOutErr);

	int64_t SocketRecv(
		mg::net::Socket aSock,
		mg::net::Buffer* aHead,
		mg::box::Error::Ptr& aOutErr);

}
}

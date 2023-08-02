#include "Socket.h"

#include "mg/box/IOVec.h"

#include "mg/net/Buffer.h"

namespace mg {
namespace sio {

	int64_t
	SocketSend(
		mg::net::Socket aSock,
		const void* aData,
		uint64_t aSize,
		mg::box::Error::Ptr& aOutErr)
	{
		mg::box::IOVec vec;
		vec.myData = (void*)aData;
		if (aSize <= UINT32_MAX)
			vec.mySize = (uint32_t)aSize;
		else
			vec.mySize = UINT32_MAX;
		return SocketSend(aSock, &vec, 1, aOutErr);
	}

	int64_t
	SocketSend(
		mg::net::Socket aSock,
		const mg::net::Buffer* aHead,
		uint32_t aByteOffset,
		mg::box::Error::Ptr& aOutErr)
	{
		mg::box::IOVec bufs[mg::box::theIOVecMaxCount];
		uint32_t count = mg::net::BuffersToIOVecsForWrite(
			aHead, aByteOffset, bufs, mg::box::theIOVecMaxCount);
		return SocketSend(aSock, bufs, count, aOutErr);
	}

	int64_t
	SocketSend(
		mg::net::Socket aSock,
		const mg::net::BufferLinkList& aList,
		uint32_t aByteOffset,
		mg::box::Error::Ptr& aOutErr)
	{
		mg::box::IOVec bufs[mg::box::theIOVecMaxCount];
		uint32_t count = mg::net::BuffersToIOVecsForWrite(
			aList, aByteOffset, bufs, mg::box::theIOVecMaxCount);
		return SocketSend(aSock, bufs, count, aOutErr);
	}

	int64_t
	SocketRecv(
		mg::net::Socket aSock,
		void* aData,
		uint64_t aSize,
		mg::box::Error::Ptr& aOutErr)
	{
		mg::box::IOVec vec;
		vec.myData = aData;
		if (aSize <= UINT32_MAX)
			vec.mySize = (uint32_t)aSize;
		else
			vec.mySize = UINT32_MAX;
		return SocketRecv(aSock, &vec, 1, aOutErr);
	}

	int64_t
	SocketRecv(
		mg::net::Socket aSock,
		mg::net::Buffer* aHead,
		mg::box::Error::Ptr& aOutErr)
	{
		mg::box::IOVec bufs[mg::box::theIOVecMaxCount];
		uint32_t count = mg::net::BuffersToIOVecsForRead(
			aHead, bufs, mg::box::theIOVecMaxCount);
		return SocketRecv(aSock, bufs, count, aOutErr);
	}

}
}

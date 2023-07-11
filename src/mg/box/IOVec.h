#pragma once

#include "mg/box/Definitions.h"

#if IS_PLATFORM_WIN
#include <winsock2.h>
#else
#include <sys/uio.h>
#endif

namespace mg {
namespace box {

	static constexpr uint32_t theIOVecMaxCount = 128;

#if IS_PLATFORM_WIN
	using IOVecNative = WSABUF;
#else
	using IOVecNative = iovec;
#endif
	//
	// A wrapper around platform-specific scatter/gather buffers used for vectorized IO.
	// The important part is that the wrapper is in fact an alias to the native vector
	// object. It allows not to spend time on converting these objects into the native
	// ones. This is achieved by using same members and types as the original platform
	// does.
	//
	struct IOVec
	{
#if IS_PLATFORM_WIN
		unsigned long mySize;
		void* myData;
#else
		void* myData;
		size_t mySize;
#endif
	};

	// These functions would be wrong to define as IOVec methods, because they usually
	// operate on IOVec arrays. They would work as methods, but would look strange to call
	// a method on a first array member to 'convert' all the members.

	static inline IOVecNative*
	IOVecToNative(
		const IOVec* aVec)
	{
		return (IOVecNative*)aVec;
	}

	void IOVecPropagate(
		IOVec*& aInOut,
		uint32_t& aInOutCount,
		uint32_t aByteOffset);

}
}

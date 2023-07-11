#pragma once

#include "mg/box/Definitions.h"

#if IS_PLATFORM_WIN
#include <winsock2.h>
#else
#include <sys/uio.h>
#endif

namespace mg {
namespace box {

#if IS_PLATFORM_WIN
	using IOVecNative = WSABUF;
#else
	using IOVecNative = iovec;
#endif

	// A wrapper around platform-specific scatter/gather buffers used for vectorized IO.
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
	// a method of a first array member to 'convert' them all.

	static inline IOVecNative*
	IOVecToNative(
		IOVec* aVec)
	{
		return (IOVecNative*)aVec;
	}

	static inline const IOVecNative*
	IOVecToNative(
		const IOVec* aVec)
	{
		return (const IOVecNative*)aVec;
	}

	void IOVecPropagate(
		IOVec*& aInOut,
		uint32_t& aInOutCount,
		uint32_t aByteOffset);

}
}

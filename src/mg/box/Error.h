#pragma once

#include "mg/box/RefCount.h"
#include "mg/box/SharedPtr.h"

#include <cerrno>
#include <string>

namespace mg {
namespace box {

#if IS_PLATFORM_WIN
	//
	// Windows errors are the ones returned by GetLastError()/WSAGetLastError().
	//

	std::string ErrorWinToString(
		int aCode);

	std::string ErrorWSAToString(
		int aCode);
#endif

	//
	// Errno errors are returned by all built-in functions on non-Windows platforms. On
	// Windows they can be returned from standard functions like open(), socket(), etc.
	// This needs to be determined per-case.
	//

	std::string ErrorErrnoToString(
		int aCode);

	// Basic errors, not specific to a service or just one library or one platform.
	//
	// They are split into groups. Each group has a range of values allocated to it. It is
	// done to simplify addition of new errors into existing groups without moving all the
	// next errors.
	//
	// Each group, assume ERR_MY_MODULE, has a generic error whose exact code usually
	// doesn't matter except for logging. It is named the same as the group -
	// ERR_MY_MODULE. Then there are detailed codes which provide some info already in
	// their name, ERR_MY_MODULE_NOT_FOUND, ERR_MY_MODULE_DUPLICATE, etc.
	//
	// It is ok to add here a new group whose module is implemented in another library.
	// For instance, there might be a group of SSL error codes, but their 'tostring' and
	// raising could be implemented in mg::net.
	//
	// The groups also have borders defined as constants so by a code it is easy to find
	// out which group it belongs to.
	//
	// It makes sense to add new errors into existing groups when you want to handle it
	// specifically, not just log. Or want to re-raise it in a platform-agnostic context.
	// Or just for better debug/readability.
	//
	enum ErrorCode : uint32_t
	{
		// The first group is for codes which don't fit any group
		// and won't ever have their own one.
					_ERR_COMMON_BEGIN		= 0,
		ERR_NONE							= _ERR_COMMON_BEGIN + 0,
		ERR_UNKNOWN							= _ERR_COMMON_BEGIN + 1,
		ERR_TIMEOUT							= _ERR_COMMON_BEGIN + 2,
		ERR_CANCEL							= _ERR_COMMON_BEGIN + 3,
					_ERR_COMMON_END,
					_ERR_SYS_BEGIN			= 1000,
		ERR_SYS								= _ERR_SYS_BEGIN + 0,
		ERR_SYS_FORBIDDEN					= _ERR_SYS_BEGIN + 1,
		ERR_SYS_BAD_DESCRIPTOR				= _ERR_SYS_BEGIN + 2,
		ERR_SYS_EXISTS						= _ERR_SYS_BEGIN + 3,
		ERR_SYS_TOO_MANY_DESCRIPTORS		= _ERR_SYS_BEGIN + 4,
		ERR_SYS_NOT_FOUND					= _ERR_SYS_BEGIN + 5,
		ERR_SYS_WOULDBLOCK					= _ERR_SYS_BEGIN + 6,
		ERR_SYS_BUSY						= _ERR_SYS_BEGIN + 7,
		ERR_SYS_IS_DIR						= _ERR_SYS_BEGIN + 8,
		ERR_SYS_BAD_ARG						= _ERR_SYS_BEGIN + 9,
		ERR_SYS_IO							= _ERR_SYS_BEGIN + 10,
		ERR_SYS_NAME_TOO_LONG				= _ERR_SYS_BEGIN + 11,
		ERR_SYS_NOT_DIR						= _ERR_SYS_BEGIN + 12,
		ERR_SYS_READ_ONLY_FS				= _ERR_SYS_BEGIN + 13,
		ERR_SYS_NO_SPACE					= _ERR_SYS_BEGIN + 14,
		ERR_SYS_BAD_ADDR					= _ERR_SYS_BEGIN + 15,
		ERR_SYS_NOT_SUPPORTED				= _ERR_SYS_BEGIN + 16,
		ERR_SYS_TOO_BIG						= _ERR_SYS_BEGIN + 17,
		ERR_SYS_TOO_MANY_LINKS				= _ERR_SYS_BEGIN + 18,
		ERR_SYS_NOT_EMPTY					= _ERR_SYS_BEGIN + 19,
		ERR_SYS_OVERFLOW					= _ERR_SYS_BEGIN + 20,
		ERR_SYS_LOOP						= _ERR_SYS_BEGIN + 21,
					_ERR_SYS_END,
					_ERR_NET_BEGIN			= 2000,
		ERR_NET								= _ERR_NET_BEGIN + 0,
		ERR_NET_CLOSE_BY_PEER				= _ERR_NET_BEGIN + 1,
		ERR_NET_ADDR_IN_USE					= _ERR_NET_BEGIN + 2,
		ERR_NET_ABORTED						= _ERR_NET_BEGIN + 3,
		ERR_NET_ADDR_NOT_AVAIL				= _ERR_NET_BEGIN + 4,
					_ERR_NET_END,
	};

#if IS_PLATFORM_WIN
	ErrorCode ErrorCodeWinGetLast();

	ErrorCode ErrorCodeFromWin(
		uint32_t aWinCode);

	ErrorCode ErrorCodeWSAGetLast();

	ErrorCode ErrorCodeFromWSA(
		uint32_t aWSACode);
#endif

	ErrorCode ErrorCodeErrno();

	ErrorCode ErrorCodeFromErrno(
		uint32_t aSysCode);

	const char* ErrorCodeMessage(
		ErrorCode aCode);

	const char* ErrorCodeTag(
		ErrorCode aCode);

	const char* ErrorCodeType(
		ErrorCode aCode);

	class Error
	{
		SHARED_PTR_API(Error, myRef)
	private:
		// Only NewShared() is allowed.
		Error();
		Error(
			ErrorCode aCode,
			uint32_t aValue,
			const char* aMessage);
		~Error() = default;

	public:
		ErrorCode myCode;
		uint32_t myValue;
		std::string myMessage;

	private:
		mg::box::RefCount myRef;
	};

	// Special wrapper for error shared pointer to allow the implicit cast to a normal
	// pointer. To simplify the most frequent usage:
	//
	//     PrivOnError(ErrorRaise(mg::box::ERR_MY_CODE));
	//
	struct ErrorPtrRaised
	{
		ErrorPtrRaised(
			Error::Ptr&& aError);
		ErrorPtrRaised(
			ErrorPtrRaised&& aSrc);
		ErrorPtrRaised(
			const ErrorPtrRaised&) = delete;

		operator Error::Ptr&&() { return std::move(myError); }
		operator Error*() { return myError.GetPointer(); }
		operator const Error*() const { return myError.GetPointer(); }
		Error* operator->() { return myError.GetPointer(); }
		const Error* operator->() const { return myError.GetPointer(); }

		Error::Ptr myError;
	};

	//////////////////////////////////////////////////////////////////////////////////////
#if IS_PLATFORM_WIN

	// GetLastError() + built-in message.
	ErrorPtrRaised ErrorRaiseWin();

	// Existing built-in error + built-in message.
	ErrorPtrRaised ErrorRaiseWin(
		uint32_t aValue);

	// Existing built-in error + a comment.
	ErrorPtrRaised ErrorRaiseWin(
		uint32_t aValue,
		const char* aComment);

	// GetLastError() + a comment.
	ErrorPtrRaised ErrorRaiseWin(
		const char* aComment);

	// Existing built-in error + a comment format.
	MG_STRFORMAT_PRINTF(2, 3)
	ErrorPtrRaised ErrorRaiseFormatWin(
		uint32_t aValue,
		const char* aCommentFormat,
		...);

	// GetLastError() + a comment format.
	MG_STRFORMAT_PRINTF(1, 2)
	ErrorPtrRaised ErrorRaiseFormatWin(
		const char* aCommentFormat,
		...);

	//////////////////////////////////////////////////////////////////////////////////////

	// WSAGetLastError() + built-in message.
	ErrorPtrRaised ErrorRaiseWSA();

	// Existing built-in error + built-in message.
	ErrorPtrRaised ErrorRaiseWSA(
		uint32_t aValue);

	// Existing built-in error + a comment.
	ErrorPtrRaised ErrorRaiseWSA(
		uint32_t aValue,
		const char* aComment);

	// WSAGetLastError() + a comment.
	ErrorPtrRaised ErrorRaiseWSA(
		const char* aComment);

	// Existing built-in error + a comment format.
	MG_STRFORMAT_PRINTF(2, 3)
	ErrorPtrRaised ErrorRaiseFormatWSA(
		uint32_t aValue,
		const char* aCommentFormat,
		...);

	// WSAGetLastError() + a comment format.
	MG_STRFORMAT_PRINTF(1, 2)
	ErrorPtrRaised ErrorRaiseFormatWSA(
		const char* aCommentFormat,
		...);

#endif
	//////////////////////////////////////////////////////////////////////////////////////

	// Errno + built-in message.
	ErrorPtrRaised ErrorRaiseErrno();

	// Existing errno + built-in message.
	ErrorPtrRaised ErrorRaiseErrno(
		uint32_t aValue);

	// Existing errno + a comment.
	ErrorPtrRaised ErrorRaiseErrno(
		uint32_t aValue,
		const char* aComment);

	// Errno + a comment.
	ErrorPtrRaised ErrorRaiseErrno(
		const char* aComment);

	// Existing errno + a comment format.
	MG_STRFORMAT_PRINTF(2, 3)
	ErrorPtrRaised ErrorRaiseFormatErrno(
		uint32_t aValue,
		const char* aCommentFormat,
		...);

	// Errno + a comment format.
	MG_STRFORMAT_PRINTF(1, 2)
	ErrorPtrRaised ErrorRaiseFormatErrno(
		const char* aCommentFormat,
		...);

	//////////////////////////////////////////////////////////////////////////////////////

	// Existing code + its default message.
	ErrorPtrRaised ErrorRaise(
		ErrorCode aCode);

	// Existing code + a detailed value + its default message. Useful when use a generic
	// code, but precise value. Even without a good message it will be helpful in the
	// logs.
	ErrorPtrRaised ErrorRaise(
		ErrorCode aCode,
		uint32_t aValue);

	// Existing code + a comment.
	ErrorPtrRaised ErrorRaise(
		ErrorCode aCode,
		const char* aComment);

	// Existing code + a comment format.
	MG_STRFORMAT_PRINTF(2, 3)
	ErrorPtrRaised ErrorRaiseFormat(
		ErrorCode aCode,
		const char* aCommentFormat,
		...);

	// Existing code + a detailed value + a comment.
	ErrorPtrRaised ErrorRaise(
		ErrorCode aCode,
		uint32_t aValue,
		const char* aComment);

	// Existing code + a detailed value + a comment format.
	MG_STRFORMAT_PRINTF(3, 4)
	ErrorPtrRaised ErrorRaiseFormat(
		ErrorCode aCode,
		uint32_t aValue,
		const char* aCommentFormat,
		...);

	// Most customizable version.
	ErrorPtrRaised ErrorRaise(
		ErrorCode aCode,
		uint32_t aValue,
		const char* aMessage,
		const char* aComment);

	//////////////////////////////////////////////////////////////////////////////////////

	// Helper to preserve the original error code upon leaving the current scope. Helpful
	// when after an error happened need to make cleanup/logging actions before returning
	// 'false' to the caller. These actions should not spoil the original error.
	class ErrorGuard
	{
	public:
		inline
		ErrorGuard()
			: myErrno(errno)
#if IS_PLATFORM_WIN
			, myWinCode((uint32_t)GetLastError())
#endif
		{
		}

		inline
		~ErrorGuard()
		{
			errno = (int)myErrno;
#if IS_PLATFORM_WIN
			SetLastError((int)myWinCode);
#endif
		}

		uint32_t myErrno;
		uint32_t myWinCode;
	};

	//////////////////////////////////////////////////////////////////////////////////////

	inline
	ErrorPtrRaised::ErrorPtrRaised(
		Error::Ptr&& aError)
		: myError(std::move(aError))
	{
	}

	inline
	ErrorPtrRaised::ErrorPtrRaised(
		ErrorPtrRaised&& aSrc)
		: myError(std::move(aSrc.myError))
	{
	}

}
}

#pragma once

#include "mg/box/SharedPtr.h"

#include <cerrno>
#include <string>

namespace mg {
namespace box {

	// Platform-agnostic error codes.
	//
	// The codes are split into groups. Each group has an allocated range of values, to
	// simplify addition of new errors into existing groups without moving all the next
	// errors. To keep related error codes close.
	//
	// Each group, assume ERR_MY_MODULE, has a generic error whose exact code usually
	// doesn't matter except for logging. It is named the same as the group -
	// ERR_MY_MODULE. Then there are detailed codes which provide some info in their name,
	// ERR_MY_MODULE_NOT_FOUND, ERR_MY_MODULE_DUPLICATE, etc.
	//
	// It is ok to add here a new group whose module is implemented in another library.
	// For instance, there might be a group of SSL error codes, but their 'tostring' and
	// raising could be implemented in mg::net.
	//
	// The groups also have borders defined as constants so by a code it is easy to find
	// out which group it belongs to.
	//
	// Reasons to add new errors into existing groups:
	// - Want to handle this new specific error, not just log it.
	// - Want to re-raise it in a platform-agnostic context.
	// - For better debug/readability.
	//
	// Reasons to add new error groups:
	// - A new generic module is implemented and is going to raise new errors, not just
	//     forward the ones received from a lower level.
	//
	enum ErrorCode : uint32_t
	{
		// The first group is for codes which don't fit any group and won't ever have
		// their own one.
					_ERR_BOX_BEGIN		= 0,
		ERR_BOX_NONE						= _ERR_BOX_BEGIN + 0,
		ERR_BOX_UNKNOWN						= _ERR_BOX_BEGIN + 1,
					_ERR_BOX_END,
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
	};

	//////////////////////////////////////////////////////////////////////////////////////

#if IS_PLATFORM_WIN
	std::string ErrorWinToString(
		int aCode);

	ErrorCode ErrorCodeWin();

	ErrorCode ErrorCodeFromWin(
		uint32_t aWinCode);
#endif

	//////////////////////////////////////////////////////////////////////////////////////

	std::string ErrorErrnoToString(
		int aCode);

	ErrorCode ErrorCodeErrno();

	ErrorCode ErrorCodeFromErrno(
		uint32_t aSysCode);

	//////////////////////////////////////////////////////////////////////////////////////

	const char* ErrorCodeMessage(
		ErrorCode aCode);

	const char* ErrorCodeTag(
		ErrorCode aCode);

	const char* ErrorCodeType(
		ErrorCode aCode);

	//////////////////////////////////////////////////////////////////////////////////////

	class Error
	{
		SHARED_PTR_API(Error, myRef)
	private:
		Error() : Error(ERR_BOX_UNKNOWN, 0, "{empty}") {}
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

	//////////////////////////////////////////////////////////////////////////////////////

	// Special wrapper for error shared pointer to allow the implicit cast to a normal
	// pointer. To simplify the most frequent usage:
	//
	//     OnError(ErrorRaise(mg::box::ERR_MY_CODE));
	//
	struct ErrorPtrRaised
	{
		ErrorPtrRaised(
			Error::Ptr&& aError) : myError(std::move(aError)) {}
		ErrorPtrRaised(
			ErrorPtrRaised&& aSrc) : myError(std::move(aSrc.myError)) {}
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

	// GetLastError() + the system message.
	ErrorPtrRaised ErrorRaiseWin();

	// Given code + the system message.
	ErrorPtrRaised ErrorRaiseWin(
		uint32_t aValue);

	// Given code + a comment.
	ErrorPtrRaised ErrorRaiseWin(
		uint32_t aValue,
		const char* aComment);

	// GetLastError() + a comment.
	ErrorPtrRaised ErrorRaiseWin(
		const char* aComment);

	// Given code + a comment format.
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

#endif
	//////////////////////////////////////////////////////////////////////////////////////

	// Errno + the system message.
	ErrorPtrRaised ErrorRaiseErrno();

	// Given code + the system message.
	ErrorPtrRaised ErrorRaiseErrno(
		uint32_t aValue);

	// Given code + a comment.
	ErrorPtrRaised ErrorRaiseErrno(
		uint32_t aValue,
		const char* aComment);

	// Errno + a comment.
	ErrorPtrRaised ErrorRaiseErrno(
		const char* aComment);

	// Given code + a comment format.
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

	// Given code + its default message.
	ErrorPtrRaised ErrorRaise(
		ErrorCode aCode);

	// Given code + a low-level value + its default message. Useful when use a generic
	// code, but precise value. Even without a good message it will be helpful in the
	// logs.
	ErrorPtrRaised ErrorRaise(
		ErrorCode aCode,
		uint32_t aValue);

	// Given code + a comment.
	ErrorPtrRaised ErrorRaise(
		ErrorCode aCode,
		const char* aComment);

	// Given code + a comment format.
	MG_STRFORMAT_PRINTF(2, 3)
	ErrorPtrRaised ErrorRaiseFormat(
		ErrorCode aCode,
		const char* aCommentFormat,
		...);

	// Given code + a detailed value + a comment.
	ErrorPtrRaised ErrorRaise(
		ErrorCode aCode,
		uint32_t aValue,
		const char* aComment);

	// Given code + a detailed value + a comment format.
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

	ErrorPtrRaised ErrorRaise(
		const Error* aCause,
		const char* aComment);

	ErrorPtrRaised ErrorRaise(
		const Error::Ptr& aCause,
		const char* aComment);

	//////////////////////////////////////////////////////////////////////////////////////

	// Helper to preserve the original error code upon leaving the current scope. Helpful
	// when after an error happened need to make cleanup/logging actions before returning
	// 'false' to the caller. These actions should not spoil the original error.
	class ErrorGuard
	{
	public:
		ErrorGuard()
			: myErrno(errno)
#if IS_PLATFORM_WIN
			, myWinCode((uint32_t)GetLastError())
#endif
		{
		}

		~ErrorGuard()
		{
			errno = (int)myErrno;
#if IS_PLATFORM_WIN
			SetLastError((DWORD)myWinCode);
#endif
		}

		uint32_t myErrno;
#if IS_PLATFORM_WIN
		uint32_t myWinCode;
#endif
	};

}
}

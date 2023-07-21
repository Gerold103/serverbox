#include "Error.h"

#include "mg/box/StringFunctions.h"

namespace mg {
namespace box {

	enum ErrorFormatFlags
	{
		ERROR_FORMAT_EMPTY = 0,
		// Message is prepended with the error's numeric code.
		ERROR_FORMAT_ADD_CODE = 1 << 0,
	};

	struct ErrorDef
	{
		const char* myTag;
		const char* myMsg;
	};

	static const ErrorDef&
	ErrorGetDef(
		ErrorCode aCode)
	{
#define ERROR_DEF_MAKE(tag, msg)															\
{																							\
	static const ErrorDef def{tag, msg};													\
	return def;																				\
}
		switch(aCode)
		{
		//////////////////////////////////////////////////////////////////////////////////
		case ERR_BOX_NONE:
			ERROR_DEF_MAKE(
				"box_none", "box none");
		case ERR_BOX_UNKNOWN:
			ERROR_DEF_MAKE(
				"error_box_unknown", "box unknown error");
		//////////////////////////////////////////////////////////////////////////////////
		case ERR_SYS:
			ERROR_DEF_MAKE(
				"error_sys", "sys error");
		case ERR_SYS_FORBIDDEN:
			ERROR_DEF_MAKE(
				"error_sys_forbidden", "sys forbidden");
		case ERR_SYS_BAD_DESCRIPTOR:
			ERROR_DEF_MAKE(
				"error_sys_bad_descriptor", "sys bad descriptor");
		case ERR_SYS_EXISTS:
			ERROR_DEF_MAKE(
				"error_sys_exists", "sys exists");
		case ERR_SYS_TOO_MANY_DESCRIPTORS:
			ERROR_DEF_MAKE(
				"error_sys_too_many_descriptors", "sys too many descriptors");
		case ERR_SYS_NOT_FOUND:
			ERROR_DEF_MAKE(
				"error_sys_not_found", "sys not found");
		case ERR_SYS_WOULDBLOCK:
			ERROR_DEF_MAKE(
				"error_sys_wouldblock", "sys wouldblock");
		case ERR_SYS_BUSY:
			ERROR_DEF_MAKE(
				"error_sys_busy", "sys busy");
		case ERR_SYS_IS_DIR:
			ERROR_DEF_MAKE(
				"error_sys_is_dir", "sys is dir");
		case ERR_SYS_BAD_ARG:
			ERROR_DEF_MAKE(
				"error_sys_bad_arg", "sys bad arg");
		case ERR_SYS_IO:
			ERROR_DEF_MAKE(
				"error_sys_io", "sys io");
		case ERR_SYS_NAME_TOO_LONG:
			ERROR_DEF_MAKE(
				"error_sys_name_too_long", "sys name too long");
		case ERR_SYS_NOT_DIR:
			ERROR_DEF_MAKE(
				"error_sys_not_dir", "sys not dir");
		case ERR_SYS_READ_ONLY_FS:
			ERROR_DEF_MAKE(
				"error_sys_ro_fs", "sys ro fs");
		case ERR_SYS_NO_SPACE:
			ERROR_DEF_MAKE(
				"error_sys_no_space", "sys no space");
		case ERR_SYS_BAD_ADDR:
			ERROR_DEF_MAKE(
				"error_sys_bad_addr", "sys bad addr");
		case ERR_SYS_NOT_SUPPORTED:
			ERROR_DEF_MAKE(
				"error_sys_not_supported", "sys not supported");
		case ERR_SYS_TOO_BIG:
			ERROR_DEF_MAKE(
				"error_sys_too_big", "sys too big");
		case ERR_SYS_TOO_MANY_LINKS:
			ERROR_DEF_MAKE(
				"error_sys_too_many_links", "sys too many links");
		case ERR_SYS_NOT_EMPTY:
			ERROR_DEF_MAKE(
				"error_sys_not_empty", "sys not empty");
		case ERR_SYS_OVERFLOW:
			ERROR_DEF_MAKE(
				"error_sys_overflow", "sys overflow");
		case ERR_SYS_LOOP:
			ERROR_DEF_MAKE(
				"error_sys_loop", "sys loop");
		//////////////////////////////////////////////////////////////////////////////////
		case _ERR_BOX_END:
		case _ERR_SYS_END:
		default:
			MG_BOX_ASSERT(!"Unknown error code");
			ERROR_DEF_MAKE(nullptr, nullptr);
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////
#if IS_PLATFORM_WIN

	static std::string
	ErrorWinToString(
		int aCode,
		int aFormat)
	{
		// Use English. Other language might not fit into ASCII, and won't be properly
		// printed/logged.
		DWORD language = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
		DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER;
		char* buf;
		int rc = FormatMessageA(flags, nullptr, aCode, language, (LPSTR)&buf, 0, nullptr);
		if (rc != 0)
		{
			std::string res;
			if ((aFormat & ERROR_FORMAT_ADD_CODE) != 0)
				res = mg::box::StringFormat("(%d) ", aCode);
			res += mg::box::StringFormat("%.*s", rc, buf);
			mg::box::StringTrim(res);
			LocalFree(buf);
			return res;
		}
		return mg::box::StringFormat("Failed to get error message for code %d due to %d",
			aCode, GetLastError());
	}

	std::string
	ErrorWinToString(
		int aCode)
	{
		return ErrorWinToString(aCode, ERROR_FORMAT_ADD_CODE);
	}

	ErrorCode
	ErrorCodeWin()
	{
		return ErrorCodeFromWin(GetLastError());
	}

	ErrorCode
	ErrorCodeFromWin(
		uint32_t aWinCode)
	{
		switch (aWinCode)
		{
		case ERROR_ACCESS_DENIED:
			return ERR_SYS_FORBIDDEN;
		case ERROR_INVALID_HANDLE:
			return ERR_SYS_BAD_DESCRIPTOR;
		case ERROR_FILE_EXISTS:
		case ERROR_ALREADY_EXISTS:
		case ERROR_OBJECT_NAME_EXISTS:
			return ERR_SYS_EXISTS;
		case ERROR_NOT_FOUND:
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:
			return ERR_SYS_NOT_FOUND;
		case ERROR_BUSY:
		case ERROR_NETWORK_BUSY:
		case ERROR_PATH_BUSY:
			return ERR_SYS_BUSY;
		case ERROR_BAD_ARGUMENTS:
		case ERROR_NOT_A_REPARSE_POINT:
			return ERR_SYS_BAD_ARG;
		default:
			return ERR_SYS;
		}
	}

#endif
	//////////////////////////////////////////////////////////////////////////////////////

	static std::string
	ErrorErrnoToString(
		int aCode,
		int aFormat)
	{
		std::string res;
		if ((aFormat & ERROR_FORMAT_ADD_CODE) != 0)
			res = mg::box::StringFormat("(%d) ", aCode);
		res += mg::box::StringFormat("%s", strerror(aCode));
		return res;
	}

	std::string
	ErrorErrnoToString(
		int aCode)
	{
		return ErrorErrnoToString(aCode, ERROR_FORMAT_ADD_CODE);
	}

	ErrorCode
	ErrorCodeErrno()
	{
		return ErrorCodeFromErrno(errno);
	}

	ErrorCode
	ErrorCodeFromErrno(
		uint32_t aSysCode)
	{
		switch (aSysCode)
		{
		case EACCES:
		case EPERM:
			return ERR_SYS_FORBIDDEN;
		case EBADF:
			return ERR_SYS_BAD_DESCRIPTOR;
		case EEXIST:
			return ERR_SYS_EXISTS;
		case EMFILE:
		case ENFILE:
			return ERR_SYS_TOO_MANY_DESCRIPTORS;
		case ENOENT:
			return ERR_SYS_NOT_FOUND;
		case EWOULDBLOCK:
			return ERR_SYS_WOULDBLOCK;
		case EBUSY:
			return ERR_SYS_BUSY;
		case EISDIR:
			return ERR_SYS_IS_DIR;
		case EINVAL:
			return ERR_SYS_BAD_ARG;
		case EIO:
			return ERR_SYS_IO;
		case ENAMETOOLONG:
			return ERR_SYS_NAME_TOO_LONG;
		case ENOTDIR:
			return ERR_SYS_NOT_DIR;
		case EROFS:
			return ERR_SYS_READ_ONLY_FS;
		case ENOSPC:
		case ENOMEM:
			return ERR_SYS_NO_SPACE;
		case EFAULT:
			return ERR_SYS_BAD_ADDR;
		case EOPNOTSUPP:
			return ERR_SYS_NOT_SUPPORTED;
		case EFBIG:
			return ERR_SYS_TOO_BIG;
		case EMLINK:
			return ERR_SYS_TOO_MANY_LINKS;
		case ENOTEMPTY:
			return ERR_SYS_NOT_EMPTY;
		case EOVERFLOW:
			return ERR_SYS_OVERFLOW;
		case ELOOP:
			return ERR_SYS_LOOP;
#if !IS_PLATFORM_WIN
		case ESTALE:
			return ERR_SYS_BAD_DESCRIPTOR;
#endif
		default:
			// Can't handle it in the switch, because on some platforms it can be ==
			// EWOULDBLOCK and that breaks the switch compilation.
			if (aSysCode == EAGAIN)
				return ERR_SYS_WOULDBLOCK;
			// It can be that ENOTSUP == EOPNOTSUPP, can't handle in the same switch.
			if (aSysCode == ENOTSUP)
				return ERR_SYS_NOT_SUPPORTED;
			return ERR_SYS;
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////

	const char*
	ErrorCodeMessage(
		ErrorCode aCode)
	{
		return ErrorGetDef(aCode).myMsg;
	}

	const char*
	ErrorCodeTag(
		ErrorCode aCode)
	{
		return ErrorGetDef(aCode).myTag;
	}

	const char*
	ErrorCodeType(
		ErrorCode aCode)
	{
#define IS_ERROR_TYPE(Code, Name)															\
	((Code) >= _ERR_##Name##_BEGIN && (Code) < _ERR_##Name##_END)

#if IS_COMPILER_MSVC
#pragma warning(push)
// "An unsigned variable was used in a comparison operation with zero."
#pragma warning(disable: 4296)
#endif

		if (IS_ERROR_TYPE(aCode, BOX))
			return "box";
		if (IS_ERROR_TYPE(aCode, SYS))
			return "sys";
		return "unknown";

#if IS_COMPILER_MSVC
#pragma warning(pop)
#endif

#undef IS_ERROR_TYPE
	}

	//////////////////////////////////////////////////////////////////////////////////////

	Error::Error(
		ErrorCode aCode,
		uint32_t aValue,
		const char* aMessage)
		: myCode(aCode)
		, myValue(aValue)
		, myMessage(aMessage)
	{
	}

#define ERROR_STRING_FROM_FORMAT(Result, FormatArgName) do {								\
	va_list va;																				\
	va_start(va, FormatArgName);															\
	(Result) = mg::box::StringVFormat(FormatArgName, va);									\
	va_end(va);																				\
} while (0)

	//////////////////////////////////////////////////////////////////////////////////////
#if IS_PLATFORM_WIN

	ErrorPtrRaised
	ErrorRaiseWin()
	{
		return ErrorRaiseWin(GetLastError());
	}

	ErrorPtrRaised
	ErrorRaiseWin(
		uint32_t aValue)
	{
		return ErrorRaiseWin(aValue, nullptr);
	}

	ErrorPtrRaised
	ErrorRaiseWin(
		uint32_t aValue,
		const char* aComment)
	{
		return ErrorRaise(ErrorCodeFromWin(aValue), aValue,
			ErrorWinToString(aValue, ERROR_FORMAT_EMPTY).c_str(), aComment);
	}

	ErrorPtrRaised
	ErrorRaiseWin(
		const char* aComment)
	{
		return ErrorRaiseWin(GetLastError(), aComment);
	}

	MG_STRFORMAT_PRINTF(2, 3)
	ErrorPtrRaised
	ErrorRaiseFormatWin(
		uint32_t aValue,
		const char* aCommentFormat,
		...)
	{
		std::string comment;
		ERROR_STRING_FROM_FORMAT(comment, aCommentFormat);
		return ErrorRaiseWin(aValue, comment.c_str());
	}

	MG_STRFORMAT_PRINTF(1, 2)
	ErrorPtrRaised
	ErrorRaiseFormatWin(
		const char* aCommentFormat,
		...)
	{
		std::string comment;
		ERROR_STRING_FROM_FORMAT(comment, aCommentFormat);
		return ErrorRaiseWin(GetLastError(), comment.c_str());
	}

#endif
	//////////////////////////////////////////////////////////////////////////////////////

	ErrorPtrRaised
	ErrorRaiseErrno()
	{
		return ErrorRaiseErrno(errno);
	}

	ErrorPtrRaised
	ErrorRaiseErrno(
		uint32_t aValue)
	{
		return ErrorRaiseErrno(aValue, nullptr);
	}

	ErrorPtrRaised
	ErrorRaiseErrno(
		uint32_t aValue,
		const char* aComment)
	{
		return ErrorRaise(ErrorCodeFromErrno(aValue), aValue,
			ErrorErrnoToString(aValue, ERROR_FORMAT_EMPTY).c_str(), aComment);
	}

	ErrorPtrRaised
	ErrorRaiseErrno(
		const char* aComment)
	{
		return ErrorRaiseErrno(errno, aComment);
	}

	MG_STRFORMAT_PRINTF(2, 3)
	ErrorPtrRaised
	ErrorRaiseFormatErrno(
		uint32_t aValue,
		const char* aCommentFormat,
		...)
	{
		std::string comment;
		ERROR_STRING_FROM_FORMAT(comment, aCommentFormat);
		return ErrorRaiseErrno(aValue, comment.c_str());
	}

	MG_STRFORMAT_PRINTF(1, 2)
	ErrorPtrRaised
	ErrorRaiseFormatErrno(
		const char* aCommentFormat,
		...)
	{
		std::string comment;
		ERROR_STRING_FROM_FORMAT(comment, aCommentFormat);
		return ErrorRaiseErrno(errno, comment.c_str());
	}

	//////////////////////////////////////////////////////////////////////////////////////

	ErrorPtrRaised
	ErrorRaise(
		ErrorCode aCode)
	{
		return ErrorRaise(aCode, 0U);
	}

	ErrorPtrRaised
	ErrorRaise(
		ErrorCode aCode,
		uint32_t aValue)
	{
		return ErrorRaise(aCode, aValue, nullptr);
	}

	ErrorPtrRaised
	ErrorRaise(
		ErrorCode aCode,
		const char* aComment)
	{
		return ErrorRaise(aCode, 0, aComment);
	}

	MG_STRFORMAT_PRINTF(2, 3)
	ErrorPtrRaised
	ErrorRaiseFormat(
		ErrorCode aCode,
		const char* aCommentFormat,
		...)
	{
		std::string comment;
		ERROR_STRING_FROM_FORMAT(comment, aCommentFormat);
		return ErrorRaise(aCode, 0, comment.c_str());
	}

	ErrorPtrRaised
	ErrorRaise(
		ErrorCode aCode,
		uint32_t aValue,
		const char* aComment)
	{
		return ErrorRaise(aCode, aValue, ErrorCodeMessage(aCode), aComment);
	}

	MG_STRFORMAT_PRINTF(3, 4)
	ErrorPtrRaised
	ErrorRaiseFormat(
		ErrorCode aCode,
		uint32_t aValue,
		const char* aCommentFormat,
		...)
	{
		std::string comment;
		ERROR_STRING_FROM_FORMAT(comment, aCommentFormat);
		return ErrorRaise(aCode, aValue, comment.c_str());
	}

#undef ERROR_STRING_FROM_FORMAT

	ErrorPtrRaised
	ErrorRaise(
		ErrorCode aCode,
		uint32_t aValue,
		const char* aMessage,
		const char* aComment)
	{
		const char* delimiter = ", ";
		std::string message = mg::box::StringFormat(
			"{code=%u%stype=\"%s\"", (uint32_t)aCode, delimiter, ErrorCodeType(aCode));
		if (aValue != 0)
			message += mg::box::StringFormat("%sval=%d", delimiter, aValue);
		MG_DEV_ASSERT(aMessage != nullptr);
		message += mg::box::StringFormat("%smsg=\"%s\"", delimiter, aMessage);
		if (aComment != nullptr)
			message += mg::box::StringFormat("%scomm=\"%s\"", delimiter, aComment);
		message += '}';

		Error::Ptr res = Error::NewShared();
		res->myCode = aCode;
		res->myValue = aValue;
		res->myMessage = std::move(message);
		return ErrorPtrRaised(std::move(res));
	}

	//////////////////////////////////////////////////////////////////////////////////////

	ErrorPtrRaised
	ErrorRaise(
		const Error* aCause,
		const char* aComment)
	{
		Error::Ptr res = Error::NewShared();
		res->myCode = aCause->myCode;
		res->myValue = aCause->myValue;
		res->myMessage = mg::box::StringFormat("{comm=\"%s\", cause=%s}",
			aComment, aCause->myMessage.c_str());
		return ErrorPtrRaised(std::move(res));
	}

	ErrorPtrRaised
	ErrorRaise(
		const Error::Ptr& aCause,
		const char* aComment)
	{
		return ErrorRaise(aCause.GetPointer(), aComment);
	}

}
}

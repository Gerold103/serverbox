#include "SSL.h"

#include "mg/box/StringFunctions.h"
#include "mg/net/SSLOpenSSL.h"

#include <cstdarg>

namespace mg {
namespace net {

	const char*
	SSLErrorToString(
		uint32_t aCode)
	{
		return OpenSSLErrorToString(aCode);
	}

	mg::box::ErrorPtrRaised
	ErrorRaiseSSL(
		uint32_t aCode)
	{
		return ErrorRaiseSSL(aCode, nullptr);
	}

	mg::box::ErrorPtrRaised
	ErrorRaiseSSL(
		uint32_t aCode,
		const char* aComment)
	{
		return ErrorRaise(mg::box::ERR_SSL, aCode, SSLErrorToString(aCode), aComment);
	}

	MG_STRFORMAT_PRINTF(2, 3)
	mg::box::ErrorPtrRaised
	ErrorRaiseFormatSSL(
		uint32_t aCode,
		const char* aCommentFormat,
		...)
	{
		va_list va;
		va_start(va, aCommentFormat);
		std::string comment = mg::box::StringVFormat(aCommentFormat, va);
		va_end(va);
		return ErrorRaiseSSL(aCode, comment.c_str());
	}

}
}
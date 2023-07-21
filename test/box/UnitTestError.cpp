#include "mg/box/Error.h"

#include "mg/box/StringFunctions.h"

#include "UnitTest.h"

namespace mg {
namespace unittests {
namespace box {
namespace error {

	static void
	UnitTestErrorWinToString()
	{
#if IS_PLATFORM_WIN
		TestCaseGuard guard("ErrorWinToString()");

		TEST_CHECK(mg::box::ErrorWinToString(ERROR_ACCESS_DENIED) ==
			"(5) Access is denied.");
		TEST_CHECK(mg::box::ErrorWinToString(ERROR_ALREADY_EXISTS) ==
			"(183) Cannot create a file when that file already exists.");
#endif
	}

	static void
	UnitTestErrorCodeWin()
	{
#if IS_PLATFORM_WIN
		TestCaseGuard guard("ErrorCodeWin()");

		SetLastError(ERROR_ACCESS_DENIED);
		TEST_CHECK(mg::box::ErrorCodeWin() == mg::box::ERR_SYS_FORBIDDEN);
		SetLastError(ERROR_ALREADY_EXISTS);
		TEST_CHECK(mg::box::ErrorCodeWin() == mg::box::ERR_SYS_EXISTS);
#endif
	}

	static void
	UnitTestErrorCodeFromWin()
	{
#if IS_PLATFORM_WIN
		TestCaseGuard guard("ErrorCodeFromWin()");

		SetLastError(ERROR_PATH_BUSY);
		TEST_CHECK(mg::box::ErrorCodeFromWin(ERROR_ACCESS_DENIED) == mg::box::ERR_SYS_FORBIDDEN);
		SetLastError(ERROR_PATH_BUSY);
		TEST_CHECK(mg::box::ErrorCodeFromWin(ERROR_ALREADY_EXISTS) == mg::box::ERR_SYS_EXISTS);
#endif
	}

	static void
	UnitTestErrorErrnoToString()
	{
		TestCaseGuard guard("ErrorErrnoToString()");

		TEST_CHECK(mg::box::ErrorErrnoToString(ENOENT) == "(2) No such file or directory");
		TEST_CHECK(mg::box::ErrorErrnoToString(EACCES) == "(13) Permission denied");
	}

	static void
	UnitTestErrorErrorCodeErrno()
	{
		TestCaseGuard guard("ErrorCodeErrno()");

		errno = ENOENT;
		TEST_CHECK(mg::box::ErrorCodeErrno() == mg::box::ERR_SYS_NOT_FOUND);
		errno = EACCES;
		TEST_CHECK(mg::box::ErrorCodeErrno() == mg::box::ERR_SYS_FORBIDDEN);
	}

	static void
	UnitTestErrorErrorCodeFromErrno()
	{
		TestCaseGuard guard("ErrorCodeFromErrno()");

		TEST_CHECK(mg::box::ErrorCodeFromErrno(ENOENT) == mg::box::ERR_SYS_NOT_FOUND);
		TEST_CHECK(mg::box::ErrorCodeFromErrno(EACCES) == mg::box::ERR_SYS_FORBIDDEN);
	}

	static void
	UnitTestErrorErrorCodeMessage()
	{
		TestCaseGuard guard("ErrorCodeMessage()");

		TEST_CHECK(mg::box::Strcmp(
			mg::box::ErrorCodeMessage(mg::box::ERR_SYS_NOT_FOUND),
			"sys not found") == 0);
		TEST_CHECK(mg::box::Strcmp(
			mg::box::ErrorCodeMessage(mg::box::ERR_SYS_FORBIDDEN),
			"sys forbidden") == 0);
	}

	static void
	UnitTestErrorErrorCodeTag()
	{
		TestCaseGuard guard("ErrorCodeTag()");

		TEST_CHECK(mg::box::Strcmp(
			mg::box::ErrorCodeTag(mg::box::ERR_SYS_NOT_FOUND),
			"error_sys_not_found") == 0);
		TEST_CHECK(mg::box::Strcmp(
			mg::box::ErrorCodeTag(mg::box::ERR_SYS_FORBIDDEN),
			"error_sys_forbidden") == 0);
	}

	static void
	UnitTestErrorErrorCodeType()
	{
		TestCaseGuard guard("ErrorCodeType()");

		TEST_CHECK(mg::box::Strcmp(
			mg::box::ErrorCodeType(mg::box::ERR_SYS_NOT_FOUND),
			"sys") == 0);
		TEST_CHECK(mg::box::Strcmp(
			mg::box::ErrorCodeType(mg::box::ERR_SYS_FORBIDDEN),
			"sys") == 0);
		TEST_CHECK(mg::box::Strcmp(
			mg::box::ErrorCodeType(mg::box::ERR_BOX_UNKNOWN),
			"box") == 0);
	}

	static void
	UnitTestErrorError()
	{
		TestCaseGuard guard("Error object");

		mg::box::Error::Ptr err = mg::box::Error::NewShared();
		TEST_CHECK(err->myCode == mg::box::ERR_BOX_UNKNOWN);
		TEST_CHECK(err->myValue == 0);
		TEST_CHECK(err->myMessage == "{empty}");

		err = mg::box::Error::NewShared(mg::box::ERR_SYS_FORBIDDEN, 123, "random msg");
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_FORBIDDEN);
		TEST_CHECK(err->myValue == 123);
		TEST_CHECK(err->myMessage == "random msg");
	}

	static void
	UnitTestErrorErrorPtrRaised()
	{
		TestCaseGuard guard("ErrorPtrRaised object");

		// Construct from Error.
		{
			mg::box::ErrorPtrRaised err(mg::box::Error::NewShared());
			TEST_CHECK(err->myCode == mg::box::ERR_BOX_UNKNOWN);
			TEST_CHECK(err->myValue == 0);
			TEST_CHECK(err->myMessage == "{empty}");

			const mg::box::ErrorPtrRaised& errConst = err;
			TEST_CHECK(errConst->myCode == mg::box::ERR_BOX_UNKNOWN);
			TEST_CHECK(errConst->myValue == 0);
			TEST_CHECK(errConst->myMessage == "{empty}");
		}
		// Construct from another raised.
		{
			mg::box::Error::Ptr errPtr = mg::box::Error::NewShared();
			mg::box::ErrorPtrRaised err(
				mg::box::ErrorPtrRaised(std::move(errPtr)));
			TEST_CHECK(err->myCode == mg::box::ERR_BOX_UNKNOWN);
			TEST_CHECK(err->myValue == 0);
			TEST_CHECK(err->myMessage == "{empty}");
		}
		// Casts.
		{
			mg::box::Error::Ptr errPtr = mg::box::Error::NewShared();
			mg::box::Error* errRaw = errPtr.GetPointer();
			mg::box::ErrorPtrRaised err(std::move(errPtr));
			TEST_CHECK(((mg::box::Error::Ptr&&)err).GetPointer() == errRaw);
			TEST_CHECK((mg::box::Error*)err == errRaw);
			TEST_CHECK((const mg::box::Error*)err == errRaw);
		}
		// Operators.
		{
			mg::box::ErrorPtrRaised err(mg::box::Error::NewShared(
				mg::box::ERR_SYS_FORBIDDEN, 123, "random msg"));
			TEST_CHECK(err->myCode == mg::box::ERR_SYS_FORBIDDEN);
			TEST_CHECK(err->myValue == 123);
			TEST_CHECK(err->myMessage == "random msg");
		}
	}

	static void
	UnitTestErrorErrorRaiseWin()
	{
#if IS_PLATFORM_WIN
		TestCaseGuard guard("ErrorRaiseWin()");
		mg::box::Error::Ptr err;
		//
		// Win code + the system message.
		//
		SetLastError(ERROR_NOT_FOUND);
		err = mg::box::ErrorRaiseWin();
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_NOT_FOUND);
		TEST_CHECK(err->myValue == ERROR_NOT_FOUND);
		TEST_CHECK(err->myMessage ==
			"{code=1005, type=\"sys\", val=1168, msg=\"Element not found.\"}");

		SetLastError(ERROR_ACCESS_DENIED);
		err = mg::box::ErrorRaiseWin();
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_FORBIDDEN);
		TEST_CHECK(err->myValue == ERROR_ACCESS_DENIED);
		TEST_CHECK(err->myMessage ==
			"{code=1001, type=\"sys\", val=5, msg=\"Access is denied.\"}");
		//
		// Given code + the system message.
		//
		TEST_CHECK(mg::box::ErrorRaiseWin(ERROR_NOT_FOUND)->myCode ==
			mg::box::ERR_SYS_NOT_FOUND);
		TEST_CHECK(mg::box::ErrorRaiseWin(ERROR_ACCESS_DENIED)->myCode ==
			mg::box::ERR_SYS_FORBIDDEN);
		//
		// Given code + a comment.
		//
		err = mg::box::ErrorRaiseWin(ERROR_NOT_FOUND, "random comment");
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_NOT_FOUND);
		TEST_CHECK(err->myMessage ==
			"{code=1005, type=\"sys\", val=1168, msg=\"Element not found.\", "
			"comm=\"random comment\"}");

		err = mg::box::ErrorRaiseWin(ERROR_ACCESS_DENIED, "random comment");
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_FORBIDDEN);
		TEST_CHECK(err->myMessage ==
			"{code=1001, type=\"sys\", val=5, msg=\"Access is denied.\", "
			"comm=\"random comment\"}");
		//
		// Errno + a comment.
		//
		SetLastError(ERROR_NOT_FOUND);
		err = mg::box::ErrorRaiseWin("random comment");
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_NOT_FOUND);
		TEST_CHECK(err->myMessage ==
			"{code=1005, type=\"sys\", val=1168, msg=\"Element not found.\", "
			"comm=\"random comment\"}");

		SetLastError(ERROR_ACCESS_DENIED);
		err = mg::box::ErrorRaiseWin("random comment");
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_FORBIDDEN);
		TEST_CHECK(err->myMessage ==
			"{code=1001, type=\"sys\", val=5, msg=\"Access is denied.\", "
			"comm=\"random comment\"}");
		//
		// Given code + a comment format.
		//
		err = mg::box::ErrorRaiseFormatWin(ERROR_NOT_FOUND, "random %d", 123);
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_NOT_FOUND);
		TEST_CHECK(err->myMessage ==
			"{code=1005, type=\"sys\", val=1168, msg=\"Element not found.\", "
			"comm=\"random 123\"}");

		err = mg::box::ErrorRaiseFormatWin(ERROR_ACCESS_DENIED, "random %d%d%d", 2, 5, 6);
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_FORBIDDEN);
		TEST_CHECK(err->myMessage ==
			"{code=1001, type=\"sys\", val=5, msg=\"Access is denied.\", "
			"comm=\"random 256\"}");
		//
		// Errno + a comment format.
		//
		SetLastError(ERROR_NOT_FOUND);
		err = mg::box::ErrorRaiseFormatWin("random %d", 123);
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_NOT_FOUND);
		TEST_CHECK(err->myMessage ==
			"{code=1005, type=\"sys\", val=1168, msg=\"Element not found.\", "
			"comm=\"random 123\"}");

		SetLastError(ERROR_ACCESS_DENIED);
		err = mg::box::ErrorRaiseFormatWin("random %d%d%d", 2, 5, 6);
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_FORBIDDEN);
		TEST_CHECK(err->myMessage ==
			"{code=1001, type=\"sys\", val=5, msg=\"Access is denied.\", "
			"comm=\"random 256\"}");
#endif
	}

	static void
	UnitTestErrorErrorRaiseErrno()
	{
		TestCaseGuard guard("ErrorRaiseErrno()");
		mg::box::Error::Ptr err;
		//
		// Errno + the system message.
		//
		errno = ENOENT;
		err = mg::box::ErrorRaiseErrno();
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_NOT_FOUND);
		TEST_CHECK(err->myValue == ENOENT);
		TEST_CHECK(err->myMessage ==
			"{code=1005, type=\"sys\", val=2, msg=\"No such file or directory\"}");

		errno = EACCES;
		err = mg::box::ErrorRaiseErrno();
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_FORBIDDEN);
		TEST_CHECK(err->myValue == EACCES);
		TEST_CHECK(err->myMessage ==
			"{code=1001, type=\"sys\", val=13, msg=\"Permission denied\"}");
		//
		// Given code + the system message.
		//
		TEST_CHECK(
			mg::box::ErrorRaiseErrno(ENOENT)->myCode == mg::box::ERR_SYS_NOT_FOUND);
		TEST_CHECK(
			mg::box::ErrorRaiseErrno(EACCES)->myCode == mg::box::ERR_SYS_FORBIDDEN);
		//
		// Given code + a comment.
		//
		err = mg::box::ErrorRaiseErrno(ENOENT, "random comment");
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_NOT_FOUND);
		TEST_CHECK(err->myMessage ==
			"{code=1005, type=\"sys\", val=2, msg=\"No such file or directory\", "
			"comm=\"random comment\"}");

		err = mg::box::ErrorRaiseErrno(EACCES, "random comment");
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_FORBIDDEN);
		TEST_CHECK(err->myMessage ==
			"{code=1001, type=\"sys\", val=13, msg=\"Permission denied\", "
			"comm=\"random comment\"}");
		//
		// Errno + a comment.
		//
		errno = ENOENT;
		err = mg::box::ErrorRaiseErrno("random comment");
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_NOT_FOUND);
		TEST_CHECK(err->myMessage ==
			"{code=1005, type=\"sys\", val=2, msg=\"No such file or directory\", "
			"comm=\"random comment\"}");

		errno = EACCES;
		err = mg::box::ErrorRaiseErrno("random comment");
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_FORBIDDEN);
		TEST_CHECK(err->myMessage ==
			"{code=1001, type=\"sys\", val=13, msg=\"Permission denied\", "
			"comm=\"random comment\"}");
		//
		// Given code + a comment format.
		//
		err = mg::box::ErrorRaiseFormatErrno(ENOENT, "random %d", 123);
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_NOT_FOUND);
		TEST_CHECK(err->myMessage ==
			"{code=1005, type=\"sys\", val=2, msg=\"No such file or directory\", "
			"comm=\"random 123\"}");

		err = mg::box::ErrorRaiseFormatErrno(EACCES, "random %d%d%d", 2, 5, 6);
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_FORBIDDEN);
		TEST_CHECK(err->myMessage ==
			"{code=1001, type=\"sys\", val=13, msg=\"Permission denied\", "
			"comm=\"random 256\"}");
		//
		// Errno + a comment format.
		//
		errno = ENOENT;
		err = mg::box::ErrorRaiseFormatErrno("random %d", 123);
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_NOT_FOUND);
		TEST_CHECK(err->myMessage ==
			"{code=1005, type=\"sys\", val=2, msg=\"No such file or directory\", "
			"comm=\"random 123\"}");

		errno = EACCES;
		err = mg::box::ErrorRaiseFormatErrno("random %d%d%d", 2, 5, 6);
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_FORBIDDEN);
		TEST_CHECK(err->myMessage ==
			"{code=1001, type=\"sys\", val=13, msg=\"Permission denied\", "
			"comm=\"random 256\"}");
	}

	static void
	UnitTestErrorErrorRaise()
	{
		TestCaseGuard guard("ErrorRaise()");
		mg::box::Error::Ptr err;
		//
		// Given code + its default message.
		//
		err = mg::box::ErrorRaise(mg::box::ERR_SYS_NOT_FOUND);
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_NOT_FOUND);
		TEST_CHECK(err->myValue == 0);
		TEST_CHECK(err->myMessage ==
			"{code=1005, type=\"sys\", msg=\"sys not found\"}");

		err = mg::box::ErrorRaise(mg::box::ERR_SYS_FORBIDDEN);
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_FORBIDDEN);
		TEST_CHECK(err->myValue == 0);
		TEST_CHECK(err->myMessage ==
			"{code=1001, type=\"sys\", msg=\"sys forbidden\"}");
		//
		// Given code + a low-level value + its default message.
		//
		err = mg::box::ErrorRaise(mg::box::ERR_SYS_NOT_FOUND, 123);
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_NOT_FOUND);
		TEST_CHECK(err->myValue == 123);
		TEST_CHECK(err->myMessage ==
			"{code=1005, type=\"sys\", val=123, msg=\"sys not found\"}");

		err = mg::box::ErrorRaise(mg::box::ERR_SYS_FORBIDDEN, 256);
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_FORBIDDEN);
		TEST_CHECK(err->myValue == 256);
		TEST_CHECK(err->myMessage ==
			"{code=1001, type=\"sys\", val=256, msg=\"sys forbidden\"}");
		//
		// Given code + a comment.
		//
		err = mg::box::ErrorRaise(mg::box::ERR_SYS_NOT_FOUND, "random 123");
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_NOT_FOUND);
		TEST_CHECK(err->myValue == 0);
		TEST_CHECK(err->myMessage ==
			"{code=1005, type=\"sys\", msg=\"sys not found\", comm=\"random 123\"}");

		err = mg::box::ErrorRaise(mg::box::ERR_SYS_FORBIDDEN, "random 256");
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_FORBIDDEN);
		TEST_CHECK(err->myValue == 0);
		TEST_CHECK(err->myMessage ==
			"{code=1001, type=\"sys\", msg=\"sys forbidden\", comm=\"random 256\"}");
		//
		// Given code + a comment format.
		//
		err = mg::box::ErrorRaiseFormat(mg::box::ERR_SYS_NOT_FOUND, "random %d", 123);
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_NOT_FOUND);
		TEST_CHECK(err->myValue == 0);
		TEST_CHECK(err->myMessage ==
			"{code=1005, type=\"sys\", msg=\"sys not found\", comm=\"random 123\"}");

		err = mg::box::ErrorRaiseFormat(mg::box::ERR_SYS_FORBIDDEN, "random %d%d%d",
			2, 5, 6);
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_FORBIDDEN);
		TEST_CHECK(err->myValue == 0);
		TEST_CHECK(err->myMessage ==
			"{code=1001, type=\"sys\", msg=\"sys forbidden\", comm=\"random 256\"}");
		//
		// Given code + a detailed value + a comment.
		//
		err = mg::box::ErrorRaise(mg::box::ERR_SYS_NOT_FOUND, 123, "random 256");
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_NOT_FOUND);
		TEST_CHECK(err->myValue == 123);
		TEST_CHECK(err->myMessage ==
			"{code=1005, type=\"sys\", val=123, msg=\"sys not found\", "
			"comm=\"random 256\"}");

		err = mg::box::ErrorRaise(mg::box::ERR_SYS_FORBIDDEN, 789, "random 1011");
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_FORBIDDEN);
		TEST_CHECK(err->myValue == 789);
		TEST_CHECK(err->myMessage ==
			"{code=1001, type=\"sys\", val=789, msg=\"sys forbidden\", "
			"comm=\"random 1011\"}");
		//
		// Given code + a detailed value + a comment format.
		//
		err = mg::box::ErrorRaiseFormat(mg::box::ERR_SYS_NOT_FOUND, 123,
			"random %d", 256);
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_NOT_FOUND);
		TEST_CHECK(err->myValue == 123);
		TEST_CHECK(err->myMessage ==
			"{code=1005, type=\"sys\", val=123, msg=\"sys not found\", "
			"comm=\"random 256\"}");

		err = mg::box::ErrorRaiseFormat(mg::box::ERR_SYS_FORBIDDEN, 789,
			"random %d%d", 10, 11);
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_FORBIDDEN);
		TEST_CHECK(err->myValue == 789);
		TEST_CHECK(err->myMessage ==
			"{code=1001, type=\"sys\", val=789, msg=\"sys forbidden\", "
			"comm=\"random 1011\"}");
		//
		// Most customizable version.
		//
		err = mg::box::ErrorRaise(mg::box::ERR_SYS_NOT_FOUND, 123, "msg1", "comm1");
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_NOT_FOUND);
		TEST_CHECK(err->myValue == 123);
		TEST_CHECK(err->myMessage ==
			"{code=1005, type=\"sys\", val=123, msg=\"msg1\", comm=\"comm1\"}");

		err = mg::box::ErrorRaise(mg::box::ERR_SYS_FORBIDDEN, 789, "msg2", "comm2");
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_FORBIDDEN);
		TEST_CHECK(err->myValue == 789);
		TEST_CHECK(err->myMessage ==
			"{code=1001, type=\"sys\", val=789, msg=\"msg2\", comm=\"comm2\"}");
		//
		// Cause raw ptr + a new comment.
		//
		err = mg::box::ErrorRaise(mg::box::ErrorRaise(
			mg::box::ERR_SYS_NOT_FOUND, 456, "old comment").myError.GetPointer(),
			"new comment");
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_NOT_FOUND);
		TEST_CHECK(err->myValue == 456);
		TEST_CHECK(err->myMessage == "{comm=\"new comment\", cause="
			"{code=1005, type=\"sys\", val=456, msg=\"sys not found\", "
				"comm=\"old comment\"}}");
		//
		// Cause ptr + a new comment.
		//
		err = mg::box::ErrorRaise(mg::box::ErrorRaise(
			mg::box::ERR_SYS_FORBIDDEN, 123, "old comment").myError, "new comment");
		TEST_CHECK(err->myCode == mg::box::ERR_SYS_FORBIDDEN);
		TEST_CHECK(err->myValue == 123);
		TEST_CHECK(err->myMessage == "{comm=\"new comment\", cause="
			"{code=1001, type=\"sys\", val=123, msg=\"sys forbidden\", "
				"comm=\"old comment\"}}");
	}

	static void
	UnitTestErrorGuard()
	{
		TestCaseGuard guard("ErrorGuard");
		{
			errno = ENOENT;
#if IS_PLATFORM_WIN
			SetLastError(ERROR_ACCESS_DENIED);
#endif
			mg::box::ErrorGuard errGuard;
			TEST_CHECK(errGuard.myErrno == ENOENT);
			errno = EACCES;
#if IS_PLATFORM_WIN
			TEST_CHECK(errGuard.myWinCode == ERROR_ACCESS_DENIED);
			SetLastError(ERROR_NOT_FOUND);
#endif
		}
		TEST_CHECK(errno == ENOENT);
#if IS_PLATFORM_WIN
		TEST_CHECK(GetLastError() == ERROR_ACCESS_DENIED);
#endif
	}
}

	void
	UnitTestError()
	{
		using namespace error;
		TestSuiteGuard suite("Error");

		// Windows.
		UnitTestErrorWinToString();
		UnitTestErrorCodeWin();
		UnitTestErrorCodeFromWin();

		// Errno.
		UnitTestErrorErrnoToString();
		UnitTestErrorErrorCodeErrno();
		UnitTestErrorErrorCodeFromErrno();

		// Generic.
		UnitTestErrorErrorCodeMessage();
		UnitTestErrorErrorCodeTag();
		UnitTestErrorErrorCodeType();
		UnitTestErrorError();
		UnitTestErrorErrorPtrRaised();

		// Platform raise.
		UnitTestErrorErrorRaiseWin();
		UnitTestErrorErrorRaiseErrno();

		// Generic.
		UnitTestErrorErrorRaise();
		UnitTestErrorGuard();
	}

}
}
}

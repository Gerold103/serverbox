#include "mg/box/Error.h"

#include "mg/box/StringFunctions.h"

#include "UnitTest.h"

#define TEST_CHECK MG_BOX_ASSERT

namespace mg {
namespace unittests {

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
		TEST_CHECK(mg::box::Strcmp(
			mg::box::ErrorCodeMessage((mg::box::ErrorCode)(UINT32_MAX - 123)),
			"unknown") == 0);
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
		TEST_CHECK(mg::box::Strcmp(
			mg::box::ErrorCodeTag((mg::box::ErrorCode)(UINT32_MAX - 123)),
			"unknown") == 0);
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
			mg::box::ErrorCodeType(mg::box::ERR_UNKNOWN),
			"common") == 0);
		TEST_CHECK(mg::box::Strcmp(
			mg::box::ErrorCodeType((mg::box::ErrorCode)(UINT32_MAX - 123)),
			"unknown") == 0);
	}

	static void
	UnitTestErrorError()
	{
		TestCaseGuard guard("Error object");

		mg::box::Error::Ptr err = mg::box::Error::NewShared();
		TEST_CHECK(err->myCode == mg::box::ERR_UNKNOWN);
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
			TEST_CHECK(err->myCode == mg::box::ERR_UNKNOWN);
			TEST_CHECK(err->myValue == 0);
			TEST_CHECK(err->myMessage == "{empty}");

			const mg::box::ErrorPtrRaised& errConst = err;
			TEST_CHECK(errConst->myCode == mg::box::ERR_UNKNOWN);
			TEST_CHECK(errConst->myValue == 0);
			TEST_CHECK(errConst->myMessage == "{empty}");
		}
		// Construct from another raised.
		{
			mg::box::Error::Ptr errPtr = mg::box::Error::NewShared();
			mg::box::ErrorPtrRaised err(
				mg::box::ErrorPtrRaised(std::move(errPtr)));
			TEST_CHECK(err->myCode == mg::box::ERR_UNKNOWN);
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
	}

	static void
	UnitTestErrorErrorRaiseErrno()
	{
		TestCaseGuard guard("ErrorRaiseErrno()");
		mg::box::Error::Ptr err;
		//
		// Global code.
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
		// Explicit code.
		//
		TEST_CHECK(
			mg::box::ErrorRaiseErrno(ENOENT)->myCode == mg::box::ERR_SYS_NOT_FOUND);
		TEST_CHECK(
			mg::box::ErrorRaiseErrno(EACCES)->myCode == mg::box::ERR_SYS_FORBIDDEN);
		//
		// Explicit code with comment.
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
		// Global code + comment.
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
		// Explicit code with comment format.
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
		// Global code with comment format.
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
		// Only code.
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
		// Code and value.
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
		// Code and comment.
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
		// Code and comment format.
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
		// Code, value, comment.
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
		// Code, value, comment format.
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
		// All parameters.
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
	}

	void
	UnitTestError()
	{
		TestSuiteGuard suite("Error");

		UnitTestErrorErrnoToString();
		UnitTestErrorErrorCodeErrno();
		UnitTestErrorErrorCodeFromErrno();
		UnitTestErrorErrorCodeMessage();
		UnitTestErrorErrorCodeTag();
		UnitTestErrorErrorCodeType();
		UnitTestErrorError();
		UnitTestErrorErrorPtrRaised();
		UnitTestErrorErrorRaiseErrno();
		UnitTestErrorErrorRaise();

		// TODO: error guard, win errors.
	}

}
}

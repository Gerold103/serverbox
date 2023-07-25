#include "mg/net/DomainToIP.h"

#include "mg/box/Signal.h"
#include "mg/box/Thread.h"

#include "UnitTest.h"

namespace mg {
namespace unittests {
namespace net {
namespace domaintoip {

	static void
	UnitTestDomainToIPBasic()
	{
		TestCaseGuard guard("Basic");

		mg::net::DomainToIPRequest req;
		mg::box::Signal sig;
		mg::net::DomainToIPAsync(req, "google.com", mg::box::theTimeDurationInf,
			[&](const char* aDomain,
				const std::vector<mg::net::DomainEndpoint>& aEndpoints,
				mg::box::Error* aError) {

			TEST_CHECK(mg::box::Strcmp(aDomain, "google.com") == 0);
			TEST_CHECK(aError == nullptr);
			TEST_CHECK(!aEndpoints.empty());
			for (const mg::net::DomainEndpoint& e : aEndpoints)
				Report("host: %s", e.myHost.ToString().c_str());
			sig.Send();
		});
		sig.ReceiveBlocking();
	}

	static void
	UnitTestDomainToIPTimeout()
	{
		TestCaseGuard guard("Timeout");

#if IS_BUILD_DEBUG
		mg::net::DomainToIPDebugSetProcessLatency(10);
#endif
		constexpr uint32_t reqCount = 50;
		mg::box::AtomicI32 endCount(0);
		mg::box::AtomicI32 timeoutCount(0);
		for (uint32_t i = 0; i < reqCount; ++i)
		{
			mg::net::DomainToIPRequest req;
			mg::net::DomainToIPAsync(req, "google.com", mg::box::TimeDuration(i % 3),
				[&](const char* aDomain,
					const std::vector<mg::net::DomainEndpoint>& aEndpoints,
					mg::box::Error* aError) {

				TEST_CHECK(mg::box::Strcmp(aDomain, "google.com") == 0);
				if (aError != nullptr)
				{
					if (aError->myCode == mg::box::ERR_BOX_TIMEOUT)
						timeoutCount.IncrementRelaxed();
					TEST_CHECK(aEndpoints.empty());
				}
				endCount.IncrementRelaxed();
			});
		}
		while (endCount.LoadRelaxed() != reqCount)
			mg::box::Sleep(1);
#if IS_BUILD_DEBUG
		TEST_CHECK(timeoutCount.LoadRelaxed() > 0);
		mg::net::DomainToIPDebugSetProcessLatency(0);
#endif
	}

	static void
	UnitTestDomainToIPCancel()
	{
		TestCaseGuard guard("Cancel");

#if IS_BUILD_DEBUG
		mg::net::DomainToIPDebugSetProcessLatency(10);
#endif
		constexpr uint32_t reqCount = 50;
		mg::box::AtomicI32 endCount(0);
		mg::box::AtomicI32 cancelCount(0);
		std::vector<mg::net::DomainToIPRequest> reqs;
		for (uint32_t i = 0; i < reqCount; ++i)
		{
			mg::net::DomainToIPRequest req;
			mg::net::DomainToIPAsync(req, "google.com", mg::box::TimeDuration(i % 10),
				[&](const char* aDomain,
					const std::vector<mg::net::DomainEndpoint>& aEndpoints,
					mg::box::Error* aError) {

				TEST_CHECK(mg::box::Strcmp(aDomain, "google.com") == 0);
				if (aError != nullptr)
				{
					if (aError->myCode == mg::box::ERR_BOX_CANCEL)
						cancelCount.IncrementRelaxed();
					TEST_CHECK(aEndpoints.empty());
				}
				endCount.IncrementRelaxed();
			});
			reqs.push_back(std::move(req));
		}
		for (uint32_t i = 0; i < reqCount; i += 2)
			mg::net::DomainToIPCancel(reqs[i]);
		while (endCount.LoadRelaxed() != reqCount)
			mg::box::Sleep(1);
#if IS_BUILD_DEBUG
		TEST_CHECK(cancelCount.LoadRelaxed() > 0);
		mg::net::DomainToIPDebugSetProcessLatency(0);
#endif
	}

	static void
	UnitTestDomainToIPBlocking()
	{
		TestCaseGuard guard("Blocking");

		std::vector<mg::net::DomainEndpoint> endpoints;
		mg::box::Error::Ptr err;
		mg::net::DomainToIPBlocking("google.com", mg::box::theTimeDurationInf,
			endpoints, err);
		TEST_CHECK(!err.IsSet());
		TEST_CHECK(!endpoints.empty());
		for (const mg::net::DomainEndpoint& e : endpoints)
			Report("host: %s", e.myHost.ToString().c_str());

#if IS_BUILD_DEBUG
		mg::net::DomainToIPDebugSetError(mg::box::ERR_NET);
		endpoints.resize(0);
		mg::net::DomainToIPBlocking("google.com", mg::box::theTimeDurationInf,
			endpoints, err);
		TEST_CHECK(err->myCode == mg::box::ERR_NET);
		TEST_CHECK(endpoints.empty());
		mg::net::DomainToIPDebugSetError(mg::box::ERR_BOX_NONE);
#endif
	}
}

	void
	UnitTestDomainToIP()
	{
		using namespace domaintoip;
		TestSuiteGuard suite("DomainToIP");

		UnitTestDomainToIPBasic();
		UnitTestDomainToIPTimeout();
		UnitTestDomainToIPCancel();
		UnitTestDomainToIPBlocking();
	}

}
}
}

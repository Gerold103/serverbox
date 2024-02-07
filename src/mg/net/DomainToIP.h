#pragma once

#include "mg/box/Error.h"
#include "mg/box/Time.h"
#include "mg/net/Host.h"

#include <functional>
#include <vector>

namespace mg {
namespace net {

	class DomainToIPContext;
	class DomainToIPWorker;

	struct DomainEndpoint
	{
		mg::net::Host myHost;
	};

	using DomainToIPCallback = std::function<void(
		const char* aDomain,
		const std::vector<DomainEndpoint>& aEndpoints,
		mg::box::Error* aError)>;

	struct DomainToIPRequest
	{
		DomainToIPRequest();
		DomainToIPRequest(
			DomainToIPContext* aContext);
		DomainToIPRequest(
			DomainToIPRequest&& aOther);
		~DomainToIPRequest();

		DomainToIPRequest& operator=(
			DomainToIPRequest&& aOther);

	private:
		DomainToIPContext* myCtx;

		friend DomainToIPWorker;
	};

	void DomainToIPAsync(
		DomainToIPRequest& aOutRequest,
		const char* aDomain,
		mg::box::TimeLimit aTimeLimit,
		const DomainToIPCallback& aCallback);

	void DomainToIPCancel(
		DomainToIPRequest& aRequest);

	bool DomainToIPBlocking(
		const char* aDomain,
		mg::box::TimeLimit aTimeLimit,
		std::vector<DomainEndpoint>& aOutEndpoints,
		mg::box::Error::Ptr& aOutError);

	static inline bool
	DomainToIPBlocking(
		const std::string& aDomain,
		mg::box::TimeLimit aTimeLimit,
		std::vector<DomainEndpoint>& aOutEndpoints,
		mg::box::Error::Ptr& aOutError)
	{
		return DomainToIPBlocking(aDomain.c_str(), aTimeLimit, aOutEndpoints, aOutError);
	}

#if IS_BUILD_DEBUG
	void DomainToIPDebugSetProcessLatency(
		uint64_t aValue);

	void DomainToIPDebugSetError(
		mg::box::ErrorCode aCode);
#endif


}
}

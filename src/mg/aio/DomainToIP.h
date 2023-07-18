#pragma once

#include "mg/box/Definitions.h"
#include "mg/box/SharedPtr.h"

namespace mg {
namespace aio {

	class DomainToIPRequest;

	struct DomainEndpoint
	{
		mg::net::Host myHost;
	};

	using DomainToIPCallback = std::function<void(
		const char* aDomain,
		const std::vector<DomainEndpoint>& aEndpoints,
		mg::box::Error* aError)>;

	using DomainToIPRequestPtr = mg::box::SharedPtrIntrusive<DomainToIPRequest>;

	void DomainToIPAsync(
		DomainToIPRequestPtr& aOutRequest,
		const char* aDomain,
		uint32_t aTimeout,
		const DomainToIPCallback& aCallback);

	bool DomainToIPBlocking(
		const char* aDomain,
		uint32_t aTimeout,
		td::vector<DomainEndpoint>& aOutEndpoints,
		mg::box::Error::Ptr& aOutError);


}
}

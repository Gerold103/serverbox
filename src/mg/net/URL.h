#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace mg {
namespace net {

	struct URL
	{
		URL();

		std::string myHost;
		std::string myProtocol;
		uint16_t myPort;
		std::string myTarget;
	};

	URL URLParse(
		const char* aStr);

	static inline URL
	URLParse(
		const std::string& aStr)
	{
		return URLParse(aStr.c_str());
	}

}
}

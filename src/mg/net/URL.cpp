#include "URL.h"

#include "mg/box/StringFunctions.h"

namespace mg {
namespace net {

	URL::URL()
		: myPort(0)
	{
	}

	URL
	URLParse(
		const char* aStr)
	{
		URL res;
		std::string str(aStr);
		size_t pos = str.find("://");
		if (pos != std::string::npos)
		{
			res.myProtocol = str.substr(0, pos);
			str = str.substr(pos + 3);
		}
		pos = str.find('/');
		if (pos != std::string::npos)
		{
			res.myTarget = str.substr(pos);
			str = str.substr(0, pos);
		}
		pos = str.find(':');
		if (pos != std::string::npos)
		{
			std::string portStr = str.substr(pos + 1);
			uint16_t port;
			if (mg::box::StringToNumber(portStr.c_str(), port))
				res.myPort = port;
			str = str.substr(0, pos);
		}
		res.myHost = str;
		return res;
	}

}
}

#include "Time.h"

namespace mg {
namespace box {

	uint64_t
	GetMilliseconds()
	{
		return ::GetTickCount64();
	}

}
}

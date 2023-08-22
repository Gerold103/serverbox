#pragma once

namespace mg {
namespace bench {
namespace io {

	class Instance
	{
	public:
		virtual ~Instance() = default;

		virtual bool IsFinished() const = 0;
	private:
	};

}
}
}

#pragma once

#include <utility>

namespace mg {
namespace box {

	// Windows defines macros 'min' and 'max' which make impossible to use std::min/max
	// without hacks. Easier to just define them again with different naming.

	template <typename T>
	const T& Max(const T& A, const T& B)
	{
		return A > B ? A : B;
	}

	template <typename T>
	const T& Min(const T& A, const T& B)
	{
		return A < B ? A : B;
	}

}
}

#pragma once

#include <utility>

namespace mg {
namespace box {

	// Windows defines macros 'min' and 'max' which make impossible to use std::min/max
	// without hacks. Easier to just define them again with different naming.

	template <typename T>
	T Max(T&& A, T&& B)
	{
		return A > B ? std::forward<T>(A) : std::forward<T>(B);
	}

	template <typename T>
	T Min(T&& A, T&& B)
	{
		return A < B ? std::forward<T>(A) : std::forward<T>(B);
	}

}
}

#include "Buffer.h"

namespace mg {
namespace net {

	Buffer::~Buffer()
	{
		// Avoid recursion for a long buffer list.
		Buffer* curr = myNext.Unwrap();
		while (curr != nullptr)
		{
			if (!curr->myRef.Dec())
				return;
			Buffer* next = curr->myNext.Unwrap();
			delete curr;
			curr = next;
		}
	}

}
}

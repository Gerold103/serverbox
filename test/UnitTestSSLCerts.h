#include "mg/box/Definitions.h"

namespace mg {
namespace unittests {

	// Private key and public certificate, version 1.
	extern const uint32_t theUnitTestCert1Size;
	extern const uint8_t theUnitTestCert1[];

	extern const uint32_t theUnitTestKey1Size;
	extern const uint8_t theUnitTestKey1[];

	// Private key and public certificate, version 2.
	extern const uint32_t theUnitTestCert2Size;
	extern const uint8_t theUnitTestCert2[];

	extern const uint32_t theUnitTestKey2Size;
	extern const uint8_t theUnitTestKey2[];

	// Private key and 2 public certificates signed by a CA,
	// version 1.
	extern const uint32_t theUnitTestCert31Size;
	extern const uint8_t theUnitTestCert31[];

	extern const uint32_t theUnitTestCert32Size;
	extern const uint8_t theUnitTestCert32[];

	extern const uint32_t theUnitTestCert33ExpiredSize;
	extern const uint8_t theUnitTestCert33Expired[];

	extern const uint32_t theUnitTestKey3Size;
	extern const uint8_t theUnitTestKey3[];

	extern const uint32_t theUnitTestCACert3Size;
	extern const uint8_t theUnitTestCACert3[];
	// The same cert, but PEM format.
	extern const uint32_t theUnitTestCACert3PemSize;
	extern const char theUnitTestCACert3Pem[];

}
}

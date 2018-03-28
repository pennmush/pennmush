/* Can't have this directly in config.h because autoconf will comment out the
 * #undef lines.
 */
#include <w32api.h>
#undef _WIN32_WINNT
#undef WINVER
/* Windows 7 or better is required for Windows builds. */
#define _WIN32_WINNT Windows7
#define WINVER Windows7

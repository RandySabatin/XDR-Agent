#include "tool_setup.h"
#include "tool_hugehelp.h"

void hugehelp(void)
{
#ifdef USE_MANUAL
  fputs("Built-in manual not included\n", stdout);
#endif
}

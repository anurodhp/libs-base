#include "objc-common.g"
#include <objc/message.h>

static void handler(void) { }

int main (void)
{
  objc_setForwardHandler((void *)handler, (void *)handler);
  return 0;
}

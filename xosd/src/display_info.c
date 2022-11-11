#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "xosd.h"

void
printerror()
{
  fprintf(stderr, "ERROR: %s\n", xosd_error);
}

int
main(int argc, char *argv[])
{
  xosd *osd;
  osd = xosd_create(2);

  if (0 != xosd_set_outline_offset(osd, 1)) {
    printerror();
  }

  if (0 != xosd_set_font(osd, (char *) osd_default_font)) {
    printerror();
  }

  if (0 != xosd_set_timeout(osd, 2)) {
    printerror();
  }
  
  display_info();

  if (0 != xosd_wait_until_no_display(osd)) {
    printerror();
  }

  sleep(10);
  
  if (0 != xosd_destroy(osd)) {
    printerror();
  }


  return EXIT_SUCCESS;
}

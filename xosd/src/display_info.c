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
  
  display_info();
  
  return EXIT_SUCCESS;
}

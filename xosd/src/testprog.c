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
  int a;
  osd = xosd_create(2);
  
  xosd_monitor(osd, 1000);
  
  if (!osd) {
    printerror();
    return 1;
  }

  if (0 != xosd_set_outline_offset(osd, 1)) {
    printerror();
  }

  if (0 != xosd_set_font(osd, (char *) osd_default_font)) {
    printerror();
  }
  


//  if (0 != xosd_set_vertical_offset(osd, 100)) {
//    printerror();
//  }
  
 //if (0 != xosd_set_horizontal_offset(osd, -500)) {
 //   printerror();
//  }


  if (0 != xosd_set_timeout(osd, 2)) {
    printerror();
  }
  
  xosd_set_align(osd, XOSD_center);
  xosd_set_pos(osd, XOSD_top);
  display_info();
  for (a = 0; a <= 100; a++) {
    if (-1 == xosd_display(osd, 0, XOSD_percentage, a))
      printerror();
    if (-1 == xosd_display(osd, 1, XOSD_percentage, 100 - a))
      printerror();
    usleep(100);
  }
  for (a = 100; a >= 0; a--) {
    if (-1 == xosd_display(osd, 0, XOSD_percentage, a))
      printerror();
    if (-1 == xosd_display(osd, 1, XOSD_percentage, 100 - a))
      printerror();
    usleep(100);
  }

  for (a = 0; a <= 100; a++) {
    if (-1 == xosd_display(osd, 0, XOSD_slider, a))
      printerror();
    if (-1 == xosd_display(osd, 1, XOSD_slider, 100 - a))
      printerror();
    usleep(100);
  }
  for (a = 100; a >= 0; a--) {
    if (-1 == xosd_display(osd, 0, XOSD_slider, a))
      printerror();
    if (-1 == xosd_display(osd, 1, XOSD_slider, 100 - a))
      printerror();
    usleep(100);
  }
  if (-1 == xosd_display(osd, 1, XOSD_string, ""))
    printerror();


  xosd_set_bar_length(osd, 14);


  if (-1 == xosd_display(osd, 0, XOSD_percentage, 80)) {
    printerror();
  }

  if (0 != xosd_wait_until_no_display(osd)) {
    printerror();
  }

  if (-1 == xosd_display(osd, 0, XOSD_slider, 36)) {
    printerror();
  }
  if (0 != xosd_wait_until_no_display(osd)) {
    printerror();
  }
  if (-1 == xosd_display(osd, 0, XOSD_string, "Blah")) {
    printerror();
  }
  xosd_monitor(osd, 1);
  if (0 != xosd_wait_until_no_display(osd)) {
    printerror();
  }

  if (-1 == xosd_display(osd, 1, XOSD_string, "blah2")) {
    printerror();
  }
  xosd_monitor(osd, 3);
        if (0 != xosd_wait_until_no_display(osd)) {
    printerror();
  }
  if (-1 == xosd_display(osd, 1, XOSD_string, "wibble")) {
    printerror();
  }
    if (0 != xosd_wait_until_no_display(osd)) {
    printerror();
  }
  xosd_monitor(osd, 2);

  if (0 != xosd_scroll(osd, 1)) {
    printerror();
  }

  if (-1 == xosd_display(osd, 1, XOSD_string, "bloggy")) {
    printerror();
  }


  if (0 != xosd_wait_until_no_display(osd)) {
    printerror();
  }

  for (int i = 0; i < screen_count(osd); i++) {
 	xosd * osd2 = xosd_clone(osd);
 	xosd_set_timeout(osd2, 10);
  	xosd_monitor(osd2, i+1);
  	xosd_set_pos(osd2, XOSD_middle);
  	xosd_set_align(osd2, XOSD_center);
  	xosd_display(osd2, 1, XOSD_string, "this is a clone");
  }

  sleep(10);

  if (0 != xosd_scroll(osd, 1)) {
    printerror();
  }

  if (0 != xosd_scroll(osd, 1)) {
    printerror();
  }

  if (0 != xosd_destroy(osd)) {
    printerror();
  }


  return EXIT_SUCCESS;
}

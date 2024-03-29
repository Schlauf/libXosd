/*
 * XOSD
 * 
 * Copyright (c) 2000 Andre Renaud (andre@ignavus.net)
 * 
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 675 Mass Ave, Cambridge, MA 02139, USA. 
 */
#include "intern.h"

#define SLIDER_SCALE 0.8
#define SLIDER_SCALE_ON 0.7
#define XOFFSET 10

const char *osd_default_font =
  "-misc-fixed-medium-r-semicondensed--*-*-*-*-c-*-*-*";
#if 0
"-adobe-helvetica-bold-r-*-*-10-*";
#endif
const char *osd_default_colour = "green";

/** Global error string. */
char *xosd_error;

/* Wait until display is in next state. {{{ */
static void
_wait_until_update(xosd * osd, int generation)
{
  pthread_mutex_lock(&osd->mutex_sync);
  while (osd->generation == generation) {
    DEBUG(Dtrace, "waiting %d %d", generation, osd->generation);
    pthread_cond_wait(&osd->cond_sync, &osd->mutex_sync);
  }
  pthread_mutex_unlock(&osd->mutex_sync);
}

/* }}} */

/* Serialize access to the X11 connection. {{{
 *
 * Background: xosd needs a thread which handles X11 exposures. XNextEvent()
 * blocks and would deny any other thread - especially the thread which calls
 * the xosd API - the usage of the same X11 connection. XInitThreads() can't be
 * used, because xosd is a library which can be loaded dynamically way after
 * the loading application has done its first X11 call, after which calling
 * XInitThreads() is no longer possible. (Debian-Bug #252170)
 *
 * The exposure-thread gets the MUTEX and sleeps on a select([X11,pipe]). When
 * an X11 event occurs, the tread can directly use X11 calls.
 * When another thread needs to do an X11 call, it uses _xosd_lock(osd) to
 * notify the exposure-thread via the pipe, which uses cond_wait to voluntarily
 * pass the right to access X11 to the signalling thread. The calling thread
 * acquire the MUTEX and can than use the X11 calls.
 * After using X11, the thread calls _xosd_unlock(osd) to remove its token from
 * the pipe and to wake up the exposure-thread via cond_signal, before
 * releasing the MUTEX.
 * The number of characters in the pipe is an indication for the number of
 * threads waiting for the X11-MUTEX.
 */
static /*inline */ void
_xosd_lock(xosd * osd)
{
  char c = 0;
  FUNCTION_START(Dlocking);
  if (write(osd->pipefd[1], &c, sizeof(c)) != -1) {
    pthread_mutex_lock(&osd->mutex);
  }
    FUNCTION_END(Dlocking);
}
static /*inline */ void
_xosd_unlock(xosd * osd)
{
  char c;
  int generation = osd->generation, update = osd->update;
  FUNCTION_START(Dlocking);
  if (read(osd->pipefd[0], &c, sizeof(c)) != -1) {
    pthread_cond_signal(&osd->cond_wait);
    pthread_mutex_unlock(&osd->mutex);
    if (update & UPD_show)
      _wait_until_update(osd, generation & ~1); /* no wait when already shown. */
  }
  FUNCTION_END(Dlocking);
}

/* }}} */

/* Draw percentage/slider bar. {{{ */
static void                     /*inline */
_draw_bar(xosd * osd, int nbars, int on, XRectangle * p, XRectangle * mod,
          int is_slider)
{
  int i;
  XRectangle rs[2];
  FUNCTION_START(Dfunction);

  rs[0].x = rs[1].x = mod->x + p->x;
  rs[0].y = (rs[1].y = mod->y + p->y) + p->height / 3;
  rs[0].width = mod->width + p->width * SLIDER_SCALE;
  rs[0].height = mod->height + p->height / 3;
  rs[1].width = mod->width + p->width * SLIDER_SCALE_ON;
  rs[1].height = mod->height + p->height;
  for (i = 0; i < nbars; i++, rs[0].x = rs[1].x += p->width) {
    XRectangle *r = &(rs[is_slider ? (i == on) : (i < on)]);
    XFillRectangles(osd->display, osd->mask_bitmap, osd->mask_gc, r, 1);
    XFillRectangles(osd->display, osd->line_bitmap, osd->gc, r, 1);
  }
  FUNCTION_END(Dfunction);
}
static void
draw_bar(xosd * osd, int line)
{
  struct xosd_bar *l = &osd->lines[line].bar;
  int is_slider = l->type == LINE_slider, nbars, on;
  XRectangle p, m;
  p.x = XOFFSET;
  p.y = osd->line_height * line;
  p.width = -osd->extent->y / 2;
  p.height = -osd->extent->y;

  assert(osd);
  FUNCTION_START(Dfunction);

  /* Calculate number of bars in automatic mode */
  if (osd->bar_length == -1) {
    nbars = (osd->screen_width * SLIDER_SCALE) / p.width;
    switch (osd->align) {
    case XOSD_center:
      p.x = osd->screen_width * ((1 - SLIDER_SCALE) / 2);
      break;
    case XOSD_right:
      p.x = osd->screen_width * (1 - SLIDER_SCALE);
    case XOSD_left:
      break;
    }
  } else {
    nbars = osd->bar_length;
    switch (osd->align) {
    case XOSD_center:
      p.x = (osd->screen_width - (nbars * p.width)) / 2;
      break;
    case XOSD_right:
      p.x = osd->screen_width - (nbars * p.width) - p.x;
    case XOSD_left:
      break;
    }
  }
  on = ((nbars - is_slider) * l->value) / 100;

  DEBUG(Dvalue, "percent=%d, nbars=%d, on=%d", l->value, nbars, on);

  /* Outline */
  if (osd->outline_offset) {
    m.x = m.y = -osd->outline_offset;
    m.width = m.height = 2 * osd->outline_offset;
    XSetForeground(osd->display, osd->gc, osd->outline_pixel);
    _draw_bar(osd, nbars, on, &p, &m, is_slider);
  }
  /* Shadow */
  if (osd->shadow_offset) {
    m.x = m.y = osd->shadow_offset;
    m.width = m.height = 0;
    XSetForeground(osd->display, osd->gc, osd->shadow_pixel);
    _draw_bar(osd, nbars, on, &p, &m, is_slider);
  }
  /* Bar/Slider */
  if (1) {
    m.x = m.y = m.width = m.height = 0;
    XSetForeground(osd->display, osd->gc, osd->pixel);
    _draw_bar(osd, nbars, on, &p, &m, is_slider);
  }
}

/* }}} */

/* Draw text. {{{ */
static void                     /*inline */
_draw_text(xosd * osd, char *string, int x, int y)
{
  int len = strlen(string);
  FUNCTION_START(Dfunction);
  XmbDrawString(osd->display, osd->mask_bitmap, osd->fontset, osd->mask_gc, x,
                y, string, len);
  XmbDrawString(osd->display, osd->line_bitmap, osd->fontset, osd->gc, x, y,
                string, len);
  FUNCTION_END(Dfunction);
}
static void
draw_text(xosd * osd, int line)
{
  int x = XOFFSET, y = osd->line_height * line - osd->extent->y;
  struct xosd_text *l = &osd->lines[line].text;

  assert(osd);
  FUNCTION_START(Dfunction);

  if (l->string != NULL) {
    
    if (l->width < 0) {
      XRectangle rect;
      XmbTextExtents(osd->fontset, l->string, strlen(l->string), NULL, &rect);
      l->width = rect.width;
    }

    switch (osd->align) {
    case XOSD_center:
      x = (osd->screen_width - l->width) / 2;
      break;
    case XOSD_right:
      x = osd->screen_width - l->width - x;
    case XOSD_left:
      break;
    }

    if (osd->shadow_offset) {
      XSetForeground(osd->display, osd->gc, osd->shadow_pixel);
      if (osd->shadow_direction) {
        switch(osd->shadow_direction) {
          case 0:
            _draw_text(osd, l->string, x, y - osd->shadow_offset);
            break;
          case 1:
            _draw_text(osd, l->string, x + osd->shadow_offset, y - osd->shadow_offset);
            break;
          case 2:
            _draw_text(osd, l->string, x + osd->shadow_offset, y);
            break;
          case 3:
            _draw_text(osd, l->string, x + osd->shadow_offset, y + osd->shadow_offset);
            break;
          case 4:
            _draw_text(osd, l->string, x, y + osd->shadow_offset);
            break;
          case 5:
            _draw_text(osd, l->string, x - osd->shadow_offset, y + osd->shadow_offset);
            break;
          case 6:
            _draw_text(osd, l->string, x - osd->shadow_offset, y);
            break;
          case 7:
            _draw_text(osd, l->string, x - osd->shadow_offset, y - osd->shadow_offset);
            break;
          default:
            //_draw_text(osd, l->string, x + osd->shadow_offset, y + osd->shadow_offset);
            break;
        }
      } else {
        _draw_text(osd, l->string, x +osd->shadow_offset, y + osd->shadow_offset);
      }
    }
    if (osd->outline_offset) {
      int i, j;
      XSetForeground(osd->display, osd->gc, osd->outline_pixel);
      /* FIXME: echo . | osd_cat -O 50 -p middle -A center */
      for (i = 1; i <= osd->outline_offset; i++)
        for (j = 0; j < 9; j++)
          if (j != 4)
            _draw_text(osd, l->string, x + (j / 3 - 1) * i,
                      y + (j % 3 - 1) * i);
    }
    if (1) {
      XSetForeground(osd->display, osd->gc, osd->pixel);
      _draw_text(osd, l->string, x, y);
    }
  }
}

/* }}} */

/* Handles X11 events, timeouts and does the drawing. {{{
 * This is running in it's own thread for Expose-events.
 * The order of update handling is important:
 * 1. The size must be correct -> UPD_size first
 * 2. Change the position, which might expose part of window -> UPD_pos
 * 3. The XShape must be set before something is drawn -> UPD_mask, UPD_lines
 * 4. The window should be mapped before something is drawn -> UPD_show
 * 5. Start the timer last to not account for processing time -> UPD_timer
 * If you change this order, you'll get a broken display. You've been warned!
 */
static void *
event_loop(void *osdv)
{
  xosd *osd = osdv;
  int xfd, max;

  FUNCTION_START(Dfunction);
  DEBUG(Dtrace, "event thread started");
  assert(osd);

  xfd = ConnectionNumber(osd->display);
  max = (osd->pipefd[0] > xfd) ? osd->pipefd[0] : xfd;

  pthread_mutex_lock(&osd->mutex);
  DEBUG(Dtrace, "Request exposure events");
  XSelectInput(osd->display, osd->window, ExposureMask);
  osd->update |= UPD_size | UPD_pos | UPD_mask;
  while (!osd->done) {
    int retval, line;
    fd_set readfds;
    struct timeval tv, *tvp = NULL;

    FD_ZERO(&readfds);
    FD_SET(xfd, &readfds);
    FD_SET(osd->pipefd[0], &readfds);

    /* Hide display requested. */
    if (osd->update & UPD_hide) {
      DEBUG(Dupdate, "UPD_hide");
      if (osd->generation & 1) {
        XUnmapWindow(osd->display, osd->window);
        osd->generation++;
      }
    }
    /* The font, outline or shadow was changed. Recalculate line height,
     * resize window and bitmaps. */
    if (osd->update & UPD_size) {
      XFontSetExtents *extents = XExtentsOfFontSet(osd->fontset);
      DEBUG(Dupdate, "UPD_size");
      osd->extent = &extents->max_logical_extent;
      osd->line_height = osd->extent->height + osd->shadow_offset + 2 *
        osd->outline_offset;
      osd->height = osd->line_height * osd->number_lines;
      for (line = 0; line < osd->number_lines; line++)
        if (osd->lines[line].type == LINE_text)
          osd->lines[line].text.width = -1;

      XResizeWindow(osd->display, osd->window, osd->screen_width,
                    osd->height);
      XFreePixmap(osd->display, osd->mask_bitmap);
      osd->mask_bitmap = XCreatePixmap(osd->display, osd->window,
                                       osd->screen_width, osd->height, 1);
      XFreePixmap(osd->display, osd->line_bitmap);
      osd->line_bitmap = XCreatePixmap(osd->display, osd->window,
                                       osd->screen_width, osd->height,
                                       osd->depth);
    }
    /* H/V offset or vertical positon was changed. Horizontal alignment is
     * handles internally as line realignment with UPD_content. */
    if (osd->update & UPD_pos) {
      int x = 0, y = 0;
      DEBUG(Dupdate, "UPD_pos");
      switch (osd->align) {
      case XOSD_left:
      case XOSD_center:
        x = osd->screen_xpos + osd->hoffset;
        break;
      case XOSD_right:
        x = osd->screen_xpos - osd->hoffset;
      }
      switch (osd->pos) {
      case XOSD_bottom:
        y = osd->screen_height - osd->height - osd->voffset;
        break;
      case XOSD_middle:
        y = (osd->screen_height - osd->height) / 2 - osd->voffset;
        break;
      case XOSD_top:
        y = osd->voffset;
      }
      XMoveWindow(osd->display, osd->window, x, y);
    }
    /* If the content changed, redraw lines in background buffer.
     * Also update XShape unless only colours were changed. */
    if (osd->update & (UPD_mask | UPD_lines)) {
      DEBUG(Dupdate, "UPD_lines");
      for (line = 0; line < osd->number_lines; line++) {
        int y = osd->line_height * line;
#ifdef DEBUG_XSHAPE
        XSetForeground(osd->display, osd->gc, osd->outline_pixel);
        XFillRectangle(osd->display, osd->line_bitmap, osd->gc, 0,
                       y, osd->screen_width, osd->line_height);
#endif
        if (osd->update & UPD_mask) {
          XFillRectangle(osd->display, osd->mask_bitmap, osd->mask_gc_back, 0,
                         y, osd->screen_width, osd->line_height);
        }
        switch (osd->lines[line].type) {
        case LINE_text:
          draw_text(osd, line);
          break;
        case LINE_percentage:
        case LINE_slider:
          draw_bar(osd, line);
        case LINE_blank:
          break;
        }
      }
    }
#ifndef DEBUG_XSHAPE
    /* More than colours was changed, also update XShape. */
    if (osd->update & UPD_mask) {
      DEBUG(Dupdate, "UPD_mask");
      XShapeCombineMask(osd->display, osd->window, ShapeBounding, 0, 0,
                        osd->mask_bitmap, ShapeSet);
    }
#endif
    /* Show display requested. */
    if (osd->update & UPD_show) {
      DEBUG(Dupdate, "UPD_show");
      if (~osd->generation & 1) {
        osd->generation++;
        XMapRaised(osd->display, osd->window);
      }
    }
    /* Copy content, if window was changed or exposed. */
    if ((osd->generation & 1)
        && osd->update & (UPD_size | UPD_pos | UPD_lines | UPD_show)) {
      DEBUG(Dupdate, "UPD_copy");
      XCopyArea(osd->display, osd->line_bitmap, osd->window, osd->gc, 0, 0,
                osd->screen_width, osd->height, 0, 0);
    }
    /* Flush all pennding X11 requests, if any. */
    if (osd->update & ~UPD_timer) {
      XFlush(osd->display);
      osd->update &= UPD_timer;
    }
    /* Restart the timer when requested. */
    if (osd->update & UPD_timer) {
      DEBUG(Dupdate, "UPD_timer");
      osd->update = UPD_none;
      if ((osd->generation & 1) && (osd->timeout > 0))
        gettimeofday(&osd->timeout_start, NULL);
      else
        timerclear(&osd->timeout_start);
    }
    /* Calculate timeout delta or hide display. */
    if (timerisset(&osd->timeout_start)) {
      gettimeofday(&tv, NULL);
      tv.tv_sec -= osd->timeout;
      if (timercmp(&tv, &osd->timeout_start, <)) {
        tv.tv_sec = osd->timeout_start.tv_sec - tv.tv_sec;
        tv.tv_usec = osd->timeout_start.tv_usec - tv.tv_usec;
        if (tv.tv_usec < 0) {
          tv.tv_usec += 1000000;
          tv.tv_sec -= 1;
        }
        tvp = &tv;
      } else {
        timerclear(&osd->timeout_start);
        if (osd->generation & 1)
          osd->update |= UPD_hide;
        continue;               /* Hide the window first and than restart the loop */
      }
    }

    /* Signal update */
    pthread_mutex_lock(&osd->mutex_sync);
    pthread_cond_broadcast(&osd->cond_sync);
    pthread_mutex_unlock(&osd->mutex_sync);

    /* Wait for the next X11 event or an API request via the pipe. */
    retval = select(max + 1, &readfds, NULL, NULL, tvp);
    DEBUG(Dvalue, "SELECT=%d PIPE=%d X11=%d", retval,
          FD_ISSET(osd->pipefd[0], &readfds), FD_ISSET(xfd, &readfds));

    if (retval == -1 && errno == EINTR) {
      DEBUG(Dselect, "select() EINTR");
      continue;
    } else if (retval == -1) {
      DEBUG(Dselect, "select() error %d", errno);
      osd->done = 1;
      break;
    } else if (retval == 0) {
      DEBUG(Dselect, "select() timeout");
      continue;                 /* timeout */
    } else if (FD_ISSET(osd->pipefd[0], &readfds)) {
      /* Another thread wants to use the X11 connection */
      pthread_cond_wait(&osd->cond_wait, &osd->mutex);
      DEBUG(Dselect, "Resume exposure thread after X11 call");
      continue;
    } else if (FD_ISSET(xfd, &readfds)) {
      XEvent report;
      /* There is a event, but it might not be an Exposure-event, so don't use
       * XWindowEvent(), since that might block. */
      XNextEvent(osd->display, &report);
      /* ignore sent by server/manual send flag */
      switch (report.type & 0x7f) {
      case Expose:
        {
          XExposeEvent *XE = &report.xexpose;
          /* http://x.holovko.ru/Xlib/chap10.html#10.9.1 */
          DEBUG(Dvalue, "expose %d: x=%d y=%d w=%d h=%d", XE->count,
                XE->x, XE->y, XE->width, XE->height);
#if 0
          if (report.xexpose.count == 0) {
            int ytop, ybot;
            ytop = report.xexpose.y / osd->line_height;
            ybot =
              (report.xexpose.y + report.xexpose.height) / osd->line_height;
            do {
              osd->lines[ytop].width = -1;
            } while (ytop++ < ybot);
          }
#endif
          XCopyArea(osd->display, osd->line_bitmap, osd->window, osd->gc,
                    report.xexpose.x, report.xexpose.y, report.xexpose.width,
                    report.xexpose.height, report.xexpose.x, report.xexpose.y);
          break;
        }
      case GraphicsExpose:
        {
          XGraphicsExposeEvent *XE = &report.xgraphicsexpose;
          DEBUG(Dvalue, "gfxexpose %d: x=%d y=%d w=%d h=%d code=%d",
                XE->count, XE->x, XE->y, XE->width, XE->height, XE->major_code);
          break;
        }
      case NoExpose:
        {
          XNoExposeEvent *XE = &report.xnoexpose;
          DEBUG(Dvalue, "noexpose: code=%d", XE->major_code);
          break;
        }
      default:
        DEBUG(Dvalue, "XEvent=%d", report.type);
        break;
      }
      continue;
    } else {
      DEBUG(Dselect, "select() FATAL %d", retval);
      exit(-1);                 /* Impossible */
    }
  }
  pthread_mutex_unlock(&osd->mutex);

  return NULL;
}

/* }}} */

/* Parse textual colour value. {{{ */
static int
parse_colour(xosd * osd, XColor * col, unsigned long *pixel,
             const char *colour)
{
  Colormap colourmap;
  int retval = 0;

  FUNCTION_START(Dfunction);
  DEBUG(Dtrace, "getting colourmap");
  colourmap = DefaultColormap(osd->display, osd->screen);

  DEBUG(Dtrace, "parsing colour");
  if (XParseColor(osd->display, colourmap, colour, col)) {
    DEBUG(Dtrace, "attempting to allocate colour");
    if (XAllocColor(osd->display, colourmap, col)) {
      DEBUG(Dtrace, "allocation sucessful");
      *pixel = col->pixel;
    } else {
      DEBUG(Dtrace, "defaulting to white. could not allocate colour");
      *pixel = WhitePixel(osd->display, osd->screen);
      retval = -1;
    }
  } else {
    DEBUG(Dtrace, "could not poarse colour. defaulting to white");
    *pixel = WhitePixel(osd->display, osd->screen);
    retval = -1;
  }

  return retval;
}

/* }}} */

/* Tell window manager to put window topmost. {{{ */
void
stay_on_top(Display * dpy, Window win)
{
  Atom gnome, net_wm, type;
  int format;
  unsigned long nitems, bytesafter;
  unsigned char *args = NULL;
  Window root = DefaultRootWindow(dpy);

  FUNCTION_START(Dfunction);
  /*
   * build atoms 
   */
  gnome = XInternAtom(dpy, "_WIN_SUPPORTING_WM_CHECK", False);
  net_wm = XInternAtom(dpy, "_NET_SUPPORTED", False);

  /*
   * gnome-compilant 
   * tested with icewm + WindowMaker 
   */
  if (Success == XGetWindowProperty
      (dpy, root, gnome, 0, (65536 / sizeof(long)), False,
       AnyPropertyType, &type, &format, &nitems, &bytesafter, &args) &&
      nitems > 0) {
    /*
     * FIXME: check capabilities 
     */
    XClientMessageEvent xev;
    Atom gnome_layer = XInternAtom(dpy, "_WIN_LAYER", False);

    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.window = win;
    xev.message_type = gnome_layer;
    xev.format = 32;
    xev.data.l[0] = 6 /* WIN_LAYER_ONTOP */ ;

    XSendEvent(dpy, DefaultRootWindow(dpy), False, SubstructureNotifyMask,
               (XEvent *) & xev);
    XFree(args);
  }
  /*
   * netwm compliant.
   * tested with kde 
   */
  else if (Success == XGetWindowProperty
           (dpy, root, net_wm, 0, (65536 / sizeof(long)), False,
            AnyPropertyType, &type, &format, &nitems, &bytesafter, &args)
           && nitems > 0) {
    XEvent e;
    Atom net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom net_wm_top = XInternAtom(dpy, "_NET_WM_STATE_STAYS_ON_TOP", False);

    memset(&e, 0, sizeof(e));
    e.xclient.type = ClientMessage;
    e.xclient.message_type = net_wm_state;
    e.xclient.display = dpy;
    e.xclient.window = win;
    e.xclient.format = 32;
    e.xclient.data.l[0] = 1 /* _NET_WM_STATE_ADD */ ;
    e.xclient.data.l[1] = net_wm_top;
    e.xclient.data.l[2] = 0l;
    e.xclient.data.l[3] = 0l;
    e.xclient.data.l[4] = 0l;

    XSendEvent(dpy, DefaultRootWindow(dpy), False,
               SubstructureRedirectMask, &e);
    XFree(args);
  }
  XRaiseWindow(dpy, win);
}

/* }}} */

/* xosd_init -- Create a new xosd "object" {{{
 * Deprecated: Use xosd_create. */
xosd *
xosd_init(const char *font, const char *colour, int timeout, xosd_pos pos,
          int voffset, int shadow_offset, int number_lines)
{
  xosd *osd = xosd_create(number_lines);

  FUNCTION_START(Dfunction);
  if (xosd_set_font(osd, font) == -1) {
    xosd_destroy(osd);
    /*
     * we do not set xosd_error, as set_font has already set it to 
     * a sensible error message. 
     */
    return NULL; 
  }
  if(osd != NULL) {
    xosd_set_colour(osd, colour);
    xosd_set_timeout(osd, timeout);
    xosd_set_pos(osd, pos);
    xosd_set_vertical_offset(osd, voffset);
    xosd_set_shadow_offset(osd, shadow_offset);
  }

  return osd;
}

/* }}} */

/* xosd_ -- Create a new xosd "object" with the same attributes as the provided xosd "object" {{{ */
xosd *
xosd_clone(xosd * osd2)
{
  xosd *osd = xosd_create(osd2->number_lines);
  osd->align = osd2->align;
  osd->bar_length = osd2->bar_length;
  osd->shadow_colour = osd2->shadow_colour;
  osd->shadow_pixel = osd2->shadow_pixel;
/* Copying original lines to the cloned xosd instance causes unintuitive behaviour
  osd->lines = osd2->lines; */
  osd->pos = osd2->pos;
  osd->hoffset = osd2->hoffset;
  osd->voffset = osd2->voffset;
  osd->colour = osd2->colour;
  osd->pixel = osd2->pixel;
  osd->outline_colour = osd2->outline_colour;
  osd->outline_pixel = osd2->outline_pixel;
  osd->shadow_offset = osd2->shadow_offset;
  osd->shadow_direction = osd2->shadow_direction;
  osd->outline_offset = osd2->outline_offset;
  osd->screen_height = osd2->screen_height;
  osd->screen_width = osd2->screen_width;
  osd->screen_xpos = osd2->screen_xpos;
  osd->nscreens = osd2->nscreens;
  return osd;
}

/* xosd_create -- Create a new xosd "object" {{{ */
xosd *
xosd_create(int number_lines)
{
  xosd *osd;
  int event_basep, error_basep, i;
  char *display;
  XSetWindowAttributes setwinattr;
  XGCValues xgcv = { .graphics_exposures = False };

  FUNCTION_START(Dfunction);
  DEBUG(Dtrace, "getting display");
  display = getenv("DISPLAY");
  if (!display) {
    xosd_error = "No display";
    osd = NULL;
  }

  DEBUG(Dtrace, "Mallocing osd");
  osd = malloc(sizeof(xosd));
  memset(osd, 0, sizeof(xosd));
  if (osd == NULL) {
    xosd_error = "Out of memory";
    free(osd);
  }

  DEBUG(Dtrace, "Creating pipe");
  if (pipe(osd->pipefd) == -1) {
    xosd_error = "Error creating pipe";
    osd = NULL;
  }

  DEBUG(Dtrace, "initializing mutex");
  pthread_mutex_init(&osd->mutex, NULL);
  pthread_mutex_init(&osd->mutex_sync, NULL);
  DEBUG(Dtrace, "initializing condition");
  pthread_cond_init(&osd->cond_wait, NULL);
  pthread_cond_init(&osd->cond_sync, NULL);

  DEBUG(Dtrace, "initializing number lines");
  osd->number_lines = number_lines;
  osd->lines = malloc(sizeof(union xosd_line) * osd->number_lines);
  if (osd->lines == NULL) {
    xosd_error = "Out of memory";
    pthread_cond_destroy(&osd->cond_sync);
    pthread_cond_destroy(&osd->cond_wait);
    pthread_mutex_destroy(&osd->mutex_sync);
    pthread_mutex_destroy(&osd->mutex);
    close(osd->pipefd[0]);
    close(osd->pipefd[1]);
  }

  for (i = 0; i < osd->number_lines; i++)
    memset(&osd->lines[i], 0, sizeof(union xosd_line));

  DEBUG(Dtrace, "misc osd variable initialization");
  osd->generation = 0;
  osd->done = 0;
  osd->pos = XOSD_top;
  osd->hoffset = 0;
  osd->align = XOSD_left;
  osd->voffset = 0;
  osd->timeout = -1;
  timerclear(&osd->timeout_start);
  osd->fontset = NULL;
  osd->bar_length = -1;         /* old automatic width calculation */

  DEBUG(Dtrace, "Display query");
  osd->display = XOpenDisplay(display);
  if (!osd->display) {
    xosd_error = "Cannot open display";
    free(osd->lines);
  }
  osd->screen = XDefaultScreen(osd->display);

  DEBUG(Dtrace, "x shape extension query");
  if (!XShapeQueryExtension(osd->display, &event_basep, &error_basep)) {
    xosd_error = "X-Server does not support shape extension";
    XCloseDisplay(osd->display);
  }

  osd->visual = DefaultVisual(osd->display, osd->screen);
  osd->depth = DefaultDepth(osd->display, osd->screen);

  DEBUG(Dtrace, "font selection info");
  xosd_set_font(osd, osd_default_font);
  if (osd->fontset == NULL) {
    /*
     * if we still don't have a fontset, then abort 
     */
    xosd_error = "Default font not found";
    XCloseDisplay(osd->display);
  }

  DEBUG(Dtrace, "width and height initialization");

  xosd_monitor(osd, 1);

  osd->line_height = 10 /*Dummy value */ ;
  osd->height = osd->line_height * osd->number_lines;

  DEBUG(Dtrace, "creating X Window");
  setwinattr.override_redirect = 1;

  osd->window = XCreateWindow(osd->display,
                              XRootWindow(osd->display, osd->screen),
                              0, 0,
                              osd->screen_width, osd->height,
                              0,
                              osd->depth,
                              CopyFromParent,
                              osd->visual, CWOverrideRedirect, &setwinattr);
  XStoreName(osd->display, osd->window, "XOSD");

  osd->mask_bitmap =
    XCreatePixmap(osd->display, osd->window, osd->screen_width,
                  osd->height, 1);
  osd->line_bitmap =
    XCreatePixmap(osd->display, osd->window, osd->screen_width,
                  osd->line_height, osd->depth);

  osd->gc = XCreateGC(osd->display, osd->window, GCGraphicsExposures, &xgcv);
  osd->mask_gc = XCreateGC(osd->display, osd->mask_bitmap, GCGraphicsExposures, &xgcv);
  osd->mask_gc_back = XCreateGC(osd->display, osd->mask_bitmap, GCGraphicsExposures, &xgcv);

  XSetBackground(osd->display, osd->gc,
                 WhitePixel(osd->display, osd->screen));

  XSetForeground(osd->display, osd->mask_gc_back,
                 BlackPixel(osd->display, osd->screen));
  XSetBackground(osd->display, osd->mask_gc_back,
                 WhitePixel(osd->display, osd->screen));

  XSetForeground(osd->display, osd->mask_gc,
                 WhitePixel(osd->display, osd->screen));
  XSetBackground(osd->display, osd->mask_gc,
                 BlackPixel(osd->display, osd->screen));


  DEBUG(Dtrace, "setting colour");
  xosd_set_colour(osd, osd_default_colour);

  DEBUG(Dtrace, "stay on top");
  stay_on_top(osd->display, osd->window);

  DEBUG(Dtrace, "initializing event thread");
  pthread_create(&osd->event_thread, NULL, event_loop, osd);

  return osd;
}

/* }}} */

/* xosd_monitor -- swap the input xosd window's monitor parameters to correspond with desired screen */
int 
xosd_monitor(xosd * osd, int monitor)
{
   int return_value = -1;  
   if (osd != NULL) {     
    #ifdef HAVE_XINERAMA
      int nscreens;
      XineramaScreenInfo *screeninfo = NULL;
      int dummy_a, dummy_b;
    #endif
    monitor--;

    FUNCTION_START(Dfunction);

    _xosd_lock(osd);

    #ifdef HAVE_XINERAMA
      if (XineramaQueryExtension(osd->display, &dummy_a, &dummy_b) &&
          (screeninfo = XineramaQueryScreens(osd->display, &nscreens)) &&
        XineramaIsActive(osd->display) && (nscreens > monitor) && (monitor >= 0)) {
      osd->screen_width = screeninfo[monitor].width;
      osd->screen_height = screeninfo[monitor].height;
      osd->screen_xpos = screeninfo[monitor].x_org;
      osd->nscreens = nscreens;
      return_value = 0;
      } else
    #endif
    {
      osd->screen_width = XDisplayWidth(osd->display, osd->screen);
      osd->screen_height = XDisplayHeight(osd->display, osd->screen);
      osd->screen_xpos = 0;
      xosd_error = "Error getting screen info from Xinerama";
      return_value = -1;

    }
    #ifdef HAVE_XINERAMA
      if (screeninfo) {
        XFree(screeninfo);
      }
    #endif
    osd->update |= UPD_pos;
    _xosd_unlock(osd);
  }
  return return_value;
}

/* }}} */

/* display_info_driver -- main for pthreads in display_info */
void* 
display_info_driver() 
{
  xosd * screen = xosd_create(1);
  int nscreens = screen->nscreens;
  int return_value = -1;
  char word[256];
  xosd * osd = malloc(sizeof(xosd*)*nscreens*3);
  xosd ** osdptr = malloc(sizeof(xosd)*nscreens*3);
  for (int i = 0; i < nscreens*3+1; i++) {
    osdptr[i] = osd+i;
  }
  osdptr[0] = xosd_create(2);
  if (osdptr[0] != NULL) {

    FUNCTION_START(Dfunction);

    xosd_set_outline_offset(osdptr[0], 1);
    xosd_set_timeout(osdptr[0], 1);
    xosd_set_font(osdptr[0], (char *) osd_default_font);
      
    for (int i = 0; i < nscreens; i++) {
      osdptr[3*i] = xosd_clone(osdptr[0]);
      xosd_monitor(osdptr[3*i], i+1);
      xosd_set_align(osdptr[3*i], XOSD_center);
      xosd_set_pos(osdptr[3*i], XOSD_middle);
      sprintf(word, "%d", i+1);
      xosd_display(osdptr[3*i], 0, XOSD_string, word);
  
      osdptr[3*i+1] = xosd_clone(osdptr[0]);
      xosd_monitor(osdptr[3*i+1], i+1);
      xosd_set_align(osdptr[3*i+1], XOSD_left);
      xosd_set_pos(osdptr[3*i+1], XOSD_top);
      sprintf(word, "%d", osdptr[i]->screen_width);
      xosd_display(osdptr[3*i+1], 0, XOSD_string, word);
      
      osdptr[3*i+2] = xosd_clone(osdptr[0]);
      xosd_monitor(osdptr[3*i+2], i+1);
      xosd_set_align(osdptr[3*i+2], XOSD_right);
      xosd_set_pos(osdptr[3*i+2], XOSD_top);
      sprintf(word, "%d", osdptr[3*i+2]->screen_height);
      xosd_display(osdptr[3*i+2], 0, XOSD_string, word);
    }
    sleep(15);
    for (int i = 0; i < nscreens*3; i++) {
      xosd_destroy(osdptr[i]);    
    }
    xosd_destroy(screen);
    free(osd);
    free(osdptr);
    return_value = 0;
    pthread_exit(&return_value);
  }
  xosd_destroy(screen);
  pthread_exit(&return_value);
}

/* display_info -- graphically displays screen parameters */
int 
display_info()
{
  int i = -1;
  pthread_t id;
  pthread_create(&id, NULL, display_info_driver, &i);
  return i;
}

/* }}} */

/* screen_info -- returns the number of back-end screens on the X11 connection */
int 
screen_count(xosd * osd)
{
  int return_value;
  if (osd->nscreens) {
    return_value = osd->nscreens;
  }
  else {
    return_value = -1;
  }
  return return_value;
}

/* }}} */

/* xosd_uninit -- Destroy an xosd "object" {{{
 * Deprecated: Use xosd_destroy. */
int
xosd_uninit(xosd * osd)
{
  FUNCTION_START(Dfunction);
  return xosd_destroy(osd);
}

/* }}} */

/* xosd_destroy -- Destroy an xosd "object" {{{ */
int
xosd_destroy(xosd * osd)
{
  int i;
  FUNCTION_START(Dfunction);
  if (osd != NULL) {

    DEBUG(Dtrace, "waiting for threads to exit");
    _xosd_lock(osd);
    osd->done = 1;
    _xosd_unlock(osd);

    DEBUG(Dtrace, "join threads");
    pthread_join(osd->event_thread, NULL);

    DEBUG(Dtrace, "freeing X resources");
    XFreeGC(osd->display, osd->gc);
    XFreeGC(osd->display, osd->mask_gc);
    XFreeGC(osd->display, osd->mask_gc_back);
    XFreePixmap(osd->display, osd->line_bitmap);
    XFreeFontSet(osd->display, osd->fontset);
    XFreePixmap(osd->display, osd->mask_bitmap);
    XDestroyWindow(osd->display, osd->window);

    XCloseDisplay(osd->display);

    DEBUG(Dtrace, "freeing lines");
    for (i = 0; i < osd->number_lines; i++)
      if (osd->lines[i].type == LINE_text && osd->lines[i].text.string)
        free(osd->lines[i].text.string);
    free(osd->lines);

    DEBUG(Dtrace, "destroying condition and mutex");
    pthread_cond_destroy(&osd->cond_sync);
    pthread_cond_destroy(&osd->cond_wait);
    pthread_mutex_destroy(&osd->mutex_sync);
    pthread_mutex_destroy(&osd->mutex);
    close(osd->pipefd[0]);
    close(osd->pipefd[1]);

    DEBUG(Dtrace, "freeing osd structure");
    free(osd);

    FUNCTION_END(Dfunction);
  }
  return 0;
}

/* }}} */

/* xosd_set_bar_length  -- Set length of percentage and slider bar {{{ */
int
xosd_set_bar_length(xosd * osd, int length)
{
  FUNCTION_START(Dfunction);
  int return_val = -1;
  if (osd != NULL && length != 0 && length > 0) {
    osd->bar_length = length;
    return_val = 0;
  } 
  return return_val;
  
}

/* }}} */

/* xosd_display -- Display information {{{ */
int
xosd_display(xosd * osd, int line, xosd_command command, ...)
{
  int return_value = -1;
  union xosd_line newline = { type:LINE_blank };
  va_list a;

  FUNCTION_START(Dfunction);
  if (osd != NULL && (line >= 0 && line < osd->number_lines)) {
    va_start(a, command);
    switch (command) {
    case XOSD_string:
    case XOSD_printf:
      {
        char buf[XOSD_MAX_PRINTF_BUF_SIZE];
        struct xosd_text *l = &newline.text;
        char *string = va_arg(a, char *);
        if (command == XOSD_printf) {
          if (vsnprintf(buf, sizeof(buf), string, a) >= sizeof(buf)) {
            xosd_error = "xosd_display: Buffer too small";
            va_end(a);
            return_value = -1;
          }
          string = buf;
        }
        if (string && *string) {
          return_value = strlen(string);
          l->type = LINE_text;
          l->string = malloc(return_value + 1);
          memcpy(l->string, string, return_value + 1);
        } else {
          return_value = 0;
          l->type = LINE_blank;
        }
        l->width = -1;
        break;
      }

    case XOSD_percentage:
    case XOSD_slider:
      {
        struct xosd_bar *l = &newline.bar;
        return_value = va_arg(a, int);
        return_value = (return_value < 0) ? 0 : (return_value > 100) ? 100 : return_value;
        l->type = (command == XOSD_percentage) ? LINE_percentage : LINE_slider;
        l->value = return_value;
        break;
      }

    default:
      {
        xosd_error = "xosd_display: Unknown command";
        va_end(a);
        return_value = -1;
      }
    }

    _xosd_lock(osd);
    /* Free old entry */
    switch (osd->lines[line].type) {
    case LINE_text:
      free(osd->lines[line].text.string);
    case LINE_blank:
    case LINE_percentage:
    case LINE_slider:
      break;
    }
    osd->lines[line] = newline;
    osd->update |= UPD_content | UPD_timer | UPD_show;
    _xosd_unlock(osd);

  }
  
  return return_value; 
}

/* }}} */

/* xosd_is_onscreen -- Returns weather the display is show {{{ */
int
xosd_is_onscreen(xosd * osd)
{
  int return_val = -1;

  FUNCTION_START(Dfunction);
  if (osd != NULL) {
    return_val = osd->generation & 1;
  }

  return return_val;
}

/* }}} */

/* xosd_wait_until_no_display -- Wait until nothing is displayed {{{ */
int
xosd_wait_until_no_display(xosd * osd)
{
  int generation;
  int return_val = -1;

  FUNCTION_START(Dfunction);
  if (osd != NULL) {
    return_val = 0;
    if ((generation = osd->generation) & 1)
      _wait_until_update(osd, generation);

    FUNCTION_END(Dfunction);
  }

  return return_val;
}

/* }}} */

/* xosd_set_colour -- Change the colour of the display {{{ */
int
xosd_set_colour(xosd * osd, const char *colour)
{
  int return_val = -1;

  FUNCTION_START(Dfunction);
  if (osd != NULL) {
    _xosd_lock(osd);
    return_val = parse_colour(osd, &osd->colour, &osd->pixel, colour);
    osd->update |= UPD_lines;
    _xosd_unlock(osd);
  }

  return return_val;
}

/* }}} */

/* xosd_set_shadow_colour -- Change the colour of the shadow {{{ */
int
xosd_set_shadow_colour(xosd * osd, const char *colour)
{
  int return_val = -1;

  FUNCTION_START(Dfunction);
  if (osd != NULL) {
    _xosd_lock(osd);
    return_val = parse_colour(osd, &osd->shadow_colour, &osd->shadow_pixel, colour);
    osd->update |= UPD_lines;
    _xosd_unlock(osd);
  }

  return return_val;
}

/* }}} */

/* xosd_set_outline_colour -- Change the colour of the outline {{{ */
int
xosd_set_outline_colour(xosd * osd, const char *colour)
{
  int return_val = -1;

  FUNCTION_START(Dfunction);
  if (osd != NULL) {
    _xosd_lock(osd);
    return_val =
      parse_colour(osd, &osd->outline_colour, &osd->outline_pixel, colour);
    osd->update |= UPD_lines;
    _xosd_unlock(osd);
  }

  return return_val;
}

/* }}} */

/* xosd_set_font -- Change the text-display font {{{
 * Might return error if fontset can't be created. **/
int
xosd_set_font(xosd * osd, const char *font)
{
  XFontSet fontset2;
  char **missing;
  int nmissing;
  char *defstr;
  int return_val = -1;

  FUNCTION_START(Dfunction);
  if (osd != NULL && font != NULL) {
    /*
    * Try to create the new font. If it doesn't succeed, keep old font. 
    */
    _xosd_lock(osd);
    fontset2 = XCreateFontSet(osd->display, font, &missing, &nmissing, &defstr);
    XFreeStringList(missing);
    if (fontset2 == NULL) {
      xosd_error = "Requested font not found";
      return_val = -1;
    } else {
      if (osd->fontset != NULL)
        XFreeFontSet(osd->display, osd->fontset);
      osd->fontset = fontset2;
      osd->update |= UPD_font;
      return_val = 0;
    }
    _xosd_unlock(osd);
  }

  return return_val;
}

/* }}} */

/* xosd_set_shadow_offset -- Change the offset of the text shadow {{{ */
int
xosd_set_shadow_offset(xosd * osd, int shadow_offset)
{
  int return_val = -1;

  FUNCTION_START(Dfunction);
  if (osd != NULL && shadow_offset > 0) {
    _xosd_lock(osd);
    osd->shadow_offset = shadow_offset;
    osd->update |= UPD_font;
    _xosd_unlock(osd);
    return_val = 0;
  }

  return return_val;
}

/* }}} */

/* xosd_set_shadow_direction -- Change the direction of the text shadow {{{*/
int
xosd_set_shadow_direction(xosd * osd, int shadow_direction) 
{
  int return_val = -1;

  FUNCTION_START(Dfunction);
  if (osd != NULL && (shadow_direction >= 0 && shadow_direction < 7)) {
    _xosd_lock(osd);
    osd->shadow_direction = shadow_direction;
    osd->update |= UPD_font;
    _xosd_unlock(osd);
    return_val = 0;
  }

  return return_val;
}

/* xosd_set_outline_offset -- Change the offset of the text outline {{{ */
int
xosd_set_outline_offset(xosd * osd, int outline_offset)
{
  int return_val = -1;

  FUNCTION_START(Dfunction);
  if (osd != NULL && outline_offset >= 0) {
    _xosd_lock(osd);
    osd->outline_offset = outline_offset;
    osd->update |= UPD_font;
    _xosd_unlock(osd);
    return_val = 0;
  }

  return return_val;
}

/* }}} */

/* xosd_set_vertical_offset -- Change the number of pixels the display is offset from the position {{{ */
int
xosd_set_vertical_offset(xosd * osd, int voffset)
{
  int return_val = -1;

  FUNCTION_START(Dfunction);
  if (osd != NULL) {
    _xosd_lock(osd);
    osd->voffset = voffset;
    osd->update |= UPD_pos;
    _xosd_unlock(osd);
    return_val = 0;
  }

  return return_val;
}

/* }}} */

/* xosd_set_horizontal_offset -- Change the number of pixels the display is offset from the position {{{ */
int
xosd_set_horizontal_offset(xosd * osd, int hoffset)
{
  int return_val = -1;

  FUNCTION_START(Dfunction);
  if (osd != NULL) {
    _xosd_lock(osd);
    osd->hoffset = hoffset;
    osd->update |= UPD_pos;
    _xosd_unlock(osd);
    return_val = 0;
  }

  return return_val;
}

/* }}} */

/* xosd_set_pos -- Change the vertical position of the display {{{ */
int
xosd_set_pos(xosd * osd, xosd_pos pos)
{
  int return_val = -1;

  FUNCTION_START(Dfunction);
  if (osd != NULL) {
    _xosd_lock(osd);
    osd->pos = pos;
    osd->update |= UPD_pos;
    _xosd_unlock(osd);
    return_val = 0;
  }

  return return_val;
}

/* }}} */

/* xosd_set_align -- Change the horizontal alignment of the display {{{ */
int
xosd_set_align(xosd * osd, xosd_align align)
{
  int return_val = -1;

  FUNCTION_START(Dfunction);
  if (osd != NULL) {
    _xosd_lock(osd);
    osd->align = align;
    osd->update |= UPD_content;   /* XOSD_right depends on text width */
    _xosd_unlock(osd);
    return_val = 0;
  }

  return return_val;
}

/* }}} */

/* xosd_get_colour -- Gets the RGB value of the display's colour {{{ */
int
xosd_get_colour(xosd * osd, int *red, int *green, int *blue)
{
  int return_val = -1;

  FUNCTION_START(Dfunction);
  if (osd != NULL) {
    if (red)
      *red = osd->colour.red;
    if (blue)
      *blue = osd->colour.blue;
    if (green)
      *green = osd->colour.green;

    return_val = 0;
}

  return return_val;
}

/* }}} */

/* xosd_set_timeout -- Change the time before display is hidden. {{{ */
int
xosd_set_timeout(xosd * osd, int timeout)
{
  int return_val = -1;

  FUNCTION_START(Dfunction);
  if (osd != NULL) {
    _xosd_lock(osd);
    osd->timeout = timeout;
    osd->update |= UPD_timer;
    _xosd_unlock(osd);
    return_val = 0;
  }

  return return_val;
}

/* }}} */

/* xosd_hide -- hide the display {{{ */
int
xosd_hide(xosd * osd)
{
  int return_val = -1;

  FUNCTION_START(Dfunction);
  if (osd != NULL) {
    if (osd->generation & 1) {
      _xosd_lock(osd);
      osd->update &= ~UPD_show;
      osd->update |= UPD_hide;
      _xosd_unlock(osd);
      return_val = 0;
    }
  }

  return return_val;
}

/* }}} */

/* xosd_show -- Show the display after being hidden {{{ */
int
xosd_show(xosd * osd)
{
  int return_val = -1;

  FUNCTION_START(Dfunction);
  if (osd != NULL) {
    if (~osd->generation & 1) {
      _xosd_lock(osd);
      osd->update &= ~UPD_hide;
      osd->update |= UPD_show | UPD_timer;
      _xosd_unlock(osd);
      return_val = 0;
    }
  }

  return return_val;
}

/* }}} */

/* xosd_scroll -- Scroll the display up "lines" number of lines {{{ */
int
xosd_scroll(xosd * osd, int lines)
{
  int i;
  union xosd_line *src, *dst;
  int return_val = -1;

  FUNCTION_START(Dfunction);
  if (osd != NULL && (lines > 0 && lines <= osd->number_lines)) {
    _xosd_lock(osd);
    /* Clear old text */
    for (i = 0, src = osd->lines; i < lines; i++, src++)
      if (src->type == LINE_text && src->text.string) {
        free(src->text.string);
        src->text.string = NULL;
      }
    /* Move following lines forward */
    for (dst = osd->lines; i < osd->number_lines; i++)
      *dst++ = *src++;
    /* Blank new lines */
    for (; dst < src; dst++) {
      dst->type = LINE_blank;
      dst->text.string = NULL;
    }
    osd->update |= UPD_content;
    _xosd_unlock(osd);
    return_val = 0;
  }

  return return_val;
}

/* }}} */

/* xosd_get_number_lines -- Get the maximum number of lines allowed {{{ */
int
xosd_get_number_lines(xosd * osd)
{
  int return_val = -1;
  FUNCTION_START(Dfunction);
  if (osd != NULL) {
    return_val = osd->number_lines;
  }
    
  return return_val;
}

/* }}} */

/* vim: foldmethod=marker tabstop=2 shiftwidth=2 expandtab
 */

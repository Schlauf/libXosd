#ifndef XOSD_H
#define XOSD_H


#ifdef __cplusplus
extern "C"
{
#endif

#ifndef __deprecated
#if ( __GNUC__ == 3 && __GNUC_MINOR__ > 0 ) || __GNUC__ > 3
#define __deprecated	__attribute__((deprecated))
#else
#define __deprecated
#endif
#endif

/* Error message when a routine returns failure */
  extern char *xosd_error;
  extern const char *osd_default_font;
  extern const char *osd_default_colour;

/* The XOSD display "object" */
  typedef struct xosd xosd;

/* The type of data that can be displayed. */
  typedef enum
  {
    XOSD_percentage,            /* Percentage bar (like a progress bar) */
    XOSD_string,                /* Text */
    XOSD_printf,                /* Formatted Text */
    XOSD_slider                 /* Slider (like a volume control) */
  } xosd_command;

/* Position of the display */
  typedef enum
  {
    XOSD_top = 0,               /* Top of the screen. */
    XOSD_bottom,                /* Bottom of the screen. */
    XOSD_middle                 /* middle of the screen. */
  } xosd_pos;

/* Alignment of the display */
  typedef enum
  {
    XOSD_left = 0,
    XOSD_center,
    XOSD_right
  } xosd_align;

/* xosd_clone -- Create a new xosd object with the same attributes as the input xosd object
 *
 * ARGUMENTS
 *    None.
 *
 * RETURNS
 *    A new xosd structure.
 */
xosd *xosd_clone(xosd * osd2);


/* xosd_create -- Create a new xosd "object"
 *
 * ARGUMENTS
 *    number_lines   Number of lines of the display.
 *
 * RETURNS
 *    A new xosd structure.
 */
 xosd *xosd_create(int number_lines);



/* xosd_monitor -- Switch an xosd objects default position to a chosen monitor
 *
 * ARGUMENTS
 *    osd             xosd object to modify.
 *    monitor         monitor the xosd object should be positioned on.
 *
 * RETURNS
 *    0 on success.
 *   -1 on failure.
 */
 int xosd_monitor(xosd * osd, int monitor);
 
 
/* display_info -- Displays monitor information using xosd objects, monitor index in the center of
 * each screen, horizontal resolution in top left and vertical resolution in the top right
 *
 * ARGUMENTS
 *    None.
 *
 * RETURNS
 *    0 on success.
 *   -1 on failure.
 */ 
 int display_info();
 
 /* screen_count -- return number of back-end screens on the X11 connection
 *
 * ARGUMENTS
 *    osd             Source X11 connection
 *
 * RETURNS
 *    number of back-end screens on success.
 *   -1 on failure.
 */
 int screen_count(xosd * osd);

/* xosd_init -- Create a new xosd "object" -- deprecated by xosd_create
 *
 * ARGUMENTS
 *     font           X Logical Font Descriptor, see "xfontsel".
 *     colour         X colour, see "rgb.txt" that comes with your X11
 *                    distribution.
 *     timeout        Seconds before the display is hidden (-1 for
 *                    never).
 *     pos            Position to write text (top or bottom).
 *     offset         Number of pixels between the "pos" and the
 *                    text.
 *     shadow_offset  Number of pixels that the shadow is offset from
 *                    the text.
 *     number_lines   Number of lines of the display.
 *
 * RETURNS
 *     A new xosd structure.
 */
  xosd *__deprecated xosd_init(const char *font, const char *colour,
                               int timeout, xosd_pos pos, int offset,
                               int shadow_offset, int number_lines);

/* xosd_destroy -- Destroy a xosd "object"
 * 
 * Not intended to be used on its own, acts as a helper
 * method for the display_info tool
 *
 * ARGUMENTS
 *     osd  The xosd "object" to destroy.
 *
 * RETURNS
 *   0 on success
 *  -1 on failure
 */
  int xosd_destroy(xosd * osd);

/* xosd_uninit -- Destroy a xosd "object -- deprecated by xosd_destroy
 *
 * ARGUMENTS
 *     osd  The xosd "object" to destroy.
 *
 * RETURNS
 *   0 on success
 *  -1 on failure
 */
  int __deprecated xosd_uninit(xosd * osd);

/* xosd_set_bar_length  -- Set length of percentage and slider bar
 *
 * ARGUMENTS
 *     osd     The xosd "object"
 *     length  Desired bar length (set to -1 for old behaviour)
 *
 * RETURNS
 *     -1 on error (invalid xosd object).
 *      0 on success
*/
  int xosd_set_bar_length(xosd * osd, int length);

/* xosd_display -- Display information
 *
 * ARGUMENTS
 *     osd      The xosd "object".
 *     line     Which one of "NLINES" to display.
 *     command  The type of information to display.
 *     ...      The argument to "command":
 *                  int     (between 0 and 100) if "command" is
 *                          "XOSD_percentage",
 *                  char *  if "command" is "XOSD_string",
 *                  int     (between 0 and 100) if "command" is
 *                          "XOSD_slider".
 * RETURNS
 *     The percentage (between 0 and 100) for "XOSD_percentage" or
 *     "XOSD_slider", or the number of characters displayed for
 *     "XOSD_string". -1 is returned on failure.
 */
  int xosd_display(xosd * osd, int line, xosd_command command, ...);

/* xosd_is_onscreen -- Returns weather the display is show
 *
 * ARGUMENTS
 *     osd      The xosd "object".
 *
 * RETURNS
 *     1 if the display is shown (mapped)
 *     0 otherwise
 *    -1 on failure
 */
  int xosd_is_onscreen(xosd * osd);

/* xosd_wait_until_no_display -- Wait until nothing is displayed
 * 
 * Not intended to be used on its own, acts as a helper
 * method for the display_info tool
 *
 * ARGUMENTS
 *     osd      The xosd "object".
 *
 * RETURNS
 *   0 on success
 *  -1 on failure
 */
  int xosd_wait_until_no_display(xosd * osd);

/* xosd_hide -- hide the display
 *
 * ARGUMENTS
 *     osd      The xosd "object".
 *
 * RETURNS
 *   0 on success
 *  -1 on failure
 */
  int xosd_hide(xosd * osd);

/* xosd_show -- Show the display after being hidden
 *
 * ARGUMENTS
 *     osd      The xosd "object".
 *
 * RETURNS
 *   0 on success
 *  -1 on failure
 */
  int xosd_show(xosd * osd);

/* xosd_set_pos -- Change the vertical position of the display
 *
 * ARGUMENTS
 *     osd      The xosd "object".
 *     pos      The new position of the display: top middle bottom
 *
 * RETURNS
 *   0 on success
 *  -1 on failure
 */
  int xosd_set_pos(xosd * osd, xosd_pos pos);

/* xosd_set_align -- Change the horizontal alignment of the display
 *
 * ARGUMENTS
 *     osd      The xosd "object".
 *     align    The new alignment of the display: left center right
 *
 * RETURNS
 *   0 on success
 *  -1 on failure
 */
  int xosd_set_align(xosd * osd, xosd_align align);

/* xosd_set_shadow_offset -- Change the offset of the text shadow
 *
 * ARGUMENTS
 *     osd            The xosd "object".
 *     shadow_offset  The new shadow offset in pixels.
 *
 * RETURNS
 *   0 on success
 *  -1 on failure
*/
  int xosd_set_shadow_offset(xosd * osd, int shadow_offset);

/* xosd_set_shadow_direction -- Change the direction of the text shadow
*
* ARGUMENTS
*     osd               The xosd "object".  
*     shadow_direction  The direction of the shadow, in 15 degree increments starting from upwards, from 0 to 7
*
* RETURNS
*   0 on success
*  -1 on failure
*/
  int xosd_set_shadow_direction(xosd * osd, int shadow_direction);

/* xosd_set_outline_offset -- Change the offset of the text outline
 *
 * ARGUMENTS
 *     osd            The xosd "object".
 *     shadow_offset  The new outline offset in pixels.
 *
 *  The outline is drawn over the shadow.
 *
 * RETURNS
 *   0 on success
 *  -1 on failure
*/
  int xosd_set_outline_offset(xosd * osd, int outline_offset);

/* xosd_set_outline_colour -- Change the colour of the outline
 *
 * Not intended to be used on its own, acts as a helper
 * method for the display_info tool
 *
 * ARGUMENTS
 *     osd      The xosd "object".
 *     colour   The new colour of the display.  (See "rgb.txt" in the
 *              X11 distribution of a list of colours.)
 *
 * RETURNS
 *   0 on success
 *  -1 on failure, and colour is set to black
 */
  int xosd_set_outline_colour(xosd * osd, const char *colour);

/* xosd_set_shadow_colour -- Change the colour of the shadow
 *
 * ARGUMENTS
 *     osd      The xosd "object".
 *     colour   The new colour of the display.  (See "rgb.txt" in the
 *              X11 distribution of a list of colours.)
 *
 * RETURNS
 *   0 on success
 *  -1 on failure, and colour is set to black
 */
  int xosd_set_shadow_colour(xosd * osd, const char *colour);

/* xosd_set_horizontal_offset -- Change the number of pixels the display is
 *                    offset from the position
 *
 * ARGUMENTS
 *     osd      The xosd "object".
 *     offset   The number of pixels the display is offset from the
 *              position.
 *
 * RETURNS
 *   0 on success
 *  -1 on failure
*/
  int xosd_set_horizontal_offset(xosd * osd, int offset);

/* xosd_set_vertical_offset -- Change the number of pixels the display is
 *                    offset from the position
 *
 * ARGUMENTS
 *     osd      The xosd "object".
 *     offset   The number of pixels the display is offset from the
 *              position.
 *
 * RETURNS
 *   0 on success
 *  -1 on failure
*/
  int xosd_set_vertical_offset(xosd * osd, int offset);

/* xosd_set_timeout -- Change the time before display is hidden.
 *
 * Not intended to be used on its own, acts as a helper
 * method for the display_info tool
 *
 * ARGUMENTS
 *     osd      The xosd "object".
 *     timeout  The number of seconds before the display is hidden.
 *
 * RETURNS
 *   0 on success
 *  -1 on failure
*/
  int xosd_set_timeout(xosd * osd, int timeout);

/* xosd_set_colour -- Change the colour of the display
 *
 * ARGUMENTS
 *     osd      The xosd "object".
 *     colour   The new colour of the display.  (See "rgb.txt" in the
 *              X11 distribution of a list of colours.)
 *
 * RETURNS
 *   0 on success
 *  -1 on failure, and colour is set to white
 */
  int xosd_set_colour(xosd * osd, const char *colour);

/* xosd_set_font -- Change the text-display font
 *
 * Not intended to be used on its own, acts as a helper
 * method for the display_info tool
 *
 * ARGUMENTS
 *     osd      The xosd "object".
 *     font     The XLFD of the new font, see "xfontsel".
 *
 * RETURNS
 *     0 on success
 *    -1 on failure
*/
  int xosd_set_font(xosd * osd, const char *font);

/* xosd_get_colour -- Gets the RGB value of the display's colour
 *
 * ARGUMENTS
 *     osd      The xosd "object".
 *     red      Return value for the red value.
 *     green    Return value for the green value.
 *     blue     Return value for the blue value.
 *
 * RETURNS
 *   0 on success
 *  -1 on failure
*/
  int xosd_get_colour(xosd * osd, int *red, int *green, int *blue);

/* xosd_scroll -- Scroll the display
 *
 * ARGUMENTS
 *     osd      The xosd "object".
 *     lines    The number of lines to scroll the display.
 *
 * RETURNS
 *   0 on success
 *  -1 on failure
*/
  int xosd_scroll(xosd * osd, int lines);

/* xosd_get_number_lines -- Get the maximum number of lines allowed
 *
 * ARGUMENTS
 *     osd      The xosd "object".
 *
 * RETURNS
 *   the number of lines on success
 *  -1 on failure
*/
  int xosd_get_number_lines(xosd * osd);

#ifdef __cplusplus
};
#endif


#endif

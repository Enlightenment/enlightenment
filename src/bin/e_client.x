
/**
 * Move window to coordinates that do not account client decorations yet.
 *
 * This call will consider given position does not account client
 * decoration, so these values (e_comp_object_frame) will be
 * accounted automatically. This is specially useful when it is a new
 * client and has not be evaluated yet, in this case
 * the frame will be zeroed and no information is known. It
 * will mark pending requests so client will be accounted on
 * evalutation phase.
 *
 * @parm x horizontal position to place window.
 * @parm y vertical position to place window.
 *
 * @see e_client_move()
 */
static inline void
e_client_util_move_without_frame(E_Client *ec, int x, int y)
{
   e_comp_object_frame_xy_adjust(ec->frame, x, y, &x, &y);
   evas_object_move(ec->frame, x, y);
}

/**
 * Resize window to values that do not account client decorations yet.
 *
 * This call will consider given size does not account client
 * decoration, so these values (e_comp_object_frame) will be
 * accounted automatically. This is specially useful when it is a new
 * client and has not be evaluated yet, in this case
 * e_comp_object_frame will be zeroed and no information is known. It
 * will mark pending requests so client will be accounted on
 * evalutation phase.
 *
 * @parm w horizontal window size.
 * @parm h vertical window size.
 *
 * @see e_client_resize()
 */
static inline void
e_client_util_resize_without_frame(E_Client *ec, int w, int h)
{
   e_comp_object_frame_wh_adjust(ec->frame, w, h, &w, &h);
   evas_object_resize(ec->frame, w, h);
}

/**
 * Move and resize window to values that do not account client decorations yet.
 *
 * This call will consider given values already accounts client
 * decorations, so it will not be considered later. This will just
 * work properly with clients that have being evaluated and client
 * decorations are known (e_comp_object_frame).
 *
 * @parm x horizontal position to place window.
 * @parm y vertical position to place window.
 * @parm w horizontal window size.
 * @parm h vertical window size.
 *
 * @see e_client_move_resize()
 */
static inline void
e_client_util_move_resize_without_frame(E_Client *ec, int x, int y, int w, int h)
{
   e_client_util_move_without_frame(ec, x, y);
   e_client_util_resize_without_frame(ec, w, h);
}

static inline Eina_Bool
e_client_util_ignored_get(const E_Client *ec)
{
   return ec->override || ec->input_only || ec->ignored;
}

static inline Eina_Bool
e_client_util_desk_visible(const E_Client *ec, const E_Desk *desk)
{
   return ec->sticky || (ec->desk == desk);
}

static inline Ecore_Window
e_client_util_pwin_get(const E_Client *ec)
{
   if (!ec->pixmap) return 0;
   return e_pixmap_parent_window_get(ec->pixmap);
}

static inline Ecore_Window
e_client_util_win_get(const E_Client *ec)
{
   if (!ec->pixmap) return 0;
   return e_pixmap_window_get(ec->pixmap);
}

static inline Eina_Bool
e_client_resizing_get(const E_Client *ec)
{
   return (ec->resize_mode != E_POINTER_RESIZE_NONE);
}

static inline Eina_Bool
e_client_util_borderless(const E_Client *ec)
{
   return (ec->borderless || ec->mwm.borderless || (!ec->border.name) || (!strcmp(ec->border.name, "borderless")));
}

#define E_COMP_X
#include "e.h"
#include "e_mod_main.h"
#define HISTORY_MAX 8
#define DEBUG_INFO 1

static E_Config_DD *conf_edd = NULL;
Config *access_config = NULL;

typedef struct
{
   E_Zone         *zone;
   Ecore_X_Window  win;
   Ecore_Timer    *timer;
   Ecore_Timer    *double_down_timer;
   Ecore_Timer    *tap_timer;
   Evas_Object    *info;
   Evas_Object    *text;
   int             x, y, dx, dy, mx, my;
   int             mouse_history[HISTORY_MAX];
   unsigned int    dt;
   Eina_Inlist    *history;
   Eina_Bool       longpressed : 1;
   Eina_Bool       two_finger_down : 1;
   Eina_Bool       double_down : 1;
} Cover;

#if DEBUG_INFO
  static Ecore_Timer    *dbg_timer = NULL;
  static Eina_Bool
  _reset_text(void *data)
  {
     Cover *cov = data;
     if(!cov) return EINA_FALSE;

     ecore_timer_del(dbg_timer);
     dbg_timer = NULL;
     evas_object_text_text_set(cov->text, "Screen Reader Mode");
     return EINA_FALSE;
  }
  #define INFO(cov, txt) \
    evas_object_text_text_set(cov->text, txt); \
    EINA_LOG_INFO("%s", txt); \
    if (dbg_timer) \
      { \
         ecore_timer_del(dbg_timer); \
         dbg_timer = NULL; \
      } \
     dbg_timer = ecore_timer_add(1.0, _reset_text, cov);
#else
  #define INFO(cov, txt) EINA_LOG_INFO("%s", txt)
#endif

typedef struct
{
   EINA_INLIST;
   int             device;
} Multi;

static E_Client *_prev_bd;

static Ecore_X_Atom _atom_access = 0;
static Ecore_X_Window target_win = 0;

static Eina_List *covers = NULL;
static Eina_List *handlers = NULL;
static Ecore_Event_Handler *client_message_handler = NULL;
static Ecore_Event_Handler *property_handler = NULL;
static int multi_device[3];

static void
_mouse_in_win_get(Cover *cov, int x, int y)
{
   Eina_List *l;
   Ecore_X_Window *skip;
   Cover *cov2;
   int i;

   skip = alloca(sizeof(Ecore_X_Window) * eina_list_count(covers));
   i = 0;
   EINA_LIST_FOREACH(covers, l, cov2)
     {
        skip[i] = cov2->win;
        i++;
     }

   /* TODO: if target window is changed without highlight object,
      then previous target window which has the highlight object
      should get the message. how? */
   target_win = ecore_x_window_shadow_tree_at_xy_with_skip_get
     (cov->e_comp->root, x, y, skip, i);
}

static unsigned int
_win_angle_get(Ecore_X_Window win)
{
   Ecore_X_Window root;

   if (!win) return 0;

   int ret;
   int count;
   int angle = 0;
   unsigned char *prop_data = NULL;

   root = ecore_x_window_root_get(win);
   ret = ecore_x_window_prop_property_get(root,
       ECORE_X_ATOM_E_ILLUME_ROTATE_ROOT_ANGLE,
                         ECORE_X_ATOM_CARDINAL,
                        32, &prop_data, &count);

   if (ret && prop_data)
      memcpy (&angle, prop_data, sizeof (int));

   if (prop_data) free (prop_data);

   return angle;
}

static void
_coordinate_calibrate(Ecore_X_Window win, int *x, int *y)
{
   int tx, ty, w, h;
   unsigned int angle;

   if (!x) return;
   if (!y) return;

   angle = _win_angle_get(win);
   ecore_x_window_geometry_get(win, NULL, NULL, &w, &h);

   tx = *x;
   ty = *y;

   switch (angle)
     {
      case 90:
        *x = h - ty;
        *y = tx;
        break;

      case 180:
        *x = w - tx;
        *y = h - ty;
        break;

      case 270:
        *x = ty;
        *y = w - tx;
        break;

      default:
        break;
     }
}

static void
_mouse_win_fake_tap(Cover *cov, Ecore_Event_Mouse_Button *ev)
{
   int x, y;

   /* find target window to send message */
   _mouse_in_win_get(cov, ev->root.x, ev->root.y);

   ecore_x_pointer_xy_get(target_win, &x, &y);
   ecore_x_mouse_in_send(target_win, x, y);
   ecore_x_mouse_move_send(target_win, x, y);
   ecore_x_mouse_down_send(target_win, x, y, 1);
   ecore_x_mouse_up_send(target_win, x, y, 1);
   ecore_x_mouse_out_send(target_win, x, y);
}

static void
_messsage_read_send(Cover *cov)
{
   int x, y;

   /* find target window to send message */
   _mouse_in_win_get(cov, cov->x, cov->y);

   ecore_x_pointer_xy_get(target_win, &x, &y);
   _coordinate_calibrate(target_win, &x, &y);

   ecore_x_client_message32_send(target_win, ECORE_X_ATOM_E_ILLUME_ACCESS_CONTROL,
                                 ECORE_X_EVENT_MASK_WINDOW_CONFIGURE,
                                 target_win,
                                 ECORE_X_ATOM_E_ILLUME_ACCESS_ACTION_READ,
                                 x, y, 0);

#if DEBUG_INFO
   Eina_List *l;
   Cover *ecov;
   Eina_Strbuf *buf;

   buf = eina_strbuf_new();
   eina_strbuf_append_printf(buf, "read x:%d, y:%d", x, y);

   EINA_LIST_FOREACH(covers, l, ecov)
     {
       INFO(ecov, eina_strbuf_string_get(buf));
     }
   eina_strbuf_free(buf);
#endif
}

static Eina_Bool
_mouse_longpress(void *data)
{
   Cover *cov = data;
   int distance = 40;
   int dx, dy;

   cov->timer = NULL;
   dx = cov->x - cov->dx;
   dy = cov->y - cov->dy;
   if (((dx * dx) + (dy * dy)) < (distance * distance))
     {
        cov->longpressed = EINA_TRUE;
        INFO(cov, "longpress");

        if (!cov->double_down) _messsage_read_send(cov);
        else
          {
             INFO(cov, "double down and longpress");
             //TODO: send message to notify start longpress
          }
     }
   return EINA_FALSE;
}

static Eina_Bool
_mouse_double_down(void *data)
{
   Cover *cov = data;
   ecore_timer_del(cov->double_down_timer);
   cov->double_down_timer = NULL;
   return EINA_FALSE;
}

static void
_double_down_timeout(Cover *cov)
{
   double long_time = 0.5;
   double short_time = 0.3;
   int distance = 40;
   int dx, dy;

   dx = cov->x - cov->dx;
   dy = cov->y - cov->dy;

   if ((cov->double_down_timer) &&
       (((dx * dx) + (dy * dy)) < (distance * distance)))
     {
        // start double tap and move from here
        cov->double_down = EINA_TRUE;

        if (cov->timer)
          {
             ecore_timer_del(cov->timer);
             cov->timer = NULL;
          }
        /* check longpress after double down */
        cov->timer = ecore_timer_add(long_time, _mouse_longpress, cov);
     }

   if (cov->double_down_timer)
     {
        ecore_timer_del(cov->double_down_timer);
        cov->double_down_timer = NULL;
        return;
     }

   cov->double_down_timer = ecore_timer_add(short_time, _mouse_double_down, cov);
}

static Eina_Bool
_mouse_tap(void *data)
{
   Cover *cov = data;
   cov->tap_timer = NULL;

   _messsage_read_send(cov);

   return EINA_FALSE;
}

static void
_mouse_down(Cover *cov, Ecore_Event_Mouse_Button *ev)
{
   double long_time = 0.5;

   cov->dx = ev->x;
   cov->dy = ev->y;
   cov->mx = ev->x;
   cov->my = ev->y;
   cov->x = ev->x;
   cov->y = ev->y;
   cov->dt = ev->timestamp;
   cov->longpressed = EINA_FALSE;
   cov->timer = ecore_timer_add(long_time, _mouse_longpress, cov);

   if (cov->tap_timer)
     {
        ecore_timer_del(cov->tap_timer);
        cov->tap_timer = NULL;
     }

   /* check mouse double down - not two fingers, refer to double click */
   _double_down_timeout(cov);
}

static void
_mouse_up(Cover *cov, Ecore_Event_Mouse_Button *ev)
{
   double timeout = 0.15;
   double double_tap_timeout = 0.25;
   int distance = 40;
   int dx, dy;
   int x, y;
   int angle = 0;

   // for two finger panning
   if (cov->two_finger_down)
     {
        ecore_x_pointer_xy_get(target_win, &x, &y);
        ecore_x_mouse_up_send(target_win, x, y, 1);
        cov->two_finger_down = EINA_FALSE;
        ecore_x_mouse_out_send(target_win, x, y);
     }

   // reset double down and moving
   cov->double_down = EINA_FALSE;

   if (cov->timer)
     {
        ecore_timer_del(cov->timer);
        cov->timer = NULL;
     }

   if (cov->longpressed)
     {
        cov->longpressed = EINA_FALSE;
        return;
     }

   dx = ev->x - cov->dx;
   dy = ev->y - cov->dy;
   if (((dx * dx) + (dy * dy)) < (distance * distance))
     {
        if (ev->double_click)
          {
             /* activate message would change focused window
                FIXME: but it is possibe to create unfocused window
                in this case, the message should go to unfocused window? */
             _prev_bd = e_client_focused_get();

             INFO(cov, "double_click");
             ecore_x_e_illume_access_action_activate_send(target_win);
          }
        else if ((ev->timestamp - cov->dt) <= (timeout * 1000))
          {
             cov->tap_timer = ecore_timer_add(double_tap_timeout,
                                         _mouse_tap, cov);
          }
        else if ((ev->timestamp - cov->dt) > (timeout * 1000) &&
                 (ev->timestamp - cov->dt) < (2 * timeout * 1000))
          {
             INFO(cov, "tap");
             _mouse_win_fake_tap(cov, ev);
          }
     }
   else if (((dx * dx) + (dy * dy)) > (4 * distance * distance)
            && ((ev->timestamp - cov->dt) < (timeout * 1000)))
     {
        /* get root window rotation */
        angle = _win_angle_get(target_win);

        if (abs(dx) > abs(dy)) // left or right
          {
             if (dx > 0) // right
               {
                  INFO(cov, "single flick right");
                  switch (angle)
                    {
                     case 180:
                     case 270:
                       ecore_x_e_illume_access_action_read_prev_send
                                                        (target_win);
                     break;

                     case 90:
                     default:
                       ecore_x_e_illume_access_action_read_next_send
                                                        (target_win);
                     break;
                    }

               }
             else // left
               {
                  INFO(cov, "single flick left");
                  switch (angle)
                    {
                     case 180:
                     case 270:
                       ecore_x_e_illume_access_action_read_next_send
                                                        (target_win);
                     break;

                     case 90:
                     default:
                       ecore_x_e_illume_access_action_read_prev_send
                                                        (target_win);
                     break;
                    }
               }
          }
        else // up or down
          {
             if (dy > 0) // down
               {
                  INFO(cov, "single flick down");
                  switch (angle)
                    {
                     case 90:
                     case 180:
                       ecore_x_e_illume_access_action_read_prev_send
                                                        (target_win);
                     break;

                     case 270:
                     default:
                       ecore_x_e_illume_access_action_read_next_send
                                                        (target_win);
                     break;
                    }
               }
             else // up
               {
                  INFO(cov, "single flick up");
                  switch (angle)
                    {
                     case 90:
                     case 180:
                       ecore_x_e_illume_access_action_read_next_send
                                                        (target_win);
                     break;

                     case 270:
                     default:
                       ecore_x_e_illume_access_action_read_prev_send
                                                        (target_win);
                     break;
                    }
               }
          }
     }
   cov->longpressed = EINA_FALSE;
}

static void
_mouse_move(Cover *cov, Ecore_Event_Mouse_Move *ev)
{
   int x, y;

   //FIXME: why here.. after long press you cannot go below..
   //if (!cov->down) return;

   //FIXME: one finger cannot come here
   //_record_mouse_history(cov, ev);

   ecore_x_pointer_xy_get(target_win, &x, &y);
   ecore_x_mouse_move_send(target_win, x, y);
}

static void
_mouse_wheel(Cover *cov EINA_UNUSED, Ecore_Event_Mouse_Wheel *ev EINA_UNUSED)
{
   if (ev->z == -1) // up
     {
        ecore_x_e_illume_access_action_up_send(target_win);
     }
   else if (ev->z == 1) // down
     {
        ecore_x_e_illume_access_action_down_send(target_win);
     }
}

static Eina_Bool
_cb_mouse_down(void    *data EINA_UNUSED,
               int      type EINA_UNUSED,
               void    *event)
{
   Ecore_Event_Mouse_Button *ev = event;
   Eina_List *l;
   Cover *cov;
   int i = 0;
   int x, y;
   E_Client *ec;

   for (i = 0; i < 3; i++)
     {
        if (multi_device[i] == -1)
          {
             multi_device[i] = ev->multi.device;
             break;
          }
        else if (multi_device[i] == ev->multi.device) break;
     }

   /* activate message would change focused window
      FIXME: but it is possibe to create unfocused window
      in this case, the message should go to focused window? */
   ec = e_client_focused_get();
   if (ec && (ec != _prev_bd)) target_win = e_client_util_win_get(ec);

   EINA_LIST_FOREACH(covers, l, cov)
     {
        if (ev->window == cov->win)
          {
             // XXX change specific number
             if (ev->multi.device == multi_device[0])
               _mouse_down(cov, ev);

             if (ev->multi.device == multi_device[1] && !(cov->two_finger_down))
               {
                  // prevent longpress client message by two finger
                  if (cov->timer)
                    {
                       ecore_timer_del(cov->timer);
                       cov->timer = NULL;
                    }

                  ecore_x_pointer_xy_get(target_win, &x, &y);

                  ecore_x_mouse_in_send(target_win, x, y);
                  ecore_x_mouse_move_send(target_win, x, y);
                  ecore_x_mouse_down_send(target_win, x, y, 1);

                  cov->two_finger_down = EINA_TRUE;
               }
             return ECORE_CALLBACK_PASS_ON;
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_cb_mouse_up(void    *data EINA_UNUSED,
             int      type EINA_UNUSED,
             void    *event)
{
   Ecore_Event_Mouse_Button *ev = event;
   Eina_List *l;
   Cover *cov;

   EINA_LIST_FOREACH(covers, l, cov)
     {
        if (ev->window == cov->win)
          {
             if (ev->buttons == 1)
               _mouse_up(cov, ev);
             return ECORE_CALLBACK_PASS_ON;
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_cb_mouse_move(void    *data EINA_UNUSED,
               int      type EINA_UNUSED,
               void    *event)
{
   Ecore_Event_Mouse_Move *ev = event;
   Eina_List *l;
   Cover *cov;
   int angle = 0;

   EINA_LIST_FOREACH(covers, l, cov)
     {
        cov->x = ev->x;
        cov->y = ev->y;

        if (ev->window == cov->win)
          {
             //if (ev->multi.device == multi_device[0] || ev->multi.device == multi_device[1])
             if (cov->two_finger_down && ev->multi.device == multi_device[1])
               _mouse_move(cov, ev);
             else if (cov->longpressed && // client message for moving is available only after long press is detected
                      !(cov->double_down) && /* mouse move after double down should not send read message */
                      !(cov->two_finger_down) && ev->multi.device == multi_device[0])
               {
                  INFO(cov, "read");
                  _messsage_read_send(cov);
               }
             else if (cov->double_down && // client message for moving is available only after long press is detected
                      !(cov->two_finger_down) && ev->multi.device == multi_device[0])
               {
                  int distance = 5;
                  int dx, dy;

                  if (cov->longpressed)
                    {
                       //TODO: send message to notify move afte longpress
                    }
                  else
                    {
                       dx = ev->x - cov->mx;
                       dy = ev->y - cov->my;

                       /* get root window rotation */
                       angle = _win_angle_get(target_win);

                       if (((dx * dx) + (dy * dy)) > (distance * distance))
                         {

                            if (abs(dx) > abs(dy)) // left or right
                              {
                                 if (dx > 0) // right
                                   {
                                      INFO(cov, "mouse double down and move - right");
                                      switch (angle)
                                        {
                                         case 180:
                                         case 270:
                                           ecore_x_e_illume_access_action_down_send(target_win);
                                         break;

                                         case 90:
                                         default:
                                           ecore_x_e_illume_access_action_up_send(target_win);
                                         break;
                                        }
                                   }
                                 else // left
                                   {
                                      INFO(cov, "mouse double down and move - left");
                                      switch (angle)
                                        {
                                         case 180:
                                         case 270:
                                           ecore_x_e_illume_access_action_up_send(target_win);
                                         break;

                                         case 90:
                                         default:
                                           ecore_x_e_illume_access_action_down_send(target_win);
                                         break;
                                        }
                                   }
                              }
                            else // up or down
                              {
                                 if (dy > 0) // down
                                   {
                                      INFO(cov, "mouse double down and move - down");
                                      switch (angle)
                                        {
                                         case 90:
                                         case 180:
                                           ecore_x_e_illume_access_action_up_send(target_win);
                                         break;

                                         case 270:
                                         default:
                                           ecore_x_e_illume_access_action_down_send(target_win);
                                         break;
                                        }
                                   }
                                 else // up
                                   {
                                      INFO(cov, "mouse double down and move - up");
                                      switch (angle)
                                        {
                                         case 90:
                                         case 180:
                                           ecore_x_e_illume_access_action_down_send(target_win);
                                         break;

                                         case 270:
                                         default:
                                           ecore_x_e_illume_access_action_up_send(target_win);
                                         break;
                                        }
                                   }
                               }

                            cov->mx = ev->x;
                            cov->my = ev->y;
                         }
                    }
               }

             return ECORE_CALLBACK_PASS_ON;
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_cb_mouse_wheel(void    *data EINA_UNUSED,
                int      type EINA_UNUSED,
                void    *event)
{
   Ecore_Event_Mouse_Wheel *ev = event;
   Eina_List *l;
   Cover *cov;

   EINA_LIST_FOREACH(covers, l, cov)
     {
        if (ev->window == cov->win)
          {
             _mouse_wheel(cov, ev);
             return ECORE_CALLBACK_PASS_ON;
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Cover *
_cover_new(E_Zone *zone)
{
   Cover *cov;

   cov = E_NEW(Cover, 1);
   if (!cov) return NULL;
   cov->zone = zone;

#if DEBUG_INFO
   Ecore_Evas *ee;
   ee = ecore_evas_new(NULL,
                       zone->x,
                       zone->y,
                       zone->w, zone->h,
                       NULL);
   ecore_evas_alpha_set(ee, EINA_TRUE);
   cov->win = (Ecore_X_Window)ecore_evas_window_get(ee);

   /* create infomation */
   Evas *e;
   e = ecore_evas_get(ee);
   cov->info = evas_object_rectangle_add(e);
   evas_object_color_set(cov->info, 255, 255, 255, 100);
   evas_object_move(cov->info, zone->x, zone->y);
   evas_object_resize(cov->info, zone->w, 30);
   evas_object_show(cov->info);

   cov->text = evas_object_text_add(e);
   evas_object_text_style_set(cov->text, EVAS_TEXT_STYLE_PLAIN);
   evas_object_text_font_set(cov->text, "DejaVu", 14);
   INFO(cov, "Screen Reader Mode");

   evas_object_color_set(cov->text, 0, 0, 0, 255);
   evas_object_resize(cov->text, (zone->w / 8), 20);
   evas_object_move(cov->text, zone->x + 5, zone->y + 5);
   evas_object_show(cov->text);

#else
   cov->win = ecore_x_window_input_new(e_comp->root,
                                       zone->x, zone->y,
                                       zone->w, zone->h);
#endif

   ecore_x_input_multi_select(cov->win);

   ecore_x_icccm_title_set(cov->win, "access-screen-reader");
   ecore_x_netwm_name_set(cov->win, "access-screen-reader");

   ecore_x_window_ignore_set(cov->win, 1);
   ecore_x_window_configure(cov->win,
                            ECORE_X_WINDOW_CONFIGURE_MASK_SIBLING |
                            ECORE_X_WINDOW_CONFIGURE_MASK_STACK_MODE,
                            0, 0, 0, 0, 0,
                            e_comp->layers[8].win,
                            ECORE_X_WINDOW_STACK_ABOVE);
   ecore_x_window_show(cov->win);
   ecore_x_window_raise(cov->win);

   return cov;
}

static void
_covers_init(void)
{
   const Eina_List *l;
   int i = 0;
   E_Zone *zone;

   EINA_LIST_FOREACH(e_comp->zones, l, zone)
     {
        Cover *cov = _cover_new(zone);
        if (!cov) continue;
        covers = eina_list_append(covers, cov);
        for (i = 0; i < HISTORY_MAX; i++) cov->mouse_history[i] = -1;
     }
}

static void
_covers_shutdown(void)
{
   Cover *cov;

   EINA_LIST_FREE(covers, cov)
     {
        ecore_x_window_ignore_set(cov->win, 0);
        ecore_x_window_free(cov->win);
        evas_object_del(cov->info);
        evas_object_del(cov->text);

        if (cov->timer)
          {
             ecore_timer_del(cov->timer);
             cov->timer = NULL;
          }

        if (cov->double_down_timer)
          {
             ecore_timer_del(cov->double_down_timer);
             cov->double_down_timer = NULL;
          }

        if (cov->tap_timer)
          {
             ecore_timer_del(cov->tap_timer);
             cov->tap_timer = NULL;
          }

#if DEBUG_INFO
        if (dbg_timer)
          {
             ecore_timer_del(dbg_timer);
             dbg_timer = NULL;
          }
#endif

        free(cov);
     }
}

static Eina_Bool
_cb_zone_add(void    *data EINA_UNUSED,
             int      type EINA_UNUSED,
             void    *event EINA_UNUSED)
{
   _covers_shutdown();
   _covers_init();
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_cb_zone_del(void    *data EINA_UNUSED,
             int      type EINA_UNUSED,
             void    *event EINA_UNUSED)
{
   _covers_shutdown();
   _covers_init();
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_cb_zone_move_resize(void    *data EINA_UNUSED,
                     int      type EINA_UNUSED,
                     void    *event EINA_UNUSED)
{
   _covers_shutdown();
   _covers_init();
   return ECORE_CALLBACK_PASS_ON;
}

static void
_events_init(void)
{
   int i = 0;

   handlers = eina_list_append
     (handlers, ecore_event_handler_add(ECORE_EVENT_MOUSE_BUTTON_DOWN,
                                        _cb_mouse_down, NULL));
   handlers = eina_list_append
     (handlers, ecore_event_handler_add(ECORE_EVENT_MOUSE_BUTTON_UP,
                                        _cb_mouse_up, NULL));
   handlers = eina_list_append
     (handlers, ecore_event_handler_add(ECORE_EVENT_MOUSE_MOVE,
                                        _cb_mouse_move, NULL));
   handlers = eina_list_append
     (handlers, ecore_event_handler_add(ECORE_EVENT_MOUSE_WHEEL,
                                        _cb_mouse_wheel, NULL));
   handlers = eina_list_append
     (handlers, ecore_event_handler_add(E_EVENT_ZONE_ADD,
                                        _cb_zone_add, NULL));
   handlers = eina_list_append
     (handlers, ecore_event_handler_add(E_EVENT_ZONE_DEL,
                                        _cb_zone_del, NULL));
   handlers = eina_list_append
     (handlers, ecore_event_handler_add(E_EVENT_ZONE_MOVE_RESIZE,
                                        _cb_zone_move_resize, NULL));

   for (i = 0; i < 3; i++) multi_device[i] = -1;
}

static void
_events_shutdown(void)
{
   E_FREE_LIST(handlers, ecore_event_handler_del);
}

static Eina_Bool
_cb_property_change(void *data EINA_UNUSED,
                   int   type EINA_UNUSED,
                   void *ev)
{
   E_Client *ec;
   Ecore_X_Event_Window_Property *event = ev;

   if (event->atom == ECORE_X_ATOM_NET_ACTIVE_WINDOW)
     {
        ec = e_client_focused_get();
        if (ec) target_win = e_client_util_win_get(ec);
	 }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_cb_client_message(void *data EINA_UNUSED,
                   int   type EINA_UNUSED,
                   void *ev)
{
   int block;
   Ecore_X_Event_Client_Message *event = ev;

   if (event->message_type != _atom_access)
     return ECORE_CALLBACK_PASS_ON;

   block = e_config_save_block_get();
   if (block) e_config_save_block_set(!block);

   if ((Eina_Bool)event->data.l[0])
     {
        EINA_LOG_INFO("[access module] module enable");
        _covers_init();
        _events_init();
        access_config->window = EINA_TRUE;
     }
   else
     {
        EINA_LOG_INFO("[access module] module disable");
        _covers_shutdown();
        _events_shutdown();
        access_config->window = EINA_FALSE;
     }

   /* save config value */
   e_config_domain_save("module.access", conf_edd, access_config);
   e_config_save_block_set(block);

   return ECORE_CALLBACK_PASS_ON;
}

/***************************************************************************/
/* module setup */
EAPI E_Module_Api e_modapi =
{
   E_MODULE_API_VERSION, "Access"
};

EAPI void *
e_modapi_init(E_Module *m)
{
   if (!_atom_access)
     _atom_access = ecore_x_atom_get("_E_MOD_ACC_SCR_READER_");

   ecore_x_event_mask_set(ecore_x_window_root_first_get(),
                          ECORE_X_EVENT_MASK_WINDOW_CONFIGURE);
   client_message_handler = ecore_event_handler_add
                             (ECORE_X_EVENT_CLIENT_MESSAGE, _cb_client_message, NULL);
   ecore_x_event_mask_set(ecore_x_window_root_first_get(),
                          ECORE_X_EVENT_MASK_WINDOW_PROPERTY);
   property_handler = ecore_event_handler_add
                       (ECORE_X_EVENT_WINDOW_PROPERTY, _cb_property_change, NULL);

   /* load config value */
   conf_edd = E_CONFIG_DD_NEW("Access_Config", Config);
   E_CONFIG_VAL(conf_edd, Config, window, UCHAR);

   access_config = e_config_domain_load("module.access", conf_edd);

   if (!access_config)
     {
        access_config = E_NEW(Config, 1);
        access_config->window = EINA_FALSE;
        return m;
     }

   if (access_config->window)
     {
        _covers_shutdown();
        _covers_init();
        _events_init();
     }
   else
     {
        _covers_shutdown();
        _events_shutdown();
     }

   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   EINA_LOG_INFO("[access module] module shutdown");
   if (client_message_handler) ecore_event_handler_del(client_message_handler);
   if (property_handler) ecore_event_handler_del(property_handler);

   _covers_shutdown();
   _events_shutdown();

   return 1;
}

EAPI int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   e_config_domain_save("module.access", conf_edd, access_config);
   return 1;
}

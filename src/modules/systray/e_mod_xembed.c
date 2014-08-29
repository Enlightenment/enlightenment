/**
 * systray implementation following freedesktop.org specification.
 *
 * @see: http://standards.freedesktop.org/systemtray-spec/latest/
 *
 * @todo: implement xembed, mostly done, at least relevant parts are done.
 *        http://standards.freedesktop.org/xembed-spec/latest/
 *
 * @todo: implement messages/popup part of the spec (anyone using this at all?)
 */
#define E_COMP_X
#include "e_mod_main.h"

#define RETRY_TIMEOUT                     2.0

#define SYSTEM_TRAY_REQUEST_DOCK          0
#define SYSTEM_TRAY_BEGIN_MESSAGE         1
#define SYSTEM_TRAY_CANCEL_MESSAGE        2
#define _NET_SYSTEM_TRAY_ORIENTATION_HORZ 0
#define _NET_SYSTEM_TRAY_ORIENTATION_VERT 1

/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY            0
#define XEMBED_WINDOW_ACTIVATE            1
#define XEMBED_WINDOW_DEACTIVATE          2
#define XEMBED_REQUEST_FOCUS              3
#define XEMBED_FOCUS_IN                   4
#define XEMBED_FOCUS_OUT                  5
#define XEMBED_FOCUS_NEXT                 6
#define XEMBED_FOCUS_PREV                 7
/* 8-9 were used for XEMBED_GRAB_KEY/XEMBED_UNGRAB_KEY */
#define XEMBED_MODALITY_ON                10
#define XEMBED_MODALITY_OFF               11
#define XEMBED_REGISTER_ACCELERATOR       12
#define XEMBED_UNREGISTER_ACCELERATOR     13
#define XEMBED_ACTIVATE_ACCELERATOR       14

/* Details for  XEMBED_FOCUS_IN: */
#define XEMBED_FOCUS_CURRENT              0
#define XEMBED_FOCUS_FIRST                1
#define XEMBED_FOCUS_LAST                 2

typedef struct _Icon     Icon;

struct _Icon
{
   Ecore_X_Window   win;
   Evas_Object     *rect;
   Instance_Xembed *xembed;
};

struct _Instance_Xembed
{
   Instance *inst;
   struct
   {
      Ecore_X_Window parent;
      Ecore_X_Window base;
      Ecore_X_Window selection;
   } win;
   Eina_List *handlers;
   struct
   {
      Ecore_Timer *retry;
   } timer;
   Eina_List *icons;

   E_Client *ec;
   Ecore_Timer *visibility_timer;
};

static Ecore_X_Atom _atom_manager = 0;
static Ecore_X_Atom _atom_st_orient = 0;
static Ecore_X_Atom _atom_st_visual = 0;
static Ecore_X_Atom _atom_st_op_code = 0;
static Ecore_X_Atom _atom_st_msg_data = 0;
static Ecore_X_Atom _atom_xembed = 0;
static Ecore_X_Atom _atom_xembed_info = 0;
static Ecore_X_Atom _atom_st_num = 0;
static int _last_st_num = -1;
static Eina_List *handlers = NULL;

static void
_xembed_win_resize(Instance_Xembed *xembed)
{
   Evas_Coord first_x, first_y, first_w, first_h, last_x, last_y;
   Icon *icon;

   if (!xembed->icons)
     return;

   icon = eina_list_data_get(xembed->icons);
   evas_object_geometry_get(icon->rect, &first_x,
                            &first_y, &first_w, &first_h);

   icon = eina_list_last_data_get(xembed->icons);
   evas_object_geometry_get(icon->rect, &last_x,
                            &last_y, NULL, NULL);

   //because we always prepend xembed icons
   xembed->ec->comp_data->pw = (first_x+first_w) - last_x;
   xembed->ec->comp_data->ph = (first_y+first_h) - last_y;
   ecore_x_window_move_resize(xembed->win.base, last_x, last_y,
                              (first_x+first_w) - last_x,
                              (first_y+first_h) - last_y);
   evas_object_geometry_set(xembed->ec->frame, last_x, last_y,
                              (first_x+first_w) - last_x,
                              (first_y+first_h) - last_y);
}

static void
_systray_xembed_restack(Instance_Xembed *xembed)
{
   E_Layer layer;
   E_Shelf *es = xembed->inst->gcc->gadcon->shelf;

   if (es)
     {
        layer = e_comp_canvas_layer_map_to(e_comp_canvas_layer_map(es->cfg->layer) + 1);
     }
   else
     layer = E_LAYER_CLIENT_DESKTOP;
   layer = E_CLAMP(layer, E_LAYER_CLIENT_DESKTOP, E_LAYER_CLIENT_ABOVE);
   evas_object_layer_set(xembed->ec->frame, layer);
}

static Eina_Bool
_systray_xembed_visible_check(Instance_Xembed *xembed)
{
   if (eina_list_count(xembed->icons) == 0)
     {
        evas_object_hide(xembed->ec->frame);
        e_pixmap_dirty(xembed->ec->pixmap);
     }
   else
     {
        _xembed_win_resize(xembed);
        _systray_xembed_restack(xembed);
        evas_object_show(xembed->ec->frame);
     }
   xembed->visibility_timer = NULL;
   return EINA_FALSE;
}

void
systray_xembed_size_updated(Instance_Xembed *xembed)
{
   if (e_comp_get(NULL)->comp_type != E_PIXMAP_TYPE_X) return;
   if (xembed->visibility_timer)
     ecore_timer_reset(xembed->visibility_timer);
   else
     xembed->visibility_timer = ecore_timer_add(0.15, (Ecore_Task_Cb)_systray_xembed_visible_check, xembed);
}

static void
_systray_xembed_cb_move(void *data, Evas *evas __UNUSED__, Evas_Object *o __UNUSED__, void *event __UNUSED__)
{
   Instance_Xembed *xembed = data;
   systray_size_updated(xembed->inst);
}

static void
_systray_xembed_cb_resize(void *data, Evas *evas __UNUSED__, Evas_Object *o __UNUSED__, void *event __UNUSED__)
{
   Instance_Xembed *xembed = data;
   systray_size_updated(xembed->inst);
}

static void
_systray_xembed_cb_hide(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance_Xembed *xembed = data;
   if (xembed->ec)
     evas_object_hide(xembed->ec->frame);
}

static void
_systray_xembed_cb_show(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Instance_Xembed *xembed = data;
   if (xembed->ec && eina_list_count(xembed->icons))
     evas_object_show(xembed->ec->frame);
}

static void
_systray_xembed_icon_geometry_apply(Icon *icon)
{
   const Evas_Object *box;
   Evas_Coord x, y, w, h, wx, wy;

   /* hack required so we reposition x window inside parent */
   box = systray_box_get(icon->xembed->inst);
   if (!box) return;

   evas_object_geometry_get(icon->rect, &x, &y, &w, &h);
   evas_object_geometry_get(box, &wx, &wy, NULL, NULL);
   ecore_x_window_move_resize(icon->win, x - wx, y - wy, w, h);
}

static void
_systray_xembed_icon_cb_move(void *data, Evas *evas __UNUSED__, Evas_Object *o __UNUSED__, void *event __UNUSED__)
{
   Icon *icon = data;
   _systray_xembed_icon_geometry_apply(icon);
}

static void
_systray_xembed_icon_cb_resize(void *data, Evas *evas __UNUSED__, Evas_Object *o __UNUSED__, void *event __UNUSED__)
{
   Icon *icon = data;
   _systray_xembed_icon_geometry_apply(icon);
}

static Ecore_X_Gravity
_systray_xembed_gravity(const Instance_Xembed *xembed)
{
   switch (systray_orient_get(xembed->inst))
     {
      case E_GADCON_ORIENT_FLOAT:
        return ECORE_X_GRAVITY_STATIC;

      case E_GADCON_ORIENT_HORIZ:
        return ECORE_X_GRAVITY_CENTER;

      case E_GADCON_ORIENT_VERT:
        return ECORE_X_GRAVITY_CENTER;

      case E_GADCON_ORIENT_LEFT:
        return ECORE_X_GRAVITY_CENTER;

      case E_GADCON_ORIENT_RIGHT:
        return ECORE_X_GRAVITY_CENTER;

      case E_GADCON_ORIENT_TOP:
        return ECORE_X_GRAVITY_CENTER;

      case E_GADCON_ORIENT_BOTTOM:
        return ECORE_X_GRAVITY_CENTER;

      case E_GADCON_ORIENT_CORNER_TL:
        return ECORE_X_GRAVITY_S;

      case E_GADCON_ORIENT_CORNER_TR:
        return ECORE_X_GRAVITY_S;

      case E_GADCON_ORIENT_CORNER_BL:
        return ECORE_X_GRAVITY_N;

      case E_GADCON_ORIENT_CORNER_BR:
        return ECORE_X_GRAVITY_N;

      case E_GADCON_ORIENT_CORNER_LT:
        return ECORE_X_GRAVITY_E;

      case E_GADCON_ORIENT_CORNER_RT:
        return ECORE_X_GRAVITY_W;

      case E_GADCON_ORIENT_CORNER_LB:
        return ECORE_X_GRAVITY_E;

      case E_GADCON_ORIENT_CORNER_RB:
        return ECORE_X_GRAVITY_W;

      default:
        return ECORE_X_GRAVITY_STATIC;
     }
}

static Icon *
_systray_xembed_icon_add(Instance_Xembed *xembed, const Ecore_X_Window win)
{
   Ecore_X_Gravity gravity;
   Evas_Object *rect;
   Evas_Coord w, h, sz = 48;
   Icon *icon;

   /* assuming systray must be on a shelf here */
   switch (systray_gadcon_get(xembed->inst)->orient)
     {
      case E_GADCON_ORIENT_HORIZ:
      case E_GADCON_ORIENT_TOP:
      case E_GADCON_ORIENT_BOTTOM:
      case E_GADCON_ORIENT_CORNER_TL:
      case E_GADCON_ORIENT_CORNER_TR:
      case E_GADCON_ORIENT_CORNER_BL:
      case E_GADCON_ORIENT_CORNER_BR:
        sz = systray_gadcon_get(xembed->inst)->shelf->h;
        break;

      case E_GADCON_ORIENT_VERT:
      case E_GADCON_ORIENT_LEFT:
      case E_GADCON_ORIENT_RIGHT:
      case E_GADCON_ORIENT_CORNER_LT:
      case E_GADCON_ORIENT_CORNER_RT:
      case E_GADCON_ORIENT_CORNER_LB:
      case E_GADCON_ORIENT_CORNER_RB:
        sz = systray_gadcon_get(xembed->inst)->shelf->w;
        break;

      default:
        break;
     }

   sz = sz - 5;
   w = h = e_util_icon_size_normalize(sz);

   rect = evas_object_rectangle_add(systray_evas_get(xembed->inst));
   if (!rect)
     return NULL;
   evas_object_color_set(rect, 0, 0, 0, 0);
   evas_object_resize(rect, w, h);
   evas_object_show(rect);

   icon = malloc(sizeof(Icon));
   if (!icon)
     {
        evas_object_del(rect);
        return NULL;
     }
   icon->win = win;
   icon->xembed = xembed;
   icon->rect = rect;

   gravity = _systray_xembed_gravity(xembed);
   ecore_x_icccm_size_pos_hints_set(win, 1, gravity,
                                    w, h, w, h, w, h, 0, 0,
                                    1.0, (double)w / (double)h);

   ecore_x_window_reparent(win, xembed->win.base, 0, 0);
   ecore_x_window_client_manage(win);
   ecore_x_window_save_set_add(win);
   ecore_x_window_shape_events_select(win, 1);

   evas_object_event_callback_add
     (rect, EVAS_CALLBACK_MOVE, _systray_xembed_icon_cb_move, icon);
   evas_object_event_callback_add
     (rect, EVAS_CALLBACK_RESIZE, _systray_xembed_icon_cb_resize, icon);

   xembed->icons = eina_list_append(xembed->icons, icon);
   systray_edje_box_prepend(xembed->inst, rect);
   systray_size_updated(xembed->inst);
   _systray_xembed_icon_geometry_apply(icon);

   ecore_x_window_show(win);
   if ((!xembed->ec) || (!xembed->ec->comp->nocomp))
     ecore_x_window_show(xembed->win.base);

   return icon;
}

static void
_systray_xembed_icon_del_list(Instance_Xembed *xembed, Eina_List *l, Icon *icon)
{
   xembed->icons = eina_list_remove_list(xembed->icons, l);

   ecore_x_window_save_set_del(icon->win);
   ecore_x_window_hide(icon->win);
   ecore_x_window_reparent(icon->win, xembed->inst->comp->man->root, 0, 0);
   evas_object_del(icon->rect);
   free(icon);

   systray_size_updated(xembed->inst);
}

static Ecore_X_Atom
_systray_xembed_atom_st_get(int screen_num)
{
   if ((_last_st_num == -1) || (_last_st_num != screen_num))
     {
        char buf[32];
        snprintf(buf, sizeof(buf), "_NET_SYSTEM_TRAY_S%d", screen_num);
        _atom_st_num = ecore_x_atom_get(buf);
        _last_st_num = screen_num;
     }

   return _atom_st_num;
}

static Eina_Bool
_systray_xembed_selection_owner_set(int screen_num, Ecore_X_Window win)
{
   Ecore_X_Atom atom;
   Ecore_X_Window cur_selection;
   Eina_Bool ret;

   atom = _systray_xembed_atom_st_get(screen_num);
   ecore_x_selection_owner_set(win, atom, ecore_x_current_time_get());
   ecore_x_sync();
   cur_selection = ecore_x_selection_owner_get(atom);

   ret = (cur_selection == win);
   if (!ret)
     fprintf(stderr, "SYSTRAY: tried to set selection to %#x, but got %#x\n",
             win, cur_selection);

   return ret;
}

static Eina_Bool
_systray_xembed_selection_owner_set_current(Instance_Xembed *xembed)
{
   return _systray_xembed_selection_owner_set
     (systray_manager_number_get(xembed->inst), xembed->win.selection);
}

static void
_systray_xembed_deactivate(Instance_Xembed *xembed)
{
   if (xembed->win.selection == 0) return;

   while (xembed->icons)
     _systray_xembed_icon_del_list(xembed, xembed->icons, xembed->icons->data);

   xembed->win.selection = 0;
   _systray_xembed_selection_owner_set_current(xembed);
   ecore_x_sync();
   if (xembed->ec)
     {
        evas_object_hide(xembed->ec->frame);
        e_object_del(E_OBJECT(xembed->ec));
     }
   ecore_x_window_free(xembed->win.base);
   xembed->win.base = 0;
}

static Eina_Bool
_systray_xembed_base_create(Instance_Xembed *xembed)
{
   const Evas_Object *box;
   Evas_Coord x, y;
   unsigned short r = 0, g = 0, b = 0;
   const char *color;

   if (systray_gadcon_get(xembed->inst)->shelf &&
       e_util_strcmp(systray_gadcon_get(xembed->inst)->shelf->style, "invisible"))
     {
        color = edje_object_data_get(systray_edje_get(xembed->inst),
                                     systray_style_get(xembed->inst));

        if (color && (sscanf(color, "%hu %hu %hu", &r, &g, &b) == 3))
          {
             r = (65535 * (unsigned int)r) / 255;
             g = (65535 * (unsigned int)g) / 255;
             b = (65535 * (unsigned int)b) / 255;
          }
        else
          r = g = b = (unsigned short)65535;
     }

   box = systray_box_get(xembed->inst);
   if (!box)
     return EINA_FALSE;

   evas_object_geometry_get(box, &x, &y, NULL, NULL);
   xembed->win.base = ecore_x_window_override_new(0, x, y, 1, 1);
   ecore_x_netwm_window_type_set(xembed->win.base, ECORE_X_WINDOW_TYPE_DOCK);
   //fprintf(stderr, "xembed->win.base = %u\n", xembed->win.base);
   ecore_x_icccm_title_set(xembed->win.base, "noshadow_systray_base");
   ecore_x_icccm_name_class_set(xembed->win.base, "systray", "holder");
   ecore_x_netwm_name_set(xembed->win.base, "noshadow_systray_base");
   ecore_x_window_background_color_set(xembed->win.base, r, g, b);
   ecore_x_window_show(xembed->win.base);

   return EINA_TRUE;
}

static Eina_Bool
_systray_xembed_activate(Instance_Xembed *xembed)
{
   unsigned int visual;
   Ecore_X_Atom atom;
   Ecore_X_Window old_win;
   Ecore_X_Window_Attributes attr;

   if (xembed->win.selection != 0) return 1;

   atom = _systray_xembed_atom_st_get(systray_manager_number_get(xembed->inst));
   old_win = ecore_x_selection_owner_get(atom);
   if (old_win && (old_win != xembed->inst->comp->cm_selection)) return 0;

   if (xembed->win.base == 0)
     {
        if (!_systray_xembed_base_create(xembed))
          return 0;
     }

   xembed->win.selection = xembed->inst->comp->cm_selection;
   if (old_win) return 1;
   if (!_systray_xembed_selection_owner_set_current(xembed))
     {
        xembed->win.selection = 0;
        ecore_x_window_free(xembed->win.base);
        xembed->win.base = 0;
        return 0;
     }

   ecore_x_window_attributes_get(xembed->win.base, &attr);

   visual = ecore_x_visual_id_get(attr.visual);
   ecore_x_window_prop_card32_set(xembed->win.selection, _atom_st_visual,
                                  (void *)&visual, 1);

   ecore_x_client_message32_send(systray_root_get(xembed->inst),
                                 _atom_manager,
                                 ECORE_X_EVENT_MASK_WINDOW_CONFIGURE,
                                 ecore_x_current_time_get(), atom,
                                 xembed->win.selection, 0, 0);

   return 1;
}

static Eina_Bool
_systray_xembed_activate_retry(void *data)
{
   Instance_Xembed *xembed = data;
   Eina_Bool ret;

   fputs("SYSTRAY: reactivate...\n", stderr);
   ret = _systray_xembed_activate(xembed);
   if (ret)
     fputs("SYSTRAY: activate success!\n", stderr);
   else
     fprintf(stderr, "SYSTRAY: activate failure! retrying in %0.1f seconds\n",
             RETRY_TIMEOUT);

   if (!ret)
     return ECORE_CALLBACK_RENEW;

   xembed->timer.retry = NULL;
   return ECORE_CALLBACK_CANCEL;
}

static void
_systray_xembed_retry(Instance_Xembed *xembed)
{
   if (xembed->timer.retry) return;
   xembed->timer.retry = ecore_timer_add
       (RETRY_TIMEOUT, _systray_xembed_activate_retry, xembed);
}

static Eina_Bool
_systray_xembed_activate_retry_first(void *data)
{
   Instance_Xembed *xembed = data;
   Eina_Bool ret;

   fputs("SYSTRAY: reactivate (first time)...\n", stderr);
   ret = _systray_xembed_activate(xembed);
   if (ret)
     {
        fputs("SYSTRAY: activate success!\n", stderr);
        xembed->timer.retry = NULL;
        return ECORE_CALLBACK_CANCEL;
     }

   fprintf(stderr, "SYSTRAY: activate failure! retrying in %0.1f seconds\n",
           RETRY_TIMEOUT);

   xembed->timer.retry = NULL;
   _systray_xembed_retry(xembed);
   return ECORE_CALLBACK_CANCEL;
}

static void
_systray_xembed_handle_request_dock(Instance_Xembed *xembed, Ecore_X_Event_Client_Message *ev)
{
   Ecore_X_Window win = (Ecore_X_Window)ev->data.l[2];
   Ecore_X_Time t;
   const Eina_List *l;
   Icon *icon;
   unsigned int val[2];
   int r;

   EINA_LIST_FOREACH(xembed->icons, l, icon)
     if (icon->win == win)
       return;

   icon = _systray_xembed_icon_add(xembed, win);
   if (!icon)
     return;

   r = ecore_x_window_prop_card32_get(win, _atom_xembed_info, val, 2);
   if (r < 2)
     {
        /*
           fprintf(stderr, "SYSTRAY: win %#x does not support _XEMBED_INFO (%d)\n",
                win, r);
         */
        return;
     }

   t = ecore_x_current_time_get();
   ecore_x_client_message32_send(win, _atom_xembed,
                                 ECORE_X_EVENT_MASK_NONE,
                                 t, XEMBED_EMBEDDED_NOTIFY, 0,
                                 xembed->win.base, 0);
}

static void
_systray_xembed_handle_op_code(Instance_Xembed *xembed, Ecore_X_Event_Client_Message *ev)
{
   unsigned long message = ev->data.l[1];

   switch (message)
     {
      case SYSTEM_TRAY_REQUEST_DOCK:
        _systray_xembed_handle_request_dock(xembed, ev);
        break;

      case SYSTEM_TRAY_BEGIN_MESSAGE:
      case SYSTEM_TRAY_CANCEL_MESSAGE:
        fputs("SYSTRAY TODO: handle messages (anyone uses this?)\n", stderr);
        break;

      default:
        fprintf(stderr,
                "SYSTRAY: error, unknown message op code: %ld, win: %#lx\n",
                message, ev->data.l[2]);
     }
}

static void
_systray_xembed_handle_message(Instance_Xembed *xembed __UNUSED__, Ecore_X_Event_Client_Message *ev)
{
   fprintf(stderr, "SYSTRAY TODO: message op: %ld, data: %ld, %ld, %ld\n",
           ev->data.l[1], ev->data.l[2], ev->data.l[3], ev->data.l[4]);
}

static void
_systray_xembed_handle_xembed(Instance_Xembed *xembed __UNUSED__, Ecore_X_Event_Client_Message *ev __UNUSED__)
{
   unsigned long message = ev->data.l[1];

   switch (message)
     {
      case XEMBED_EMBEDDED_NOTIFY:
      case XEMBED_WINDOW_ACTIVATE:
      case XEMBED_WINDOW_DEACTIVATE:
      case XEMBED_REQUEST_FOCUS:
      case XEMBED_FOCUS_IN:
      case XEMBED_FOCUS_OUT:
      case XEMBED_FOCUS_NEXT:
      case XEMBED_FOCUS_PREV:
      case XEMBED_MODALITY_ON:
      case XEMBED_MODALITY_OFF:
      case XEMBED_REGISTER_ACCELERATOR:
      case XEMBED_UNREGISTER_ACCELERATOR:
      case XEMBED_ACTIVATE_ACCELERATOR:
      default:
        fprintf(stderr,
                "SYSTRAY: unsupported xembed: %#lx, %#lx, %#lx, %#lx\n",
                ev->data.l[1], ev->data.l[2], ev->data.l[3], ev->data.l[4]);
     }
}

static Eina_Bool
_systray_xembed_cb_client_message(void *data, int type __UNUSED__, void *event)
{
   Ecore_X_Event_Client_Message *ev = event;
   Instance_Xembed *xembed = data;

   if (ev->message_type == _atom_st_op_code)
     _systray_xembed_handle_op_code(xembed, ev);
   else if (ev->message_type == _atom_st_msg_data)
     _systray_xembed_handle_message(xembed, ev);
   else if (ev->message_type == _atom_xembed)
     _systray_xembed_handle_xembed(xembed, ev);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_systray_xembed_cb_window_destroy(void *data, int type __UNUSED__, void *event)
{
   Ecore_X_Event_Window_Destroy *ev = event;
   Instance_Xembed *xembed = data;
   Icon *icon;
   Eina_List *l;

   EINA_LIST_FOREACH(xembed->icons, l, icon)
     if (icon->win == ev->win)
       {
          _systray_xembed_icon_del_list(xembed, l, icon);
          break;
       }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_systray_xembed_cb_window_show(void *data, int type __UNUSED__, void *event)
{
   Ecore_X_Event_Window_Show *ev = event;
   Instance_Xembed *xembed = data;
   Icon *icon;
   Eina_List *l;

   EINA_LIST_FOREACH(xembed->icons, l, icon)
     if (icon->win == ev->win)
       {
          _systray_xembed_icon_geometry_apply(icon);
          break;
       }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_systray_xembed_cb_window_configure(void *data, int type __UNUSED__, void *event)
{
   Ecore_X_Event_Window_Configure *ev = event;
   Instance_Xembed *xembed = data;
   Icon *icon;
   const Eina_List *l;

   EINA_LIST_FOREACH(xembed->icons, l, icon)
     if (icon->win == ev->win)
       {
          _systray_xembed_icon_geometry_apply(icon);
          break;
       }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_systray_xembed_cb_reparent_notify(void *data, int type __UNUSED__, void *event)
{
   Ecore_X_Event_Window_Reparent *ev = event;
   Instance_Xembed *xembed = data;
   Icon *icon;
   Eina_List *l;

   EINA_LIST_FOREACH(xembed->icons, l, icon)
     if ((icon->win == ev->win) && (ev->parent != xembed->win.base))
       {
          _systray_xembed_icon_del_list(xembed, l, icon);
          break;
       }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_systray_xembed_cb_selection_clear(void *data, int type __UNUSED__, void *event)
{
   Ecore_X_Event_Selection_Clear *ev = event;
   Instance_Xembed *xembed = data;
   int manager = systray_manager_number_get(xembed->inst);

   if ((ev->win == xembed->win.selection) && (xembed->win.selection) &&
       (ev->atom == _systray_xembed_atom_st_get(manager)) &&
       (ecore_x_selection_owner_get(ev->atom) != xembed->win.selection))
     {
        while (xembed->icons)
          _systray_xembed_icon_del_list(xembed, xembed->icons,
                                        xembed->icons->data);

        xembed->win.selection = 0;
        ecore_x_window_free(xembed->win.base);
        xembed->win.base = 0;
        _systray_xembed_retry(xembed);
     }
   return ECORE_CALLBACK_PASS_ON;
}

void
systray_xembed_orient_set(Instance_Xembed *xembed, E_Gadcon_Orient orient)
{
   unsigned int systray_orient;

   if (e_comp_get(NULL)->comp_type != E_PIXMAP_TYPE_X) return;
   EINA_SAFETY_ON_NULL_RETURN(xembed);

   switch (orient)
     {
      case E_GADCON_ORIENT_FLOAT:
        systray_orient = _NET_SYSTEM_TRAY_ORIENTATION_HORZ;
        break;

      case E_GADCON_ORIENT_HORIZ:
        systray_orient = _NET_SYSTEM_TRAY_ORIENTATION_HORZ;
        break;

      case E_GADCON_ORIENT_VERT:
        systray_orient = _NET_SYSTEM_TRAY_ORIENTATION_VERT;
        break;

      case E_GADCON_ORIENT_LEFT:
        systray_orient = _NET_SYSTEM_TRAY_ORIENTATION_VERT;
        break;

      case E_GADCON_ORIENT_RIGHT:
        systray_orient = _NET_SYSTEM_TRAY_ORIENTATION_VERT;
        break;

      case E_GADCON_ORIENT_TOP:
        systray_orient = _NET_SYSTEM_TRAY_ORIENTATION_HORZ;
        break;

      case E_GADCON_ORIENT_BOTTOM:
        systray_orient = _NET_SYSTEM_TRAY_ORIENTATION_HORZ;
        break;

      case E_GADCON_ORIENT_CORNER_TL:
        systray_orient = _NET_SYSTEM_TRAY_ORIENTATION_HORZ;
        break;

      case E_GADCON_ORIENT_CORNER_TR:
        systray_orient = _NET_SYSTEM_TRAY_ORIENTATION_HORZ;
        break;

      case E_GADCON_ORIENT_CORNER_BL:
        systray_orient = _NET_SYSTEM_TRAY_ORIENTATION_HORZ;
        break;

      case E_GADCON_ORIENT_CORNER_BR:
        systray_orient = _NET_SYSTEM_TRAY_ORIENTATION_HORZ;
        break;

      case E_GADCON_ORIENT_CORNER_LT:
        systray_orient = _NET_SYSTEM_TRAY_ORIENTATION_VERT;
        break;

      case E_GADCON_ORIENT_CORNER_RT:
        systray_orient = _NET_SYSTEM_TRAY_ORIENTATION_VERT;
        break;

      case E_GADCON_ORIENT_CORNER_LB:
        systray_orient = _NET_SYSTEM_TRAY_ORIENTATION_VERT;
        break;

      case E_GADCON_ORIENT_CORNER_RB:
        systray_orient = _NET_SYSTEM_TRAY_ORIENTATION_VERT;
        break;

      default:
        systray_orient = _NET_SYSTEM_TRAY_ORIENTATION_HORZ;
     }

   ecore_x_window_prop_card32_set
     (xembed->win.selection, _atom_st_orient, &systray_orient, 1);

   systray_size_updated(xembed->inst);
}

static Eina_Bool
_systray_xembed_client_add(Instance_Xembed *xembed, int t EINA_UNUSED, E_Event_Client *ev)
{
   if (e_client_util_win_get(ev->ec) == xembed->win.base)
     {
        /* this is some bullshit. */
        xembed->ec = ev->ec;
        ev->ec->internal_no_remember = ev->ec->borderless = ev->ec->visible = ev->ec->internal = 1;
        ev->ec->border.changed = 1;
        ecore_x_window_shape_events_select(e_client_util_win_get(ev->ec), 0);
        _xembed_win_resize(xembed);
        ev->ec->icccm.take_focus = ev->ec->icccm.accepts_focus = 0;
        EC_CHANGED(ev->ec);
        if (eina_list_count(xembed->icons) == 0)
          evas_object_hide(xembed->ec->frame);
        else
          evas_object_show(xembed->ec->frame);
        _systray_xembed_restack(xembed);
        evas_object_data_set(ev->ec->frame, "comp_skip", (void*)1);
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_systray_xembed_comp_enable(void *d EINA_UNUSED, int t EINA_UNUSED, void *ev EINA_UNUSED)
{
   if (systray_ctx_get()->config->use_xembed)
     {
        instance->xembed = systray_xembed_new(instance);
        systray_size_updated(instance);
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_systray_xembed_comp_disable(void *d EINA_UNUSED, int t EINA_UNUSED, void *ev EINA_UNUSED)
{
   E_FREE_FUNC(instance->xembed, systray_xembed_free);
   return ECORE_CALLBACK_RENEW;
}

Instance_Xembed *
systray_xembed_new(Instance *inst)
{
   Evas_Object *ui = systray_edje_get(inst);
   E_Gadcon *gc = inst->gcc->gadcon;
   Instance_Xembed *xembed;

   if (e_comp_get(NULL)->comp_type != E_PIXMAP_TYPE_X) return NULL;
   xembed = calloc(1, sizeof(Instance_Xembed));

   EINA_SAFETY_ON_NULL_RETURN_VAL(xembed, NULL);
   xembed->inst = inst;

   xembed->win.parent = e_comp_get(gc)->man->root;

   xembed->win.base = 0;
   xembed->win.selection = 0;

   if (!_systray_xembed_activate(xembed))
     {
        if (!xembed->timer.retry)
          xembed->timer.retry = ecore_timer_add
              (0.1, _systray_xembed_activate_retry_first, xembed);
     }

   evas_object_event_callback_add(ui, EVAS_CALLBACK_MOVE,
                                  _systray_xembed_cb_move, xembed);
   evas_object_event_callback_add(ui, EVAS_CALLBACK_RESIZE,
                                  _systray_xembed_cb_resize, xembed);
   if (inst->gcc->gadcon->shelf)
     {
        evas_object_event_callback_add(inst->gcc->gadcon->shelf->comp_object, EVAS_CALLBACK_HIDE, _systray_xembed_cb_hide, xembed);
        evas_object_event_callback_add(inst->gcc->gadcon->shelf->comp_object, EVAS_CALLBACK_SHOW, _systray_xembed_cb_show, xembed);
     }

   E_LIST_HANDLER_APPEND(xembed->handlers, E_EVENT_CLIENT_ADD, (Ecore_Event_Handler_Cb)_systray_xembed_client_add, xembed);
   E_LIST_HANDLER_APPEND(xembed->handlers, ECORE_X_EVENT_CLIENT_MESSAGE, _systray_xembed_cb_client_message, xembed);
   E_LIST_HANDLER_APPEND(xembed->handlers, ECORE_X_EVENT_WINDOW_DESTROY, _systray_xembed_cb_window_destroy, xembed);
   E_LIST_HANDLER_APPEND(xembed->handlers, ECORE_X_EVENT_WINDOW_SHOW, _systray_xembed_cb_window_show, xembed);
   E_LIST_HANDLER_APPEND(xembed->handlers, ECORE_X_EVENT_WINDOW_REPARENT, _systray_xembed_cb_reparent_notify, xembed);
   E_LIST_HANDLER_APPEND(xembed->handlers, ECORE_X_EVENT_SELECTION_CLEAR, _systray_xembed_cb_selection_clear, xembed);
   E_LIST_HANDLER_APPEND(xembed->handlers, ECORE_X_EVENT_WINDOW_CONFIGURE, _systray_xembed_cb_window_configure, xembed);

   return xembed;
}

void
systray_xembed_free(Instance_Xembed *xembed)
{
   Evas_Object *ui;

   if (!xembed) return;

   ui = systray_edje_get(xembed->inst);

   evas_object_event_callback_del(ui, EVAS_CALLBACK_MOVE,
                                  _systray_xembed_cb_move);
   evas_object_event_callback_del(ui, EVAS_CALLBACK_RESIZE,
                                  _systray_xembed_cb_resize);
   if (xembed->inst->gcc->gadcon->shelf)
     {
        evas_object_event_callback_del_full(xembed->inst->gcc->gadcon->shelf->comp_object, EVAS_CALLBACK_HIDE, _systray_xembed_cb_hide, xembed);
        evas_object_event_callback_del_full(xembed->inst->gcc->gadcon->shelf->comp_object, EVAS_CALLBACK_SHOW, _systray_xembed_cb_show, xembed);
     }

   _systray_xembed_deactivate(xembed);
   ecore_timer_del(xembed->visibility_timer);

   E_FREE_LIST(xembed->handlers, ecore_event_handler_del);
   if (xembed->timer.retry)
     ecore_timer_del(xembed->timer.retry);

   free(xembed);
}

void
systray_xembed_init(void)
{
   if (e_comp_get(NULL)->comp_type != E_PIXMAP_TYPE_X) return;
   if (!_atom_manager)
     _atom_manager = ecore_x_atom_get("MANAGER");
   if (!_atom_st_orient)
     _atom_st_orient = ecore_x_atom_get("_NET_SYSTEM_TRAY_ORIENTATION");
   if (!_atom_st_visual)
     _atom_st_visual = ecore_x_atom_get("_NET_SYSTEM_TRAY_VISUAL");
   if (!_atom_st_op_code)
     _atom_st_op_code = ecore_x_atom_get("_NET_SYSTEM_TRAY_OPCODE");
   if (!_atom_st_msg_data)
     _atom_st_msg_data = ecore_x_atom_get("_NET_SYSTEM_TRAY_MESSAGE_DATA");
   if (!_atom_xembed)
     _atom_xembed = ecore_x_atom_get("_XEMBED");
   if (!_atom_xembed_info)
     _atom_xembed_info = ecore_x_atom_get("_XEMBED_INFO");
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_COMPOSITOR_ENABLE, _systray_xembed_comp_enable, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_COMPOSITOR_DISABLE, _systray_xembed_comp_disable, NULL);
}

void
systray_xembed_shutdown(void)
{
   E_FREE_LIST(handlers, ecore_event_handler_del);
}

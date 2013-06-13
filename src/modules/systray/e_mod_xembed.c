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
   Evas_Object     *o;
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
   struct
   {
      Ecore_Event_Handler *message;
      Ecore_Event_Handler *destroy;
      Ecore_Event_Handler *show;
      Ecore_Event_Handler *reparent;
      Ecore_Event_Handler *sel_clear;
      Ecore_Event_Handler *configure;
   } handler;
   struct
   {
      Ecore_Timer *retry;
   } timer;
   Eina_List *icons;
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

/* TODO: remove me later: */
static const char _part_box[] = "e.xembed.box";
static const char _part_size[] = "e.xembed.size";
static const char _sig_enable[] = "e,action,xembed,enable";
static const char _sig_disable[] = "e,action,xembed,disable";
/* END TODO: remove me later */

void
systray_xembed_size_updated(Instance_Xembed *xembed)
{
   const Evas_Object *o;
   Evas_Object *ui = systray_edje_get(xembed->inst);
   Evas_Coord x, y, w, h, mw = 1, mh = 1;

   /* this hack is required so we resize the base xwindow */

   edje_object_message_signal_process(ui);
   o = edje_object_part_object_get(ui, _part_box);
   if (!o) return;
   evas_object_size_hint_min_get(o, &w, &h);

   if (w < 1) w = 1;
   if (h < 1) h = 1;

   if (eina_list_count(xembed->icons) == 0)
     ecore_x_window_hide(xembed->win.base);
   else
     ecore_x_window_show(xembed->win.base);
   edje_object_size_min_calc(systray_edje_get(xembed->inst), &mw, &mh);
   e_gadcon_client_min_size_set(systray_gadcon_client_get(xembed->inst), mw, mh);

   evas_object_geometry_get(o, &x, &y, &w, &h);
   ecore_x_window_move_resize(xembed->win.base, x, y, w, h);
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
_systray_xembed_icon_geometry_apply(Icon *icon)
{
   const Evas_Object *o, *ui = systray_edje_get(icon->xembed->inst);
   Evas_Coord x, y, w, h, wx, wy;

   /* hack required so we reposition x window inside parent */
   o = edje_object_part_object_get(ui, _part_size);
   if (!o) return;

   evas_object_geometry_get(icon->o, &x, &y, &w, &h);
   evas_object_geometry_get(o, &wx, &wy, NULL, NULL);
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
   Evas_Object *o;
   Evas_Coord w, h, sz = 48;
   Icon *icon;

   edje_object_part_geometry_get(systray_edje_get(xembed->inst), _part_size,
                                 NULL, NULL, &w, &h);
   if (w > h)
     w = h;
   else
     h = w;

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
      default:
        break;
     }
   if ((w < 16) && (sz > 16))
     w = h = sz - 5;

   w = h = e_util_icon_size_normalize(w);
   if (w > sz - 5)
     w = h = e_util_icon_size_normalize(sz - 5);

   o = evas_object_rectangle_add(systray_evas_get(xembed->inst));
   if (!o)
     return NULL;
   evas_object_color_set(o, 0, 0, 0, 0);
   evas_object_resize(o, w, h);
   evas_object_show(o);

   icon = malloc(sizeof(*icon));
   if (!icon)
     {
        evas_object_del(o);
        return NULL;
     }
   icon->win = win;
   icon->xembed = xembed;
   icon->o = o;

   gravity = _systray_xembed_gravity(xembed);
   ecore_x_icccm_size_pos_hints_set(win, 1, gravity,
                                    w, h, w, h, w, h, 0, 0,
                                    1.0, (double)w / (double)h);

   ecore_x_window_reparent(win, xembed->win.base, 0, 0);
   ecore_x_window_raise(win);
   ecore_x_window_client_manage(win);
   ecore_x_window_save_set_add(win);
   ecore_x_window_shape_events_select(win, 1);

   //ecore_x_window_geometry_get(win, NULL, NULL, &w, &h);

   evas_object_event_callback_add
     (o, EVAS_CALLBACK_MOVE, _systray_xembed_icon_cb_move, icon);
   evas_object_event_callback_add
     (o, EVAS_CALLBACK_RESIZE, _systray_xembed_icon_cb_resize, icon);

   xembed->icons = eina_list_append(xembed->icons, icon);
   systray_edje_box_append(xembed->inst, _part_box, o);
   systray_size_updated(xembed->inst);
   _systray_xembed_icon_geometry_apply(icon);

   ecore_x_window_show(win);

   return icon;
}

static void
_systray_xembed_icon_del_list(Instance_Xembed *xembed, Eina_List *l, Icon *icon)
{
   xembed->icons = eina_list_remove_list(xembed->icons, l);

   ecore_x_window_save_set_del(icon->win);
   ecore_x_window_reparent(icon->win, 0, 0, 0);
   evas_object_del(icon->o);
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

   systray_edje_emit(xembed->inst, _sig_disable);

   while (xembed->icons)
     _systray_xembed_icon_del_list(xembed, xembed->icons, xembed->icons->data);

   xembed->win.selection = 0;
   _systray_xembed_selection_owner_set_current(xembed);
   ecore_x_sync();
   ecore_x_window_free(xembed->win.base);
   xembed->win.base = 0;
}

static Eina_Bool
_systray_xembed_base_create(Instance_Xembed *xembed)
{
   const Evas_Object *o, *ui = systray_edje_get(xembed->inst);
   Evas_Coord x, y, w, h;
   unsigned short r, g, b;
   const char *color;
   Eina_Bool invis = EINA_FALSE;

   if (systray_gadcon_get(xembed->inst)->shelf &&
       (!e_util_strcmp(systray_gadcon_get(xembed->inst)->shelf->style, "invisible")))
     invis = EINA_TRUE;
   else
     {
        color = edje_object_data_get(ui, systray_style_get(xembed->inst));

        if (color && (sscanf(color, "%hu %hu %hu", &r, &g, &b) == 3))
          {
             r = (65535 * (unsigned int)r) / 255;
             g = (65535 * (unsigned int)g) / 255;
             b = (65535 * (unsigned int)b) / 255;
          }
        else
          r = g = b = (unsigned short)65535;
     }

   o = edje_object_part_object_get(ui, _part_size);
   if (!o)
     return EINA_FALSE;

   evas_object_geometry_get(o, &x, &y, &w, &h);
   if (w < 1) w = 1;
   if (h < 1) h = 1;
   xembed->win.base = ecore_x_window_new(0, x, y, w, h);
   e_comp_ignore_win_add(xembed->win.base);
   ecore_x_icccm_title_set(xembed->win.base, "noshadow_systray_base");
   ecore_x_icccm_name_class_set(xembed->win.base, "systray", "holder");
   ecore_x_netwm_name_set(xembed->win.base, "noshadow_systray_base");
   ecore_x_window_reparent(xembed->win.base, xembed->win.parent, x, y);
   if (!invis)
     ecore_x_window_background_color_set(xembed->win.base, r, g, b);
   ecore_x_window_show(xembed->win.base);
   e_container_window_raise(xembed->inst->con, xembed->win.base, E_LAYER_ABOVE);
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
   if (old_win && (old_win != e_comp_get(xembed->inst->con)->cm_selection)) return 0;

   if (xembed->win.base == 0)
     {
        if (!_systray_xembed_base_create(xembed))
          return 0;
     }

   xembed->win.selection = e_comp_get(xembed->inst->con)->cm_selection;
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

   systray_edje_emit(xembed->inst, _sig_enable);

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

   systray_edje_emit(xembed->inst, _sig_disable);

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
   Ecore_X_Window_Attributes attr;
   const Eina_List *l;
   Icon *icon;
   unsigned int val[2];
   int r;

   EINA_LIST_FOREACH(xembed->icons, l, icon)
     if (icon->win == win)
       return;

   if (!ecore_x_window_attributes_get(win, &attr))
     {
        fprintf(stderr, "SYSTRAY: could not get attributes of win %#x\n", win);
        return;
     }

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
                                 xembed->win.selection, 0);
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
        systray_edje_emit(xembed->inst, _sig_disable);

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

Instance_Xembed *
systray_xembed_new(Instance *inst)
{
   Evas_Object *ui = systray_edje_get(inst);
   E_Gadcon *gc = inst->gcc->gadcon;
   Instance_Xembed *xembed = calloc(1, sizeof(Instance_Xembed));

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
        else
          systray_edje_emit(xembed->inst, _sig_disable);
     }

   evas_object_event_callback_add(ui, EVAS_CALLBACK_MOVE,
                                  _systray_xembed_cb_move, xembed);
   evas_object_event_callback_add(ui, EVAS_CALLBACK_RESIZE,
                                  _systray_xembed_cb_resize, xembed);

   xembed->handler.message = ecore_event_handler_add
       (ECORE_X_EVENT_CLIENT_MESSAGE, _systray_xembed_cb_client_message,
        xembed);
   xembed->handler.destroy = ecore_event_handler_add
       (ECORE_X_EVENT_WINDOW_DESTROY, _systray_xembed_cb_window_destroy,
        xembed);
   xembed->handler.show = ecore_event_handler_add
       (ECORE_X_EVENT_WINDOW_SHOW, _systray_xembed_cb_window_show,
        xembed);
   xembed->handler.reparent = ecore_event_handler_add
       (ECORE_X_EVENT_WINDOW_REPARENT, _systray_xembed_cb_reparent_notify,
        xembed);
   xembed->handler.sel_clear = ecore_event_handler_add
       (ECORE_X_EVENT_SELECTION_CLEAR, _systray_xembed_cb_selection_clear,
        xembed);
   xembed->handler.configure = ecore_event_handler_add
       (ECORE_X_EVENT_WINDOW_CONFIGURE, _systray_xembed_cb_window_configure,
        xembed);

   return xembed;
}

void
systray_xembed_free(Instance_Xembed *xembed)
{
   EINA_SAFETY_ON_NULL_RETURN(xembed);

   _systray_xembed_deactivate(xembed);

   if (xembed->handler.message)
     ecore_event_handler_del(xembed->handler.message);
   if (xembed->handler.destroy)
     ecore_event_handler_del(xembed->handler.destroy);
   if (xembed->handler.show)
     ecore_event_handler_del(xembed->handler.show);
   if (xembed->handler.reparent)
     ecore_event_handler_del(xembed->handler.reparent);
   if (xembed->handler.sel_clear)
     ecore_event_handler_del(xembed->handler.sel_clear);
   if (xembed->handler.configure)
     ecore_event_handler_del(xembed->handler.configure);
   if (xembed->timer.retry)
     ecore_timer_del(xembed->timer.retry);

   free(xembed);
}

void
systray_xembed_init(void)
{
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
}

void
systray_xembed_shutdown(void)
{
}

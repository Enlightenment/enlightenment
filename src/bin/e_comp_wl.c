#define E_COMP_WL
#include "e.h"

#include <Evas_Engine_Drm.h>
#include <Ecore_Drm.h>

/* handle include for printing uint64_t */
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#define COMPOSITOR_VERSION 3

#define E_COMP_WL_PIXMAP_CHECK \
   if (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL) return

/* Resource Data Mapping: (wl_resource_get_user_data)
 * 
 * wl_surface == e_pixmap
 * wl_region == eina_tiler
 * wl_subsurface == e_client
 * 
 */

static void _e_comp_wl_subsurface_parent_commit(E_Client *ec, Eina_Bool parent_synchronized);
static void _e_comp_wl_client_idler_add(E_Client *ec);

/* local variables */
/* static Eina_Hash *clients_win_hash = NULL; */
static Eina_List *handlers = NULL;
static Eina_List *_idle_clients = NULL;
static Ecore_Idle_Enterer *_client_idler = NULL;
static double _last_event_time = 0.0;

/* local functions */
static void 
_e_comp_wl_focus_down_set(E_Client *ec)
{
   Ecore_Window win = 0;

   win = e_client_util_pwin_get(ec);
   e_bindings_mouse_grab(E_BINDING_CONTEXT_WINDOW, win);
   e_bindings_wheel_grab(E_BINDING_CONTEXT_WINDOW, win);
}

static void 
_e_comp_wl_focus_check(E_Comp *comp)
{
   E_Client *ec;

   if (stopping) return;
   ec = e_client_focused_get();
   if ((!ec) || (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL))
     e_grabinput_focus(comp->ee_win, E_FOCUS_METHOD_PASSIVE);
}

static void 
_e_comp_wl_log_cb_print(const char *format, va_list args)
{
   EINA_LOG_DOM_INFO(e_log_dom, format, args);
}

static Eina_Bool 
_e_comp_wl_cb_read(void *data, Ecore_Fd_Handler *hdlr EINA_UNUSED)
{
   E_Comp_Data *cdata;

   if (!(cdata = data)) return ECORE_CALLBACK_RENEW;

   /* dispatch pending wayland events */
   wl_event_loop_dispatch(cdata->wl.loop, 0);

   return ECORE_CALLBACK_RENEW;
}

static void 
_e_comp_wl_cb_prepare(void *data, Ecore_Fd_Handler *hdlr EINA_UNUSED)
{
   E_Comp_Data *cdata;

   if (!(cdata = data)) return;

   /* flush pending client events */
   wl_display_flush_clients(cdata->wl.disp);
}

static Eina_Bool 
_e_comp_wl_cb_module_idle(void *data)
{
   E_Comp_Data *cdata;
   E_Module  *mod = NULL;

   if (!(cdata = data)) return ECORE_CALLBACK_RENEW;

   /* check if we are still loading modules */
   if (e_module_loading_get()) return ECORE_CALLBACK_RENEW;

   if (!(mod = e_module_find("wl_desktop_shell")))
     mod = e_module_new("wl_desktop_shell");

   if (mod)
     {
        e_module_enable(mod);

        /* FIXME: NB: 
         * Do we need to dispatch pending wl events here ?? */

        return ECORE_CALLBACK_CANCEL;
     }

   return ECORE_CALLBACK_RENEW;
}

static void 
_e_comp_wl_evas_cb_show(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec, *tmp;
   Eina_List *l;

   if (!(ec = data)) return;

   if (!ec->override) e_hints_window_visible_set(ec);

   if ((!ec->override) && (!ec->re_manage) && (!ec->comp_data->reparented) && 
       (!ec->comp_data->need_reparent))
     {
        ec->comp_data->need_reparent = EINA_TRUE;
        ec->visible = EINA_TRUE;
     }
   if (!e_client_util_ignored_get(ec))
     {
        ec->take_focus = !starting;
        EC_CHANGED(ec);
     }

   if (!ec->comp_data->need_reparent)
     {
        if ((ec->hidden) || (ec->iconic))
          {
             evas_object_hide(ec->frame);
             e_comp_object_damage(ec->frame, 0, 0, ec->w, ec->h);
          }
        else if (!ec->internal_elm_win)
          evas_object_show(ec->frame);

        if (ec->internal_elm_win)
          {
             _e_comp_wl_client_idler_add(ec);
             ec->post_move = EINA_TRUE;
             ec->post_resize = EINA_TRUE;
          }
     }

   EINA_LIST_FOREACH(ec->e.state.video_child, l, tmp)
     evas_object_show(tmp->frame);
}

static void 
_e_comp_wl_evas_cb_hide(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec, *tmp;
   Eina_List *l;

   if (!(ec = data)) return;

   EINA_LIST_FOREACH(ec->e.state.video_child, l, tmp)
     evas_object_hide(tmp->frame);
}

static void 
_e_comp_wl_evas_cb_mouse_in(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec;
   Evas_Event_Mouse_In *ev;
   struct wl_resource *res;
   struct wl_client *wc;
   Eina_List *l;
   uint32_t serial;

   ev = event;
   if (!(ec = data)) return;
   if (e_object_is_del(E_OBJECT(ec))) return;

   if (!ec->comp_data->surface) return;

   wc = wl_resource_get_client(ec->comp_data->surface);
   serial = wl_display_next_serial(ec->comp->wl_comp_data->wl.disp);
   EINA_LIST_FOREACH(ec->comp->wl_comp_data->ptr.resources, l, res)
     {
        if (!e_comp_wl_input_pointer_check(res)) continue;
        if (wl_resource_get_client(res) != wc) continue;
        wl_pointer_send_enter(res, serial, ec->comp_data->surface, 
                              wl_fixed_from_int(ev->canvas.x), 
                              wl_fixed_from_int(ev->canvas.y));
     }
}

static void 
_e_comp_wl_evas_cb_mouse_out(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec;
   struct wl_resource *res;
   struct wl_client *wc;
   Eina_List *l;
   uint32_t serial;

   if (!(ec = data)) return;
   if (ec->cur_mouse_action) return;
   /* FIXME? this is a hack to just reset the cursor whenever we mouse out. not sure if accurate */
   {
      Evas_Object *o;

      ecore_evas_cursor_get(e_comp->ee, &o, NULL, NULL, NULL);
      if (e_comp->pointer->o_ptr != o)
        e_pointer_object_set(e_comp->pointer, NULL, 0, 0);
   }
   if (e_object_is_del(E_OBJECT(ec))) return;

   if (!ec->comp_data->surface) return;

   wc = wl_resource_get_client(ec->comp_data->surface);
   serial = wl_display_next_serial(ec->comp->wl_comp_data->wl.disp);
   EINA_LIST_FOREACH(ec->comp->wl_comp_data->ptr.resources, l, res)
     {
        if (!e_comp_wl_input_pointer_check(res)) continue;
        if (wl_resource_get_client(res) != wc) continue;
        wl_pointer_send_leave(res, serial, ec->comp_data->surface);
     }
}

static void 
_e_comp_wl_evas_cb_mouse_move(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   E_Client *ec;
   Evas_Event_Mouse_Move *ev;
   struct wl_resource *res;
   struct wl_client *wc;
   Eina_List *l;

   ev = event;
   if (!(ec = data)) return;
   if (ec->cur_mouse_action) return;
   if (e_object_is_del(E_OBJECT(ec))) return;
   if (e_client_util_ignored_get(ec)) return;

   ec->comp->wl_comp_data->ptr.x = 
     wl_fixed_from_int(ev->cur.canvas.x - ec->client.x);
   ec->comp->wl_comp_data->ptr.y = 
     wl_fixed_from_int(ev->cur.canvas.y - ec->client.y);

   if (!ec->comp_data->surface) return;

   wc = wl_resource_get_client(ec->comp_data->surface);
   EINA_LIST_FOREACH(ec->comp->wl_comp_data->ptr.resources, l, res)
     {
        if (!e_comp_wl_input_pointer_check(res)) continue;
        if (wl_resource_get_client(res) != wc) continue;
        wl_pointer_send_motion(res, ev->timestamp, 
                               ec->comp->wl_comp_data->ptr.x, 
                               ec->comp->wl_comp_data->ptr.y);
     }
}

static void 
_e_comp_wl_evas_cb_mouse_down(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   E_Client *ec;
   Evas_Event_Mouse_Down *ev;
   struct wl_resource *res;
   struct wl_client *wc;
   Eina_List *l;
   uint32_t serial, btn;

   ev = event;
   if (!(ec = data)) return;
   if (ec->cur_mouse_action) return;
   if (e_object_is_del(E_OBJECT(ec))) return;
   if (e_client_util_ignored_get(ec)) return;

   switch (ev->button)
     {
      case 1:
        btn = BTN_LEFT;
        break;
      case 2:
        btn = BTN_MIDDLE;
        break;
      case 3:
        btn = BTN_RIGHT;
        break;
      default:
        btn = ev->button;
        break;
     }

   ec->comp->wl_comp_data->ptr.button = btn;

   if (!ec->comp_data->surface) return;

   wc = wl_resource_get_client(ec->comp_data->surface);
   serial = wl_display_next_serial(ec->comp->wl_comp_data->wl.disp);
   EINA_LIST_FOREACH(ec->comp->wl_comp_data->ptr.resources, l, res)
     {
        if (!e_comp_wl_input_pointer_check(res)) continue;
        if (wl_resource_get_client(res) != wc) continue;
        wl_pointer_send_button(res, serial, ev->timestamp, btn, 
                               WL_POINTER_BUTTON_STATE_PRESSED);
     }
}

static void 
_e_comp_wl_evas_cb_mouse_up(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   E_Client *ec;
   Evas_Event_Mouse_Up *ev;
   struct wl_resource *res;
   struct wl_client *wc;
   Eina_List *l;
   uint32_t serial, btn;

   ev = event;
   if (!(ec = data)) return;
   if (ec->cur_mouse_action) return;
   if (e_object_is_del(E_OBJECT(ec))) return;
   if (e_client_util_ignored_get(ec)) return;

   switch (ev->button)
     {
      case 1:
        btn = BTN_LEFT;
        break;
      case 2:
        btn = BTN_MIDDLE;
        break;
      case 3:
        btn = BTN_RIGHT;
        break;
      default:
        btn = ev->button;
        break;
     }

   ec->comp->wl_comp_data->ptr.button = btn;

   if (!ec->comp_data->surface) return;

   wc = wl_resource_get_client(ec->comp_data->surface);
   serial = wl_display_next_serial(ec->comp->wl_comp_data->wl.disp);
   EINA_LIST_FOREACH(ec->comp->wl_comp_data->ptr.resources, l, res)
     {
        if (!e_comp_wl_input_pointer_check(res)) continue;
        if (wl_resource_get_client(res) != wc) continue;
        wl_pointer_send_button(res, serial, ev->timestamp, btn, 
                               WL_POINTER_BUTTON_STATE_RELEASED);
     }
}

static void 
_e_comp_wl_evas_cb_mouse_wheel(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   E_Client *ec;
   Evas_Event_Mouse_Wheel *ev;
   struct wl_resource *res;
   struct wl_client *wc;
   Eina_List *l;
   uint32_t axis, dir;

   ev = event;
   if (!(ec = data)) return;
   if (ec->cur_mouse_action) return;
   if (e_object_is_del(E_OBJECT(ec))) return;
   if (e_client_util_ignored_get(ec)) return;

   if (ev->direction == 0)
     axis = WL_POINTER_AXIS_VERTICAL_SCROLL;
   else
     axis = WL_POINTER_AXIS_HORIZONTAL_SCROLL;

   if (ev->z < 0)
     dir = -wl_fixed_from_int(abs(ev->z));
   else
     dir = wl_fixed_from_int(ev->z);

   if (!ec->comp_data->surface) return;

   wc = wl_resource_get_client(ec->comp_data->surface);
   EINA_LIST_FOREACH(ec->comp->wl_comp_data->ptr.resources, l, res)
     {
        if (!e_comp_wl_input_pointer_check(res)) continue;
        if (wl_resource_get_client(res) != wc) continue;
        wl_pointer_send_axis(res, ev->timestamp, axis, dir);
     }
}

static void 
_e_comp_wl_client_priority_adjust(int pid, int set, int adj, Eina_Bool use_adj, Eina_Bool adj_child, Eina_Bool do_child)
{
   int n;

   n = set;
   if (use_adj) n = (getpriority(PRIO_PROCESS, pid) + adj);
   setpriority(PRIO_PROCESS, pid, n);

   if (do_child)
     {
        Eina_List *files;
        char *file, buff[PATH_MAX];
        FILE *f;
        int pid2, ppid;

        files = ecore_file_ls("/proc");
        EINA_LIST_FREE(files, file)
          {
             if (isdigit(file[0]))
               {
                  snprintf(buff, sizeof(buff), "/proc/%s/stat", file);
                  if ((f = fopen(buff, "r")))
                    {
                       pid2 = -1;
                       ppid = -1;
                       if (fscanf(f, "%i %*s %*s %i %*s", &pid2, &ppid) == 2)
                         {
                            fclose(f);
                            if (ppid == pid)
                              {
                                 if (adj_child)
                                   _e_comp_wl_client_priority_adjust(pid2, set, 
                                                                     adj, EINA_TRUE,
                                                                     adj_child, do_child);
                                 else
                                   _e_comp_wl_client_priority_adjust(pid2, set, 
                                                                     adj, use_adj,
                                                                     adj_child, do_child);
                              }
                         }
                       else 
                         fclose(f);
                    }
               }
             free(file);
          }
     }
}

static void 
_e_comp_wl_client_priority_raise(E_Client *ec)
{
   if (ec->netwm.pid <= 0) return;
   if (ec->netwm.pid == getpid()) return;
   _e_comp_wl_client_priority_adjust(ec->netwm.pid, 
                                     e_config->priority - 1, -1, 
                                     EINA_FALSE, EINA_TRUE, EINA_FALSE);
}

static void 
_e_comp_wl_client_priority_normal(E_Client *ec)
{
   if (ec->netwm.pid <= 0) return;
   if (ec->netwm.pid == getpid()) return;
   _e_comp_wl_client_priority_adjust(ec->netwm.pid, e_config->priority, 1, 
                                     EINA_FALSE, EINA_TRUE, EINA_FALSE);
}

static Eina_Bool 
_e_comp_wl_client_cb_idle(void *data EINA_UNUSED)
{
   E_Client *ec;
   E_Comp_Client_Data *cdata;

   EINA_LIST_FREE(_idle_clients, ec)
     {
        if (e_object_is_del(E_OBJECT(ec))) continue;

        if (!(cdata = ec->comp_data)) continue;

        if ((ec->post_resize) && (!ec->maximized))
          {
             if (cdata->shell.configure_send)
               cdata->shell.configure_send(cdata->shell.surface, 
                                           ec->comp->wl_comp_data->resize.edges,
                                           ec->client.w, ec->client.h);
          }

        ec->post_move = EINA_FALSE;
        ec->post_resize = EINA_FALSE;
     }

   _client_idler = NULL;
   return EINA_FALSE;
}

static void 
_e_comp_wl_client_idler_add(E_Client *ec)
{
   if (!ec) return;

   if (!_client_idler)
     _client_idler = ecore_idle_enterer_add(_e_comp_wl_client_cb_idle, NULL);

   if (!eina_list_data_find(_idle_clients, ec))
     _idle_clients = eina_list_append(_idle_clients, ec);
}

static void
_e_comp_wl_client_focus(E_Client *ec)
{
   struct wl_resource *res;
   struct wl_client *wc;
   uint32_t serial, *k;
   Eina_List *l;

   /* update keyboard modifier state */
   wl_array_for_each(k, &e_comp->wl_comp_data->kbd.keys)
     e_comp_wl_input_keyboard_state_update(e_comp->wl_comp_data, *k, EINA_TRUE);

   /* send keyboard_enter to all keyboard resources */
   wc = wl_resource_get_client(ec->comp_data->surface);
   serial = wl_display_next_serial(e_comp->wl_comp_data->wl.disp);
   ec->comp_data->focus_update = 1;
   EINA_LIST_FOREACH(e_comp->wl_comp_data->kbd.resources, l, res)
     {
        if (wl_resource_get_client(res) != wc) continue;
        wl_keyboard_send_enter(res, serial, ec->comp_data->surface,
                               &e_comp->wl_comp_data->kbd.keys);
        ec->comp_data->focus_update = 0;
     }
}

static void 
_e_comp_wl_evas_cb_focus_in(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec, *focused;

   if (!(ec = data)) return;
   if (e_object_is_del(E_OBJECT(ec))) return;
   if (ec->iconic) return;

   E_COMP_WL_PIXMAP_CHECK;

   /* block spurious focus events */
   focused = e_client_focused_get();
   if ((focused) && (ec != focused)) return;

   /* raise client priority */
   _e_comp_wl_client_priority_raise(ec);

   _e_comp_wl_client_focus(ec);
}

static void 
_e_comp_wl_evas_cb_focus_out(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec;
   E_Comp_Data *cdata;
   struct wl_resource *res;
   struct wl_client *wc;
   uint32_t serial, *k;
   Eina_List *l;

   if (!(ec = data)) return;
   if (e_object_is_del(E_OBJECT(ec))) return;

   E_COMP_WL_PIXMAP_CHECK;

   /* lower client priority */
   _e_comp_wl_client_priority_normal(ec);

   cdata = ec->comp->wl_comp_data;

   /* update keyboard modifier state */
   wl_array_for_each(k, &cdata->kbd.keys)
     e_comp_wl_input_keyboard_state_update(cdata, *k, EINA_FALSE);

   if (!ec->comp_data->surface) return;

   /* send keyboard_leave to all keyboard resources */
   wc = wl_resource_get_client(ec->comp_data->surface);
   serial = wl_display_next_serial(cdata->wl.disp);
   EINA_LIST_FOREACH(cdata->kbd.resources, l, res)
     {
        if (wl_resource_get_client(res) != wc) continue;
        wl_keyboard_send_leave(res, serial, ec->comp_data->surface);
     }
   ec->comp_data->focus_update = 0;
}

static void 
_e_comp_wl_evas_cb_resize(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec;

   if (!(ec = data)) return;

   E_COMP_WL_PIXMAP_CHECK;

   if ((ec->shading) || (ec->shaded)) return;
   ec->post_resize = EINA_TRUE;
   _e_comp_wl_client_idler_add(ec);
}

static void 
_e_comp_wl_evas_cb_delete_request(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec;

   if (!(ec = data)) return;
   if (ec->netwm.ping) e_client_ping(ec);

   e_comp_ignore_win_del(E_PIXMAP_TYPE_WL, e_pixmap_window_get(ec->pixmap));

   e_object_del(E_OBJECT(ec));

   _e_comp_wl_focus_check(ec->comp);

   /* TODO: Delete request send ??
    * NB: No such animal wrt wayland */
}

static void 
_e_comp_wl_evas_cb_kill_request(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec;

   if (!(ec = data)) return;
   /* if (ec->netwm.ping) e_client_ping(ec); */

   e_comp_ignore_win_del(E_PIXMAP_TYPE_WL, e_pixmap_window_get(ec->pixmap));
   if (ec->comp_data)
     {
        if (ec->comp_data->reparented)
          e_client_comp_hidden_set(ec, EINA_TRUE);
     }

   evas_object_pass_events_set(ec->frame, EINA_TRUE);
   if (ec->visible) evas_object_hide(ec->frame);
   if (!ec->internal) e_object_del(E_OBJECT(ec));

   _e_comp_wl_focus_check(ec->comp);
}

static void 
_e_comp_wl_evas_cb_ping(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec;

   if (!(ec = data)) return;

   if (ec->comp_data->shell.ping)
     {
        if (ec->comp_data->shell.surface)
          ec->comp_data->shell.ping(ec->comp_data->shell.surface);
     }
}

static void 
_e_comp_wl_evas_cb_color_set(void *data, Evas_Object *obj, void *event EINA_UNUSED)
{
   E_Client *ec;
   int a = 0;

   if (!(ec = data)) return;
   evas_object_color_get(obj, NULL, NULL, NULL, &a);
   if (ec->netwm.opacity == a) return;
   ec->netwm.opacity = a;
   ec->netwm.opacity_changed = EINA_TRUE;
}

static void 
_e_comp_wl_client_evas_init(E_Client *ec)
{
   if (ec->comp_data->evas_init) return;
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_SHOW, 
                                  _e_comp_wl_evas_cb_show, ec);
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_HIDE, 
                                  _e_comp_wl_evas_cb_hide, ec);

   /* setup input callbacks */
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_MOUSE_IN, 
                                  _e_comp_wl_evas_cb_mouse_in, ec);
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_MOUSE_OUT, 
                                  _e_comp_wl_evas_cb_mouse_out, ec);
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_MOUSE_MOVE, 
                                  _e_comp_wl_evas_cb_mouse_move, ec);
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_MOUSE_DOWN, 
                                  _e_comp_wl_evas_cb_mouse_down, ec);
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_MOUSE_UP, 
                                  _e_comp_wl_evas_cb_mouse_up, ec);
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_MOUSE_WHEEL, 
                                  _e_comp_wl_evas_cb_mouse_wheel, ec);

   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_FOCUS_IN, 
                                  _e_comp_wl_evas_cb_focus_in, ec);
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_FOCUS_OUT, 
                                  _e_comp_wl_evas_cb_focus_out, ec);

   if (!ec->override)
     {
        evas_object_smart_callback_add(ec->frame, "client_resize", 
                                       _e_comp_wl_evas_cb_resize, ec);
     }

   /* setup delete/kill callbacks */
   evas_object_smart_callback_add(ec->frame, "delete_request", 
                                  _e_comp_wl_evas_cb_delete_request, ec);
   evas_object_smart_callback_add(ec->frame, "kill_request", 
                                  _e_comp_wl_evas_cb_kill_request, ec);

   /* setup ping callback */
   evas_object_smart_callback_add(ec->frame, "ping", 
                                  _e_comp_wl_evas_cb_ping, ec);

   evas_object_smart_callback_add(ec->frame, "color_set", 
                                  _e_comp_wl_evas_cb_color_set, ec);

   ec->comp_data->evas_init = EINA_TRUE;
}

static Eina_Bool 
_e_comp_wl_cb_comp_object_add(void *data EINA_UNUSED, int type EINA_UNUSED, E_Event_Comp_Object *ev)
{
   E_Client *ec;

   /* try to get the client from the object */
   if (!(ec = e_comp_object_client_get(ev->comp_object))) 
     return ECORE_CALLBACK_RENEW;

   /* check for client being deleted */
   if (e_object_is_del(E_OBJECT(ec))) return ECORE_CALLBACK_RENEW;

   /* check for wayland pixmap */
   E_COMP_WL_PIXMAP_CHECK ECORE_CALLBACK_RENEW;

   /* if we have not setup evas callbacks for this client, do it */
   if (!ec->comp_data->evas_init) _e_comp_wl_client_evas_init(ec);

   return ECORE_CALLBACK_RENEW;
}

static void 
_e_comp_wl_cb_key_down(void *event)
{
   E_Comp_Data *cdata;
   E_Client *ec;
   Ecore_Event_Key *ev;
   uint32_t serial, *end, *k, keycode;

   ev = event;
   keycode = (ev->keycode - 8);
   if (!(cdata = e_comp->wl_comp_data)) return;

#ifdef HAVE_WAYLAND_ONLY
 #ifndef E_RELEASE_BUILD
   if ((ev->modifiers & ECORE_EVENT_MODIFIER_CTRL) &&
       ((ev->modifiers & ECORE_EVENT_MODIFIER_ALT) ||
       (ev->modifiers & ECORE_EVENT_MODIFIER_ALTGR)) &&
       eina_streq(ev->key, "BackSpace"))
     exit(0);
 #endif
#endif

   end = (uint32_t *)cdata->kbd.keys.data + (cdata->kbd.keys.size / sizeof(*k));

   for (k = cdata->kbd.keys.data; k < end; k++)
     {
        /* ignore server-generated key repeats */
        if (*k == keycode) return;
     }

   cdata->kbd.keys.size = (const char *)end - (const char *)cdata->kbd.keys.data;
   k = wl_array_add(&cdata->kbd.keys, sizeof(*k));
   *k = keycode;

   /* update modifier state */
   e_comp_wl_input_keyboard_state_update(cdata, keycode, EINA_TRUE);

   if ((ec = e_client_focused_get()))
     {
        if (ec->comp_data->surface)
          {
             struct wl_client *wc;
             struct wl_resource *res;
             Eina_List *l;

             wc = wl_resource_get_client(ec->comp_data->surface);
             serial = wl_display_next_serial(cdata->wl.disp);
             EINA_LIST_FOREACH(cdata->kbd.resources, l, res)
               {
                  if (wl_resource_get_client(res) != wc) continue;
                  wl_keyboard_send_key(res, serial, ev->timestamp, 
                                       keycode, WL_KEYBOARD_KEY_STATE_PRESSED);
               }
          }
     }

   if (cdata->kbd.mod_changed)
     {
        e_comp_wl_input_keyboard_modifiers_update(cdata);
        cdata->kbd.mod_changed = 0;
     }
}

static void 
_e_comp_wl_cb_key_up(void *event)
{
   E_Client *ec;
   E_Comp_Data *cdata;
   Ecore_Event_Key *ev;
   uint32_t serial, *end, *k, keycode;

   ev = event;
   keycode = (ev->keycode - 8);
   if (!(cdata = e_comp->wl_comp_data)) return;

   end = (uint32_t *)cdata->kbd.keys.data + (cdata->kbd.keys.size / sizeof(*k));
   for (k = cdata->kbd.keys.data; k < end; k++)
     if (*k == keycode) *k = *--end;

   cdata->kbd.keys.size = (const char *)end - (const char *)cdata->kbd.keys.data;

   /* update modifier state */
   e_comp_wl_input_keyboard_state_update(cdata, keycode, EINA_FALSE);

   if ((ec = e_client_focused_get()))
     {
        if (ec->comp_data->surface)
          {
             struct wl_client *wc;
             struct wl_resource *res;
             Eina_List *l;

             wc = wl_resource_get_client(ec->comp_data->surface);
             serial = wl_display_next_serial(cdata->wl.disp);
             EINA_LIST_FOREACH(cdata->kbd.resources, l, res)
               {
                  if (wl_resource_get_client(res) != wc) continue;
                  wl_keyboard_send_key(res, serial, ev->timestamp, 
                                       keycode, WL_KEYBOARD_KEY_STATE_RELEASED);
               }
          }
     }

   if (cdata->kbd.mod_changed)
     {
        e_comp_wl_input_keyboard_modifiers_update(cdata);
        cdata->kbd.mod_changed = 0;
     }
}

static Eina_Bool 
_e_comp_wl_cb_input_event(void *data EINA_UNUSED, int type, void *ev)
{
   _last_event_time = ecore_loop_time_get();

   if (type == ECORE_EVENT_KEY_DOWN)
     _e_comp_wl_cb_key_down(ev);
   else if (type == ECORE_EVENT_KEY_UP)
     _e_comp_wl_cb_key_up(ev);

   return ECORE_CALLBACK_RENEW;
}

static void 
_e_comp_wl_surface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   E_Pixmap *ep;

   DBG("Surface Cb Destroy: %d", wl_resource_get_id(resource));

   /* unset the pixmap resource */
   if ((ep = wl_resource_get_user_data(resource)))
     e_pixmap_resource_set(ep, NULL);

   /* destroy this resource */
   wl_resource_destroy(resource);
}

static void 
_e_comp_wl_surface_cb_attach(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *buffer_resource, int32_t sx, int32_t sy)
{
   E_Pixmap *ep;
   E_Client *ec;

   /* get the e_pixmap reference */
   if (!(ep = wl_resource_get_user_data(resource))) return;

   /* try to find the associated e_client */
   if (!(ec = e_pixmap_client_get(ep)))
     {
        ERR("\t\tCould not find client from pixmap %p", ep);
        return;
     }

   /* check if client is being deleted */
   if (e_object_is_del(E_OBJECT(ec)))
     {
        DBG("\tE_Client scheduled for deletion");
        return;
     }

   /* check for valid comp_data */
   if (!ec->comp_data)
     {
        ERR("\tE_Client has no comp data");
        return;
     }

   /* DBG("Surface Attach: %d", wl_resource_get_id(resource)); */

   /* reset client pending information */
   ec->comp_data->pending.x = sx;
   ec->comp_data->pending.y = sy;
   ec->comp_data->pending.w = 0;
   ec->comp_data->pending.h = 0;
   ec->comp_data->pending.buffer = buffer_resource;
   ec->comp_data->pending.new_attach = EINA_TRUE;

   if (buffer_resource)
     {
        struct wl_shm_buffer *shmb;
 
        /* check for this resource being a shm buffer */
        if ((shmb = wl_shm_buffer_get(buffer_resource)))
          {
             /* update pending size */
             ec->comp_data->pending.w = wl_shm_buffer_get_width(shmb);
             ec->comp_data->pending.h = wl_shm_buffer_get_height(shmb);
          }
     }
}

static void 
_e_comp_wl_surface_cb_damage(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int32_t x, int32_t y, int32_t w, int32_t h)
{
   E_Pixmap *ep;
   E_Client *ec;
   Eina_Rectangle *dmg = NULL;

   /* get the e_pixmap reference */
   if (!(ep = wl_resource_get_user_data(resource))) return;

   /* try to find the associated e_client */
   if (!(ec = e_pixmap_client_get(ep)))
     {
        ERR("\tCould not find client from pixmap %p", ep);
        return;
     }

   if (e_object_is_del(E_OBJECT(ec))) return;
   if (!ec->comp_data) return;

   /* DBG("Surface Cb Damage: %d", wl_resource_get_id(resource)); */
   /* DBG("\tGeom: %d %d %d %d", x, y, w, h); */

   /* create new damage rectangle */
   if (!(dmg = eina_rectangle_new(x, y, w, h))) return;

   /* add damage rectangle to list of pending damages */
   ec->comp_data->pending.damages = 
     eina_list_append(ec->comp_data->pending.damages, dmg);
}

static void 
_e_comp_wl_frame_cb_destroy(struct wl_resource *resource)
{
   E_Client *ec;

   if (!(ec = wl_resource_get_user_data(resource))) return;

   if (e_object_is_del(E_OBJECT(ec))) return;

   /* remove this frame callback */
   ec->comp_data->frames = eina_list_remove(ec->comp_data->frames, resource);
}

static void 
_e_comp_wl_surface_cb_frame(struct wl_client *client, struct wl_resource *resource, uint32_t callback)
{
   E_Pixmap *ep;
   E_Client *ec;
   struct wl_resource *res;

   if (!(ep = wl_resource_get_user_data(resource))) return;

   /* try to find the associated e_client */
   if (!(ec = e_pixmap_client_get(ep)))
     {
        ERR("\tCould not find client from pixmap %p", ep);
        return;
     }

   if (e_object_is_del(E_OBJECT(ec))) return;

   /* create frame callback */
   if (!(res = 
         wl_resource_create(client, &wl_callback_interface, 1, callback)))
     {
        wl_resource_post_no_memory(resource);
        return;
     }

   wl_resource_set_implementation(res, NULL, ec, _e_comp_wl_frame_cb_destroy);

   /* append this frame callback to the client list */
   ec->comp_data->frames = eina_list_append(ec->comp_data->frames, res);
}

static void 
_e_comp_wl_surface_cb_opaque_region_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *region_resource)
{
   E_Pixmap *ep;
   E_Client *ec;

   /* get the e_pixmap reference */
   if (!(ep = wl_resource_get_user_data(resource))) return;

   /* try to find the associated e_client */
   if (!(ec = e_pixmap_client_get(ep)))
     {
        ERR("\tCould not find client from pixmap %p", ep);
        return;
     }

   /* trap for clients which are being deleted */
   if (e_object_is_del(E_OBJECT(ec))) return;

   if (region_resource)
     {
        Eina_Tiler *tmp;
        Eina_Iterator *it;
        Eina_Rectangle *rect;

        /* try to get the tiler from the region resource */
        if (!(tmp = wl_resource_get_user_data(region_resource)))
          return;

        it = eina_tiler_iterator_new(tmp);
        EINA_ITERATOR_FOREACH(it, rect)
          {
             e_pixmap_image_opaque_set(ec->pixmap, rect->x, rect->y, rect->w, rect->h);
             break;
          }

        eina_iterator_free(it);
     }
   else
     e_pixmap_image_opaque_set(ec->pixmap, 0, 0, 0, 0);
}

static void 
_e_comp_wl_surface_cb_input_region_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *region_resource)
{
   E_Pixmap *ep;
   E_Client *ec;

   /* get the e_pixmap reference */
   if (!(ep = wl_resource_get_user_data(resource))) return;

   /* try to find the associated e_client */
   if (!(ec = e_pixmap_client_get(ep)))
     {
        ERR("\tCould not find client from pixmap %p", ep);
        return;
     }

   /* trap for clients which are being deleted */
   if (e_object_is_del(E_OBJECT(ec))) return;

   eina_tiler_clear(ec->comp_data->pending.input);
   if (region_resource)
     {
        Eina_Tiler *tmp;

        /* try to get the tiler from the region resource */
        if (!(tmp = wl_resource_get_user_data(region_resource)))
          return;

        eina_tiler_union(ec->comp_data->pending.input, tmp);
     }
   else
     {
        eina_tiler_rect_add(ec->comp_data->pending.input, 
                            &(Eina_Rectangle){0, 0, ec->client.w, ec->client.h});
     }
}

static void 
_e_comp_wl_surface_cb_commit(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   E_Pixmap *ep;
   E_Client *ec, *subc;
   Eina_List *l;

   /* get the e_pixmap reference */
   if (!(ep = wl_resource_get_user_data(resource))) return;

   /* try to find the associated e_client */
   if (!(ec = e_pixmap_client_get(ep)))
     {
        ERR("\tCould not find client from pixmap %p", ep);
        return;
     }

   /* trap for clients which are being deleted */
   if (e_object_is_del(E_OBJECT(ec))) return;

   /* DBG("Surface Commit: %d", wl_resource_get_id(resource)); */

   /* call the subsurface commit function
    * 
    * NB: Returns true on success */
   if (e_comp_wl_subsurface_commit(ec)) return;

   /* handle actual surface commit */
   if (!e_comp_wl_surface_commit(ec))
     ERR("Failed to commit surface: %d", wl_resource_get_id(resource));

   EINA_LIST_FOREACH(ec->comp_data->sub.list, l, subc)
     {
        if (ec != subc)
          _e_comp_wl_subsurface_parent_commit(subc, EINA_FALSE);
     }
}

static void 
_e_comp_wl_surface_cb_buffer_transform_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, int32_t transform EINA_UNUSED)
{
   DBG("Surface Buffer Transform: %d", wl_resource_get_id(resource));
}

static void 
_e_comp_wl_surface_cb_buffer_scale_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, int32_t scale EINA_UNUSED)
{
   DBG("Surface Buffer Scale: %d", wl_resource_get_id(resource));
}

static const struct wl_surface_interface _e_surface_interface = 
{
   _e_comp_wl_surface_cb_destroy,
   _e_comp_wl_surface_cb_attach,
   _e_comp_wl_surface_cb_damage,
   _e_comp_wl_surface_cb_frame,
   _e_comp_wl_surface_cb_opaque_region_set,
   _e_comp_wl_surface_cb_input_region_set,
   _e_comp_wl_surface_cb_commit,
   _e_comp_wl_surface_cb_buffer_transform_set,
   _e_comp_wl_surface_cb_buffer_scale_set
};

static void 
_e_comp_wl_surface_destroy(struct wl_resource *resource)
{
   E_Pixmap *ep;
   E_Client *ec;

   if (!(ep = wl_resource_get_user_data(resource))) return;

   /* try to get the e_client from this pixmap */
   if (!(ec = e_pixmap_client_get(ep)))
     return;

   evas_object_hide(ec->frame);
   e_object_del(E_OBJECT(ec));
}

static void 
_e_comp_wl_compositor_cb_surface_create(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
   E_Comp *comp;
   struct wl_resource *res;
   E_Pixmap *ep;
   uint64_t win;
   pid_t pid;

   if (!(comp = wl_resource_get_user_data(resource))) return;

   DBG("Compositor Cb Surface Create: %d", id);

   /* try to create an internal surface */
   if (!(res = wl_resource_create(client, &wl_surface_interface, 
                                  wl_resource_get_version(resource), id)))
     {
        ERR("Could not create compositor surface");
        wl_client_post_no_memory(client);
        return;
     }

   DBG("\tCreated Resource: %d", wl_resource_get_id(res));

   /* set implementation on resource */
   wl_resource_set_implementation(res, &_e_surface_interface, NULL, 
                                  _e_comp_wl_surface_destroy);

   wl_client_get_credentials(client, &pid, NULL, NULL);
   win = e_comp_wl_id_get(id, pid);
   /* check for existing pixmap */
   if (!(ep = e_pixmap_find(E_PIXMAP_TYPE_WL, win)))
     {
        /* try to create new pixmap */
        if (!(ep = e_pixmap_new(E_PIXMAP_TYPE_WL, win)))
          {
             ERR("Could not create new pixmap");
             wl_resource_destroy(res);
             wl_client_post_no_memory(client);
             return;
          }
     }

   DBG("\tUsing Pixmap: %d", id);

   /* set reference to pixmap so we can fetch it later */
   wl_resource_set_user_data(res, ep);

   /* emit surface create signal */
   wl_signal_emit(&comp->wl_comp_data->signals.surface.create, res);
}

static void 
_e_comp_wl_region_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   DBG("Region Destroy: %d", wl_resource_get_id(resource));
   wl_resource_destroy(resource);
}

static void 
_e_comp_wl_region_cb_add(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int32_t x, int32_t y, int32_t w, int32_t h)
{
   Eina_Tiler *tiler;

   DBG("Region Add: %d", wl_resource_get_id(resource));
   DBG("\tGeom: %d %d %d %d", x, y, w, h);

   /* get the tiler from the resource */
   if ((tiler = wl_resource_get_user_data(resource)))
     {
        Eina_Tiler *src;

        src = eina_tiler_new(w, h);
        eina_tiler_tile_size_set(src, 1, 1);
        eina_tiler_rect_add(src, &(Eina_Rectangle){x, y, w, h});
        eina_tiler_union(tiler, src);
        eina_tiler_free(src);
     }
}

static void 
_e_comp_wl_region_cb_subtract(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int32_t x, int32_t y, int32_t w, int32_t h)
{
   Eina_Tiler *tiler;

   DBG("Region Subtract: %d", wl_resource_get_id(resource));
   DBG("\tGeom: %d %d %d %d", x, y, w, h);

   /* get the tiler from the resource */
   if ((tiler = wl_resource_get_user_data(resource)))
     {
        Eina_Tiler *src;

        src = eina_tiler_new(w, h);
        eina_tiler_tile_size_set(src, 1, 1);
        eina_tiler_rect_add(src, &(Eina_Rectangle){x, y, w, h});

        eina_tiler_subtract(tiler, src);
        eina_tiler_free(src);
     }
}

static const struct wl_region_interface _e_region_interface = 
{
   _e_comp_wl_region_cb_destroy, 
   _e_comp_wl_region_cb_add, 
   _e_comp_wl_region_cb_subtract
};

static void 
_e_comp_wl_compositor_cb_region_destroy(struct wl_resource *resource)
{
   Eina_Tiler *tiler;

   DBG("Compositor Region Destroy: %d", wl_resource_get_id(resource));

   /* try to get the tiler from the region resource */
   if ((tiler = wl_resource_get_user_data(resource)))
     eina_tiler_free(tiler);
}

static void 
_e_comp_wl_compositor_cb_region_create(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
   E_Comp *comp;
   Eina_Tiler *tiler;
   struct wl_resource *res;

   /* get the compositor from the resource */
   if (!(comp = wl_resource_get_user_data(resource))) return;

   DBG("Region Create: %d", wl_resource_get_id(resource));

   /* try to create new tiler */
   if (!(tiler = eina_tiler_new(comp->man->w, comp->man->h)))
     {
        ERR("Could not create Eina_Tiler");
        wl_resource_post_no_memory(resource);
        return;
     }

   /* set tiler size */
   eina_tiler_tile_size_set(tiler, 1, 1);

   if (!(res = wl_resource_create(client, &wl_region_interface, 1, id)))
     {
        ERR("\tFailed to create region resource");
        wl_resource_post_no_memory(resource);
        return;
     }

   wl_resource_set_implementation(res, &_e_region_interface, tiler, 
                                  _e_comp_wl_compositor_cb_region_destroy);
}

static const struct wl_compositor_interface _e_comp_interface = 
{
   _e_comp_wl_compositor_cb_surface_create,
   _e_comp_wl_compositor_cb_region_create
};

static void 
_e_comp_wl_compositor_cb_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
   E_Comp *comp;
   struct wl_resource *res;

   if (!(comp = data)) return;

   if (!(res = 
         wl_resource_create(client, &wl_compositor_interface, 
                            MIN(version, COMPOSITOR_VERSION), id)))
     {
        ERR("Could not create compositor resource: %m");
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res, &_e_comp_interface, comp, NULL);
}

static void 
_e_comp_wl_compositor_cb_del(E_Comp *comp)
{
   E_Comp_Data *cdata;

   /* get existing compositor data */
   if (!(cdata = comp->wl_comp_data)) return;

   /* delete fd handler */
   if (cdata->fd_hdlr) ecore_main_fd_handler_del(cdata->fd_hdlr);

   eina_list_free(cdata->output.resources);

   /* free allocated data structure */
   free(cdata);
}

static void 
_e_comp_wl_subsurface_destroy(struct wl_resource *resource)
{
   E_Client *ec;
   E_Comp_Wl_Subsurf_Data *sdata;
   Eina_Rectangle *dmg;

   /* try to get the client from resource data */
   if (!(ec = wl_resource_get_user_data(resource))) return;

   if (!ec->comp_data) return;

   if (!(sdata = ec->comp_data->sub.data)) return;

   if (sdata->parent)
     {
        /* remove this client from parents sub list */
        sdata->parent->comp_data->sub.list = 
          eina_list_remove(sdata->parent->comp_data->sub.list, ec);
     }

   /* release buffer */
   if (sdata->cached.buffer) wl_buffer_send_release(sdata->cached.buffer);

   /* the client is getting deleted, which means the pixmap will be getting 
    * freed. We need to unset the surface user data */
   /* wl_resource_set_user_data(ec->comp_data->surface, NULL); */

   EINA_LIST_FREE(sdata->cached.damages, dmg)
     eina_rectangle_free(dmg);

   if (sdata->cached.input)
     eina_tiler_free(sdata->cached.input);

   E_FREE(sdata);

   ec->comp_data->sub.data = NULL;
}

static Eina_Bool 
_e_comp_wl_subsurface_synchronized_get(E_Comp_Wl_Subsurf_Data *sdata)
{
   while (sdata)
     {
        if (sdata->synchronized) return EINA_TRUE;
        if (!sdata->parent) return EINA_FALSE;
        sdata = sdata->parent->comp_data->sub.data;
     }

   return EINA_FALSE;
}

static void 
_e_comp_wl_subsurface_commit_to_cache(E_Client *ec)
{
   E_Comp_Client_Data *cdata;
   E_Comp_Wl_Subsurf_Data *sdata;
   Eina_Rectangle *dmg;
   Eina_List *l;

   if (!(cdata = ec->comp_data)) return;
   if (!(sdata = cdata->sub.data)) return;

   DBG("Subsurface Commit to Cache");

   /* move pending damage to cached */
   EINA_LIST_FOREACH(cdata->pending.damages, l, dmg)
     eina_list_move(&sdata->cached.damages, &cdata->pending.damages, dmg);

   DBG("\tList Count After Move: %d", eina_list_count(cdata->pending.damages));

   sdata->cached.x = cdata->pending.x;
   sdata->cached.y = cdata->pending.y;
   sdata->cached.buffer = cdata->pending.buffer;
   sdata->cached.new_attach = cdata->pending.new_attach;

   eina_tiler_union(sdata->cached.input, cdata->pending.input);

   sdata->cached.has_data = EINA_TRUE;
}

static void 
_e_comp_wl_subsurface_commit_from_cache(E_Client *ec)
{
   E_Comp_Client_Data *cdata;
   E_Comp_Wl_Subsurf_Data *sdata;
   E_Pixmap *ep;
   Eina_Rectangle *dmg;
   Eina_Tiler *src, *tmp;

   if (!(cdata = ec->comp_data)) return;
   if (!(sdata = cdata->sub.data)) return;
   if (!(ep = ec->pixmap)) return;

   DBG("Subsurface Commit from Cache");

   if (sdata->cached.buffer)
     {
        /* mark the pixmap as usable or not */
        e_pixmap_usable_set(ep, (sdata->cached.buffer != NULL));
     }

   /* mark the pixmap as dirty */
   e_pixmap_dirty(ep);

   e_pixmap_image_clear(ep, EINA_FALSE);
   e_pixmap_resource_set(ep, sdata->cached.buffer);

   /* refresh pixmap */
   if (e_pixmap_refresh(ep))
     {
        e_comp->post_updates = eina_list_append(e_comp->post_updates, ec);
        e_object_ref(E_OBJECT(ec));
     }

   /* check if we need to map this surface */
   if (sdata->cached.buffer)
     {
        /* if this surface is not mapped yet, map it */
        if (!cdata->mapped)
          {
             /* if the client has a shell map, call it */
             if ((cdata->shell.surface) && (cdata->shell.map))
               cdata->shell.map(cdata->shell.surface);
          }
     }
   else
     {
        /* no pending buffer to attach. unmap the surface */
        if (cdata->mapped)
          {
             /* if the client has a shell map, call it */
             if ((cdata->shell.surface) && (cdata->shell.unmap))
               cdata->shell.unmap(cdata->shell.surface);
          }
     }

   /* check for any pending attachments */
   if (sdata->cached.new_attach)
     {
        int x, y, nw, nh;
        Eina_Bool placed = EINA_TRUE;

        e_pixmap_size_get(ec->pixmap, &nw, &nh);
        if (ec->changes.pos)
          e_comp_object_frame_xy_adjust(ec->frame, ec->x, ec->y, &x, &y);
        else
          x = ec->client.x, y = ec->client.y;
        if (ec->new_client)
          placed = ec->placed;
        /* if the client has a shell configure, call it */
        if ((cdata->shell.surface) && (cdata->shell.configure))
          cdata->shell.configure(cdata->shell.surface, x, y, nw, nh);
        if (ec->new_client)
          ec->placed = placed;
     }

   if (!cdata->mapped) 
     {
        DBG("\tSurface Not Mapped. Skip to Unmapped");
        goto unmap;
     }

   /* commit any pending damages */
   if ((!ec->comp->nocomp) && (ec->frame))
     {
        EINA_LIST_FREE(sdata->cached.damages, dmg)
          {
             e_comp_object_damage(ec->frame, dmg->x, dmg->y, dmg->w, dmg->h);
             eina_rectangle_free(dmg);
          }
     }

   /* handle pending input */
   if (sdata->cached.input)
     {
        tmp = eina_tiler_new(ec->w, ec->h);
        eina_tiler_tile_size_set(tmp, 1, 1);
        eina_tiler_rect_add(tmp, 
                            &(Eina_Rectangle){0, 0, ec->client.w, ec->client.h});

        if ((src = eina_tiler_intersection(sdata->cached.input, tmp)))
          {
             Eina_Rectangle *rect;
             Eina_Iterator *itr;
             int i = 0;

             ec->shape_input_rects_num = 0;

             itr = eina_tiler_iterator_new(src);
             EINA_ITERATOR_FOREACH(itr, rect)
               ec->shape_input_rects_num += 1;

             ec->shape_input_rects = 
               malloc(sizeof(Eina_Rectangle) * ec->shape_input_rects_num);

             if (ec->shape_input_rects)
               {
                  EINA_ITERATOR_FOREACH(itr, rect)
                    {
                       ec->shape_input_rects[i] = 
                         *(Eina_Rectangle *)((char *)rect);

                       ec->shape_input_rects[i].x = rect->x;
                       ec->shape_input_rects[i].y = rect->y;
                       ec->shape_input_rects[i].w = rect->w;
                       ec->shape_input_rects[i].h = rect->h;

                       i++;
                    }
               }

             eina_iterator_free(itr);
             eina_tiler_free(src);
          }

        eina_tiler_free(tmp);
        eina_tiler_clear(sdata->cached.input);
     }

   return;

unmap:

   /* surface is not visible, clear damages */
   EINA_LIST_FREE(sdata->cached.damages, dmg)
     eina_rectangle_free(dmg);

   /* clear pending input regions */
   if (sdata->cached.input)
     eina_tiler_clear(sdata->cached.input);
}

static void 
_e_comp_wl_subsurface_parent_commit(E_Client *ec, Eina_Bool parent_synchronized)
{
   E_Client *parent;
   E_Comp_Wl_Subsurf_Data *sdata;

   if (!(sdata = ec->comp_data->sub.data)) return;
   if (!(parent = sdata->parent)) return;

   if (sdata->position.set)
     {
        evas_object_move(ec->frame, parent->x + sdata->position.x, 
                         parent->y + sdata->position.y);
        sdata->position.set = EINA_FALSE;
     }

   if ((parent_synchronized) || (sdata->synchronized))
     {
        E_Client *subc;
        Eina_List *l;

        if (sdata->cached.has_data)
          _e_comp_wl_subsurface_commit_from_cache(ec);

        EINA_LIST_FOREACH(ec->comp_data->sub.list, l, subc)
          {
             if (ec != subc)
               _e_comp_wl_subsurface_parent_commit(subc, EINA_TRUE);
          }
     }
}

static void 
_e_comp_wl_subsurface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void 
_e_comp_wl_subsurface_cb_position_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int32_t x, int32_t y)
{
   E_Client *ec;
   E_Comp_Wl_Subsurf_Data *sdata;

   DBG("Subsurface Cb Position Set: %d", wl_resource_get_id(resource));

   /* try to get the client from resource data */
   if (!(ec = wl_resource_get_user_data(resource))) return;

   if (!(sdata = ec->comp_data->sub.data)) return;

   sdata->position.x = x;
   sdata->position.y = y;
   sdata->position.set = EINA_TRUE;
}

static void 
_e_comp_wl_subsurface_cb_place_above(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *sibling_resource)
{
   E_Client *ec, *ecs;
   E_Client *parent;

   DBG("Subsurface Cb Place Above: %d", wl_resource_get_id(resource));

   /* try to get the client from resource data */
   if (!(ec = wl_resource_get_user_data(resource))) return;

   if (!ec->comp_data->sub.data) return;

   /* try to get the client from the sibling resource */
   if (!(ecs = wl_resource_get_user_data(sibling_resource))) return;

   if (!ecs->comp_data->sub.data) return;

   if (!(parent = ec->comp_data->sub.data->parent)) return;

   parent->comp_data->sub.list = 
     eina_list_remove(parent->comp_data->sub.list, ec);

   parent->comp_data->sub.list = 
     eina_list_append_relative(parent->comp_data->sub.list, ec, ecs);

   parent->comp_data->sub.restack_target = parent;
}

static void 
_e_comp_wl_subsurface_cb_place_below(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *sibling_resource)
{
   E_Client *ec, *ecs;
   E_Client *parent;

   DBG("Subsurface Cb Place Below: %d", wl_resource_get_id(resource));

   /* try to get the client from resource data */
   if (!(ec = wl_resource_get_user_data(resource))) return;

   if (!ec->comp_data->sub.data) return;

   /* try to get the client from the sibling resource */
   if (!(ecs = wl_resource_get_user_data(sibling_resource))) return;

   if (!ecs->comp_data->sub.data) return;

   if (!(parent = ec->comp_data->sub.data->parent)) return;

   parent->comp_data->sub.list = 
     eina_list_remove(parent->comp_data->sub.list, ec);

   parent->comp_data->sub.list = 
     eina_list_prepend_relative(parent->comp_data->sub.list, ec, ecs);

   parent->comp_data->sub.restack_target = parent;
}

static void 
_e_comp_wl_subsurface_cb_sync_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   E_Client *ec;
   E_Comp_Wl_Subsurf_Data *sdata;

   DBG("Subsurface Cb Sync Set: %d", wl_resource_get_id(resource));

   /* try to get the client from resource data */
   if (!(ec = wl_resource_get_user_data(resource))) return;

   if (!(sdata = ec->comp_data->sub.data)) return;

   sdata->synchronized = EINA_TRUE;
}

static void 
_e_comp_wl_subsurface_cb_desync_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   E_Client *ec;
   E_Comp_Wl_Subsurf_Data *sdata;

   DBG("Subsurface Cb Desync Set: %d", wl_resource_get_id(resource));

   /* try to get the client from resource data */
   if (!(ec = wl_resource_get_user_data(resource))) return;

   if (!(sdata = ec->comp_data->sub.data)) return;

   sdata->synchronized = EINA_FALSE;
}

static const struct wl_subsurface_interface _e_subsurface_interface = 
{
   _e_comp_wl_subsurface_cb_destroy,
   _e_comp_wl_subsurface_cb_position_set,
   _e_comp_wl_subsurface_cb_place_above,
   _e_comp_wl_subsurface_cb_place_below,
   _e_comp_wl_subsurface_cb_sync_set, 
   _e_comp_wl_subsurface_cb_desync_set
};

static Eina_Bool 
_e_comp_wl_subsurface_create(E_Client *ec, E_Client *epc, uint32_t id, struct wl_resource *surface_resource)
{
   struct wl_client *client;
   struct wl_resource *res;
   E_Comp_Wl_Subsurf_Data *sdata;

   /* try to get the wayland client from the surface resource */
   if (!(client = wl_resource_get_client(surface_resource)))
     {
        ERR("Could not get client from resource %d", 
            wl_resource_get_id(surface_resource));
        return EINA_FALSE;
     }

   /* try to allocate subsurface data */
   if (!(sdata = E_NEW(E_Comp_Wl_Subsurf_Data, 1)))
     {
        ERR("Could not allocate space for subsurface data");
        goto dat_err;
     }

   /* try to create the subsurface resource */
   if (!(res = wl_resource_create(client, &wl_subsurface_interface, 1, id)))
     {
        ERR("Failed to create subsurface resource");
        wl_resource_post_no_memory(surface_resource);
        goto res_err;
     }

   /* set resource implementation */
   wl_resource_set_implementation(res, &_e_subsurface_interface, ec, 
                                  _e_comp_wl_subsurface_destroy);

   /* set subsurface data properties */
   sdata->resource = res;
   sdata->synchronized = EINA_TRUE;
   sdata->parent = epc;

   /* create subsurface tilers */
   sdata->cached.input = eina_tiler_new(ec->w, ec->h);
   eina_tiler_tile_size_set(sdata->cached.input, 1, 1);

   /* set subsurface client properties */
   ec->borderless = EINA_TRUE;
   ec->argb = EINA_TRUE;
   ec->lock_border = EINA_TRUE;
   ec->lock_focus_in = ec->lock_focus_out = EINA_TRUE;
   ec->netwm.state.skip_taskbar = EINA_TRUE;
   ec->netwm.state.skip_pager = EINA_TRUE;

   if (epc)
     {
        if (epc->comp_data)
          {
             /* append this client to the parents subsurface list */
             epc->comp_data->sub.list = 
               eina_list_append(epc->comp_data->sub.list, ec);
          }

        /* TODO: add callbacks ?? */
     }

   ec->comp_data->surface = surface_resource;
   ec->comp_data->sub.data = sdata;

   return EINA_TRUE;

res_err:
   free(sdata);
dat_err:
   return EINA_FALSE;
}

static void 
_e_comp_wl_subcompositor_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);

   /* TODO: destroy iconify/uniconify handlers */
}

static void 
_e_comp_wl_subcompositor_cb_subsurface_get(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, uint32_t id, struct wl_resource *surface_resource, struct wl_resource *parent_resource)
{
   E_Pixmap *ep, *epp;
   E_Client *ec, *epc = NULL;
   static const char where[] = "get_subsurface: wl_subsurface@";

   DBG("Subcompositor Create Subsurface for Surface: %d", 
       wl_resource_get_id(surface_resource));

   /* try to get the surface pixmap */
   if (!(ep = wl_resource_get_user_data(surface_resource)))
     {
        wl_resource_post_error(resource, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE, 
                               "%s%d: wl_surface@%d is invalid.", where, id, 
                               wl_resource_get_id(surface_resource));
        return;
     }

   /* try to get the parent pixmap */
   if (!(epp = wl_resource_get_user_data(parent_resource)))
     {
        wl_resource_post_error(resource, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE, 
                               "%s%d: wl_surface@%d is invalid.", where, id, 
                               wl_resource_get_id(parent_resource));
        return;
     }

   if (ep == epp)
     {
        wl_resource_post_error(resource, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                               "%s%d: wl_surface@%d cannot be its own parent",
                               where, id, wl_resource_get_id(surface_resource));
        return;
     }

   /* try to find the associated e_client */
   if (!(ec = e_pixmap_client_get(ep)))
     ERR("\tCould not find client from pixmap %p", ep);

   if (!ec)
     {
        /* no client exists for this pixmap yet */
        if (!(ec = e_client_new(NULL, ep, 0, 0)))
          {
             ERR("Failed to create new client");
             wl_resource_post_no_memory(resource);
             return;
          }

        if (ec->comp_data)
          ec->comp_data->surface = surface_resource;
     }
   else
     {
        /* trap for clients which are being deleted */
        if (e_object_is_del(E_OBJECT(ec))) return;
     }

   /* try to find the parents associated e_client */
   if (!(epc = e_pixmap_client_get(epp)))
     ERR("\tCould not find client from pixmap %p", epp);

   /* trap for clients which are being deleted */
   if ((epc) && (e_object_is_del(E_OBJECT(epc)))) return;

   /* check if this surface is already a sub-surface */
   if ((ec->comp_data) && (ec->comp_data->sub.data))
     {
        wl_resource_post_error(resource,
                               WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                               "%s%d: wl_surface@%d is already a sub-surface",
                               where, id, wl_resource_get_id(surface_resource));
        return;
     }

   /* try to create a new subsurface */
   if (!_e_comp_wl_subsurface_create(ec, epc, id, surface_resource))
     ERR("Failed to create subsurface for surface %d", 
         wl_resource_get_id(surface_resource));
}

static const struct wl_subcompositor_interface _e_subcomp_interface = 
{
   _e_comp_wl_subcompositor_cb_destroy,
   _e_comp_wl_subcompositor_cb_subsurface_get
};

static void 
_e_comp_wl_subcompositor_cb_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
   E_Comp *comp;
   struct wl_resource *res;

   if (!(comp = data)) return;

   if (!(res = 
         wl_resource_create(client, &wl_subcompositor_interface, 
                            MIN(version, 1), id)))
     {
        ERR("Could not create subcompositor resource: %m");
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res, &_e_subcomp_interface, comp, NULL);

   /* TODO: add handlers for client iconify/uniconify */
}

static void 
_e_comp_wl_client_cb_new(void *data EINA_UNUSED, E_Client *ec)
{
   uint64_t win;

   /* make sure this is a wayland client */
   E_COMP_WL_PIXMAP_CHECK;

   /* get window id from pixmap */
   win = e_pixmap_window_get(ec->pixmap);

   /* ignore fake root windows */
   if ((ec->override) && ((ec->x == -77) && (ec->y == -77)))
     {
        e_comp_ignore_win_add(E_PIXMAP_TYPE_WL, win);
        e_object_del(E_OBJECT(ec));
        return;
     }

   if (!(ec->comp_data = E_NEW(E_Comp_Client_Data, 1)))
     {
        ERR("Could not allocate new client data structure");
        return;
     }

   /* create client tilers */
   ec->comp_data->pending.input = eina_tiler_new(ec->w, ec->h);
   eina_tiler_tile_size_set(ec->comp_data->pending.input, 1, 1);

   /* set initial client properties */
   ec->argb = EINA_TRUE;
   ec->no_shape_cut = EINA_TRUE;
   ec->ignored = e_comp_ignore_win_find(win);
   ec->border_size = 0;

   /* NB: could not find a better place to do this, BUT for internal windows, 
    * we need to set delete_request else the close buttons on the frames do 
    * basically nothing */
   if ((ec->internal) || (ec->internal_elm_win)) 
     ec->icccm.delete_request = EINA_TRUE;

   /* set initial client data properties */
   ec->comp_data->mapped = EINA_FALSE;
   ec->comp_data->first_damage = ec->internal;

   ec->comp_data->need_reparent = !ec->internal;

   /* add this client to the hash */
   /* eina_hash_add(clients_win_hash, &win, ec); */
   e_hints_client_list_set();
}

static void 
_e_comp_wl_client_cb_del(void *data EINA_UNUSED, E_Client *ec)
{
   /* uint64_t win; */
   Eina_Rectangle *dmg;

   /* make sure this is a wayland client */
   E_COMP_WL_PIXMAP_CHECK;

   /* get window id from pixmap */
   /* win = e_pixmap_window_get(ec->pixmap); */
   /* eina_hash_del_by_key(clients_win_hash, &win); */

   if ((!ec->already_unparented) && (ec->comp_data->reparented))
     _e_comp_wl_focus_down_set(ec);

   ec->already_unparented = EINA_TRUE;
   if (ec->comp_data->reparented)
     {
        /* get the parent window */
        /* win = e_client_util_pwin_get(ec); */

        /* remove the parent from the hash */
        /* eina_hash_del_by_key(clients_win_hash, &win); */

        /* reset pixmap parent window */
        e_pixmap_parent_window_set(ec->pixmap, 0);
     }

   if ((ec->parent) && (ec->parent->modal == ec))
     {
        ec->parent->lock_close = EINA_FALSE;
        ec->parent->modal = NULL;
     }

   /* the client is getting deleted, which means the pixmap will be getting 
    * freed. We need to unset the surface user data */
   if (ec->comp_data->surface)
     wl_resource_set_user_data(ec->comp_data->surface, NULL);

   EINA_LIST_FREE(ec->comp_data->pending.damages, dmg)
     eina_rectangle_free(dmg);

   if (ec->comp_data->pending.input)
     eina_tiler_free(ec->comp_data->pending.input);

   E_FREE(ec->comp_data);

   _e_comp_wl_focus_check(ec->comp);
}

static void 
_e_comp_wl_client_cb_post_new(void *data EINA_UNUSED, E_Client *ec)
{
   E_COMP_WL_PIXMAP_CHECK;

   if (e_object_is_del(E_OBJECT(ec))) return;

   ec->need_shape_merge = EINA_FALSE;

   if (ec->need_shape_export)
     {
//        ec->shape_changed = EINA_TRUE;
        e_comp_shape_queue(ec->comp);
        ec->need_shape_export = EINA_FALSE;
     }
}

static void 
_e_comp_wl_client_cb_pre_frame(void *data EINA_UNUSED, E_Client *ec)
{
   uint64_t parent;

   E_COMP_WL_PIXMAP_CHECK;

   if (!ec->comp_data->need_reparent) return;

   DBG("Client Pre Frame: %d", wl_resource_get_id(ec->comp_data->surface));

   parent = e_client_util_pwin_get(ec);

   /* set pixmap parent window */
   e_pixmap_parent_window_set(ec->pixmap, parent);

   ec->border_size = 0;
   ec->border.changed = EINA_TRUE;
   ec->changes.shape = EINA_TRUE;
   ec->changes.shape_input = EINA_TRUE;
   EC_CHANGED(ec);

   if (ec->visible)
     {
        if ((ec->comp_data->set_win_type) && (ec->internal_elm_win))
          {
             int type = ECORE_WL_WINDOW_TYPE_TOPLEVEL;

             switch (ec->netwm.type)
               {
                case E_WINDOW_TYPE_DIALOG:
                  /* NB: If there is No transient set, then dialogs get 
                   * treated as Normal Toplevel windows */
                  if (ec->icccm.transient_for)
                    type = ECORE_WL_WINDOW_TYPE_TRANSIENT;
                  break;
                case E_WINDOW_TYPE_DESKTOP:
                  type = ECORE_WL_WINDOW_TYPE_FULLSCREEN;
                  break;
                case E_WINDOW_TYPE_DND:
                  type = ECORE_WL_WINDOW_TYPE_DND;
                  break;
                case E_WINDOW_TYPE_MENU:
                case E_WINDOW_TYPE_DROPDOWN_MENU:
                case E_WINDOW_TYPE_POPUP_MENU:
                  type = ECORE_WL_WINDOW_TYPE_MENU;
                  break;
                case E_WINDOW_TYPE_NORMAL:
                default:
                    break;
               }

             ecore_evas_wayland_type_set(e_win_ee_get(ec->internal_elm_win), type);
             ec->comp_data->set_win_type = EINA_FALSE;
          }
     }

   e_bindings_mouse_grab(E_BINDING_CONTEXT_WINDOW, parent);
   e_bindings_wheel_grab(E_BINDING_CONTEXT_WINDOW, parent);

   _e_comp_wl_client_evas_init(ec);

   /* if ((ec->netwm.ping) && (!ec->ping_poller)) */
   /*   e_client_ping(ec); */

   if (ec->visible) evas_object_show(ec->frame);

   ec->comp_data->need_reparent = EINA_FALSE;
   ec->redirected = EINA_TRUE;

   if (ec->comp_data->change_icon)
     {
        ec->comp_data->change_icon = EINA_FALSE;
        ec->changes.icon = EINA_TRUE;
        EC_CHANGED(ec);
     }

   ec->comp_data->reparented = EINA_TRUE;
}

static void 
_e_comp_wl_client_cb_focus_set(void *data EINA_UNUSED, E_Client *ec)
{
   E_COMP_WL_PIXMAP_CHECK;

   /* send configure */
   if (ec->comp_data->shell.configure_send)
     {
        if (ec->comp_data->shell.surface)
          ec->comp_data->shell.configure_send(ec->comp_data->shell.surface, 
                                              0, 0, 0);
     }

   if ((ec->icccm.take_focus) && (ec->icccm.accepts_focus))
     e_grabinput_focus(e_client_util_win_get(ec), 
                       E_FOCUS_METHOD_LOCALLY_ACTIVE);
   else if (!ec->icccm.accepts_focus)
     e_grabinput_focus(e_client_util_win_get(ec), 
                       E_FOCUS_METHOD_GLOBALLY_ACTIVE);
   else if (!ec->icccm.take_focus)
     e_grabinput_focus(e_client_util_win_get(ec), E_FOCUS_METHOD_PASSIVE);

   if (ec->comp->wl_comp_data->kbd.focus != ec->comp_data->surface)
     {
        ec->comp->wl_comp_data->kbd.focus = ec->comp_data->surface;
        e_comp_wl_data_device_keyboard_focus_set(ec->comp->wl_comp_data);
     }
}

static void 
_e_comp_wl_client_cb_focus_unset(void *data EINA_UNUSED, E_Client *ec)
{
   E_COMP_WL_PIXMAP_CHECK;

   /* send configure */
   if (ec->comp_data->shell.configure_send)
     {
        if (ec->comp_data->shell.surface)
          ec->comp_data->shell.configure_send(ec->comp_data->shell.surface, 
                                              0, 0, 0);
     }

   _e_comp_wl_focus_check(ec->comp);

   if (ec->comp->wl_comp_data->kbd.focus == ec->comp_data->surface)
     ec->comp->wl_comp_data->kbd.focus = NULL;
}

static void 
_e_comp_wl_client_cb_resize_begin(void *data EINA_UNUSED, E_Client *ec)
{
   E_COMP_WL_PIXMAP_CHECK;

   switch (ec->resize_mode)
     {
      case E_POINTER_RESIZE_T: // 1
        ec->comp->wl_comp_data->resize.edges = 1;
        break;
      case E_POINTER_RESIZE_B: // 2
        ec->comp->wl_comp_data->resize.edges = 2;
        break;
      case E_POINTER_RESIZE_L: // 4
        ec->comp->wl_comp_data->resize.edges = 4;
        break;
      case E_POINTER_RESIZE_R: // 8
        ec->comp->wl_comp_data->resize.edges = 8;
        break;
      case E_POINTER_RESIZE_TL: // 5
        ec->comp->wl_comp_data->resize.edges = 5;
        break;
      case E_POINTER_RESIZE_TR: // 9
        ec->comp->wl_comp_data->resize.edges = 9;
        break;
      case E_POINTER_RESIZE_BL: // 6
        ec->comp->wl_comp_data->resize.edges = 6;
        break;
      case E_POINTER_RESIZE_BR: // 10
        ec->comp->wl_comp_data->resize.edges = 10;
        break;
      default:
        ec->comp->wl_comp_data->resize.edges = 0;
        break;
     }
}

static void 
_e_comp_wl_client_cb_resize_end(void *data EINA_UNUSED, E_Client *ec)
{
   if (e_object_is_del(E_OBJECT(ec))) return;

   E_COMP_WL_PIXMAP_CHECK;

   ec->comp->wl_comp_data->resize.edges = 0;
   ec->comp->wl_comp_data->resize.resource = NULL;

   if (ec->pending_resize)
     {

        ec->changes.pos = EINA_TRUE;
        ec->changes.size = EINA_TRUE;
        EC_CHANGED(ec);
     }

   E_FREE_LIST(ec->pending_resize, free);
}

static void
_e_comp_wl_cb_output_unbind(struct wl_resource *resource)
{
   E_Comp_Data *cdata = wl_resource_get_user_data(resource);

   cdata->output.resources = 
     eina_list_remove(cdata->output.resources, resource);
}

static void
_e_comp_wl_output_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
   E_Comp_Data *cdata = data;
   Evas_Engine_Info_Drm *einfo;
   Ecore_Drm_Device *dev;
   Eina_List *l;
   Ecore_Drm_Output *output;
   struct wl_resource *resource;

   einfo = (Evas_Engine_Info_Drm *)evas_engine_info_get(e_comp->evas);
   dev = einfo->info.dev;

   resource = 
     wl_resource_create(client, &wl_output_interface, MIN(version, 2), id);
   if (resource == NULL)
     {
        wl_client_post_no_memory(client);
        return;
     }

   cdata->output.resources = 
     eina_list_append(cdata->output.resources, resource);

   wl_resource_set_implementation(resource, NULL, data, NULL);
   wl_resource_set_user_data(resource, cdata);
   EINA_LIST_FOREACH(dev->outputs, l, output)
     {
        int ox, oy, rw, rh, pw, ph;
        unsigned int spo, rr;
        const char *make, *model;

        ecore_drm_output_position_get(output, &ox, &oy);
        ecore_drm_output_current_resolution_get(output, &rw, &rh, &rr);
        ecore_drm_output_physical_size_get(output, &pw, &ph);
        spo = ecore_drm_output_subpixel_order_get(output);
        make = ecore_drm_output_model_get(output);
        model = ecore_drm_output_make_get(output);

        wl_output_send_geometry(resource, ox, oy, pw, ph, spo, make, model,
                                0/* output transform*/);

        if (version >= WL_OUTPUT_SCALE_SINCE_VERSION)
          wl_output_send_scale(resource, 1/* current scale */);

        wl_output_send_mode(resource, 3/*preferred + current */, rw, rh, rr);
     }

   if (version >= WL_OUTPUT_DONE_SINCE_VERSION)
     wl_output_send_done(resource);
}

static Eina_Bool 
_e_comp_wl_compositor_create(void)
{
   E_Comp *comp;
   E_Comp_Data *cdata;
   const char *name;
   int fd = 0;

   /* check for existing compositor. create if needed */
   if (!(comp = e_comp))
     {
        comp = e_comp_new();
        comp->comp_type = E_PIXMAP_TYPE_WL;
        E_OBJECT_DEL_SET(comp, _e_comp_wl_compositor_cb_del);
     }

   /* create new compositor data */
   cdata = E_NEW(E_Comp_Data, 1);

   /* set wayland log handler */
   wl_log_set_handler_server(_e_comp_wl_log_cb_print);

   /* try to create a wayland display */
   if (!(cdata->wl.disp = wl_display_create()))
     {
        ERR("Could not create a Wayland display: %m");
        goto disp_err;
     }

   /* try to setup wayland socket */
   if (!(name = wl_display_add_socket_auto(cdata->wl.disp)))
     {
        ERR("Could not create Wayland display socket: %m");
        goto sock_err;
     }

   /* set wayland display environment variable */
   e_env_set("WAYLAND_DISPLAY", name);

   /* initialize compositor signals */
   wl_signal_init(&cdata->signals.surface.create);
   wl_signal_init(&cdata->signals.surface.activate);
   wl_signal_init(&cdata->signals.surface.kill);

   /* try to add compositor to wayland globals */
   if (!wl_global_create(cdata->wl.disp, &wl_compositor_interface, 
                         COMPOSITOR_VERSION, comp, 
                         _e_comp_wl_compositor_cb_bind))
     {
        ERR("Could not add compositor to wayland globals: %m");
        goto comp_global_err;
     }

   /* try to add subcompositor to wayland globals */
   if (!wl_global_create(cdata->wl.disp, &wl_subcompositor_interface, 1, 
                         comp, _e_comp_wl_subcompositor_cb_bind))
     {
        ERR("Could not add subcompositor to wayland globals: %m");
        goto comp_global_err;
     }
   if (!wl_global_create(cdata->wl.disp, &wl_output_interface, 2,
                         cdata, _e_comp_wl_output_bind))
     {
        ERR("Could not add output to wayland globals: %m");
        goto comp_global_err;
     }

   /* try to init data manager */
   if (!e_comp_wl_data_manager_init(cdata))
     {
        ERR("Could not initialize data manager");
        goto data_err;
     }

   /* try to init input */
   if (!e_comp_wl_input_init(cdata))
     {
        ERR("Could not initialize input");
        goto input_err;
     }

#ifndef HAVE_WAYLAND_ONLY
   if (getenv("DISPLAY"))
     {
        E_Config_XKB_Layout *ekbd;
        Ecore_X_Atom xkb = 0;
        Ecore_X_Window root = 0;
        int len = 0;
        unsigned char *dat;
        char *rules, *model, *layout;

        if ((ekbd = e_xkb_layout_get()))
          {
             model = strdup(ekbd->model);
             layout = strdup(ekbd->name);
          }

        root = ecore_x_window_root_first_get();
        xkb = ecore_x_atom_get("_XKB_RULES_NAMES");
        ecore_x_window_prop_property_get(root, xkb, ECORE_X_ATOM_STRING, 
                                         1024, &dat, &len);
        if ((dat) && (len > 0))
          {
             rules = (char *)dat;
             dat += strlen((const char *)dat) + 1;
             if (!model) model = strdup((const char *)dat);
             dat += strlen((const char *)dat) + 1;
             if (!layout) layout = strdup((const char *)dat);
          }

        /* fallback */
        if (!rules) rules = strdup("evdev");
        if (!model) model = strdup("pc105");
        if (!layout) layout = strdup("us");

        /* update compositor keymap */
        e_comp_wl_input_keymap_set(cdata, rules, model, layout);
     }
#endif

   /* initialize shm mechanism */
   wl_display_init_shm(cdata->wl.disp);

   /* get the wayland display loop */
   cdata->wl.loop = wl_display_get_event_loop(cdata->wl.disp);

   /* get the file descriptor of the wayland event loop */
   fd = wl_event_loop_get_fd(cdata->wl.loop);

   /* create a listener for wayland main loop events */
   cdata->fd_hdlr = 
     ecore_main_fd_handler_add(fd, (ECORE_FD_READ | ECORE_FD_ERROR), 
                               _e_comp_wl_cb_read, cdata, NULL, NULL);
   ecore_main_fd_handler_prepare_callback_set(cdata->fd_hdlr, 
                                              _e_comp_wl_cb_prepare, cdata);

   /* setup module idler to load shell mmodule */
   ecore_idler_add(_e_comp_wl_cb_module_idle, cdata);

   if (comp->comp_type == E_PIXMAP_TYPE_X)
     {
        e_comp_wl_input_pointer_enabled_set(cdata, EINA_TRUE);
        e_comp_wl_input_keyboard_enabled_set(cdata, EINA_TRUE);
     }

   /* set compositor wayland data */
   comp->wl_comp_data = cdata;

   return EINA_TRUE;

input_err:
   e_comp_wl_data_manager_shutdown(cdata);
data_err:
comp_global_err:
   e_env_unset("WAYLAND_DISPLAY");
sock_err:
   wl_display_destroy(cdata->wl.disp);
disp_err:
   free(cdata);
   return EINA_FALSE;
}

/* public functions */
EAPI Eina_Bool 
e_comp_wl_init(void)
{
   /* set gl available if we have ecore_evas support */
   if (ecore_evas_engine_type_supported_get(ECORE_EVAS_ENGINE_WAYLAND_EGL) || 
       ecore_evas_engine_type_supported_get(ECORE_EVAS_ENGINE_OPENGL_DRM))
     e_comp_gl_set(EINA_TRUE);

   /* try to create a wayland compositor */
   if (!_e_comp_wl_compositor_create())
     {
        e_error_message_show(_("Enlightenment cannot create a Wayland Compositor!\n"));
        return EINA_FALSE;
     }

   /* try to init ecore_wayland */
   if (!ecore_wl_init(NULL))
     {
        e_error_message_show(_("Enlightenment cannot initialize Ecore_Wayland!\n"));
        return EINA_FALSE;
     }

   /* create hash to store clients */
   /* clients_win_hash = eina_hash_int64_new(NULL); */

   /* add event handlers to catch E events */
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_COMP_OBJECT_ADD, 
                         _e_comp_wl_cb_comp_object_add, NULL);

   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_KEY_DOWN, 
                         _e_comp_wl_cb_input_event, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_KEY_UP, 
                         _e_comp_wl_cb_input_event, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_MOUSE_BUTTON_DOWN, 
                         _e_comp_wl_cb_input_event, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_MOUSE_BUTTON_UP, 
                         _e_comp_wl_cb_input_event, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_MOUSE_MOVE, 
                         _e_comp_wl_cb_input_event, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_MOUSE_WHEEL, 
                         _e_comp_wl_cb_input_event, NULL);

   /* add hooks to catch e_client events */
   e_client_hook_add(E_CLIENT_HOOK_NEW_CLIENT, _e_comp_wl_client_cb_new, NULL);
   e_client_hook_add(E_CLIENT_HOOK_DEL, _e_comp_wl_client_cb_del, NULL);

   e_client_hook_add(E_CLIENT_HOOK_EVAL_POST_NEW_CLIENT, 
                     _e_comp_wl_client_cb_post_new, NULL);
   /* e_client_hook_add(E_CLIENT_HOOK_EVAL_PRE_FRAME_ASSIGN,  */
   /*                   _e_comp_wl_client_cb_pre_frame, NULL); */

   e_client_hook_add(E_CLIENT_HOOK_FOCUS_SET, 
                     _e_comp_wl_client_cb_focus_set, NULL);
   e_client_hook_add(E_CLIENT_HOOK_FOCUS_UNSET, 
                     _e_comp_wl_client_cb_focus_unset, NULL);

   e_client_hook_add(E_CLIENT_HOOK_RESIZE_BEGIN, 
                     _e_comp_wl_client_cb_resize_begin, NULL);
   e_client_hook_add(E_CLIENT_HOOK_RESIZE_END, 
                     _e_comp_wl_client_cb_resize_end, NULL);

   _last_event_time = ecore_loop_time_get();

   return EINA_TRUE;
}

EAPI struct wl_signal 
e_comp_wl_surface_create_signal_get(E_Comp *comp)
{
   return comp->wl_comp_data->signals.surface.create;
}

/* internal functions */
EINTERN void 
e_comp_wl_shutdown(void)
{
   /* free handlers */
   E_FREE_LIST(handlers, ecore_event_handler_del);

   /* free the clients win hash */
   /* E_FREE_FUNC(clients_win_hash, eina_hash_free); */

   /* shutdown ecore_wayland */
   ecore_wl_shutdown();
}

EINTERN struct wl_resource *
e_comp_wl_surface_create(struct wl_client *client, int version, uint32_t id)
{
   struct wl_resource *ret = NULL;

   if ((ret = wl_resource_create(client, &wl_surface_interface, version, id)))
     DBG("Created Surface: %d", wl_resource_get_id(ret));

   return ret;
}

EINTERN Eina_Bool 
e_comp_wl_surface_commit(E_Client *ec)
{
   E_Pixmap *ep;
   Eina_Rectangle *dmg;
   Eina_Tiler *src, *tmp;
   Eina_Bool first;

   if (!(ep = ec->pixmap)) return EINA_FALSE;
   _e_comp_wl_client_evas_init(ec);
   if (ec->focused && ec->comp_data->focus_update)
     _e_comp_wl_client_focus(ec);

   first = !e_pixmap_usable_get(ep);
   /* mark the pixmap as usable or not */
   e_pixmap_usable_set(ep, (ec->comp_data->pending.buffer != NULL));

   /* mark the pixmap as dirty */
   e_pixmap_dirty(ep);

   e_pixmap_image_clear(ep, EINA_FALSE);
   e_pixmap_resource_set(ep, ec->comp_data->pending.buffer);

   /* refresh pixmap */
   if (e_pixmap_refresh(ep))
     {
        e_comp->post_updates = eina_list_append(e_comp->post_updates, ec);
        e_object_ref(E_OBJECT(ec));
     }

   /* check if we need to map this surface */
   if (ec->comp_data->pending.buffer)
     {
        /* if this surface is not mapped yet, map it */
        if (!ec->comp_data->mapped)
          {
             /* if the client has a shell map, call it */
             if ((ec->comp_data->shell.surface) && (ec->comp_data->shell.map))
               ec->comp_data->shell.map(ec->comp_data->shell.surface);
             else
               {
                  evas_object_show(ec->frame);
                  ec->comp_data->mapped = evas_object_visible_get(ec->frame);
               }
          }
     }
   else
     {
        /* no pending buffer to attach. unmap the surface */
        if (ec->comp_data->mapped)
          {
             /* if the client has a shell map, call it */
             if ((ec->comp_data->shell.surface) && (ec->comp_data->shell.unmap))
               ec->comp_data->shell.unmap(ec->comp_data->shell.surface);
             else
               {
                  evas_object_hide(ec->frame);
                  ec->comp_data->mapped = evas_object_visible_get(ec->frame);
               }
          }
     }

   /* check for any pending attachments */
   if (ec->comp_data->pending.new_attach)
     {
        int x, y, nw, nh;
        Eina_Bool placed = EINA_TRUE;;

        e_pixmap_size_get(ec->pixmap, &nw, &nh);
        if (ec->changes.pos)
          e_comp_object_frame_xy_adjust(ec->frame, ec->x, ec->y, &x, &y);
        else
          x = ec->client.x, y = ec->client.y;
        if (ec->new_client)
          placed = ec->placed;
        /* if the client has a shell configure, call it */
        if ((ec->comp_data->shell.surface) && (ec->comp_data->shell.configure))
          ec->comp_data->shell.configure(ec->comp_data->shell.surface, x, y, nw, nh);
        else
          e_client_util_move_resize_without_frame(ec, x, y, nw, nh);
        if (ec->new_client)
          ec->placed = placed;
        else if (first && ec->placed)
          {
             ec->x = ec->y = 0;
             ec->placed = 0;
             ec->new_client = 1;
          }
     }

   if (!ec->comp_data->mapped) 
     {
        DBG("\tSurface Not Mapped. Skip to Unmapped");
        goto unmap;
     }

   /* commit any pending damages */
   if ((!ec->comp->nocomp) && (ec->frame))
     {
        EINA_LIST_FREE(ec->comp_data->pending.damages, dmg)
          {
             e_comp_object_damage(ec->frame, dmg->x, dmg->y, dmg->w, dmg->h);
             eina_rectangle_free(dmg);
          }
     }

   /* handle pending input */
   if (ec->comp_data->pending.input)
     {
        tmp = eina_tiler_new(ec->w, ec->h);
        eina_tiler_tile_size_set(tmp, 1, 1);
        eina_tiler_rect_add(tmp, 
                            &(Eina_Rectangle){0, 0, ec->client.w, ec->client.h});

        if ((src = eina_tiler_intersection(ec->comp_data->pending.input, tmp)))
          {
             Eina_Rectangle *rect;
             Eina_Iterator *itr;

             itr = eina_tiler_iterator_new(src);
             /* this should be exactly 1 rect */
             EINA_ITERATOR_FOREACH(itr, rect)
               e_comp_object_input_area_set(ec->frame, rect->x, rect->y, rect->w, rect->h);

             eina_iterator_free(itr);
             eina_tiler_free(src);
          }
        else
          e_comp_object_input_area_set(ec->frame, 0, 0, ec->w, ec->h);

        eina_tiler_free(tmp);
     }

   return EINA_TRUE;

unmap:

   /* surface is not visible, clear damages */
   EINA_LIST_FREE(ec->comp_data->pending.damages, dmg)
     eina_rectangle_free(dmg);

   /* clear pending input regions */
   if (ec->comp_data->pending.input)
     eina_tiler_clear(ec->comp_data->pending.input);

   return EINA_TRUE;
}

EINTERN Eina_Bool 
e_comp_wl_subsurface_commit(E_Client *ec)
{
   E_Comp_Wl_Subsurf_Data *sdata;

   /* check for valid subcompositor data */
   if (!(sdata = ec->comp_data->sub.data)) return EINA_FALSE;

   if (_e_comp_wl_subsurface_synchronized_get(sdata))
     _e_comp_wl_subsurface_commit_to_cache(ec);
   else
     {
        E_Client *subc;
        Eina_List *l;

        if (sdata->cached.has_data)
          {
             _e_comp_wl_subsurface_commit_to_cache(ec);
             _e_comp_wl_subsurface_commit_from_cache(ec);
          }
        else
          e_comp_wl_surface_commit(ec);

        EINA_LIST_FOREACH(ec->comp_data->sub.list, l, subc)
          {
             if (ec != subc)
               _e_comp_wl_subsurface_parent_commit(subc, EINA_FALSE);
          }
     }

   return EINA_TRUE;
}

EAPI double 
e_comp_wl_idle_time_get(void)
{
   return (ecore_loop_time_get() - _last_event_time);
}

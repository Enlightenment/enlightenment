#define E_COMP_WL
#include "e.h"

/* handle include for printing uint64_t */
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#define COMPOSITOR_VERSION 3

/* Resource Data Mapping: (wl_resource_get_user_data)
 *
 * wl_surface == e_pixmap
 * wl_region == eina_tiler
 * wl_subsurface == e_client
 *
 */

static void _e_comp_wl_subsurface_parent_commit(E_Client *ec, Eina_Bool parent_synchronized);

/* local variables */
/* static Eina_Hash *clients_win_hash = NULL; */
static Eina_List *handlers = NULL;
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
_e_comp_wl_focus_check(void)
{
   E_Client *ec;

   if (stopping) return;
   ec = e_client_focused_get();
   if ((!ec) || (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL))
     e_grabinput_focus(e_comp->ee_win, E_FOCUS_METHOD_PASSIVE);
}

static void
_e_comp_wl_log_cb_print(const char *format, va_list args)
{
   EINA_LOG_DOM_INFO(e_log_dom, format, args);
}

static Eina_Bool
_e_comp_wl_cb_read(void *data EINA_UNUSED, Ecore_Fd_Handler *hdlr EINA_UNUSED)
{
   /* dispatch pending wayland events */
   wl_event_loop_dispatch(e_comp->wl_comp_data->wl.loop, 0);

   return ECORE_CALLBACK_RENEW;
}

static void
_e_comp_wl_cb_prepare(void *data EINA_UNUSED, Ecore_Fd_Handler *hdlr EINA_UNUSED)
{
   /* flush pending client events */
   wl_display_flush_clients(e_comp->wl_comp_data->wl.disp);
}

static Eina_Bool
_e_comp_wl_cb_module_idle(void *data EINA_UNUSED)
{
   E_Module  *mod = NULL;
   const char **m, *mods[] =
   {
      "wl_desktop_shell",
      "xwayland",
      NULL
   };

   /* check if we are still loading modules */
   if (e_module_loading_get()) return ECORE_CALLBACK_RENEW;

   for (m = mods; *m; m++)
     {
        E_Module *mod = e_module_find(*m);

        if (!mod)
          mod = e_module_new(*m);

        if (mod)
          e_module_enable(mod);
     }

   /* FIXME: NB:
    * Do we need to dispatch pending wl events here ?? */

   return ECORE_CALLBACK_CANCEL;
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

   if (!eina_list_count(e_comp->wl_comp_data->ptr.resources)) return;

   wc = wl_resource_get_client(ec->comp_data->surface);
   serial = wl_display_next_serial(e_comp->wl_comp_data->wl.disp);
   EINA_LIST_FOREACH(e_comp->wl_comp_data->ptr.resources, l, res)
     {
        if (!e_comp_wl_input_pointer_check(res)) continue;
        if (wl_resource_get_client(res) != wc) continue;
        wl_pointer_send_enter(res, serial, ec->comp_data->surface,
                              wl_fixed_from_int(ev->canvas.x - ec->client.x),
                              wl_fixed_from_int(ev->canvas.y - ec->client.y));
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

   if (!eina_list_count(e_comp->wl_comp_data->ptr.resources)) return;

   wc = wl_resource_get_client(ec->comp_data->surface);
   serial = wl_display_next_serial(e_comp->wl_comp_data->wl.disp);
   EINA_LIST_FOREACH(e_comp->wl_comp_data->ptr.resources, l, res)
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
   if (!ec->comp_data->surface) return;

   wc = wl_resource_get_client(ec->comp_data->surface);
   EINA_LIST_FOREACH(e_comp->wl_comp_data->ptr.resources, l, res)
     {
        if (!e_comp_wl_input_pointer_check(res)) continue;
        if (wl_resource_get_client(res) != wc) continue;
        wl_pointer_send_motion(res, ev->timestamp,
                               wl_fixed_from_int(ev->cur.canvas.x - ec->client.x),
                               wl_fixed_from_int(ev->cur.canvas.y - ec->client.y));
     }
}

static Eina_Bool
_e_comp_wl_evas_handle_mouse_button(E_Client *ec, uint32_t timestamp, uint32_t button_id, uint32_t state)
{
   Eina_List *l;
   struct wl_client *wc;
   uint32_t serial, btn;
   struct wl_resource *res;

   if (ec->cur_mouse_action || ec->border_menu) return EINA_FALSE;
   if (e_object_is_del(E_OBJECT(ec))) return EINA_FALSE;
   if (e_client_util_ignored_get(ec)) return EINA_FALSE;

   switch (button_id)
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
        btn = button_id;
        break;
     }

   e_comp->wl_comp_data->ptr.button = btn;

   if (!ec->comp_data->surface) return EINA_FALSE;

   if (!eina_list_count(e_comp->wl_comp_data->ptr.resources))
     return EINA_TRUE;

   wc = wl_resource_get_client(ec->comp_data->surface);
   serial = wl_display_next_serial(e_comp->wl_comp_data->wl.disp);

   EINA_LIST_FOREACH(e_comp->wl_comp_data->ptr.resources, l, res)
     {
        if (wl_resource_get_client(res) != wc) continue;
        if (!e_comp_wl_input_pointer_check(res)) continue;
        wl_pointer_send_button(res, serial, timestamp, btn, state);
     }
   return EINA_TRUE;
}

static void
_e_comp_wl_evas_cb_mouse_down(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   E_Client *ec = data;
   Evas_Event_Mouse_Down *ev = event;

   _e_comp_wl_evas_handle_mouse_button(ec, ev->timestamp, ev->button,
                                       WL_POINTER_BUTTON_STATE_PRESSED);
}

static void
_e_comp_wl_evas_cb_mouse_up(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   E_Client *ec = data;
   Evas_Event_Mouse_Up *ev = event;

   _e_comp_wl_evas_handle_mouse_button(ec, ev->timestamp, ev->button,
                                       WL_POINTER_BUTTON_STATE_RELEASED);
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
   EINA_LIST_FOREACH(e_comp->wl_comp_data->ptr.resources, l, res)
     {
        if (!e_comp_wl_input_pointer_check(res)) continue;
        if (wl_resource_get_client(res) != wc) continue;
        wl_pointer_send_axis(res, ev->timestamp, axis, dir);
     }
}

static void
_e_comp_wl_evas_cb_multi_down(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   Eina_List *l;
   struct wl_client *wc;
   uint32_t serial;
   struct wl_resource *res;
   E_Client *ec = data;
   Evas_Event_Multi_Down *ev = event;
   wl_fixed_t x, y;

   if (!ec->comp_data->surface) return;

   wc = wl_resource_get_client(ec->comp_data->surface);
   serial = wl_display_next_serial(e_comp->wl_comp_data->wl.disp);

   x = wl_fixed_from_int(ev->canvas.x - ec->client.x);
   y = wl_fixed_from_int(ev->canvas.y - ec->client.y);

   EINA_LIST_FOREACH(e_comp->wl_comp_data->touch.resources, l, res)
     {
        if (wl_resource_get_client(res) != wc) continue;
        if (!e_comp_wl_input_touch_check(res)) continue;
        wl_touch_send_down(res, serial, ev->timestamp, ec->comp_data->surface, ev->device, x, y);
     }
}

static void
_e_comp_wl_evas_cb_multi_up(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   Eina_List *l;
   struct wl_client *wc;
   uint32_t serial;
   struct wl_resource *res;
   E_Client *ec = data;
   Evas_Event_Multi_Up *ev = event;

   if (!ec->comp_data->surface) return;

   wc = wl_resource_get_client(ec->comp_data->surface);
   serial = wl_display_next_serial(e_comp->wl_comp_data->wl.disp);

   EINA_LIST_FOREACH(e_comp->wl_comp_data->touch.resources, l, res)
     {
        if (wl_resource_get_client(res) != wc) continue;
        if (!e_comp_wl_input_touch_check(res)) continue;
        wl_touch_send_up(res, serial, ev->timestamp, ev->device);
     }
}

static void
_e_comp_wl_evas_cb_multi_move(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   Eina_List *l;
   struct wl_client *wc;
   struct wl_resource *res;
   E_Client *ec = data;
   Evas_Event_Multi_Move *ev = event;
   wl_fixed_t x, y;

   if (!ec->comp_data->surface) return;

   wc = wl_resource_get_client(ec->comp_data->surface);

   x = wl_fixed_from_int(ev->cur.canvas.x - ec->client.x);
   y = wl_fixed_from_int(ev->cur.canvas.y - ec->client.y);

   EINA_LIST_FOREACH(e_comp->wl_comp_data->touch.resources, l, res)
     {
        if (wl_resource_get_client(res) != wc) continue;
        if (!e_comp_wl_input_touch_check(res)) continue;
        wl_touch_send_motion(res, ev->timestamp, ev->device, x, y);
     }
}


static void
_e_comp_wl_client_priority_adjust(int pid, int set, int adj, Eina_Bool use_adj, Eina_Bool adj_child, Eina_Bool do_child)
{
   Eina_List *files;
   char *file, buff[PATH_MAX];
   FILE *f;
   int pid2, ppid;
   int num_read;
   int n;

   if (use_adj)
      n = (getpriority(PRIO_PROCESS, pid) + adj);
   else
      n = set;
   setpriority(PRIO_PROCESS, pid, n);

   if (adj_child)
      use_adj = EINA_TRUE;

   if (!do_child)
      return;

   files = ecore_file_ls("/proc");
   EINA_LIST_FREE(files, file)
      {
         if (!isdigit(file[0]))
            continue;

         snprintf(buff, sizeof(buff), "/proc/%s/stat", file);
         if ((f = fopen(buff, "r")))
            {
               pid2 = -1;
               ppid = -1;
               num_read = fscanf(f, "%i %*s %*s %i %*s", &pid2, &ppid);
               fclose(f);
               if (num_read == 2 && ppid == pid)
                  _e_comp_wl_client_priority_adjust(pid2, set,
                                                    adj, use_adj,
                                                    adj_child, do_child);
            }

         free(file);
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

static void
_e_comp_wl_evas_cb_focus_in(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec, *focused;
   uint32_t *k;

   if (!(ec = data)) return;
   if (e_object_is_del(E_OBJECT(ec))) return;
   if (ec->iconic) return;

   /* block spurious focus events */
   focused = e_client_focused_get();
   if ((focused) && (ec != focused)) return;

   /* raise client priority */
   _e_comp_wl_client_priority_raise(ec);

   /* update keyboard modifier state */
   wl_array_for_each(k, &e_comp->wl_comp_data->kbd.keys)
     e_comp_wl_input_keyboard_state_update(*k, EINA_TRUE);

   e_comp_wl_input_keyboard_enter_send(ec);
}

static void
_e_comp_wl_evas_cb_focus_out(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec;
   E_Comp_Wl_Data *cdata;
   struct wl_resource *res;
   struct wl_client *wc;
   uint32_t serial, *k;
   Eina_List *l;

   if (!(ec = data)) return;

   cdata = e_comp->wl_comp_data;

   /* update keyboard modifier state */
   wl_array_for_each(k, &cdata->kbd.keys)
     e_comp_wl_input_keyboard_state_update(*k, EINA_FALSE);

   if (e_object_is_del(E_OBJECT(ec))) return;

   /* lower client priority */
   _e_comp_wl_client_priority_normal(ec);

   if (!ec->comp_data->surface) return;

   if (!eina_list_count(cdata->kbd.resources)) return;

   /* send keyboard_leave to all keyboard resources */
   wc = wl_resource_get_client(ec->comp_data->surface);
   serial = wl_display_next_serial(cdata->wl.disp);
   EINA_LIST_FOREACH(cdata->kbd.resources, l, res)
     {
        if (wl_resource_get_client(res) != wc) continue;
        wl_keyboard_send_leave(res, serial, ec->comp_data->surface);
     }
}

static void
_e_comp_wl_evas_cb_resize(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec;

   if (!(ec = data)) return;

   if ((ec->shading) || (ec->shaded)) return;
   if (!ec->comp_data->shell.configure_send) return;
   if (e_client_util_resizing_get(ec))
     {
        int x, y, ax, ay;

        x = ec->mouse.last_down[ec->moveinfo.down.button - 1].w;
        y = ec->mouse.last_down[ec->moveinfo.down.button - 1].h;
        if (ec->comp_data->shell.window.w && ec->comp_data->shell.window.h)
          {
             ax = ec->client.w - ec->comp_data->shell.window.w;
             ay = ec->client.h - ec->comp_data->shell.window.h;
          }
        else
          ax = ay = 0;

        switch (ec->resize_mode)
          {
           case E_POINTER_RESIZE_TL:
           case E_POINTER_RESIZE_L:
           case E_POINTER_RESIZE_BL:
             x += ec->mouse.last_down[ec->moveinfo.down.button - 1].mx -
               ec->mouse.current.mx - ec->comp_data->shell.window.x;
             break;
           case E_POINTER_RESIZE_TR:
           case E_POINTER_RESIZE_R:
           case E_POINTER_RESIZE_BR:
             x += ec->mouse.current.mx - ec->mouse.last_down[ec->moveinfo.down.button - 1].mx -
               ec->comp_data->shell.window.x;
             break;
           default:
             x -= ax;
          }
        switch (ec->resize_mode)
          {
           case E_POINTER_RESIZE_TL:
           case E_POINTER_RESIZE_T:
           case E_POINTER_RESIZE_TR:
             y += ec->mouse.last_down[ec->moveinfo.down.button - 1].my -
               ec->mouse.current.my - ec->comp_data->shell.window.y;
             break;
           case E_POINTER_RESIZE_BL:
           case E_POINTER_RESIZE_B:
           case E_POINTER_RESIZE_BR:
             y += ec->mouse.current.my - ec->mouse.last_down[ec->moveinfo.down.button - 1].my -
               ec->comp_data->shell.window.y;
             break;
           default:
             y -= ay;
          }
        x = E_CLAMP(x, 1, x);
        y = E_CLAMP(y, 1, y);
        ec->comp_data->shell.configure_send(ec->comp_data->shell.surface,
                                 e_comp->wl_comp_data->resize.edges,
                                 x, y);
     }
   else
     ec->comp_data->shell.configure_send(ec->comp_data->shell.surface,
                                 e_comp->wl_comp_data->resize.edges,
                                 ec->client.w, ec->client.h);
}

static void
_e_comp_wl_evas_cb_state_update(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec = data;

   if (e_object_is_del(E_OBJECT(ec))) return;

   /* check for wayland pixmap */

   if (ec->comp_data->shell.configure_send)
     ec->comp_data->shell.configure_send(ec->comp_data->shell.surface, 0, ec->client.w, ec->client.h);
}

static void
_e_comp_wl_evas_cb_delete_request(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec;

   if (!(ec = data)) return;
   if (ec->netwm.ping) e_client_ping(ec);

   e_comp_ignore_win_del(E_PIXMAP_TYPE_WL, e_pixmap_window_get(ec->pixmap));

   e_object_del(E_OBJECT(ec));

   _e_comp_wl_focus_check();

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

   _e_comp_wl_focus_check();
}

static void
_e_comp_wl_evas_cb_ping(void *data, Evas_Object *obj EINA_UNUSED, void *event EINA_UNUSED)
{
   E_Client *ec;

   if (!(ec = data)) return;
   if (!(ec->comp_data->shell.ping)) return;
   if (!(ec->comp_data->shell.surface)) return;

   ec->comp_data->shell.ping(ec->comp_data->shell.surface);
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
_e_comp_wl_buffer_reference_cb_destroy(struct wl_listener *listener, void *data)
{
   E_Comp_Wl_Buffer_Ref *ref;

   ref = container_of(listener, E_Comp_Wl_Buffer_Ref, destroy_listener);
   if ((E_Comp_Wl_Buffer *)data != ref->buffer) return;
   ref->buffer = NULL;
}

static void
_e_comp_wl_buffer_cb_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Comp_Wl_Buffer *buffer;

   buffer = container_of(listener, E_Comp_Wl_Buffer, destroy_listener);
   wl_signal_emit(&buffer->destroy_signal, buffer);
   free(buffer);
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
   evas_object_event_callback_priority_add(ec->frame, EVAS_CALLBACK_MOUSE_IN, EVAS_CALLBACK_PRIORITY_AFTER,
                                  _e_comp_wl_evas_cb_mouse_in, ec);
   evas_object_event_callback_priority_add(ec->frame, EVAS_CALLBACK_MOUSE_OUT, EVAS_CALLBACK_PRIORITY_AFTER,
                                  _e_comp_wl_evas_cb_mouse_out, ec);
   evas_object_event_callback_priority_add(ec->frame, EVAS_CALLBACK_MOUSE_MOVE, EVAS_CALLBACK_PRIORITY_AFTER,
                                  _e_comp_wl_evas_cb_mouse_move, ec);
   evas_object_event_callback_priority_add(ec->frame, EVAS_CALLBACK_MOUSE_DOWN, EVAS_CALLBACK_PRIORITY_AFTER,
                                  _e_comp_wl_evas_cb_mouse_down, ec);
   evas_object_event_callback_priority_add(ec->frame, EVAS_CALLBACK_MOUSE_UP, EVAS_CALLBACK_PRIORITY_AFTER,
                                  _e_comp_wl_evas_cb_mouse_up, ec);
   evas_object_event_callback_priority_add(ec->frame, EVAS_CALLBACK_MOUSE_WHEEL, EVAS_CALLBACK_PRIORITY_AFTER,
                                  _e_comp_wl_evas_cb_mouse_wheel, ec);

   evas_object_event_callback_priority_add(ec->frame, EVAS_CALLBACK_MULTI_DOWN, EVAS_CALLBACK_PRIORITY_AFTER,
                                  _e_comp_wl_evas_cb_multi_down, ec);
   evas_object_event_callback_priority_add(ec->frame, EVAS_CALLBACK_MULTI_UP, EVAS_CALLBACK_PRIORITY_AFTER,
                                  _e_comp_wl_evas_cb_multi_up, ec);
   evas_object_event_callback_priority_add(ec->frame, EVAS_CALLBACK_MULTI_MOVE, EVAS_CALLBACK_PRIORITY_AFTER,
                                  _e_comp_wl_evas_cb_multi_move, ec);


   evas_object_event_callback_priority_add(ec->frame, EVAS_CALLBACK_FOCUS_IN, EVAS_CALLBACK_PRIORITY_AFTER,
                                  _e_comp_wl_evas_cb_focus_in, ec);
   evas_object_event_callback_priority_add(ec->frame, EVAS_CALLBACK_FOCUS_OUT, EVAS_CALLBACK_PRIORITY_AFTER,
                                  _e_comp_wl_evas_cb_focus_out, ec);

   if (!ec->override)
     {
        evas_object_smart_callback_add(ec->frame, "client_resize",
                                       _e_comp_wl_evas_cb_resize, ec);
        evas_object_smart_callback_add(ec->frame, "maximize_done",
                                       _e_comp_wl_evas_cb_state_update, ec);
        evas_object_smart_callback_add(ec->frame, "unmaximize_done",
                                       _e_comp_wl_evas_cb_state_update, ec);
        evas_object_smart_callback_add(ec->frame, "fullscreen",
                                       _e_comp_wl_evas_cb_state_update, ec);
        evas_object_smart_callback_add(ec->frame, "unfullscreen",
                                       _e_comp_wl_evas_cb_state_update, ec);
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
_e_comp_wl_cb_randr_change(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Eina_List *l;
   E_Randr2_Screen *screen;
   unsigned int transform = WL_OUTPUT_TRANSFORM_NORMAL;

   if (!e_randr2) return ECORE_CALLBACK_RENEW;

   EINA_LIST_FOREACH(e_randr2->screens, l, screen)
     {
        if (!screen->config.enabled)
          {
             e_comp_wl_output_remove(screen->id);
             continue;
          }

        switch (screen->config.rotation)
          {
           case 90:
             transform = WL_OUTPUT_TRANSFORM_90;
             break;
           case 180:
             transform = WL_OUTPUT_TRANSFORM_180;
             break;
           case 270:
             transform = WL_OUTPUT_TRANSFORM_270;
             break;
           case 0:
           default:
             transform = WL_OUTPUT_TRANSFORM_NORMAL;
             break;
          }

        if (!e_comp_wl_output_init(screen->id, screen->info.name,
                                   screen->info.screen,
                                   screen->config.geom.x, screen->config.geom.y,
                                   screen->config.geom.w, screen->config.geom.h,
                                   screen->info.size.w, screen->info.size.h,
                                   screen->config.mode.refresh, 0, transform))
          ERR("Could not initialize screen %s", screen->info.name);
     }

   return ECORE_CALLBACK_RENEW;
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
   if (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL) return ECORE_CALLBACK_RENEW;

   /* if we have not setup evas callbacks for this client, do it */
   if (!ec->comp_data->evas_init) _e_comp_wl_client_evas_init(ec);

   return ECORE_CALLBACK_RENEW;
}

static void
_e_comp_wl_cb_key_down(void *event)
{
   E_Comp_Wl_Data *cdata;
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
   if (!(k = wl_array_add(&cdata->kbd.keys, sizeof(*k))))
     {
        DBG("wl_array_add: Out of memory\n");
        return;
     }
   *k = keycode;

   if ((ec = e_client_focused_get()))
     {
        /* update modifier state */
        e_comp_wl_input_keyboard_state_update(keycode, EINA_TRUE);

        if (ec->comp_data->surface)
          {
             struct wl_client *wc;
             struct wl_resource *res;
             Eina_List *l;

             if (eina_list_count(cdata->kbd.resources))
               {
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
     }

   if (cdata->kbd.mod_changed)
     {
        e_comp_wl_input_keyboard_modifiers_update();
        cdata->kbd.mod_changed = 0;
     }
}

static void
_e_comp_wl_cb_key_up(void *event)
{
   E_Client *ec;
   E_Comp_Wl_Data *cdata;
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
   e_comp_wl_input_keyboard_state_update(keycode, EINA_FALSE);

   if ((ec = e_client_focused_get()))
     {
        if (ec->comp_data->surface)
          {
             struct wl_client *wc;
             struct wl_resource *res;
             Eina_List *l;

             if (eina_list_count(cdata->kbd.resources))
               {
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
     }

   if (cdata->kbd.mod_changed)
     {
        e_comp_wl_input_keyboard_modifiers_update();
        cdata->kbd.mod_changed = 0;
     }
}

static void
_e_comp_wl_cb_mouse_move(void *event)
{
   E_Comp_Wl_Data *cdata;
   Ecore_Event_Mouse_Move *ev;

   if (!(cdata = e_comp->wl_comp_data)) return;

   ev = event;
   cdata->ptr.x = wl_fixed_from_int(ev->x);
   cdata->ptr.y = wl_fixed_from_int(ev->y);
}

static Eina_Bool
_e_comp_wl_cb_input_event(void *data EINA_UNUSED, int type, void *ev)
{
   _last_event_time = ecore_loop_time_get();

   if (type == ECORE_EVENT_KEY_DOWN)
     _e_comp_wl_cb_key_down(ev);
   else if (type == ECORE_EVENT_KEY_UP)
     _e_comp_wl_cb_key_up(ev);
   else if (type == ECORE_EVENT_MOUSE_MOVE)
     _e_comp_wl_cb_mouse_move(ev);

   return ECORE_CALLBACK_RENEW;
}

static void
_e_comp_wl_surface_state_size_update(E_Client *ec, E_Comp_Wl_Surface_State *state)
{
   int w = 0, h = 0;
   /* double scale = 0.0; */

   if (!ec->comp_data->buffer_ref.buffer)
     {
        state->bw = 0;
        state->bh = 0;
        return;
     }

   /* scale = e_comp->wl_comp_data->output.scale; */
   /* switch (e_comp->wl_comp_data->output.transform) */
   /*   { */
   /*    case WL_OUTPUT_TRANSFORM_90: */
   /*    case WL_OUTPUT_TRANSFORM_270: */
   /*    case WL_OUTPUT_TRANSFORM_FLIPPED_90: */
   /*    case WL_OUTPUT_TRANSFORM_FLIPPED_270: */
   /*      w = ec->comp_data->buffer_ref.buffer->h / scale; */
   /*      h = ec->comp_data->buffer_ref.buffer->w / scale; */
   /*      break; */
   /*    default: */
   /*      w = ec->comp_data->buffer_ref.buffer->w / scale; */
   /*      h = ec->comp_data->buffer_ref.buffer->h / scale; */
   /*      break; */
   /*   } */

   w = ec->comp_data->buffer_ref.buffer->w;
   h = ec->comp_data->buffer_ref.buffer->h;

   state->bw = w;
   state->bh = h;
}

static void
_e_comp_wl_surface_state_cb_buffer_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   E_Comp_Wl_Surface_State *state;

   state =
     container_of(listener, E_Comp_Wl_Surface_State, buffer_destroy_listener);
   state->buffer = NULL;
}

static void
_e_comp_wl_surface_state_init(E_Comp_Wl_Surface_State *state, int w, int h)
{
   state->new_attach = EINA_FALSE;
   state->buffer = NULL;
   state->buffer_destroy_listener.notify =
     _e_comp_wl_surface_state_cb_buffer_destroy;
   state->sx = state->sy = 0;

   state->input = eina_tiler_new(w, h);
   eina_tiler_tile_size_set(state->input, 1, 1);

   state->opaque = eina_tiler_new(w, h);
   eina_tiler_tile_size_set(state->opaque, 1, 1);
}

static void
_e_comp_wl_surface_state_finish(E_Comp_Wl_Surface_State *state)
{
   struct wl_resource *cb;
   Eina_Rectangle *dmg;

   EINA_LIST_FREE(state->frames, cb)
     wl_resource_destroy(cb);

   EINA_LIST_FREE(state->damages, dmg)
     eina_rectangle_free(dmg);

   if (state->opaque) eina_tiler_free(state->opaque);
   state->opaque = NULL;

   if (state->input) eina_tiler_free(state->input);
   state->input = NULL;

   if (state->buffer) wl_list_remove(&state->buffer_destroy_listener.link);
   state->buffer = NULL;
}

static void
_e_comp_wl_surface_state_buffer_set(E_Comp_Wl_Surface_State *state, E_Comp_Wl_Buffer *buffer)
{
   if (state->buffer == buffer) return;
   if (state->buffer)
     wl_list_remove(&state->buffer_destroy_listener.link);
   state->buffer = buffer;
   if (state->buffer)
     wl_signal_add(&state->buffer->destroy_signal,
                   &state->buffer_destroy_listener);
}

static void
_e_comp_wl_surface_state_commit(E_Client *ec, E_Comp_Wl_Surface_State *state)
{
   Eina_Bool first = EINA_FALSE;
   Eina_Rectangle *dmg;
   Eina_Bool placed = EINA_TRUE;
   int x = 0, y = 0;

   first = !e_pixmap_usable_get(ec->pixmap);

   if (state->new_attach)
     e_comp_wl_surface_attach(ec, state->buffer);

   _e_comp_wl_surface_state_buffer_set(state, NULL);

   if (state->new_attach)
     {
        _e_comp_wl_surface_state_size_update(ec, &ec->comp_data->pending);

        if (ec->changes.pos)
          e_comp_object_frame_xy_unadjust(ec->frame, ec->x, ec->y, &x, &y);
        else
          x = ec->client.x, y = ec->client.y;

        if (ec->new_client) placed = ec->placed;

        ec->w = ec->client.w = state->bw;
        ec->h = ec->client.h = state->bh;
     }
   if (!e_pixmap_usable_get(ec->pixmap))
     {
        if (ec->comp_data->mapped)
          {
             if ((ec->comp_data->shell.surface) && (ec->comp_data->shell.unmap))
               ec->comp_data->shell.unmap(ec->comp_data->shell.surface);
             else
               {
                  evas_object_hide(ec->frame);
                  ec->comp_data->mapped = evas_object_visible_get(ec->frame);
               }
          }
     }
   else
     {
        if (!ec->comp_data->mapped)
          {
             if ((ec->comp_data->shell.surface) && (ec->comp_data->shell.map))
               ec->comp_data->shell.map(ec->comp_data->shell.surface);
             else
               {
                  evas_object_show(ec->frame);
                  ec->comp_data->mapped = evas_object_visible_get(ec->frame);
               }
          }
     }

   if (state->new_attach)
     {
        if ((ec->comp_data->shell.surface) && (ec->comp_data->shell.configure))
          ec->comp_data->shell.configure(ec->comp_data->shell.surface,
                                         x, y, state->bw, state->bh);
        else
          e_client_util_move_resize_without_frame(ec, x, y, state->bw, state->bh);

        if (ec->new_client)
          ec->placed = placed;
        else if ((first) && (ec->placed) && (!ec->internal))
          {
             ec->x = ec->y = 0;
             ec->placed = EINA_FALSE;
             ec->new_client = EINA_TRUE;
          }
     }
   state->sx = 0;
   state->sy = 0;
   state->new_attach = EINA_FALSE;

   if (!ec->comp_data->mapped) goto unmapped;

   /* put state damages into surface */
   if ((!e_comp->nocomp) && (ec->frame))
     {
        EINA_LIST_FREE(state->damages, dmg)
          {
             e_comp_object_damage(ec->frame, dmg->x, dmg->y, dmg->w, dmg->h);
             eina_rectangle_free(dmg);
          }
     }

   /* put state opaque into surface */
   if (state->opaque)
     {
        Eina_Rectangle *rect;
        Eina_Iterator *itr;

        itr = eina_tiler_iterator_new(state->opaque);
        EINA_ITERATOR_FOREACH(itr, rect)
          {
             e_pixmap_image_opaque_set(ec->pixmap, rect->x, rect->y,
                                       rect->w, rect->h);
             break;
          }

        eina_iterator_free(itr);
     }
   else
     e_pixmap_image_opaque_set(ec->pixmap, 0, 0, 0, 0);

   /* put state input into surface */
   if (state->input)
     {
        Eina_Tiler *src, *tmp;

        tmp = eina_tiler_new(state->bw, state->bh);
        eina_tiler_tile_size_set(tmp, 1, 1);
        eina_tiler_rect_add(tmp,
                            &(Eina_Rectangle){0, 0, state->bw, state->bh});
        if ((src = eina_tiler_intersection(state->input, tmp)))
          {
             Eina_Rectangle *rect;
             Eina_Iterator *itr;

             itr = eina_tiler_iterator_new(src);
             EINA_ITERATOR_FOREACH(itr, rect)
               e_comp_object_input_area_set(ec->frame, rect->x, rect->y,
                                            rect->w, rect->h);

             eina_iterator_free(itr);
             eina_tiler_free(src);
          }
        else
          e_comp_object_input_area_set(ec->frame, 0, 0, ec->w, ec->h);

        eina_tiler_free(tmp);
     }

   /* insert state frame callbacks into comp_data->frames
    * NB: This clears state->frames list */
   ec->comp_data->frames = eina_list_merge(ec->comp_data->frames, state->frames);
   state->frames = NULL;

   return;

unmapped:
   /* clear pending damages */
   EINA_LIST_FREE(state->damages, dmg)
     eina_rectangle_free(dmg);

   /* clear input tiler */
   if (state->input)
     eina_tiler_clear(state->input);
}

static void
_e_comp_wl_surface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   DBG("Surface Cb Destroy: %d", wl_resource_get_id(resource));
   wl_resource_destroy(resource);
}

static void
_e_comp_wl_surface_cb_attach(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *buffer_resource, int32_t sx, int32_t sy)
{
   E_Client *ec;
   E_Comp_Wl_Buffer *buffer = NULL;

   if (!(ec = wl_resource_get_user_data(resource))) return;
   if (e_object_is_del(E_OBJECT(ec))) return;

   if (buffer_resource)
     {
        if (!(buffer = e_comp_wl_buffer_get(buffer_resource)))
          {
             ERR("Could not get buffer from resource");
             wl_client_post_no_memory(client);
             return;
          }
     }

   _e_comp_wl_surface_state_buffer_set(&ec->comp_data->pending, buffer);

   ec->comp_data->pending.sx = sx;
   ec->comp_data->pending.sy = sy;
   ec->comp_data->pending.new_attach = EINA_TRUE;
}

static void
_e_comp_wl_surface_cb_damage(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int32_t x, int32_t y, int32_t w, int32_t h)
{
   E_Client *ec;
   Eina_Rectangle *dmg = NULL;

   if (!(ec = wl_resource_get_user_data(resource))) return;
   if (e_object_is_del(E_OBJECT(ec))) return;

   if (!(dmg = eina_rectangle_new(x, y, w, h))) return;

   ec->comp_data->pending.damages =
     eina_list_append(ec->comp_data->pending.damages, dmg);
}

static void
_e_comp_wl_frame_cb_destroy(struct wl_resource *resource)
{
   E_Client *ec;

   if (!(ec = wl_resource_get_user_data(resource))) return;
   if (e_object_is_del(E_OBJECT(ec))) return;

   ec->comp_data->frames =
     eina_list_remove(ec->comp_data->frames, resource);
}

static void
_e_comp_wl_surface_cb_frame(struct wl_client *client, struct wl_resource *resource, uint32_t callback)
{
   E_Client *ec;
   struct wl_resource *res;

   if (!(ec = wl_resource_get_user_data(resource))) return;
   if (e_object_is_del(E_OBJECT(ec))) return;

   /* create frame callback */
   if (!(res =
         wl_resource_create(client, &wl_callback_interface, 1, callback)))
     {
        wl_resource_post_no_memory(resource);
        return;
     }

   wl_resource_set_implementation(res, NULL, ec, _e_comp_wl_frame_cb_destroy);

   ec->comp_data->pending.frames =
     eina_list_prepend(ec->comp_data->pending.frames, res);
}

static void
_e_comp_wl_surface_cb_opaque_region_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *region_resource)
{
   E_Client *ec;

   if (!(ec = wl_resource_get_user_data(resource))) return;
   if (e_object_is_del(E_OBJECT(ec))) return;

   if (region_resource)
     {
        Eina_Tiler *tmp;

        if (!(tmp = wl_resource_get_user_data(region_resource)))
          return;

        eina_tiler_union(ec->comp_data->pending.opaque, tmp);
     }
   else
     {
        if (ec->comp_data->pending.opaque)
          {
             eina_tiler_clear(ec->comp_data->pending.opaque);
             /* eina_tiler_free(ec->comp_data->pending.opaque); */
          }
     }
}

static void
_e_comp_wl_surface_cb_input_region_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *region_resource)
{
   E_Client *ec;

   if (!(ec = wl_resource_get_user_data(resource))) return;
   if (e_object_is_del(E_OBJECT(ec))) return;

   if (region_resource)
     {
        Eina_Tiler *tmp;

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
   E_Client *ec, *subc;
   Eina_List *l;

   if (!(ec = wl_resource_get_user_data(resource))) return;
   if (e_object_is_del(E_OBJECT(ec))) return;

   if (e_comp_wl_subsurface_commit(ec)) return;

   e_comp_wl_surface_commit(ec);

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
_e_comp_wl_surface_render_stop(E_Client *ec)
{
   /* FIXME: this may be fine after e_pixmap can create textures for wl clients? */
   //if ((!ec->internal) && (!e_comp_gl_get()))
     ec->dead = ec->hidden = 1;
   evas_object_hide(ec->frame);
}

static void
_e_comp_wl_surface_destroy(struct wl_resource *resource)
{
   E_Client *ec;

   if (!(ec = wl_resource_get_user_data(resource))) return;

   _e_comp_wl_surface_render_stop(ec);
   e_object_del(E_OBJECT(ec));
}

static void
_e_comp_wl_compositor_cb_surface_create(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
   struct wl_resource *res;
   E_Client *ec = NULL;
   pid_t pid;

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
   if (pid == getpid()) //internal!
     ec = e_pixmap_find_client(E_PIXMAP_TYPE_WL, (uintptr_t)id);
   if (!ec)
     {
        E_Pixmap *ep;

        /* try to create new pixmap */
        if (!(ep = e_pixmap_new(E_PIXMAP_TYPE_WL, res)))
          {
             ERR("Could not create new pixmap");
             wl_resource_destroy(res);
             wl_client_post_no_memory(client);
             return;
          }
        DBG("\tUsing Pixmap: %p", ep);

        if ((ec = e_client_new(ep, 0, 0)))
          {
             ec->new_client = 0;
             e_comp->new_clients--;
             ec->client.w = ec->client.h = 1;
             ec->ignored = 1;
             ec->comp_data->surface = res;
          }
     }

   /* set reference to pixmap so we can fetch it later */
   wl_resource_set_user_data(res, ec);

   /* emit surface create signal */
   wl_signal_emit(&e_comp->wl_comp_data->signals.surface.create, res);
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
   Eina_Tiler *tiler;
   struct wl_resource *res;

   DBG("Region Create: %d", wl_resource_get_id(resource));

   /* try to create new tiler */
   if (!(tiler = eina_tiler_new(e_comp->w, e_comp->h)))
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
_e_comp_wl_compositor_cb_bind(struct wl_client *client, void *data EINA_UNUSED, uint32_t version, uint32_t id)
{
   struct wl_resource *res;

   if (!(res =
         wl_resource_create(client, &wl_compositor_interface,
                            MIN(version, COMPOSITOR_VERSION), id)))
     {
        ERR("Could not create compositor resource: %m");
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res, &_e_comp_interface, e_comp, NULL);
}

static void
_e_comp_wl_compositor_cb_del(void *data EINA_UNUSED)
{
   E_Comp_Wl_Data *cdata;
   E_Comp_Wl_Output *output;

   cdata = e_comp->wl_comp_data;

   EINA_LIST_FREE(cdata->outputs, output)
     {
        if (output->id) eina_stringshare_del(output->id);
        if (output->make) eina_stringshare_del(output->make);
        if (output->model) eina_stringshare_del(output->model);
        free(output);
     }

   /* delete fd handler */
   if (cdata->fd_hdlr) ecore_main_fd_handler_del(cdata->fd_hdlr);

   /* free allocated data structure */
   free(cdata);
}

static void
_e_comp_wl_subsurface_destroy(struct wl_resource *resource)
{
   E_Client *ec;
   E_Comp_Wl_Subsurf_Data *sdata;

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

   _e_comp_wl_surface_state_finish(&sdata->cached);
   e_comp_wl_buffer_reference(&sdata->cached_buffer_ref, NULL);

   /* the client is getting deleted, which means the pixmap will be getting
    * freed. We need to unset the surface user data */
   /* wl_resource_set_user_data(ec->comp_data->surface, NULL); */

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
   struct wl_resource *cb;
   Eina_List *l;
   Eina_Iterator *itr;
   Eina_Rectangle *rect;

   if (!(cdata = ec->comp_data)) return;
   if (!(sdata = cdata->sub.data)) return;

   DBG("Subsurface Commit to Cache");

   /* move pending damage to cached */
   EINA_LIST_FOREACH(cdata->pending.damages, l, rect)
     eina_list_move(&sdata->cached.damages, &cdata->pending.damages, rect);

   if (cdata->pending.new_attach)
     {
        sdata->cached.new_attach = EINA_TRUE;
        _e_comp_wl_surface_state_buffer_set(&sdata->cached,
                                            cdata->pending.buffer);
        e_comp_wl_buffer_reference(&sdata->cached_buffer_ref,
                                   cdata->pending.buffer);
     }

   sdata->cached.sx = cdata->pending.sx;
   sdata->cached.sy = cdata->pending.sy;
   /* sdata->cached.buffer = cdata->pending.buffer; */
   sdata->cached.new_attach = cdata->pending.new_attach;

   /* _e_comp_wl_surface_state_buffer_set(&cdata->pending, NULL); */
   /* cdata->pending.sx = 0; */
   /* cdata->pending.sy = 0; */
   /* cdata->pending.new_attach = EINA_FALSE; */

   /* copy cdata->pending.opaque into sdata->cached.opaque */
   itr = eina_tiler_iterator_new(cdata->pending.opaque);
   EINA_ITERATOR_FOREACH(itr, rect)
     eina_tiler_rect_add(sdata->cached.opaque, rect);
   eina_iterator_free(itr);

   /* repeat for input */
   itr = eina_tiler_iterator_new(cdata->pending.input);
   EINA_ITERATOR_FOREACH(itr, rect)
     eina_tiler_rect_add(sdata->cached.input, rect);
   eina_iterator_free(itr);

   EINA_LIST_FOREACH(cdata->pending.frames, l, cb)
     eina_list_move(&sdata->cached.frames, &cdata->pending.frames, cb);

   sdata->cached.has_data = EINA_TRUE;
}

static void
_e_comp_wl_subsurface_commit_from_cache(E_Client *ec)
{
   E_Comp_Client_Data *cdata;
   E_Comp_Wl_Subsurf_Data *sdata;

   if (!(cdata = ec->comp_data)) return;
   if (!(sdata = cdata->sub.data)) return;

   DBG("Subsurface Commit from Cache");

   _e_comp_wl_surface_state_commit(ec, &sdata->cached);

   e_comp_wl_buffer_reference(&sdata->cached_buffer_ref, NULL);

   /* schedule repaint */
   if (e_pixmap_refresh(ec->pixmap))
     {
        e_comp->post_updates = eina_list_append(e_comp->post_updates, ec);
        e_object_ref(E_OBJECT(ec));
     }
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

   _e_comp_wl_surface_state_init(&sdata->cached, ec->w, ec->h);

   /* set subsurface data properties */
   sdata->cached_buffer_ref.buffer = NULL;
   sdata->resource = res;
   sdata->synchronized = EINA_TRUE;
   sdata->parent = epc;

   /* set subsurface client properties */
   ec->borderless = EINA_TRUE;
   ec->argb = EINA_TRUE;
   ec->lock_border = EINA_TRUE;
   ec->lock_focus_in = ec->lock_focus_out = EINA_TRUE;
   ec->netwm.state.skip_taskbar = EINA_TRUE;
   ec->netwm.state.skip_pager = EINA_TRUE;
   ec->no_shape_cut = EINA_TRUE;
   ec->border_size = 0;

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
}

static void
_e_comp_wl_subcompositor_cb_subsurface_get(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, uint32_t id, struct wl_resource *surface_resource, struct wl_resource *parent_resource)
{
   E_Client *ec, *epc = NULL;
   static const char where[] = "get_subsurface: wl_subsurface@";

   if (!(ec = wl_resource_get_user_data(surface_resource))) return;
   if (!(epc = wl_resource_get_user_data(parent_resource))) return;

   if (ec == epc)
     {
        wl_resource_post_error(resource, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                               "%s%d: wl_surface@%d cannot be its own parent",
                               where, id, wl_resource_get_id(surface_resource));
        return;
     }

   if (e_object_is_del(E_OBJECT(ec))) return;
   if (e_object_is_del(E_OBJECT(epc))) return;

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
_e_comp_wl_subcompositor_cb_bind(struct wl_client *client, void *data EINA_UNUSED, uint32_t version, uint32_t id)
{
   struct wl_resource *res;

   if (!(res =
         wl_resource_create(client, &wl_subcompositor_interface,
                            MIN(version, 1), id)))
     {
        ERR("Could not create subcompositor resource: %m");
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res, &_e_subcomp_interface, e_comp, NULL);

   /* TODO: add handlers for client iconify/uniconify */
}

static void
_e_comp_wl_client_cb_new(void *data EINA_UNUSED, E_Client *ec)
{
   uint64_t win;

   /* make sure this is a wayland client */
   if (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL) return;

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

   wl_signal_init(&ec->comp_data->destroy_signal);

   _e_comp_wl_surface_state_init(&ec->comp_data->pending, ec->w, ec->h);

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
   /* Eina_Rectangle *dmg; */
   struct wl_resource *cb;
   E_Client *subc;

   /* make sure this is a wayland client */
   if (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL) return;

   if ((!ec->already_unparented) && (ec->comp_data->reparented))
     _e_comp_wl_focus_down_set(ec);

   ec->already_unparented = EINA_TRUE;
   if (ec->comp_data->reparented)
     {
        /* reset pixmap parent window */
        e_pixmap_parent_window_set(ec->pixmap, 0);
     }

   /* remove sub list */
   EINA_LIST_FREE(ec->comp_data->sub.list, subc)
     subc->comp_data->sub.data->parent = NULL;

   if ((ec->parent) && (ec->parent->modal == ec))
     {
        ec->parent->lock_close = EINA_FALSE;
        ec->parent->modal = NULL;
     }

   wl_signal_emit(&ec->comp_data->destroy_signal, &ec->comp_data->surface);

   _e_comp_wl_surface_state_finish(&ec->comp_data->pending);

   e_comp_wl_buffer_reference(&ec->comp_data->buffer_ref, NULL);

   EINA_LIST_FREE(ec->comp_data->frames, cb)
     wl_resource_destroy(cb);

   if (ec->comp_data->surface)
     wl_resource_set_user_data(ec->comp_data->surface, NULL);

   E_FREE(ec->comp_data);
   if (ec->internal_elm_win)
     _e_comp_wl_surface_render_stop(ec);
   _e_comp_wl_focus_check();
}

#if 0
static void
_e_comp_wl_client_cb_pre_frame(void *data EINA_UNUSED, E_Client *ec)
{
   uint64_t parent;

   if (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL) return;
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
#endif

static void
_e_comp_wl_client_cb_focus_set(void *data EINA_UNUSED, E_Client *ec)
{
   if (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL) return;

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

   if (e_comp->wl_comp_data->kbd.focus != ec->comp_data->surface)
     {
        e_comp->wl_comp_data->kbd.focus = ec->comp_data->surface;
        e_comp_wl_data_device_keyboard_focus_set();
     }
}

static void
_e_comp_wl_client_cb_focus_unset(void *data EINA_UNUSED, E_Client *ec)
{
   if (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL) return;

   /* send configure */
   if (ec->comp_data->shell.configure_send)
     {
        if (ec->comp_data->shell.surface)
          ec->comp_data->shell.configure_send(ec->comp_data->shell.surface,
                                              0, 0, 0);
     }

   _e_comp_wl_focus_check();

   if (e_comp->wl_comp_data->kbd.focus == ec->comp_data->surface)
     e_comp->wl_comp_data->kbd.focus = NULL;
}

static void
_e_comp_wl_client_cb_resize_begin(void *data EINA_UNUSED, E_Client *ec)
{
   if (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL) return;

   switch (ec->resize_mode)
     {
      case E_POINTER_RESIZE_T: // 1
        e_comp->wl_comp_data->resize.edges = 1;
        break;
      case E_POINTER_RESIZE_B: // 2
        e_comp->wl_comp_data->resize.edges = 2;
        break;
      case E_POINTER_RESIZE_L: // 4
        e_comp->wl_comp_data->resize.edges = 4;
        break;
      case E_POINTER_RESIZE_R: // 8
        e_comp->wl_comp_data->resize.edges = 8;
        break;
      case E_POINTER_RESIZE_TL: // 5
        e_comp->wl_comp_data->resize.edges = 5;
        break;
      case E_POINTER_RESIZE_TR: // 9
        e_comp->wl_comp_data->resize.edges = 9;
        break;
      case E_POINTER_RESIZE_BL: // 6
        e_comp->wl_comp_data->resize.edges = 6;
        break;
      case E_POINTER_RESIZE_BR: // 10
        e_comp->wl_comp_data->resize.edges = 10;
        break;
      default:
        e_comp->wl_comp_data->resize.edges = 0;
        break;
     }
}

static void
_e_comp_wl_client_cb_resize_end(void *data EINA_UNUSED, E_Client *ec)
{
   if (e_object_is_del(E_OBJECT(ec))) return;
   if (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL) return;

   e_comp->wl_comp_data->resize.edges = 0;
   e_comp->wl_comp_data->resize.resource = NULL;

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
   E_Comp_Wl_Output *output;

   if (!(output = wl_resource_get_user_data(resource))) return;

   output->resources = eina_list_remove(output->resources, resource);
}

static void
_e_comp_wl_cb_output_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
   E_Comp_Wl_Output *output;
   struct wl_resource *resource;

   if (!(output = data)) return;

   resource =
     wl_resource_create(client, &wl_output_interface, MIN(version, 2), id);
   if (!resource)
     {
        wl_client_post_no_memory(client);
        return;
     }

   DBG("Bound Output: %s", output->id);
   DBG("\tGeom: %d %d %d %d", output->x, output->y, output->w, output->h);

   output->resources = eina_list_append(output->resources, resource);

   wl_resource_set_implementation(resource, NULL, output,
                                  _e_comp_wl_cb_output_unbind);
   wl_resource_set_user_data(resource, output);

   wl_output_send_geometry(resource, output->x, output->y,
                           output->phys_width, output->phys_height,
                           output->subpixel, output->make ?: "", output->model ?: "",
                           output->transform);

   if (version >= WL_OUTPUT_SCALE_SINCE_VERSION)
     wl_output_send_scale(resource, output->scale);

   /* 3 == preferred + current */
   wl_output_send_mode(resource, 3, output->w, output->h, output->refresh);

   if (version >= WL_OUTPUT_DONE_SINCE_VERSION)
     wl_output_send_done(resource);
}

static Eina_Bool
_e_comp_wl_compositor_create(void)
{
   E_Comp_Wl_Data *cdata;
   const char *name;
   int fd = 0;

   /* check for existing compositor. create if needed */
   if (e_comp->comp_type == E_PIXMAP_TYPE_NONE)
     E_OBJECT_DEL_SET(e_comp, _e_comp_wl_compositor_cb_del);

   /* create new compositor data */
   if (!(cdata = E_NEW(E_Comp_Wl_Data, 1)))
     {
       ERR("Could not create compositor data: %m");
       return EINA_FALSE;
     }

   /* set compositor wayland data */
   e_comp->wl_comp_data = cdata;

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

   /* cdata->output.transform = WL_OUTPUT_TRANSFORM_NORMAL; */
   /* cdata->output.scale = e_scale; */

   /* try to add compositor to wayland globals */
   if (!wl_global_create(cdata->wl.disp, &wl_compositor_interface,
                         COMPOSITOR_VERSION, e_comp,
                         _e_comp_wl_compositor_cb_bind))
     {
        ERR("Could not add compositor to wayland globals: %m");
        goto comp_global_err;
     }

   /* try to add subcompositor to wayland globals */
   if (!wl_global_create(cdata->wl.disp, &wl_subcompositor_interface, 1,
                         e_comp, _e_comp_wl_subcompositor_cb_bind))
     {
        ERR("Could not add subcompositor to wayland globals: %m");
        goto comp_global_err;
     }

   /* _e_comp_wl_cb_randr_change(NULL, 0, NULL); */

   /* try to init data manager */
   if (!e_comp_wl_data_manager_init())
     {
        ERR("Could not initialize data manager");
        goto data_err;
     }

   /* try to init input */
   if (!e_comp_wl_input_init())
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
        e_comp_wl_input_keymap_set(rules, model, layout);
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

   if (e_comp->comp_type == E_PIXMAP_TYPE_X)
     {
        e_comp_wl_input_pointer_enabled_set(EINA_TRUE);
        e_comp_wl_input_keyboard_enabled_set(EINA_TRUE);
        e_comp_wl_input_touch_enabled_set(EINA_TRUE);
     }

   return EINA_TRUE;

input_err:
   e_comp_wl_data_manager_shutdown();
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

/**
 * Creates and initializes a Wayland compositor with ecore.
 * Registers callback handlers for keyboard and mouse activity
 * and other client events.
 *
 * @returns true on success, false if initialization failed.
 */
E_API Eina_Bool
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

   ecore_wl_server_mode_set(1);

   /* try to init ecore_wayland */
   if (!ecore_wl_init(NULL))
     {
        e_error_message_show(_("Enlightenment cannot initialize Ecore_Wayland!\n"));
        return EINA_FALSE;
     }

   /* create hash to store clients */
   /* clients_win_hash = eina_hash_int64_new(NULL); */

   /* add event handlers to catch E events */
   if (e_comp->comp_type != E_PIXMAP_TYPE_X)
     if (e_randr2_init())
       e_randr2_screens_setup(-1, -1);

   E_LIST_HANDLER_APPEND(handlers, E_EVENT_RANDR_CHANGE,
                                _e_comp_wl_cb_randr_change, NULL);

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

/**
 * Get the signal that is fired for the creation of a Wayland surface.
 *
 * @returns the corresponding Wayland signal
 */
E_API struct wl_signal
e_comp_wl_surface_create_signal_get(void)
{
   return e_comp->wl_comp_data->signals.surface.create;
}

/* internal functions */
EINTERN void
e_comp_wl_shutdown(void)
{
#ifndef HAVE_WAYLAND_ONLY
   _e_comp_wl_compositor_cb_del(e_comp);
#endif

   /* free handlers */
   E_FREE_LIST(handlers, ecore_event_handler_del);

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

EINTERN void
e_comp_wl_surface_attach(E_Client *ec, E_Comp_Wl_Buffer *buffer)
{
   e_comp_wl_buffer_reference(&ec->comp_data->buffer_ref, buffer);

   /* set usable early because shell module checks this */
   e_pixmap_usable_set(ec->pixmap, (buffer != NULL));
   e_pixmap_resource_set(ec->pixmap, buffer);
   e_pixmap_dirty(ec->pixmap);

   _e_comp_wl_surface_state_size_update(ec, &ec->comp_data->pending);
}

EINTERN Eina_Bool
e_comp_wl_surface_commit(E_Client *ec)
{
   _e_comp_wl_surface_state_commit(ec, &ec->comp_data->pending);

   /* schedule repaint */
   if (e_pixmap_refresh(ec->pixmap))
     {
        e_comp->post_updates = eina_list_append(e_comp->post_updates, ec);
        e_object_ref(E_OBJECT(ec));
     }

   if (!e_pixmap_usable_get(ec->pixmap))
     {
        if (ec->comp_data->mapped)
          {
             if ((ec->comp_data->shell.surface) && (ec->comp_data->shell.unmap))
               ec->comp_data->shell.unmap(ec->comp_data->shell.surface);
             else
               {
                  evas_object_hide(ec->frame);
                  ec->comp_data->mapped = evas_object_visible_get(ec->frame);
               }
          }
     }
   else
     {
        if (!ec->comp_data->mapped)
          {
             if ((ec->comp_data->shell.surface) && (ec->comp_data->shell.map))
               ec->comp_data->shell.map(ec->comp_data->shell.surface);
             else
               {
                  evas_object_show(ec->frame);
                  ec->comp_data->mapped = evas_object_visible_get(ec->frame);
               }
          }
     }
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

EINTERN void
e_comp_wl_buffer_reference(E_Comp_Wl_Buffer_Ref *ref, E_Comp_Wl_Buffer *buffer)
{
   if ((ref->buffer) && (buffer != ref->buffer))
     {
        ref->buffer->busy--;
        if (ref->buffer->busy == 0)
          {
             if (!wl_resource_get_client(ref->buffer->resource)) return;
             wl_resource_queue_event(ref->buffer->resource, WL_BUFFER_RELEASE);
          }
        wl_list_remove(&ref->destroy_listener.link);
     }

   if ((buffer) && (buffer != ref->buffer))
     {
        buffer->busy++;
        wl_signal_add(&buffer->destroy_signal, &ref->destroy_listener);
     }

   ref->buffer = buffer;
   ref->destroy_listener.notify = _e_comp_wl_buffer_reference_cb_destroy;
}

/**
 * Get the buffer for a given resource.
 *
 * Retrieves the Wayland SHM buffer for the resource and
 * uses it to create a new E_Comp_Wl_Buffer object. This
 * buffer will be freed when the resource is destroyed.
 *
 * @param resource that owns the desired buffer
 * @returns a new E_Comp_Wl_Buffer object
 */
E_API E_Comp_Wl_Buffer *
e_comp_wl_buffer_get(struct wl_resource *resource)
{
   E_Comp_Wl_Buffer *buffer;
   struct wl_listener *listener;
   struct wl_shm_buffer *shmbuff;

   listener =
     wl_resource_get_destroy_listener(resource, _e_comp_wl_buffer_cb_destroy);
   if (listener)
     return container_of(listener, E_Comp_Wl_Buffer, destroy_listener);

   if (!(shmbuff = wl_shm_buffer_get(resource))) return NULL;
   if (!(buffer = E_NEW(E_Comp_Wl_Buffer, 1))) return NULL;

   buffer->w = wl_shm_buffer_get_width(shmbuff);
   buffer->h = wl_shm_buffer_get_height(shmbuff);

   buffer->resource = resource;
   wl_signal_init(&buffer->destroy_signal);
   buffer->destroy_listener.notify = _e_comp_wl_buffer_cb_destroy;
   wl_resource_add_destroy_listener(resource, &buffer->destroy_listener);

   return buffer;
}

/**
 * Computes the time since the last input event.
 *
 * @returns time in seconds.
 */
E_API double
e_comp_wl_idle_time_get(void)
{
   return (ecore_loop_time_get() - _last_event_time);
}

static E_Comp_Wl_Output *
_e_comp_wl_output_get(Eina_List *outputs, const char *id)
{
   Eina_List *l;
   E_Comp_Wl_Output *output;

   EINA_LIST_FOREACH(outputs, l, output)
     {
       if (!strcmp(output->id, id))
         return output;
     }

   return NULL;
}

/**
 * Initializes information about one display output.
 *
 * Adds or updates the given data about a single display output,
 * with an id matching the provided id.
 *
 * @param id         identification of output to be added or changed
 * @param make       manufacturer name of the display output
 * @param model      model name of the display output
 * @param x          output's top left corner x coordinate
 * @param y          output's top left corner y coordinate
 * @param w          output's width in pixels
 * @param h          output's height in pixels
 * @param pw         output's physical width in millimeters
 * @param ph         output's physical height in millimeters
 * @param refresh    output's refresh rate in Hz
 * @param subpixel   output's subpixel layout
 * @param transform  output's rotation and/or mirror transformation
 *
 * @returns True if a display output object could be added or updated
 */
E_API Eina_Bool
e_comp_wl_output_init(const char *id, const char *make, const char *model,
                      int x, int y, int w, int h, int pw, int ph,
                      unsigned int refresh, unsigned int subpixel,
                      unsigned int transform)
{
   E_Comp_Wl_Data *cdata;
   E_Comp_Wl_Output *output;
   Eina_List *l2;
   struct wl_resource *resource;

   if (!(cdata = e_comp->wl_comp_data)) return EINA_FALSE;

   /* retrieve named output; or create it if it doesn't exist */
   output = _e_comp_wl_output_get(cdata->outputs, id);
   if (!output)
     {
        if (!(output = E_NEW(E_Comp_Wl_Output, 1))) return EINA_FALSE;

        if (id) output->id = eina_stringshare_add(id);
        if (make) output->make = eina_stringshare_add(make);
        if (model) output->model = eina_stringshare_add(model);

        cdata->outputs = eina_list_append(cdata->outputs, output);

        output->global = 
          wl_global_create(cdata->wl.disp, &wl_output_interface,
                           2, output, _e_comp_wl_cb_output_bind);

        output->resources = NULL;
        output->scale = e_scale;
     }

   /* update the output details */
   output->x = x;
   output->y = y;
   output->w = w;
   output->h = h;
   output->phys_width = pw;
   output->phys_height = ph;
   output->refresh = refresh * 1000;
   output->subpixel = subpixel;
   output->transform = transform;

   if (output->scale <= 0)
     output->scale = e_scale;

   /* if we have bound resources, send updates */
   EINA_LIST_FOREACH(output->resources, l2, resource)
     {
        wl_output_send_geometry(resource,
                                output->x, output->y,
                                output->phys_width,
                                output->phys_height,
                                output->subpixel,
                                output->make ?: "", output->model ?: "",
                                output->transform);

        if (wl_resource_get_version(resource) >= WL_OUTPUT_SCALE_SINCE_VERSION)
          wl_output_send_scale(resource, output->scale);

        /* 3 == preferred + current */
        wl_output_send_mode(resource, 3, output->w, output->h, output->refresh);

        if (wl_resource_get_version(resource) >= WL_OUTPUT_DONE_SINCE_VERSION)
          wl_output_send_done(resource);
     }

   return EINA_TRUE;
}

E_API void
e_comp_wl_output_remove(const char *id)
{
   E_Comp_Wl_Data *cdata;
   E_Comp_Wl_Output *output;

   if (!(cdata = e_comp->wl_comp_data)) return;

   output = _e_comp_wl_output_get(cdata->outputs, id);
   if (output)
     {
        cdata->outputs = eina_list_remove(cdata->outputs, output);

        /* wl_global_destroy(output->global); */

        /* eina_stringshare_del(output->id); */
        /* eina_stringshare_del(output->make); */
        /* eina_stringshare_del(output->model); */

        /* free(output); */
     }
}

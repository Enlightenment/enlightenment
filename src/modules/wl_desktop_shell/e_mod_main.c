#define EXECUTIVE_MODE_ENABLED
#define E_COMP_WL
#include "e.h"
#include "e_desktop_shell_protocol.h"

#define XDG_SERVER_VERSION 5

static void
_e_shell_surface_parent_set(E_Client *ec, struct wl_resource *parent_resource)
{
   E_Client *pc;
   uint64_t pwin = 0;

   if (!parent_resource)
     {
        ec->icccm.fetch.transient_for = EINA_FALSE;
        ec->icccm.transient_for = 0;
        if (ec->parent)
          {
             ec->parent->transients =
                eina_list_remove(ec->parent->transients, ec);
             if (ec->parent->modal == ec) ec->parent->modal = NULL;
             ec->parent = NULL;
          }
        return;
     }
   else if (!(pc = wl_resource_get_user_data(parent_resource)))
     {
        ERR("Could not get parent resource client");
        return;
     }

   pwin = e_pixmap_window_get(pc->pixmap);

   e_pixmap_parent_window_set(ec->pixmap, pwin);

   /* If we already have a parent, remove it */
   if (ec->parent)
     {
        if (pc != ec->parent)
          {
             ec->parent->transients =
                eina_list_remove(ec->parent->transients, ec);
             if (ec->parent->modal == ec) ec->parent->modal = NULL;
             ec->parent = NULL;
          }
        else
          pc = NULL;
     }

   if ((pc != ec) &&
       (eina_list_data_find(pc->transients, ec) != ec))
     {
        pc->transients = eina_list_append(pc->transients, ec);
        ec->parent = pc;
     }

   ec->icccm.fetch.transient_for = EINA_TRUE;
   ec->icccm.transient_for = pwin;
}

static void
_e_shell_surface_mouse_down_helper(E_Client *ec, E_Binding_Event_Mouse_Button *ev, Eina_Bool move)
{
   if (move)
     {
        /* tell E to start moving the client */
        e_client_act_move_begin(ec, ev);

        /* we have to get a reference to the window_move action here, or else
         * when e_client stops the move we will never get notified */
        ec->cur_mouse_action = e_action_find("window_move");
        if (ec->cur_mouse_action)
          e_object_ref(E_OBJECT(ec->cur_mouse_action));
     }
   else
     {
        /* tell E to start resizing the client */
        e_client_act_resize_begin(ec, ev);

        /* we have to get a reference to the window_resize action here,
         * or else when e_client stops the resize we will never get notified */
        ec->cur_mouse_action = e_action_find("window_resize");
        if (ec->cur_mouse_action)
          e_object_ref(E_OBJECT(ec->cur_mouse_action));
     }

   e_focus_event_mouse_down(ec);
}

static void
_e_shell_surface_destroy(struct wl_resource *resource)
{
   E_Client *ec;

   /* DBG("Shell Surface Destroy: %d", wl_resource_get_id(resource)); */

   /* get the client for this resource */
   if ((ec = wl_resource_get_user_data(resource)))
     {
        if (e_object_is_del(E_OBJECT(ec))) return;

        if (ec->comp_data)
          {
             if (ec->comp_data->mapped)
               {
                  if ((ec->comp_data->shell.surface) &&
                      (ec->comp_data->shell.unmap))
                    ec->comp_data->shell.unmap(ec->comp_data->shell.surface);
               }
             if (ec->parent)
               {
                  ec->parent->transients =
                    eina_list_remove(ec->parent->transients, ec);
               }
             /* wl_resource_destroy(ec->comp_data->shell.surface); */
             ec->comp_data->shell.surface = NULL;
          }
     }
}

static void
_e_shell_surface_cb_destroy(struct wl_resource *resource)
{
   /* DBG("Shell Surface Destroy: %d", wl_resource_get_id(resource)); */

   _e_shell_surface_destroy(resource);
}

static void
_e_shell_surface_cb_pong(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, uint32_t serial EINA_UNUSED)
{
   E_Client *ec;

   if ((ec = wl_resource_get_user_data(resource)))
     {
        ec->ping_ok = EINA_TRUE;
        ec->hung = EINA_FALSE;
     }
}

static void
_e_shell_surface_cb_move(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *seat_resource EINA_UNUSED, uint32_t serial EINA_UNUSED)
{
   E_Client *ec;
   E_Comp_Data *cdata;
   E_Binding_Event_Mouse_Button ev;

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   if ((ec->maximized) || (ec->fullscreen)) return;

   cdata = e_comp->wl_comp_data;
   switch (cdata->ptr.button)
     {
      case BTN_LEFT:
        ev.button = 1;
        break;
      case BTN_MIDDLE:
        ev.button = 2;
        break;
      case BTN_RIGHT:
        ev.button = 3;
        break;
      default:
        ev.button = cdata->ptr.button;
        break;
     }

   e_comp_object_frame_xy_unadjust(ec->frame,
                                   wl_fixed_to_int(cdata->ptr.x),
                                   wl_fixed_to_int(cdata->ptr.y),
                                   &ev.canvas.x, &ev.canvas.y);

   _e_shell_surface_mouse_down_helper(ec, &ev, EINA_TRUE);
}

static void
_e_shell_surface_cb_resize(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *seat_resource EINA_UNUSED, uint32_t serial EINA_UNUSED, uint32_t edges)
{
   E_Client *ec;
   E_Comp_Data *cdata;
   E_Binding_Event_Mouse_Button ev;

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   if ((edges == 0) || (edges > 15) ||
       ((edges & 3) == 3) || ((edges & 12) == 12)) return;

   if ((ec->maximized) || (ec->fullscreen)) return;

   cdata = e_comp->wl_comp_data;

   DBG("Comp Resize Edges Set: %d", edges);

   cdata->resize.resource = resource;
   cdata->resize.edges = edges;
   cdata->ptr.grab_x = cdata->ptr.x - wl_fixed_from_int(ec->client.x);
   cdata->ptr.grab_y = cdata->ptr.y - wl_fixed_from_int(ec->client.y);

   switch (cdata->ptr.button)
     {
      case BTN_LEFT:
        ev.button = 1;
        break;
      case BTN_MIDDLE:
        ev.button = 2;
        break;
      case BTN_RIGHT:
        ev.button = 3;
        break;
      default:
        ev.button = cdata->ptr.button;
        break;
     }

   e_comp_object_frame_xy_unadjust(ec->frame,
                                   wl_fixed_to_int(cdata->ptr.x),
                                   wl_fixed_to_int(cdata->ptr.y),
                                   &ev.canvas.x, &ev.canvas.y);

   _e_shell_surface_mouse_down_helper(ec, &ev, EINA_FALSE);
}

static void
_e_shell_surface_cb_toplevel_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   E_Client *ec;

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   /* set toplevel client properties */
   if (!ec->internal)
     ec->borderless = !ec->internal;

   ec->lock_border = EINA_TRUE;
   if (!ec->internal)
     ec->border.changed = ec->changes.border = !ec->borderless;
   ec->netwm.type = E_WINDOW_TYPE_NORMAL;
   ec->comp_data->set_win_type = EINA_TRUE;
   if ((!ec->lock_user_maximize) && (ec->maximized))
     e_client_unmaximize(ec, E_MAXIMIZE_BOTH);
   if ((!ec->lock_user_fullscreen) && (ec->fullscreen))
     e_client_unfullscreen(ec);
   EC_CHANGED(ec);
}

static void
_e_shell_surface_cb_transient_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *parent_resource, int32_t x EINA_UNUSED, int32_t y EINA_UNUSED, uint32_t flags EINA_UNUSED)
{
   E_Client *ec;

   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   /* set this client as a transient for parent */
   _e_shell_surface_parent_set(ec, parent_resource);

   EC_CHANGED(ec);
}

static void
_e_shell_surface_cb_fullscreen_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, uint32_t method EINA_UNUSED, uint32_t framerate EINA_UNUSED, struct wl_resource *output_resource EINA_UNUSED)
{
   E_Client *ec;

   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   if (!ec->lock_user_fullscreen)
     e_client_fullscreen(ec, e_config->fullscreen_policy);
}

static void
_e_shell_surface_cb_popup_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *seat_resource EINA_UNUSED, uint32_t serial EINA_UNUSED, struct wl_resource *parent_resource, int32_t x, int32_t y, uint32_t flags EINA_UNUSED)
{
   E_Client *ec;

   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   if (ec->comp_data)
     {
        ec->comp_data->popup.x = x;
        ec->comp_data->popup.y = y;
     }

   if (!ec->internal)
     ec->borderless = !ec->internal_elm_win;
   ec->lock_border = EINA_TRUE;
   if (!ec->internal)
     ec->border.changed = ec->changes.border = !ec->borderless;
   ec->changes.icon = !!ec->icccm.class;
   ec->netwm.type = E_WINDOW_TYPE_POPUP_MENU;
   ec->comp_data->set_win_type = EINA_TRUE;
   ec->layer = E_LAYER_CLIENT_POPUP;

   /* set this client as a transient for parent */
   _e_shell_surface_parent_set(ec, parent_resource);

   EC_CHANGED(ec);
}

static void
_e_shell_surface_cb_maximized_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *output_resource EINA_UNUSED)
{
   E_Client *ec;

   /* DBG("WL_SHELL: Surface Maximize: %d", wl_resource_get_id(resource)); */

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   /* tell E to maximize this client */
   if (!ec->lock_user_maximize)
     {
        unsigned int edges = 0;

        e_client_maximize(ec, ((e_config->maximize_policy & E_MAXIMIZE_TYPE) |
                               E_MAXIMIZE_BOTH));

        edges = (WL_SHELL_SURFACE_RESIZE_TOP | WL_SHELL_SURFACE_RESIZE_LEFT);
        wl_shell_surface_send_configure(resource, edges, ec->w, ec->h);
     }
}

static void
_e_shell_surface_cb_title_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *title)
{
   E_Client *ec;

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   /* set title */
   eina_stringshare_replace(&ec->icccm.title, title);
   if (ec->frame) e_comp_object_frame_title_set(ec->frame, title);
}

static void
_e_shell_surface_cb_class_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *clas)
{
   E_Client *ec;

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   /* use the wl_client to get the pid * and set it in the netwm props */
   wl_client_get_credentials(client, &ec->netwm.pid, NULL, NULL);

   /* set class */
   eina_stringshare_replace(&ec->icccm.class, clas);
   ec->changes.icon = !!ec->icccm.class;
   EC_CHANGED(ec);
}

static const struct wl_shell_surface_interface _e_shell_surface_interface =
{
   _e_shell_surface_cb_pong,
   _e_shell_surface_cb_move,
   _e_shell_surface_cb_resize,
   _e_shell_surface_cb_toplevel_set,
   _e_shell_surface_cb_transient_set,
   _e_shell_surface_cb_fullscreen_set,
   _e_shell_surface_cb_popup_set,
   _e_shell_surface_cb_maximized_set,
   _e_shell_surface_cb_title_set,
   _e_shell_surface_cb_class_set,
};

static void
_e_shell_surface_configure_send(struct wl_resource *resource, uint32_t edges, int32_t width, int32_t height)
{
   wl_shell_surface_send_configure(resource, edges, width, height);
}

static void
_e_shell_surface_configure(struct wl_resource *resource, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
   E_Client *ec;

   /* DBG("WL_SHELL: Surface Configure: %d \t%d %d %d %d",  */
   /*     wl_resource_get_id(resource), x, y, w, h); */

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   if (ec->parent)
     {
        if ((ec->netwm.type == E_WINDOW_TYPE_MENU) ||
            (ec->netwm.type == E_WINDOW_TYPE_POPUP_MENU) ||
            (ec->netwm.type == E_WINDOW_TYPE_DROPDOWN_MENU))
          {
             x = E_CLAMP(ec->parent->client.x + ec->comp_data->popup.x,
                         ec->parent->client.x,
                         ec->parent->client.x +
                         ec->parent->client.w - ec->client.w);
             y = E_CLAMP(ec->parent->client.y + ec->comp_data->popup.y,
                         ec->parent->client.y,
                         ec->parent->client.y +
                         ec->parent->client.h - ec->client.h);
          }
     }

   e_client_util_move_resize_without_frame(ec, x, y, w, h);
}

static void
_e_shell_surface_ping(struct wl_resource *resource)
{
   E_Client *ec;
   uint32_t serial;

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   serial = wl_display_next_serial(e_comp->wl_comp_data->wl.disp);
   wl_shell_surface_send_ping(ec->comp_data->shell.surface, serial);
}

static void
_e_shell_surface_map(struct wl_resource *resource)
{
   E_Client *ec;

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   /* map this surface if needed */
   if ((!ec->comp_data->mapped) && (e_pixmap_usable_get(ec->pixmap)))
     {
        ec->visible = EINA_TRUE;
        evas_object_geometry_set(ec->frame, ec->x, ec->y, ec->w, ec->h);
        evas_object_show(ec->frame);
        ec->comp_data->mapped = EINA_TRUE;
     }
}

static void
_e_shell_surface_unmap(struct wl_resource *resource)
{
   E_Client *ec;

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   if (ec->comp_data->mapped)
     {
        ec->visible = EINA_FALSE;
        evas_object_hide(ec->frame);
        ec->comp_data->mapped = EINA_FALSE;
     }
}

static void
_e_shell_cb_shell_surface_get(struct wl_client *client, struct wl_resource *resource EINA_UNUSED, uint32_t id, struct wl_resource *surface_resource)
{
   E_Client *ec;
   E_Comp_Client_Data *cdata;

   /* get the pixmap from this surface so we can find the client */
   if (!(ec = wl_resource_get_user_data(surface_resource)))
     {
        wl_resource_post_error(surface_resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Pixmap Set On Surface");
        return;
     }

   EC_CHANGED(ec);
   ec->new_client = ec->netwm.ping = EINA_TRUE;
   e_comp->new_clients++;
   ec->ignored = 0;

   /* get the client data */
   if (!(cdata = ec->comp_data))
     {
        wl_resource_post_error(surface_resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Data For Client");
        return;
     }

   /* check for existing shell surface */
   if (cdata->shell.surface)
     {
        wl_resource_post_error(surface_resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "Client already has shell surface");
        return;
     }

   /* try to create a shell surface */
   if (!(cdata->shell.surface =
         wl_resource_create(client, &wl_shell_surface_interface, 1, id)))
     {
        wl_resource_post_no_memory(surface_resource);
        return;
     }

   wl_resource_set_implementation(cdata->shell.surface,
                                  &_e_shell_surface_interface,
                                  ec, _e_shell_surface_cb_destroy);

   cdata->surface = surface_resource;
   cdata->shell.configure_send = _e_shell_surface_configure_send;
   cdata->shell.configure = _e_shell_surface_configure;
   cdata->shell.ping = _e_shell_surface_ping;
   cdata->shell.map = _e_shell_surface_map;
   cdata->shell.unmap = _e_shell_surface_unmap;
}

static void
_e_xdg_surface_state_add(struct wl_resource *resource, struct wl_array *states, uint32_t state)
{
  uint32_t *s;

   s = wl_array_add(states, sizeof(*s));
  if (s)
    *s = state;
  else
    wl_resource_post_no_memory(resource);
}

static void
_e_xdg_shell_surface_configure_send(struct wl_resource *resource, uint32_t edges, int32_t width, int32_t height)
{
   E_Client *ec;
   struct wl_array states;
   uint32_t serial;

   /* DBG("XDG_SHELL: Surface Configure Send: %d \t%d %d\tEdges: %d", */
   /*     wl_resource_get_id(resource), width, height, edges); */

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   wl_array_init(&states);

   if (ec->fullscreen)
     _e_xdg_surface_state_add(resource, &states, XDG_SURFACE_STATE_FULLSCREEN);
   else if (ec->maximized)
     _e_xdg_surface_state_add(resource, &states, XDG_SURFACE_STATE_MAXIMIZED);
   if (edges != 0)
     _e_xdg_surface_state_add(resource, &states, XDG_SURFACE_STATE_RESIZING);
   if (ec->focused)
     _e_xdg_surface_state_add(resource, &states, XDG_SURFACE_STATE_ACTIVATED);

   if (ec->netwm.type != E_WINDOW_TYPE_POPUP_MENU)
     {
        serial = wl_display_next_serial(e_comp->wl_comp_data->wl.disp);
        xdg_surface_send_configure(resource, width, height, &states, serial);
     }

   wl_array_release(&states);
}

static void
_e_xdg_shell_surface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   _e_shell_surface_destroy(resource);
}

static void
_e_xdg_shell_surface_cb_parent_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *parent_resource)
{
   E_Client *ec;

   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   /* set this client as a transient for parent */
   _e_shell_surface_parent_set(ec, parent_resource);
}

static void
_e_xdg_shell_surface_cb_title_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *title)
{
   E_Client *ec;

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   /* set title */
   eina_stringshare_replace(&ec->icccm.title, title);
   if (ec->frame) e_comp_object_frame_title_set(ec->frame, title);
}

static void
_e_xdg_shell_surface_cb_app_id_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *id)
{
   E_Client *ec;

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   /* use the wl_client to get the pid * and set it in the netwm props */
   wl_client_get_credentials(client, &ec->netwm.pid, NULL, NULL);

   /* set class */
   eina_stringshare_replace(&ec->icccm.class, id);
   /* eina_stringshare_replace(&ec->netwm.name, id); */
   ec->changes.icon = !!ec->icccm.class;
   EC_CHANGED(ec);
}

static void
_e_xdg_shell_surface_cb_window_menu_show(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *seat_resource EINA_UNUSED, uint32_t serial EINA_UNUSED, int32_t x, int32_t y)
{
   E_Client *ec;
   double timestamp;

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   timestamp = ecore_loop_time_get();
   e_int_client_menu_show(ec, x, y, 0, timestamp);
}

static void
_e_xdg_shell_surface_cb_move(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *seat_resource EINA_UNUSED, uint32_t serial EINA_UNUSED)
{
   E_Client *ec;
   E_Comp_Data *cdata;
   E_Binding_Event_Mouse_Button ev;

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   if ((ec->maximized) || (ec->fullscreen)) return;

   cdata = e_comp->wl_comp_data;
   switch (cdata->ptr.button)
     {
      case BTN_LEFT:
        ev.button = 1;
        break;
      case BTN_MIDDLE:
        ev.button = 2;
        break;
      case BTN_RIGHT:
        ev.button = 3;
        break;
      default:
        ev.button = cdata->ptr.button;
        break;
     }

   e_comp_object_frame_xy_unadjust(ec->frame,
                                   wl_fixed_to_int(cdata->ptr.x),
                                   wl_fixed_to_int(cdata->ptr.y),
                                   &ev.canvas.x, &ev.canvas.y);

   _e_shell_surface_mouse_down_helper(ec, &ev, EINA_TRUE);
}

static void
_e_xdg_shell_surface_cb_resize(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *seat_resource EINA_UNUSED, uint32_t serial EINA_UNUSED, uint32_t edges)
{
   E_Client *ec;
   E_Comp_Data *cdata;
   E_Binding_Event_Mouse_Button ev;

   /* DBG("XDG_SHELL: Surface Resize: %d\tEdges: %d",  */
   /*     wl_resource_get_id(resource), edges); */

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   if ((edges == 0) || (edges > 15) ||
       ((edges & 3) == 3) || ((edges & 12) == 12)) return;

   if ((ec->maximized) || (ec->fullscreen)) return;

   cdata = e_comp->wl_comp_data;
   cdata->resize.resource = resource;
   cdata->resize.edges = edges;
   cdata->ptr.grab_x = cdata->ptr.x - wl_fixed_from_int(ec->client.x);
   cdata->ptr.grab_y = cdata->ptr.y - wl_fixed_from_int(ec->client.y);

   switch (cdata->ptr.button)
     {
      case BTN_LEFT:
        ev.button = 1;
        break;
      case BTN_MIDDLE:
        ev.button = 2;
        break;
      case BTN_RIGHT:
        ev.button = 3;
        break;
      default:
        ev.button = cdata->ptr.button;
        break;
     }

   e_comp_object_frame_xy_unadjust(ec->frame,
                                   wl_fixed_to_int(cdata->ptr.x),
                                   wl_fixed_to_int(cdata->ptr.y),
                                   &ev.canvas.x, &ev.canvas.y);

   _e_shell_surface_mouse_down_helper(ec, &ev, EINA_FALSE);
}

static void
_e_xdg_shell_surface_cb_ack_configure(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, uint32_t serial EINA_UNUSED)
{
   /* No-Op */
}

static void
_e_xdg_shell_surface_cb_window_geometry_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int32_t x, int32_t y, int32_t w, int32_t h)
{
   E_Client *ec;

   ec = wl_resource_get_user_data(resource);
   if (!ec)
     {
        wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
                                "No Client For Shell Surface");
        return;
     }
   EINA_RECTANGLE_SET(&ec->comp_data->shell.window, x, y, w, h);
   //DBG("XDG_SHELL: Window Geom Set: %d \t%d %d, %d %d", wl_resource_get_id(resource), x, y, w, h);
}

static void
_e_xdg_shell_surface_cb_maximized_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   E_Client *ec;

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   if (!ec->lock_user_maximize)
     {
        e_client_maximize(ec, ((e_config->maximize_policy & E_MAXIMIZE_TYPE) |
                               E_MAXIMIZE_BOTH));
        _e_xdg_shell_surface_configure_send(resource, 0, ec->w, ec->h);
     }
}

static void
_e_xdg_shell_surface_cb_maximized_unset(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   E_Client *ec;

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   e_client_unmaximize(ec, E_MAXIMIZE_BOTH);
   _e_xdg_shell_surface_configure_send(resource, 0, ec->w, ec->h);
}

static void
_e_xdg_shell_surface_cb_fullscreen_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *output_resource EINA_UNUSED)
{
   E_Client *ec;

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   if (!ec->lock_user_fullscreen)
     e_client_fullscreen(ec, e_config->fullscreen_policy);
}

static void
_e_xdg_shell_surface_cb_fullscreen_unset(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   E_Client *ec;

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   if (!ec->lock_user_fullscreen)
     e_client_unfullscreen(ec);
}

static void
_e_xdg_shell_surface_cb_minimized_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   E_Client *ec;

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   if (!ec->lock_client_iconify)
     e_client_iconify(ec);
}

static const struct xdg_surface_interface _e_xdg_surface_interface =
{
   _e_xdg_shell_surface_cb_destroy,
   _e_xdg_shell_surface_cb_parent_set,
   _e_xdg_shell_surface_cb_title_set,
   _e_xdg_shell_surface_cb_app_id_set,
   _e_xdg_shell_surface_cb_window_menu_show,
   _e_xdg_shell_surface_cb_move,
   _e_xdg_shell_surface_cb_resize,
   _e_xdg_shell_surface_cb_ack_configure,
   _e_xdg_shell_surface_cb_window_geometry_set,
   _e_xdg_shell_surface_cb_maximized_set,
   _e_xdg_shell_surface_cb_maximized_unset,
   _e_xdg_shell_surface_cb_fullscreen_set,
   _e_xdg_shell_surface_cb_fullscreen_unset,
   _e_xdg_shell_surface_cb_minimized_set,
};

static void
_e_xdg_shell_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void
_e_xdg_shell_cb_unstable_version(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int32_t version)
{
   if (version > 1)
     wl_resource_post_error(resource, 1, "XDG Version Not Implemented Yet");
}

static void
_e_xdg_shell_surface_configure(struct wl_resource *resource, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
   E_Client *ec;

   /* DBG("XDG_SHELL: Surface Configure: %d \t%d %d %d %d",  */
   /*     wl_resource_get_id(resource), x, y, w, h); */

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   if (ec->parent)
     {
        if ((ec->netwm.type == E_WINDOW_TYPE_MENU) ||
            (ec->netwm.type == E_WINDOW_TYPE_POPUP_MENU) ||
            (ec->netwm.type == E_WINDOW_TYPE_DROPDOWN_MENU))
          {
             x = E_CLAMP(ec->parent->client.x + ec->comp_data->popup.x,
                         ec->parent->client.x,
                         ec->parent->client.x + 
                         ec->parent->client.w - ec->client.w);
             y = E_CLAMP(ec->parent->client.y + ec->comp_data->popup.y,
                         ec->parent->client.y,
                         ec->parent->client.y + 
                         ec->parent->client.h - ec->client.h);
          }
     }

   e_client_util_move_resize_without_frame(ec, x, y, w, h);
   /* TODO: ack configure ?? */
}

static void
_e_xdg_shell_surface_ping(struct wl_resource *resource)
{
   E_Client *ec;
   uint32_t serial;

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   if (e_comp->wl_comp_data->shell_interface.xdg_shell)
     {
       serial = wl_display_next_serial(e_comp->wl_comp_data->wl.disp);
       xdg_shell_send_ping(e_comp->wl_comp_data->shell_interface.xdg_shell, serial);
     }
}

static void
_e_xdg_shell_surface_map(struct wl_resource *resource)
{
   E_Client *ec;

   /* DBG("XDG_SHELL: Map Surface: %d", wl_resource_get_id(resource)); */

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   if ((!ec->comp_data->mapped) && (e_pixmap_usable_get(ec->pixmap)))
     {
        /* map this surface if needed */
        ec->visible = EINA_TRUE;
        evas_object_show(ec->frame);
        ec->comp_data->mapped = EINA_TRUE;

        /* FIXME: sometimes popup surfaces Do Not raise above their
         * respective parents... */
        /* if (ec->netwm.type == E_WINDOW_TYPE_POPUP_MENU) */
        /*   e_client_raise_latest_set(ec); */
     }
}

static void
_e_xdg_shell_surface_unmap(struct wl_resource *resource)
{
   E_Client *ec;

   /* DBG("XDG_SHELL: Unmap Surface: %d", wl_resource_get_id(resource)); */

   /* get the client for this resource */
   if (!(ec = wl_resource_get_user_data(resource)))
     {
        wl_resource_post_error(resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Client For Shell Surface");
        return;
     }

   if (ec->comp_data->mapped)
     {
        ec->visible = EINA_FALSE;
        evas_object_hide(ec->frame);
        ec->comp_data->mapped = EINA_FALSE;
     }
}

static void
_e_xdg_shell_cb_surface_get(struct wl_client *client, struct wl_resource *resource EINA_UNUSED, uint32_t id, struct wl_resource *surface_resource)
{
   E_Client *ec;
   E_Comp_Client_Data *cdata;

   /* DBG("XDG_SHELL: Surface Get %d", wl_resource_get_id(surface_resource)); */

   /* get the pixmap from this surface so we can find the client */
   if (!(ec = wl_resource_get_user_data(surface_resource)))
     {
        wl_resource_post_error(surface_resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Pixmap Set On Surface");
        return;
     }

   EC_CHANGED(ec);
   ec->new_client = ec->netwm.ping = EINA_TRUE;
   e_comp->new_clients++;
   ec->ignored = 0;

   /* get the client data */
   if (!(cdata = ec->comp_data))
     {
        wl_resource_post_error(surface_resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Data For Client");
        return;
     }

   /* check for existing shell surface */
   if (cdata->shell.surface)
     {
        wl_resource_post_error(surface_resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "Client already has XDG shell surface");
        return;
     }

   /* try to create a shell surface */
   if (!(cdata->shell.surface =
         wl_resource_create(client, &xdg_surface_interface, 1, id)))
     {
        ERR("Could not create xdg shell surface");
        wl_resource_post_no_memory(surface_resource);
        return;
     }

   wl_resource_set_implementation(cdata->shell.surface,
                                  &_e_xdg_surface_interface, ec,
                                  _e_shell_surface_cb_destroy);

   cdata->surface = surface_resource;
   cdata->shell.configure_send = _e_xdg_shell_surface_configure_send;
   cdata->shell.configure = _e_xdg_shell_surface_configure;
   cdata->shell.ping = _e_xdg_shell_surface_ping;
   cdata->shell.map = _e_xdg_shell_surface_map;
   cdata->shell.unmap = _e_xdg_shell_surface_unmap;

   /* set toplevel client properties */
   ec->icccm.accepts_focus = 1;
   if (!ec->internal)
     ec->borderless = 1;
   ec->lock_border = EINA_TRUE;
   if ((!ec->internal) || (!ec->borderless))
     ec->border.changed = ec->changes.border = !ec->borderless;
   ec->netwm.type = E_WINDOW_TYPE_NORMAL;
   ec->comp_data->set_win_type = EINA_TRUE;
}

static void
_e_xdg_shell_popup_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   _e_shell_surface_destroy(resource);
}

static const struct xdg_popup_interface _e_xdg_popup_interface =
{
   _e_xdg_shell_popup_cb_destroy,
};

static void
_e_xdg_shell_cb_popup_get(struct wl_client *client, struct wl_resource *resource EINA_UNUSED, uint32_t id, struct wl_resource *surface_resource, struct wl_resource *parent_resource, struct wl_resource *seat_resource EINA_UNUSED, uint32_t serial EINA_UNUSED, int32_t x, int32_t y)
{
   E_Client *ec;
   E_Comp_Client_Data *cdata;

   /* DBG("XDG_SHELL: Popup Get"); */
   /* DBG("\tSurface: %d", wl_resource_get_id(surface_resource)); */
   /* DBG("\tParent Surface: %d", wl_resource_get_id(parent_resource)); */
   /* DBG("\tLocation: %d %d", x, y); */

   /* get the pixmap from this surface so we can find the client */
   if (!(ec = wl_resource_get_user_data(surface_resource)))
     {
        wl_resource_post_error(surface_resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Pixmap Set On Surface");
        return;
     }

   /* get the client data */
   if (!(cdata = ec->comp_data))
     {
        wl_resource_post_error(surface_resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Data For Client");
        return;
     }

   /* check for existing shell surface */
   if (cdata->shell.surface)
     {
        wl_resource_post_error(surface_resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "Client already has shell popup surface");
        return;
     }

   /* check for the parent surface */
   if (!parent_resource)
     {
        wl_resource_post_error(surface_resource,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "Popup requires a parent shell surface");
        return;
     }

   /* try to create a shell surface */
   if (!(cdata->shell.surface =
         wl_resource_create(client, &xdg_popup_interface, 1, id)))
     {
        ERR("Could not create xdg shell surface");
        wl_resource_post_no_memory(surface_resource);
        return;
     }

   wl_resource_set_implementation(cdata->shell.surface,
                                  &_e_xdg_popup_interface, ec, NULL);

   cdata->surface = surface_resource;
   cdata->shell.configure_send = _e_xdg_shell_surface_configure_send;
   cdata->shell.configure = _e_xdg_shell_surface_configure;
   cdata->shell.ping = _e_xdg_shell_surface_ping;
   cdata->shell.map = _e_xdg_shell_surface_map;
   cdata->shell.unmap = _e_xdg_shell_surface_unmap;

   EC_CHANGED(ec);
   ec->new_client = ec->override = 1;
   ec->ignored = 0;
   e_comp->new_clients++;
   e_client_focus_stack_set(eina_list_remove(e_client_focus_stack_get(), ec));
   if (!ec->internal)
     ec->borderless = !ec->internal_elm_win;
   ec->lock_border = EINA_TRUE;
   if (!ec->internal)
     ec->border.changed = ec->changes.border = !ec->borderless;
   ec->changes.icon = !!ec->icccm.class;
   ec->netwm.type = E_WINDOW_TYPE_POPUP_MENU;
   ec->comp_data->set_win_type = EINA_TRUE;
   evas_object_layer_set(ec->frame, E_LAYER_CLIENT_POPUP);
   e_client_focus_stack_set(eina_list_remove(e_client_focus_stack_get(), ec));

   /* set this client as a transient for parent */
   _e_shell_surface_parent_set(ec, parent_resource);

   if (ec->parent)
     {
        cdata->popup.x = E_CLAMP(x, 0, ec->parent->client.w);
        cdata->popup.y = E_CLAMP(y, 0, ec->parent->client.h);
     }
   else
     {
        cdata->popup.x = x;
        cdata->popup.y = y;
     }

}

static void
_e_xdg_shell_cb_pong(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, uint32_t serial EINA_UNUSED)
{
   E_Client *ec;

   if ((ec = wl_resource_get_user_data(resource)))
     {
        ec->ping_ok = EINA_TRUE;
        ec->hung = EINA_FALSE;
     }
}

static const struct wl_shell_interface _e_shell_interface =
{
   _e_shell_cb_shell_surface_get
};

static const struct xdg_shell_interface _e_xdg_shell_interface =
{
   _e_xdg_shell_cb_destroy,
   _e_xdg_shell_cb_unstable_version,
   _e_xdg_shell_cb_surface_get,
   _e_xdg_shell_cb_popup_get,
   _e_xdg_shell_cb_pong
};

static void
_e_xdg_shell_cb_unbind(struct wl_resource *resource EINA_UNUSED)
{
   e_comp->wl_comp_data->shell_interface.xdg_shell = NULL;
}

static int
_e_xdg_shell_cb_dispatch(const void *implementation EINA_UNUSED, void *target, uint32_t opcode, const struct wl_message *message EINA_UNUSED, union wl_argument *args)
{
   struct wl_resource *res;

   if (!(res = target)) return 0;

   if (opcode != 1)
     {
        wl_resource_post_error(res, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "Must call use_unstable_version first");
        return 0;
     }

   if (args[0].i != XDG_SERVER_VERSION)
     {
        wl_resource_post_error(res, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "Incompatible versions. "
                               "Server: %d, Client: %d",
                               XDG_SERVER_VERSION, args[0].i);
        return 0;
     }

   wl_resource_set_implementation(res, &_e_xdg_shell_interface,
                                  e_comp->wl_comp_data,
                                  _e_xdg_shell_cb_unbind);

   return 1;
}

static void
_e_shell_cb_unbind(struct wl_resource *resource EINA_UNUSED)
{
   e_comp->wl_comp_data->shell_interface.shell = NULL;
}

static void
_e_shell_cb_bind(struct wl_client *client, void *data EINA_UNUSED, uint32_t version, uint32_t id)
{
   struct wl_resource *res;

   if (!(res = wl_resource_create(client, &wl_shell_interface, MIN(version, 1), id)))
     {
        wl_client_post_no_memory(client);
        return;
     }

   e_comp->wl_comp_data->shell_interface.shell = res;
   wl_resource_set_implementation(res, &_e_shell_interface,
                                  e_comp->wl_comp_data,
                                  _e_shell_cb_unbind);
}

static void
_e_xdg_shell_cb_bind(struct wl_client *client, void *data EINA_UNUSED, uint32_t version, uint32_t id)
{
   struct wl_resource *res;

   if (!(res = wl_resource_create(client, &xdg_shell_interface, MIN(version, 1), id)))
     {
        wl_client_post_no_memory(client);
        return;
     }

   e_comp->wl_comp_data->shell_interface.xdg_shell = res;
   wl_resource_set_dispatcher(res, _e_xdg_shell_cb_dispatch, NULL,
                              e_comp->wl_comp_data, NULL);
}

E_API E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Wl_Desktop_Shell" };

E_API void *
e_modapi_init(E_Module *m)
{
   E_Comp_Data *cdata;

   /* try to get the current compositor */
   if (!e_comp) return NULL;

   /* make sure it's a wayland compositor */
   /* if (e_comp->comp_type != E_PIXMAP_TYPE_WL) return NULL; */

   /* try to get the compositor data */
   if (!(cdata = e_comp->wl_comp_data)) return NULL;

   /* try to create global shell interface */
   if (!wl_global_create(cdata->wl.disp, &wl_shell_interface, 1,
                         cdata, _e_shell_cb_bind))
     {
        ERR("Could not create shell global: %m");
        return NULL;
     }

   /* try to create global xdg_shell interface */
   if (!wl_global_create(cdata->wl.disp, &xdg_shell_interface, 1,
                         cdata, _e_xdg_shell_cb_bind))
     {
        ERR("Could not create xdg_shell global: %m");
        return NULL;
     }

   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   return 1;
}

#define EXECUTIVE_MODE_ENABLED
#define E_COMP_WL
#include "e.h"
#include <sys/mman.h>

static void
_e_comp_wl_input_update_seat_caps(E_Comp_Data *cdata)
{
   Eina_List *l;
   struct wl_resource *res;
   enum wl_seat_capability caps = 0;

   if (cdata->ptr.enabled)
     caps |= WL_SEAT_CAPABILITY_POINTER;
   if (cdata->kbd.enabled)
     caps |= WL_SEAT_CAPABILITY_KEYBOARD;
   if (cdata->touch.enabled)
     caps |= WL_SEAT_CAPABILITY_TOUCH;

   EINA_LIST_FOREACH(cdata->seat.resources, l, res)
        wl_seat_send_capabilities(res, caps);
}

static void
_e_comp_wl_input_cb_resource_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void
_e_comp_wl_input_pointer_cb_cursor_set(struct wl_client *client, struct wl_resource *resource EINA_UNUSED, uint32_t serial EINA_UNUSED, struct wl_resource *surface_resource, int32_t x, int32_t y)
{
   E_Comp_Data *cdata;
   E_Client *ec;
   Eina_Bool got_mouse = EINA_FALSE;

   /* get compositor data */
   if (!(cdata = wl_resource_get_user_data(resource))) return;
   E_CLIENT_FOREACH(ec)
     {
       if (!ec->comp_data->surface) continue;
       if (client != wl_resource_get_client(ec->comp_data->surface)) continue;
       if (ec->mouse.in)
         {
           got_mouse = EINA_TRUE;
           break;
         }
     }
   if (!got_mouse) return;
   if (!surface_resource)
     {
        e_pointer_object_set(e_comp->pointer, NULL, x, y);
        return;
     }
   ec = wl_resource_get_user_data(surface_resource);
   if (!ec->re_manage)
     {
        ec->re_manage = 1;
        ec->ignored = 0;

        ec->lock_focus_out = ec->layer_block = ec->visible = ec->override = 1;
        ec->icccm.title = eina_stringshare_add("noshadow");
        evas_object_pass_events_set(ec->frame, 1);
        e_client_focus_stack_set(eina_list_remove(e_client_focus_stack_get(), ec));
        EC_CHANGED(ec);
     }

   /* ignore cursor changes during resize/move I guess */
   if (e_client_action_get()) return;
   e_pointer_object_set(e_comp->pointer, ec->frame, x, y);
}

static const struct wl_pointer_interface _e_pointer_interface =
{
   _e_comp_wl_input_pointer_cb_cursor_set,
   _e_comp_wl_input_cb_resource_destroy
};

static const struct wl_keyboard_interface _e_keyboard_interface =
{
   _e_comp_wl_input_cb_resource_destroy
};

static void
_e_comp_wl_input_cb_pointer_unbind(struct wl_resource *resource)
{
   E_Comp_Data *cdata;

   /* get compositor data */
   if (!(cdata = wl_resource_get_user_data(resource))) return;

   cdata->ptr.resources = eina_list_remove(cdata->ptr.resources, resource);
}

static void
_e_comp_wl_input_cb_pointer_get(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
   E_Comp_Data *cdata;
   struct wl_resource *res;

   /* get compositor data */
   if (!(cdata = wl_resource_get_user_data(resource))) return;

   /* try to create pointer resource */
   res = wl_resource_create(client, &wl_pointer_interface,
                            wl_resource_get_version(resource), id);
   if (!res)
     {
        ERR("Could not create pointer on seat %s: %m", cdata->seat.name);
        wl_client_post_no_memory(client);
        return;
     }

   cdata->ptr.resources = eina_list_append(cdata->ptr.resources, res);
   wl_resource_set_implementation(res, &_e_pointer_interface, cdata,
                                 _e_comp_wl_input_cb_pointer_unbind);
}

static void
_e_comp_wl_input_cb_keyboard_unbind(struct wl_resource *resource)
{
   E_Comp_Data *cdata;

   /* get compositor data */
   if (!(cdata = wl_resource_get_user_data(resource))) return;

   cdata->kbd.resources = eina_list_remove(cdata->kbd.resources, resource);
}

void
e_comp_wl_input_keyboard_enter_send(E_Client *ec)
{
   struct wl_resource *res;
   struct wl_client *wc;
   Eina_List *l;
   uint32_t serial;

   if (!ec->comp_data->surface) return;

   if (!eina_list_count(e_comp->wl_comp_data->kbd.resources)) return;

   e_comp_wl_input_keyboard_modifiers_serialize(e_comp->wl_comp_data);

   wc = wl_resource_get_client(ec->comp_data->surface);
   serial = wl_display_next_serial(e_comp->wl_comp_data->wl.disp);

   EINA_LIST_FOREACH(e_comp->wl_comp_data->kbd.resources, l, res)
     {
        if (wl_resource_get_client(res) != wc) continue;
        wl_keyboard_send_enter(res, serial, ec->comp_data->surface,
                               &e_comp->wl_comp_data->kbd.keys);
        wl_keyboard_send_modifiers(res, serial,
                                   e_comp->wl_comp_data->kbd.mod_depressed,
                                   e_comp->wl_comp_data->kbd.mod_latched,
                                   e_comp->wl_comp_data->kbd.mod_locked,
                                   e_comp->wl_comp_data->kbd.mod_group);
     }
}

static void
_e_comp_wl_input_cb_keyboard_get(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
   E_Client *focused;
   E_Comp_Data *cdata;
   struct wl_resource *res;

   /* get compositor data */
   if (!(cdata = wl_resource_get_user_data(resource))) return;

   /* try to create keyboard resource */
   res = wl_resource_create(client, &wl_keyboard_interface,
                            wl_resource_get_version(resource), id);
   if (!res)
     {
        ERR("Could not create keyboard on seat %s: %m", cdata->seat.name);
        wl_client_post_no_memory(client);
        return;
     }

   cdata->kbd.resources = eina_list_append(cdata->kbd.resources, res);
   wl_resource_set_implementation(res, &_e_keyboard_interface, cdata,
                                  _e_comp_wl_input_cb_keyboard_unbind);

   /* send current keymap */
   wl_keyboard_send_keymap(res, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                           cdata->xkb.fd, cdata->xkb.size);

   /* if the client owns the focused surface, we need to send an enter */
   focused = e_client_focused_get();
   if (!focused)
     return;

   e_comp_wl_input_keyboard_enter_send(focused);
}

static void
_e_comp_wl_input_cb_touch_get(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, uint32_t id EINA_UNUSED)
{
   E_Comp_Data *cdata;

   /* DBG("Input Touch Get"); */

   /* NB: Needs new resource !! */

   /* get compositor data */
   if (!(cdata = wl_resource_get_user_data(resource))) return;
}

static const struct wl_seat_interface _e_seat_interface =
{
   _e_comp_wl_input_cb_pointer_get,
   _e_comp_wl_input_cb_keyboard_get,
   _e_comp_wl_input_cb_touch_get,
};

static void
_e_comp_wl_input_cb_unbind_seat(struct wl_resource *resource)
{
   E_Comp_Data *cdata;

   if (!(cdata = wl_resource_get_user_data(resource))) return;

   cdata->seat.resources = eina_list_remove(cdata->seat.resources, resource);
}

static void
_e_comp_wl_input_cb_bind_seat(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
   E_Comp_Data *cdata;
   struct wl_resource *res;

   /* try to create the seat resource */
   cdata = data;
   res = wl_resource_create(client, &wl_seat_interface, MIN(version, 4), id);
   if (!res)
     {
        ERR("Could not create seat resource: %m");
        return;
     }

   /* store version of seat interface for reuse in updating capabilities */
   cdata->seat.version = version;
   cdata->seat.resources = eina_list_append(cdata->seat.resources, res);

   wl_resource_set_implementation(res, &_e_seat_interface, cdata,
                                  _e_comp_wl_input_cb_unbind_seat);

   _e_comp_wl_input_update_seat_caps(cdata);
   if (cdata->seat.version >= WL_SEAT_NAME_SINCE_VERSION)
     wl_seat_send_name(res, cdata->seat.name);
}

static int
_e_comp_wl_input_keymap_fd_get(off_t size)
{
   int fd = 0, blen = 0, len = 0;
   const char *path;
   char tmp[PATH_MAX];
   long flags;

   blen = sizeof(tmp) - 1;

   if (!(path = getenv("XDG_RUNTIME_DIR")))
     return -1;

   len = strlen(path);
   if (len < blen)
     {
        strcpy(tmp, path);
        strcat(tmp, "/e-wl-keymap-XXXXXX");
     }
   else
     return -1;

   if ((fd = mkstemp(tmp)) < 0) return -1;

   flags = fcntl(fd, F_GETFD);
   if (flags < 0)
     {
        close(fd);
        return -1;
     }

   if (fcntl(fd, F_SETFD, (flags | FD_CLOEXEC)) == -1)
     {
        close(fd);
        return -1;
     }

   if (ftruncate(fd, size) < 0)
     {
        close(fd);
        return -1;
     }

   unlink(tmp);
   return fd;
}

static void
_e_comp_wl_input_keymap_update(E_Comp_Data *cdata, struct xkb_keymap *keymap)
{
   char *tmp;
   xkb_mod_mask_t latched = 0, locked = 0, group = 0;
   struct wl_resource *res;
   Eina_List *l;
   uint32_t serial;

   /* unreference any existing keymap */
   if (cdata->xkb.keymap) xkb_map_unref(cdata->xkb.keymap);

   /* unmap any existing keyboard area */
   if (cdata->xkb.area) munmap(cdata->xkb.area, cdata->xkb.size);
   if (cdata->xkb.fd >= 0) close(cdata->xkb.fd);

   /* unreference any existing keyboard state */
   if (cdata->xkb.state)
     {
        latched =
          xkb_state_serialize_mods(cdata->xkb.state, XKB_STATE_MODS_LATCHED);
        locked =
          xkb_state_serialize_mods(cdata->xkb.state, XKB_STATE_MODS_LOCKED);
        group =
          xkb_state_serialize_layout(cdata->xkb.state,
                                     XKB_STATE_LAYOUT_EFFECTIVE);
        xkb_state_unref(cdata->xkb.state);
     }

   /* create a new xkb state */
   cdata->xkb.state = xkb_state_new(keymap);

   if ((latched) || (locked) || (group))
     xkb_state_update_mask(cdata->xkb.state, 0, latched, locked, 0, 0, group);

   /* increment keymap reference */
   cdata->xkb.keymap = xkb_map_ref(keymap);

   /* fetch updated modifiers */
   cdata->kbd.mod_shift = xkb_map_mod_get_index(keymap, XKB_MOD_NAME_SHIFT);
   cdata->kbd.mod_caps = xkb_map_mod_get_index(keymap, XKB_MOD_NAME_CAPS);
   cdata->kbd.mod_ctrl = xkb_map_mod_get_index(keymap, XKB_MOD_NAME_CTRL);
   cdata->kbd.mod_alt = xkb_map_mod_get_index(keymap, XKB_MOD_NAME_ALT);
   cdata->kbd.mod_super = xkb_map_mod_get_index(keymap, XKB_MOD_NAME_LOGO);

   if (!(tmp = xkb_map_get_as_string(keymap)))
     {
        ERR("Could not get keymap string");
        return;
     }

   cdata->xkb.size = strlen(tmp) + 1;
   cdata->xkb.fd = _e_comp_wl_input_keymap_fd_get(cdata->xkb.size);
   if (cdata->xkb.fd < 0)
     {
        ERR("Could not create keymap file");
        free(tmp);
        return;
     }

   cdata->xkb.area =
     mmap(NULL, cdata->xkb.size, (PROT_READ | PROT_WRITE),
          MAP_SHARED, cdata->xkb.fd, 0);
   if (cdata->xkb.area == MAP_FAILED)
     {
        ERR("Failed to mmap keymap area: %m");
        free(tmp);
        return;
     }

   strcpy(cdata->xkb.area, tmp);
   free(tmp);

   /* send updated keymap */
   EINA_LIST_FOREACH(cdata->kbd.resources, l, res)
     wl_keyboard_send_keymap(res, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                             cdata->xkb.fd, cdata->xkb.size);

   /* update modifiers */
   e_comp_wl_input_keyboard_modifiers_update(cdata);

   if ((!latched) && (!locked)) return;

   /* send modifiers */
   serial = wl_display_get_serial(cdata->wl.disp);
   EINA_LIST_FOREACH(cdata->kbd.resources, l, res)
     wl_keyboard_send_modifiers(res, serial, cdata->kbd.mod_depressed,
                                cdata->kbd.mod_latched, cdata->kbd.mod_locked,
                                cdata->kbd.mod_group);
}

EINTERN Eina_Bool
e_comp_wl_input_init(E_Comp_Data *cdata)
{
   /* check for valid compositor data */
   if (!cdata)
     {
        ERR("No compositor data");
        return EINA_FALSE;
     }

   /* set default seat name */
   if (!cdata->seat.name) cdata->seat.name = "default";

   cdata->xkb.fd = -1;

   /* create the global resource for input seat */
   cdata->seat.global =
     wl_global_create(cdata->wl.disp, &wl_seat_interface, 4,
                      cdata, _e_comp_wl_input_cb_bind_seat);
   if (!cdata->seat.global)
     {
        ERR("Could not create global for seat: %m");
        return EINA_FALSE;
     }

   wl_array_init(&cdata->kbd.keys);

   return EINA_TRUE;
}

EINTERN void
e_comp_wl_input_shutdown(E_Comp_Data *cdata)
{
   struct wl_resource *res;

   /* check for valid compositor data */
   if (!cdata)
     {
        ERR("No compositor data");
        return;
     }

   /* destroy pointer resources */
   EINA_LIST_FREE(cdata->ptr.resources, res)
     wl_resource_destroy(res);

   /* destroy keyboard resources */
   EINA_LIST_FREE(cdata->kbd.resources, res)
     wl_resource_destroy(res);

   /* destroy touch resources */
   EINA_LIST_FREE(cdata->touch.resources, res)
     wl_resource_destroy(res);

   /* destroy cdata->kbd.keys array */
   wl_array_release(&cdata->kbd.keys);

   /* unreference any existing keymap */
   if (cdata->xkb.keymap) xkb_map_unref(cdata->xkb.keymap);

   /* unmap any existing keyboard area */
   if (cdata->xkb.area) munmap(cdata->xkb.area, cdata->xkb.size);
   if (cdata->xkb.fd >= 0) close(cdata->xkb.fd);

   /* unreference any existing keyboard state */
   if (cdata->xkb.state) xkb_state_unref(cdata->xkb.state);

   /* unreference any existing context */
   if (cdata->xkb.context) xkb_context_unref(cdata->xkb.context);

   /* destroy the global seat resource */
   if (cdata->seat.global) wl_global_destroy(cdata->seat.global);
   cdata->seat.global = NULL;
}

EINTERN Eina_Bool
e_comp_wl_input_pointer_check(struct wl_resource *res)
{
   return wl_resource_instance_of(res, &wl_pointer_interface,
                                  &_e_pointer_interface);
}

EINTERN Eina_Bool
e_comp_wl_input_keyboard_check(struct wl_resource *res)
{
   return wl_resource_instance_of(res, &wl_keyboard_interface,
                                  &_e_keyboard_interface);
}

EINTERN void
e_comp_wl_input_keyboard_modifiers_serialize(E_Comp_Data *cdata)
{
   cdata->kbd.mod_depressed =
     xkb_state_serialize_mods(cdata->xkb.state, XKB_STATE_DEPRESSED);
   cdata->kbd.mod_latched =
     xkb_state_serialize_mods(cdata->xkb.state, XKB_STATE_MODS_LATCHED);
   cdata->kbd.mod_locked =
     xkb_state_serialize_mods(cdata->xkb.state, XKB_STATE_MODS_LOCKED);
   cdata->kbd.mod_group =
     xkb_state_serialize_layout(cdata->xkb.state, XKB_STATE_LAYOUT_EFFECTIVE);
}

EINTERN void
e_comp_wl_input_keyboard_modifiers_update(E_Comp_Data *cdata)
{
   uint32_t serial;
   struct wl_resource *res;
   Eina_List *l;

   e_comp_wl_input_keyboard_modifiers_serialize(cdata);

   if (!eina_list_count(cdata->kbd.resources)) return;

   serial = wl_display_next_serial(cdata->wl.disp);
   EINA_LIST_FOREACH(cdata->kbd.resources, l, res)
     wl_keyboard_send_modifiers(res, serial,
                                cdata->kbd.mod_depressed,
                                cdata->kbd.mod_latched,
                                cdata->kbd.mod_locked,
                                cdata->kbd.mod_group);
}

EINTERN void
e_comp_wl_input_keyboard_state_update(E_Comp_Data *cdata, uint32_t keycode, Eina_Bool pressed)
{
   enum xkb_key_direction dir;

   if (!cdata->xkb.state) return;

   if (pressed) dir = XKB_KEY_DOWN;
   else dir = XKB_KEY_UP;

   cdata->kbd.mod_changed =
     xkb_state_update_key(cdata->xkb.state, keycode + 8, dir);
}

EAPI void
e_comp_wl_input_pointer_enabled_set(E_Comp_Data *cdata, Eina_Bool enabled)
{
   /* check for valid compositor data */
   if (!cdata)
     {
        ERR("No compositor data");
        return;
     }

   cdata->ptr.enabled = enabled;
   _e_comp_wl_input_update_seat_caps(cdata);
}

EAPI void
e_comp_wl_input_keyboard_enabled_set(E_Comp_Data *cdata, Eina_Bool enabled)
{
   /* check for valid compositor data */
   if (!cdata)
     {
        ERR("No compositor data");
        return;
     }

   cdata->kbd.enabled = enabled;
   _e_comp_wl_input_update_seat_caps(cdata);
}

EAPI void
e_comp_wl_input_keymap_set(E_Comp_Data *cdata, const char *rules, const char *model, const char *layout)
{
   struct xkb_keymap *keymap;
   struct xkb_rule_names names;

   /* check for valid compositor data */
   if (!cdata)
     {
        ERR("No compositor data");
        return;
     }

   /* DBG("COMP_WL: Keymap Set: %s %s %s", rules, model, layout); */

   /* assemble xkb_rule_names so we can fetch keymap */
   memset(&names, 0, sizeof(names));
   if (rules) names.rules = strdup(rules);
   if (model) names.model = strdup(model);
   if (layout) names.layout = strdup(layout);

   /* unreference any existing context */
   if (cdata->xkb.context) xkb_context_unref(cdata->xkb.context);

   /* create a new xkb context */
   cdata->xkb.context = xkb_context_new(0);

   /* fetch new keymap based on names */
   keymap = xkb_map_new_from_names(cdata->xkb.context, &names, 0);

   /* update compositor keymap */
   _e_comp_wl_input_keymap_update(cdata, keymap);

   /* cleanup */
   free((char *)names.rules);
   free((char *)names.model);
   free((char *)names.layout);
}

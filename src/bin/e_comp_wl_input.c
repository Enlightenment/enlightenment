#define EXECUTIVE_MODE_ENABLED
#define E_COMP_WL
#include "e.h"
#include <sys/mman.h>
#ifdef HAVE_WL_DRM
# ifdef HAVE_DRM2
#  include <Ecore_Drm2.h>
# else
#  include <Ecore_Drm.h>
# endif
#endif

E_API int E_EVENT_TEXT_INPUT_PANEL_VISIBILITY_CHANGE = -1;
static xkb_keycode_t (*_xkb_keymap_key_by_name)(void *, const char *);
static void _e_comp_wl_input_context_keymap_set(struct xkb_keymap *keymap, struct xkb_context *context);

static Eina_Hash *input_gen_modifiers;

//the following two fields are just set by e_comp_wl_input_keymap_set if it is called before e_comp_wl is valid.
//when then later init is called those two fields are used in the keymap of the e_comp_wl struct
static struct xkb_context *cached_context;
static struct xkb_keymap *cached_keymap;
static xkb_layout_index_t choosen_group;

static void
_e_comp_wl_input_update_seat_caps(void)
{
   Eina_List *l;
   struct wl_resource *res;
   enum wl_seat_capability caps = 0;

   if (e_comp_wl->ptr.enabled)
     caps |= WL_SEAT_CAPABILITY_POINTER;
   if (e_comp_wl->kbd.enabled)
     caps |= WL_SEAT_CAPABILITY_KEYBOARD;
   if (e_comp_wl->touch.enabled)
     caps |= WL_SEAT_CAPABILITY_TOUCH;

   EINA_LIST_FOREACH(e_comp_wl->seat.resources, l, res)
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
   E_Client *ec;
   Eina_Bool got_mouse = EINA_FALSE;

   E_CLIENT_FOREACH(ec)
     {
       if (e_object_is_del(E_OBJECT(ec))) continue;
       if (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL) continue;
       if (!ec->comp_data->surface) continue;
       if (client != wl_resource_get_client(ec->comp_data->surface)) continue;
       if (ec->mouse.in)
         {
            if (e_client_has_xwindow(ec))
              got_mouse = E_INSIDE(ec->mouse.current.mx, ec->mouse.current.my,
                ec->client.x, ec->client.y, ec->client.w, ec->client.h);
            else
              got_mouse = EINA_TRUE;
            break;
         }
     }
   if (!got_mouse)
     {
        if (ec && ec->mouse.in && (!surface_resource))
          e_pointer_object_set(e_comp->pointer, NULL, 0, 0);
        return;
     }
   if (!surface_resource)
     {
        if (e_comp_object_frame_exists(ec->frame) &&
            ec->mouse.in && (!ec->comp_data->ssd_mouse_in))
          e_pointer_object_set(e_comp->pointer, NULL, 0, 0);
        else
          {
             ecore_evas_cursor_unset(e_comp->ee);
             evas_object_hide(e_comp->pointer->o_ptr);
          }
        return;
     }
   ec = wl_resource_get_user_data(surface_resource);
   if (!ec->re_manage)
     {
        ec->comp_data->cursor = ec->re_manage = 1;
        ec->ignored = 0;
        evas_object_layer_set(ec->frame, E_LAYER_CLIENT_PRIO);

        ec->lock_focus_out = ec->layer_block = ec->visible = ec->override = 1;
        ec->icccm.title = eina_stringshare_add("noshadow");
        evas_object_pass_events_set(ec->frame, 1);
        evas_object_show(ec->frame);
        if (e_comp_object_damage_exists(ec->frame))
          e_comp_object_render_update_add(ec->frame);
        ec->comp_data->mapped = 1;
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

static const struct wl_touch_interface _e_touch_interface =
{
   _e_comp_wl_input_cb_resource_destroy
};

static void
_e_comp_wl_input_cb_pointer_unbind(struct wl_resource *resource)
{
   e_comp_wl->ptr.resources =
     eina_list_remove(e_comp_wl->ptr.resources, resource);
}

static void
_e_comp_wl_input_cb_pointer_get(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
   struct wl_resource *res;

   /* try to create pointer resource */
   res = wl_resource_create(client, &wl_pointer_interface,
                            wl_resource_get_version(resource), id);
   if (!res)
     {
        ERR("Could not create pointer on seat %s",
            e_comp_wl->seat.name);
        wl_client_post_no_memory(client);
        return;
     }

   e_comp_wl->ptr.resources =
     eina_list_append(e_comp_wl->ptr.resources, res);
   wl_resource_set_user_data(res, resource);
   wl_resource_set_implementation(res, &_e_pointer_interface,
                                  e_comp->wl_comp_data,
                                 _e_comp_wl_input_cb_pointer_unbind);
}

static void
_e_comp_wl_input_cb_keyboard_unbind(struct wl_resource *resource)
{
   Eina_List *l, *ll;
   struct wl_resource *res;

   e_comp_wl->kbd.resources =
     eina_list_remove(e_comp_wl->kbd.resources, resource);
   EINA_LIST_FOREACH_SAFE(e_comp_wl->kbd.focused, l, ll, res)
     {
        if (res == resource)
          {
             e_comp_wl->kbd.focused =
               eina_list_remove_list(e_comp_wl->kbd.focused, l);
          }
     }
}

void
e_comp_wl_input_keyboard_enter_send(E_Client *ec)
{
   struct wl_resource *res;
   Eina_List *l;
   uint32_t serial;

   if (!ec->comp_data->surface) return;

   if (!e_comp_wl->kbd.focused) return;

   e_comp_wl_input_keyboard_modifiers_serialize();

   serial = wl_display_next_serial(e_comp_wl->wl.disp);

   EINA_LIST_FOREACH(e_comp_wl->kbd.focused, l, res)
     {
        wl_keyboard_send_enter(res, serial, ec->comp_data->surface,
                               &e_comp_wl->kbd.keys);
        wl_keyboard_send_modifiers(res, serial,
                                   e_comp_wl->kbd.mod_depressed,
                                   e_comp_wl->kbd.mod_latched,
                                   e_comp_wl->kbd.mod_locked,
                                   e_comp_wl->kbd.mod_group);
     }
}

static void
_e_comp_wl_input_cb_keyboard_get(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
   E_Client *focused;
   struct wl_resource *res;

   /* try to create keyboard resource */
   res = wl_resource_create(client, &wl_keyboard_interface,
                            wl_resource_get_version(resource), id);
   if (!res)
     {
        ERR("Could not create keyboard on seat %s", e_comp_wl->seat.name);
        wl_client_post_no_memory(client);
        return;
     }
   wl_resource_set_user_data(res, resource);

   e_comp_wl->kbd.resources =
     eina_list_append(e_comp_wl->kbd.resources, res);
   wl_resource_set_implementation(res, &_e_keyboard_interface,
                                  e_comp->wl_comp_data,
                                  _e_comp_wl_input_cb_keyboard_unbind);

   /* send current repeat_info */
   if (wl_resource_get_version(res) >= WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION)
     wl_keyboard_send_repeat_info(res, e_config->keyboard.repeat_rate, e_config->keyboard.repeat_delay);

   /* send current keymap */
   wl_keyboard_send_keymap(res, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                           e_comp_wl->xkb.fd, e_comp_wl->xkb.size);

   /* if the client owns the focused surface, we need to send an enter */
   focused = e_client_focused_get();
   if (!focused) return;

   if (client != wl_resource_get_client(focused->comp_data->surface)) return;
   e_comp_wl->kbd.focused = eina_list_append(e_comp_wl->kbd.focused, res);

   e_comp_wl_input_keyboard_enter_send(focused);
}

static void
_e_comp_wl_input_cb_touch_unbind(struct wl_resource *resource)
{
   e_comp_wl->touch.resources =
     eina_list_remove(e_comp_wl->touch.resources, resource);
}

static void
_e_comp_wl_input_cb_touch_get(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, uint32_t id EINA_UNUSED)
{
   struct wl_resource *res;

    /* try to create pointer resource */
   res = wl_resource_create(client, &wl_touch_interface,
                            wl_resource_get_version(resource), id);
   if (!res)
     {
        ERR("Could not create touch on seat %s",
            e_comp_wl->seat.name);
        wl_client_post_no_memory(client);
        return;
     }

   e_comp_wl->touch.resources =
     eina_list_append(e_comp_wl->touch.resources, res);
   wl_resource_set_implementation(res, &_e_touch_interface,
                                  e_comp->wl_comp_data,
                                  _e_comp_wl_input_cb_touch_unbind);
}

static void
_e_comp_wl_input_cb_release(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static const struct wl_seat_interface _e_seat_interface =
{
   _e_comp_wl_input_cb_pointer_get,
   _e_comp_wl_input_cb_keyboard_get,
   _e_comp_wl_input_cb_touch_get,
   _e_comp_wl_input_cb_release,
};

static void
_e_comp_wl_input_cb_unbind_seat(struct wl_resource *resource)
{
   e_comp_wl->seat.resources =
     eina_list_remove(e_comp_wl->seat.resources, resource);
}

static void
_e_comp_wl_input_cb_bind_seat(struct wl_client *client, void *data EINA_UNUSED, uint32_t version, uint32_t id)
{
   struct wl_resource *res;

   res = wl_resource_create(client, &wl_seat_interface, version, id);
   if (!res)
     {
        ERR("Could not create seat resource");
        return;
     }

   /* store version of seat interface for reuse in updating capabilities */
   e_comp_wl->seat.version = version;
   e_comp_wl->seat.resources =
     eina_list_append(e_comp_wl->seat.resources, res);

   wl_resource_set_implementation(res, &_e_seat_interface,
                                  e_comp->wl_comp_data,
                                  _e_comp_wl_input_cb_unbind_seat);

   _e_comp_wl_input_update_seat_caps();
   if (e_comp_wl->seat.version >= WL_SEAT_NAME_SINCE_VERSION)
     wl_seat_send_name(res, e_comp_wl->seat.name);
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
_e_comp_wl_input_state_update(void)
{
   xkb_mod_mask_t latched = 0, locked = 0;

   /* unreference any existing keyboard state */
   if (e_comp_wl->xkb.state)
     {
        latched =
          xkb_state_serialize_mods(e_comp_wl->xkb.state,
                                   XKB_STATE_MODS_LATCHED);
        locked =
          xkb_state_serialize_mods(e_comp_wl->xkb.state,
                                   XKB_STATE_MODS_LOCKED);
        xkb_state_unref(e_comp_wl->xkb.state);
     }

   /* create a new xkb state */
   e_comp_wl->xkb.state = xkb_state_new(e_comp_wl->xkb.keymap);

   xkb_state_update_mask(e_comp_wl->xkb.state, 0,
                         latched, locked, e_comp_wl->kbd.choosen_group,
                         0, 0);
}

static void
_e_comp_wl_input_keymap_update(struct xkb_keymap *keymap)
{
   char *tmp;
   struct wl_resource *res;
   Eina_List *l;

   /* unreference any existing keymap */
   if (e_comp_wl->xkb.keymap)
     xkb_map_unref(e_comp_wl->xkb.keymap);

   /* unmap any existing keyboard area */
   if (e_comp_wl->xkb.area)
     munmap(e_comp_wl->xkb.area, e_comp_wl->xkb.size);
   if (e_comp_wl->xkb.fd >= 0) close(e_comp_wl->xkb.fd);


   /* increment keymap reference */
   e_comp_wl->xkb.keymap = keymap;

   /* update the state */
   _e_comp_wl_input_state_update();

   if (!(tmp = xkb_map_get_as_string(keymap)))
     {
        ERR("Could not get keymap string");
        return;
     }

   e_comp_wl->xkb.size = strlen(tmp) + 1;
   e_comp_wl->xkb.fd =
     _e_comp_wl_input_keymap_fd_get(e_comp_wl->xkb.size);
   if (e_comp_wl->xkb.fd < 0)
     {
        ERR("Could not create keymap file");
        free(tmp);
        return;
     }

   e_comp_wl->xkb.area =
     mmap(NULL, e_comp_wl->xkb.size, (PROT_READ | PROT_WRITE),
          MAP_SHARED, e_comp_wl->xkb.fd, 0);
   if (e_comp_wl->xkb.area == MAP_FAILED)
     {
        ERR("Failed to mmap keymap area: %m");
        free(tmp);
        return;
     }

   strcpy(e_comp_wl->xkb.area, tmp);
   free(tmp);

   /* send updated keymap */
   EINA_LIST_FOREACH(e_comp_wl->kbd.resources, l, res)
     wl_keyboard_send_keymap(res, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1,
                             e_comp_wl->xkb.fd, e_comp_wl->xkb.size);

   /* update modifiers */
   e_comp_wl_input_keyboard_modifiers_update();
}

EINTERN Eina_Bool
e_comp_wl_input_init(void)
{
   /* set default seat name */
   if (!e_comp_wl->seat.name)
     e_comp_wl->seat.name = "default";

   e_comp_wl->xkb.fd = -1;

   /* create the global resource for input seat */
   e_comp_wl->seat.global =
     wl_global_create(e_comp_wl->wl.disp, &wl_seat_interface, 4,
                      e_comp->wl_comp_data, _e_comp_wl_input_cb_bind_seat);
   if (!e_comp_wl->seat.global)
     {
        ERR("Could not create global for seat");
        return EINA_FALSE;
     }

   wl_array_init(&e_comp_wl->kbd.keys);

   E_EVENT_TEXT_INPUT_PANEL_VISIBILITY_CHANGE = ecore_event_type_new();

   _xkb_keymap_key_by_name = dlsym(NULL, "xkb_keymap_key_by_name");

    if (cached_keymap)
      _e_comp_wl_input_context_keymap_set(cached_keymap, cached_context);
    else
      e_comp_wl_input_keymap_set(NULL, NULL, NULL, NULL, NULL);

    if (choosen_group)
      e_comp_wl_input_keymap_index_set(choosen_group);
    else
      e_comp_wl_input_keymap_index_set(0);

   e_comp_wl_input_keyboard_modifiers_update();

   return EINA_TRUE;
}

EINTERN void
e_comp_wl_input_shutdown(void)
{
   struct wl_resource *res;

   E_FREE_FUNC(input_gen_modifiers, eina_hash_free);

   /* destroy pointer resources */
   EINA_LIST_FREE(e_comp_wl->ptr.resources, res)
     wl_resource_destroy(res);

   /* destroy keyboard resources */
   EINA_LIST_FREE(e_comp_wl->kbd.resources, res)
     wl_resource_destroy(res);
   e_comp_wl->kbd.resources = eina_list_free(e_comp_wl->kbd.resources);

   /* destroy touch resources */
   EINA_LIST_FREE(e_comp_wl->touch.resources, res)
     wl_resource_destroy(res);

   /* destroy e_comp_wl->kbd.keys array */
   wl_array_release(&e_comp_wl->kbd.keys);

   /* unmap any existing keyboard area */
   if (e_comp_wl->xkb.area)
     munmap(e_comp_wl->xkb.area, e_comp_wl->xkb.size);
   if (e_comp_wl->xkb.fd >= 0) close(e_comp_wl->xkb.fd);

   /* unreference any existing keyboard state */
   if (e_comp_wl->xkb.state)
     xkb_state_unref(e_comp_wl->xkb.state);

   /* unreference any existing keymap */
   if (e_comp_wl->xkb.keymap)
     xkb_map_unref(e_comp_wl->xkb.keymap);

   /* unreference any existing context */
   if (e_comp_wl->xkb.context)
     xkb_context_unref(e_comp_wl->xkb.context);

   /* destroy the global seat resource */
   if (e_comp_wl->seat.global)
     wl_global_destroy(e_comp_wl->seat.global);
   e_comp_wl->seat.global = NULL;
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

EINTERN Eina_Bool
e_comp_wl_input_keyboard_modifiers_serialize(void)
{
   Eina_Bool changed = EINA_FALSE;
   xkb_mod_mask_t mod;
   xkb_layout_index_t grp;

   mod = xkb_state_serialize_mods(e_comp_wl->xkb.state, XKB_STATE_DEPRESSED);
   changed |= mod != e_comp_wl->kbd.mod_depressed;
   e_comp_wl->kbd.mod_depressed = mod;

   mod = xkb_state_serialize_mods(e_comp_wl->xkb.state, XKB_STATE_MODS_LATCHED);
   changed |= mod != e_comp_wl->kbd.mod_latched;
   e_comp_wl->kbd.mod_latched = mod;

   mod = xkb_state_serialize_mods(e_comp_wl->xkb.state, XKB_STATE_MODS_LOCKED);
   changed |= mod != e_comp_wl->kbd.mod_locked;
   e_comp_wl->kbd.mod_locked = mod;

   grp = xkb_state_serialize_layout(e_comp_wl->xkb.state,
                                    XKB_STATE_LAYOUT_EFFECTIVE);
   if (grp != e_comp_wl->kbd.mod_group)
     {
        e_config->xkb.cur_group = grp;
        ecore_event_add(E_EVENT_XKB_CHANGED, NULL, NULL, NULL);
     }
   changed |= grp != e_comp_wl->kbd.mod_group;
   e_comp_wl->kbd.mod_group = grp;
   return changed;
}

EINTERN void
e_comp_wl_input_keyboard_modifiers_update(void)
{
   uint32_t serial;
   struct wl_resource *res;
   Eina_List *l;

   if (!e_comp_wl_input_keyboard_modifiers_serialize()) return;

   if ((!e_comp_wl->kbd.focused) || e_comp->input_key_grabs) return;

   serial = wl_display_next_serial(e_comp_wl->wl.disp);
   EINA_LIST_FOREACH(e_comp_wl->kbd.focused, l, res)
     wl_keyboard_send_modifiers(res, serial,
                                e_comp_wl->kbd.mod_depressed,
                                e_comp_wl->kbd.mod_latched,
                                e_comp_wl->kbd.mod_locked,
                                e_comp_wl->kbd.mod_group);
}

EINTERN void
e_comp_wl_input_keyboard_state_update(uint32_t keycode, Eina_Bool pressed)
{
   enum xkb_key_direction dir;

   if (!e_comp_wl->xkb.state) return;

   if (pressed) dir = XKB_KEY_DOWN;
   else dir = XKB_KEY_UP;

   e_comp_wl->kbd.mod_changed =
     xkb_state_update_key(e_comp_wl->xkb.state, keycode + 8, dir);

   e_comp_wl_input_keyboard_modifiers_update();
}

E_API void
e_comp_wl_input_pointer_enabled_set(Eina_Bool enabled)
{
   /* check for valid compositor data */
   if (!e_comp->wl_comp_data)
     {
        ERR("No compositor data");
        return;
     }

   e_comp_wl->ptr.enabled = !!enabled;
   _e_comp_wl_input_update_seat_caps();
}

E_API void
e_comp_wl_input_keyboard_enabled_set(Eina_Bool enabled)
{
   /* check for valid compositor data */
   if (!e_comp->wl_comp_data)
     {
        ERR("No compositor data");
        return;
     }

   e_comp_wl->kbd.enabled = !!enabled;
   _e_comp_wl_input_update_seat_caps();
}

static void
_e_comp_wl_input_context_keymap_set(struct xkb_keymap *keymap, struct xkb_context *context)
{
   if (e_comp_wl)
     {
        //we have a initializied and running compositor here
        if (e_comp_wl->xkb.context)
          xkb_context_unref(e_comp_wl->xkb.context);
        e_comp_wl->xkb.context = context;

        /* update compositor keymap */
        _e_comp_wl_input_keymap_update(keymap);
     }
   else
     {
        //we dont have a running compositor here, cache the values, init will pick them
        cached_context = context;
        cached_keymap = keymap;
     }

//set the values to the drm devices
#ifdef HAVE_WL_DRM
# ifdef HAVE_DRM2
   if (e_config->xkb.use_cache)
     {
        Ecore_Drm2_Device *dev;

        dev = ecore_evas_data_get(e_comp->ee, "device");
        if (dev)
          {
             ecore_drm2_device_keyboard_cached_context_set(dev, context);
             ecore_drm2_device_keyboard_cached_keymap_set(dev, keymap);
          }
     }
# else
   if (e_config->xkb.use_cache)
     ecore_drm_device_keyboard_cached_context_set(context);
   if (e_config->xkb.use_cache)
     ecore_drm_device_keyboard_cached_keymap_set(keymap);
# endif
#endif
}

E_API void
e_comp_wl_input_keymap_index_set(xkb_layout_index_t index)
{
   if (e_comp_wl)
     {
        e_comp_wl->kbd.choosen_group = index;
        _e_comp_wl_input_state_update();
        e_comp_wl_input_keyboard_modifiers_update();

     }
   else
     choosen_group = index;
}

E_API void
e_comp_wl_input_keymap_set(const char *rules, const char *model, const char *layout, const char *variant, const char *options)
{
   struct xkb_keymap *keymap;
   struct xkb_context *context;
   struct xkb_rule_names names;

   /* DBG("COMP_WL: Keymap Set: %s %s %s", rules, model, layout); */

   /* assemble xkb_rule_names so we can fetch keymap */
   memset(&names, 0, sizeof(names));

   if (rules) names.rules = rules;
   else names.rules = "evdev";

   if (model) names.model = model;
   else names.model = "pc105";

   if (layout) names.layout = layout;
   else names.layout = "us";

   if (variant) names.variant = variant;
   else names.variant = NULL;

   if (options) names.options = options;
   else names.options = NULL;

   /* create a new xkb context */
   context = xkb_context_new(0);

   /* fetch new keymap based on names */
   keymap = xkb_map_new_from_names(context, &names, 0);

   if (!keymap)
     {
        ERR("Failed to compile keymap");
        return;
     }

   _e_comp_wl_input_context_keymap_set(keymap, context);
}

E_API void
e_comp_wl_input_touch_enabled_set(Eina_Bool enabled)
{
   /* check for valid compositor data */
   if (!e_comp->wl_comp_data)
     {
        ERR("No compositor data");
        return;
     }

   e_comp_wl->touch.enabled = !!enabled;
   _e_comp_wl_input_update_seat_caps();
}

EINTERN Eina_Bool
e_comp_wl_input_touch_check(struct wl_resource *res)
{
   return wl_resource_instance_of(res, &wl_touch_interface,
                                  &_e_touch_interface);
}

EINTERN void
e_comp_wl_input_keyboard_modifers_clear(void)
{
   e_comp_wl->kbd.mod_depressed = 0;
   e_comp_wl->kbd.mod_latched = 0;
   e_comp_wl->kbd.mod_locked = 0;
   e_comp_wl->kbd.mod_group = 0;

   e_comp_wl_input_keyboard_modifiers_serialize();
}

static void
_event_generate(const char *key, const char *keyname, int mods, Eina_Bool up)
{
   Ecore_Event_Key *ev;
   int keycode;

   keycode = _xkb_keymap_key_by_name(e_comp_wl->xkb.keymap, keyname ?: key);
   if (keycode == -1)
     {
        ERR("no keycode found for key '%s'", keyname ?: key);
        return;
     }
   ev = calloc(1, sizeof(Ecore_Event_Key) + (2 * (strlen(key) + 1)));

   ev->keyname = (char *)(ev + 1);
   ev->key = ev->keyname + strlen(key) + 1;

   strcpy((char *)ev->keyname, key);
   strcpy((char *)ev->key, key);

   ev->window = e_comp->ee_win;
   ev->event_window = e_comp->ee_win;
   ev->timestamp = 0;
   ev->modifiers = mods;
   ev->keycode = keycode;
   ecore_event_add(up ? ECORE_EVENT_KEY_UP : ECORE_EVENT_KEY_DOWN, ev, NULL, NULL);
}

static void
_event_generate_mods(int mods, Eina_Bool up)
{
   if (!mods) return;
   if (mods & ECORE_EVENT_MODIFIER_SHIFT)
     _event_generate("Shift", "LFSH", mods, up);
   if (mods & ECORE_EVENT_MODIFIER_CTRL)
     _event_generate("Control_L", "LCTL", mods, up);
   if (mods & ECORE_EVENT_MODIFIER_ALT)
     _event_generate("Alt_L", "LALT", mods, up);
   if (mods & ECORE_EVENT_MODIFIER_WIN)
     _event_generate("Super_L", "LWIN", mods, up);
   if (mods & ECORE_EVENT_MODIFIER_ALTGR)
     _event_generate("Mode_switch", "ALGR", mods, up);
}

E_API void
e_comp_wl_input_keyboard_event_generate(const char *key, int mods, Eina_Bool up)
{
   const char *keyname = NULL;
   /* assumes qwerty layout */
   /* /usr/share/X11/xkb/keycodes/evdev */
   static const char *keycodes[] =
   {
      ['`'] = "TLDE",
      ['1'] = "AE01",
      ['2'] = "AE02",
      ['3'] = "AE03",
      ['4'] = "AE04",
      ['5'] = "AE05",
      ['6'] = "AE06",
      ['7'] = "AE07",
      ['8'] = "AE08",
      ['9'] = "AE09",
      ['0'] = "AE10",
      ['-'] = "AE11",
      ['='] = "AE12",
      //''] = "BKSP",
      ['\t'] = "TAB",
      ['q'] = "AD01",
      ['w'] = "AD02",
      ['e'] = "AD03",
      ['r'] = "AD04",
      ['t'] = "AD05",
      ['y'] = "AD06",
      ['u'] = "AD07",
      ['i'] = "AD08",
      ['o'] = "AD09",
      ['p'] = "AD10",
      ['['] = "AD11",
      [']'] = "AD12",
      ['\\'] = "BKSL",
      ['\r'] = "RTRN",
      //''] = "CAPS",
      ['a'] = "AC01",
      ['s'] = "AC02",
      ['d'] = "AC03",
      ['f'] = "AC04",
      ['g'] = "AC05",
      ['h'] = "AC06",
      ['j'] = "AC07",
      ['k'] = "AC08",
      ['l'] = "AC09",
      [';'] = "AC10",
      ['\''] = "AC11",
      //''] = "LFSH",
      ['z'] = "AB01",
      ['x'] = "AB02",
      ['c'] = "AB03",
      ['v'] = "AB04",
      ['b'] = "AB05",
      ['n'] = "AB06",
      ['m'] = "AB07",
      [','] = "AB08",
      ['.'] = "AB09",
      ['/'] = "AB10",
      //''] = "RTSH",
      [' '] = "SPCE",
   };

   if (!_xkb_keymap_key_by_name)
     {
        ERR("xkbcommon >= 0.6.0 required for keyboard event generation!");
        return;
     }
   EINA_SAFETY_ON_NULL_RETURN(key);
   EINA_SAFETY_ON_TRUE_RETURN(!key[0]);
   if (!input_gen_modifiers)
     {
//<RTSH> = 62;
//<LALT> = 64;
//<LCTL> = 37;
//<RCTL> = 105;
//<RALT> = 108;
//<LWIN> = 133;
//<RWIN> = 134;
//<COMP> = 135;
//alias <MENU> = <COMP>;
//<ESC> = 9;
        static const char *modcodes[] =
        {
           "Shift_L",
           "LFSH",

           "Control_L",
           "LCTL",

           "Super_L",
           "LWIN",

           "Alt_L",
           "LALT",

           "Escape",
           "ESC",

           "Alt_R",
           "RALT",

           "Super_R",
           "RWIN",

           "Menu",
           "MENU",

           "Control_R",
           "RCTRL",

           "Mode_switch",
           "ALGR",

           "Return",
           "RTRN",

           "Caps_Lock",
           "CAPS",
        };
        unsigned int i;

        input_gen_modifiers = eina_hash_string_superfast_new(NULL);
        for (i = 0; i < EINA_C_ARRAY_LENGTH(modcodes); i += 2)
          eina_hash_add(input_gen_modifiers, modcodes[i], modcodes[i + 1]);
     }

   if (!up)
     _event_generate_mods(mods, up);
   keyname = eina_hash_find(input_gen_modifiers, key);
   if ((!keyname) && (!key[1]))
     {
        if (key[0] < (int)EINA_C_ARRAY_LENGTH(keycodes))
          keyname = keycodes[(unsigned char)key[0]];
     }
   _event_generate(key, keyname, mods, up);
   if (up)
     _event_generate_mods(mods, up);
}

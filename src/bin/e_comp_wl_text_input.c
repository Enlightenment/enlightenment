/* zwp_text_input_v3 (the application side) and zwp_input_method_v2 (the
 * input method side). Neither client can see the other, so the compositor
 * relays between them: each side's requests are double-buffered and only the
 * state latched by its commit is forwarded, followed by the done event that
 * applies it.
 *
 * Text-input focus follows keyboard focus: e_comp_wl.c calls
 * e_comp_wl_text_input_focus_set/unset where it resolves the surface
 * wl_keyboard enters. */
#define E_COMP_WL
#include "e.h"
#include "text-input-unstable-v3-server-protocol.h"
#include "input-method-unstable-v2-server-protocol.h"

typedef struct
{
   Eina_Bool enabled;
   /* NULL until set: "does not support surrounding text" is not "" */
   char *surrounding;
   int32_t cursor, anchor;
   uint32_t change_cause;
   Eina_Bool change_cause_set;
   uint32_t content_hint, content_purpose;
   Eina_Bool content_set;
} Text_Input_State;

typedef struct
{
   struct wl_resource *res;
   Text_Input_State pending, current;
   /* the number of commit requests, echoed in every done event */
   uint32_t serial;
   Eina_Bool entered;
} Text_Input;

typedef struct
{
   struct wl_resource *res;
   /* a second input method on the seat is sent unavailable and ignored */
   Eina_Bool inert;
   struct
   {
      char *preedit;
      int32_t preedit_begin, preedit_end;
      Eina_Bool preedit_set;
      char *commit;
      Eina_Bool commit_set;
      uint32_t delete_before, delete_after;
      Eina_Bool delete_set;
   } pending;
} Input_Method;

static struct wl_global *_text_input_manager_global;
static struct wl_global *_input_method_manager_global;
static Eina_List *_text_inputs;
/* E has one seat, so there is at most one input method and one enabled
 * text input. */
static Input_Method *_input_method;
static Text_Input *_active_ti;
static struct wl_resource *_focus_surface;
static struct wl_client *_focus_client;
static struct wl_listener _focus_surface_destroy;

static void
_e_text_input_state_reset(Text_Input_State *st)
{
   free(st->surrounding);
   memset(st, 0, sizeof(*st));
   st->change_cause = ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD;
}

static void
_e_text_input_state_copy(Text_Input_State *dst, const Text_Input_State *src)
{
   char *surrounding = src->surrounding ? strdup(src->surrounding) : NULL;

   free(dst->surrounding);
   *dst = *src;
   dst->surrounding = surrounding;
}

static void
_e_input_method_pending_clear(Input_Method *im)
{
   free(im->pending.preedit);
   free(im->pending.commit);
   memset(&im->pending, 0, sizeof(im->pending));
}

static Input_Method *
_e_input_method_get(void)
{
   if ((!_input_method) || _input_method->inert) return NULL;
   return _input_method;
}

/* Only what the client has set is sent: an input method may take a missing
 * surrounding_text before the first done to mean the field cannot give it. */
static void
_e_input_method_state_send(Input_Method *im, Text_Input *ti)
{
   if (ti->current.surrounding)
     zwp_input_method_v2_send_surrounding_text(im->res, ti->current.surrounding,
                                               ti->current.cursor,
                                               ti->current.anchor);
   if (ti->current.change_cause_set)
     zwp_input_method_v2_send_text_change_cause(im->res,
                                                ti->current.change_cause);
   if (ti->current.content_set)
     zwp_input_method_v2_send_content_type(im->res, ti->current.content_hint,
                                           ti->current.content_purpose);
   zwp_input_method_v2_send_done(im->res);
}

static void
_e_input_method_activate(Input_Method *im, Text_Input *ti)
{
   zwp_input_method_v2_send_activate(im->res);
   _e_input_method_pending_clear(im);
   _e_input_method_state_send(im, ti);
}

static void
_e_input_method_deactivate(void)
{
   Input_Method *im = _e_input_method_get();

   if (!_active_ti) return;
   _active_ti->current.enabled = EINA_FALSE;
   _active_ti = NULL;
   if (!im) return;
   zwp_input_method_v2_send_deactivate(im->res);
   zwp_input_method_v2_send_done(im->res);
   /* text composed for this field must not reach the next one */
   _e_input_method_pending_clear(im);
}

static void
_e_text_input_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void
_e_text_input_cb_enable(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   Text_Input *ti = wl_resource_get_user_data(resource);

   if (!ti->entered) return;
   _e_text_input_state_reset(&ti->pending);
   ti->pending.enabled = EINA_TRUE;
}

static void
_e_text_input_cb_disable(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   Text_Input *ti = wl_resource_get_user_data(resource);

   if (!ti->entered) return;
   ti->pending.enabled = EINA_FALSE;
}

static void
_e_text_input_cb_surrounding_text_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *text, int32_t cursor, int32_t anchor)
{
   Text_Input *ti = wl_resource_get_user_data(resource);

   if (!ti->entered) return;
   free(ti->pending.surrounding);
   ti->pending.surrounding = strdup(text);
   ti->pending.cursor = cursor;
   ti->pending.anchor = anchor;
}

static void
_e_text_input_cb_text_change_cause_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, uint32_t cause)
{
   Text_Input *ti = wl_resource_get_user_data(resource);

   if (!ti->entered) return;
   ti->pending.change_cause = cause;
   ti->pending.change_cause_set = EINA_TRUE;
}

static void
_e_text_input_cb_content_type_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, uint32_t hint, uint32_t purpose)
{
   Text_Input *ti = wl_resource_get_user_data(resource);

   if (!ti->entered) return;
   ti->pending.content_hint = hint;
   ti->pending.content_purpose = purpose;
   ti->pending.content_set = EINA_TRUE;
}

static void
_e_text_input_cb_cursor_rectangle_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, int32_t x EINA_UNUSED, int32_t y EINA_UNUSED, int32_t w EINA_UNUSED, int32_t h EINA_UNUSED)
{
}

static void
_e_text_input_cb_commit(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   Text_Input *ti = wl_resource_get_user_data(resource);
   Input_Method *im;
   Eina_Bool was_enabled;

   /* the serial counts every commit, even those of an unfocused object */
   ti->serial++;
   if (!ti->entered) return;
   /* only one text input may be enabled at a time */
   if (ti->pending.enabled && _active_ti && (_active_ti != ti)) return;

   was_enabled = ti->current.enabled;
   _e_text_input_state_copy(&ti->current, &ti->pending);
   /* the change cause describes one change, so it does not carry over */
   ti->pending.change_cause = ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD;
   ti->pending.change_cause_set = EINA_FALSE;

   if (ti->current.enabled)
     {
        _active_ti = ti;
        im = _e_input_method_get();
        if (!im) return;
        if (was_enabled)
          _e_input_method_state_send(im, ti);
        else
          _e_input_method_activate(im, ti);
     }
   else if (was_enabled)
     _e_input_method_deactivate();
}

static const struct zwp_text_input_v3_interface _e_text_input_interface =
{
   _e_text_input_cb_destroy,
   _e_text_input_cb_enable,
   _e_text_input_cb_disable,
   _e_text_input_cb_surrounding_text_set,
   _e_text_input_cb_text_change_cause_set,
   _e_text_input_cb_content_type_set,
   _e_text_input_cb_cursor_rectangle_set,
   _e_text_input_cb_commit,
};

static void
_e_text_input_res_destroy(struct wl_resource *resource)
{
   Text_Input *ti = wl_resource_get_user_data(resource);

   _text_inputs = eina_list_remove(_text_inputs, ti);
   if (_active_ti == ti) _e_input_method_deactivate();
   _e_text_input_state_reset(&ti->pending);
   _e_text_input_state_reset(&ti->current);
   free(ti);
}

static void
_e_text_input_manager_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void
_e_text_input_manager_cb_text_input_get(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *seat EINA_UNUSED)
{
   Text_Input *ti;
   struct wl_resource *res;

   ti = E_NEW(Text_Input, 1);
   if (!ti)
     {
        wl_resource_post_no_memory(resource);
        return;
     }
   res = wl_resource_create(client, &zwp_text_input_v3_interface,
                            wl_resource_get_version(resource), id);
   if (!res)
     {
        free(ti);
        wl_resource_post_no_memory(resource);
        return;
     }
   ti->res = res;
   ti->current.change_cause = ti->pending.change_cause =
     ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD;
   wl_resource_set_implementation(res, &_e_text_input_interface, ti,
                                  _e_text_input_res_destroy);
   _text_inputs = eina_list_append(_text_inputs, ti);

   /* browsers create a text input per field, long after their window got
    * the focus */
   if (_focus_client != client) return;
   ti->entered = EINA_TRUE;
   zwp_text_input_v3_send_enter(res, _focus_surface);
}

static const struct zwp_text_input_manager_v3_interface _e_text_input_manager_interface =
{
   _e_text_input_manager_cb_destroy,
   _e_text_input_manager_cb_text_input_get,
};

static void
_e_text_input_manager_cb_bind(struct wl_client *client, void *data EINA_UNUSED, uint32_t version, uint32_t id)
{
   struct wl_resource *res;

   res = wl_resource_create(client, &zwp_text_input_manager_v3_interface,
                            version, id);
   if (!res)
     {
        wl_client_post_no_memory(client);
        return;
     }
   wl_resource_set_implementation(res, &_e_text_input_manager_interface, NULL,
                                  NULL);
}

static void
_e_input_method_cb_commit_string(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *text)
{
   Input_Method *im = wl_resource_get_user_data(resource);

   if (im->inert) return;
   free(im->pending.commit);
   im->pending.commit = strdup(text);
   im->pending.commit_set = EINA_TRUE;
}

static void
_e_input_method_cb_preedit_string_set(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, const char *text, int32_t cursor_begin, int32_t cursor_end)
{
   Input_Method *im = wl_resource_get_user_data(resource);

   if (im->inert) return;
   free(im->pending.preedit);
   im->pending.preedit = strdup(text);
   im->pending.preedit_begin = cursor_begin;
   im->pending.preedit_end = cursor_end;
   im->pending.preedit_set = EINA_TRUE;
}

static void
_e_input_method_cb_surrounding_text_delete(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, uint32_t before_length, uint32_t after_length)
{
   Input_Method *im = wl_resource_get_user_data(resource);

   if (im->inert) return;
   im->pending.delete_before = before_length;
   im->pending.delete_after = after_length;
   im->pending.delete_set = EINA_TRUE;
}

/* The serial is not checked: a stale one means the input method answered an
 * older state of the field, and dropping its text would lose what the user
 * typed. */
static void
_e_input_method_cb_commit(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, uint32_t serial EINA_UNUSED)
{
   Input_Method *im = wl_resource_get_user_data(resource);
   Text_Input *ti = _active_ti;

   if (im->inert) return;
   if (ti)
     {
        if (im->pending.preedit_set)
          zwp_text_input_v3_send_preedit_string(ti->res, im->pending.preedit,
                                                im->pending.preedit_begin,
                                                im->pending.preedit_end);
        if (im->pending.delete_set)
          zwp_text_input_v3_send_delete_surrounding_text(ti->res,
                                                         im->pending.delete_before,
                                                         im->pending.delete_after);
        if (im->pending.commit_set)
          zwp_text_input_v3_send_commit_string(ti->res, im->pending.commit);
        zwp_text_input_v3_send_done(ti->res, ti->serial);
     }
   _e_input_method_pending_clear(im);
}

static void
_e_input_method_cb_resource_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static const struct zwp_input_popup_surface_v2_interface _e_input_popup_surface_interface =
{
   _e_input_method_cb_resource_destroy,
};

static const struct zwp_input_method_keyboard_grab_v2_interface _e_input_method_keyboard_grab_interface =
{
   _e_input_method_cb_resource_destroy,
};

/* Not implemented yet: the popup is never shown and the grab never gets a
 * key, so an input method composing from the keyboard does not work. */
static void
_e_input_method_cb_input_popup_surface_get(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface EINA_UNUSED)
{
   struct wl_resource *res;

   res = wl_resource_create(client, &zwp_input_popup_surface_v2_interface,
                            wl_resource_get_version(resource), id);
   if (!res)
     {
        wl_client_post_no_memory(client);
        return;
     }
   wl_resource_set_implementation(res, &_e_input_popup_surface_interface,
                                  NULL, NULL);
}

static void
_e_input_method_cb_keyboard_grab(struct wl_client *client, struct wl_resource *resource, uint32_t id)
{
   struct wl_resource *res;

   res = wl_resource_create(client, &zwp_input_method_keyboard_grab_v2_interface,
                            wl_resource_get_version(resource), id);
   if (!res)
     {
        wl_client_post_no_memory(client);
        return;
     }
   wl_resource_set_implementation(res, &_e_input_method_keyboard_grab_interface,
                                  NULL, NULL);
}

static const struct zwp_input_method_v2_interface _e_input_method_interface =
{
   _e_input_method_cb_commit_string,
   _e_input_method_cb_preedit_string_set,
   _e_input_method_cb_surrounding_text_delete,
   _e_input_method_cb_commit,
   _e_input_method_cb_input_popup_surface_get,
   _e_input_method_cb_keyboard_grab,
   _e_input_method_cb_resource_destroy,
};

static void
_e_input_method_res_destroy(struct wl_resource *resource)
{
   Input_Method *im = wl_resource_get_user_data(resource);

   if (_input_method == im) _input_method = NULL;
   _e_input_method_pending_clear(im);
   free(im);
}

static void
_e_input_method_manager_cb_input_method_get(struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat EINA_UNUSED, uint32_t id)
{
   Input_Method *im;
   struct wl_resource *res;

   im = E_NEW(Input_Method, 1);
   if (!im)
     {
        wl_resource_post_no_memory(resource);
        return;
     }
   res = wl_resource_create(client, &zwp_input_method_v2_interface,
                            wl_resource_get_version(resource), id);
   if (!res)
     {
        free(im);
        wl_resource_post_no_memory(resource);
        return;
     }
   im->res = res;
   wl_resource_set_implementation(res, &_e_input_method_interface, im,
                                  _e_input_method_res_destroy);

   /* the newcomer loses, so a second input method cannot take over the one
    * the user is typing with */
   if (_input_method)
     {
        im->inert = EINA_TRUE;
        zwp_input_method_v2_send_unavailable(res);
        return;
     }
   _input_method = im;
   if (_active_ti) _e_input_method_activate(im, _active_ti);
}

static void
_e_input_method_manager_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static const struct zwp_input_method_manager_v2_interface _e_input_method_manager_interface =
{
   _e_input_method_manager_cb_input_method_get,
   _e_input_method_manager_cb_destroy,
};

static void
_e_input_method_manager_cb_bind(struct wl_client *client, void *data EINA_UNUSED, uint32_t version, uint32_t id)
{
   struct wl_resource *res;

   res = wl_resource_create(client, &zwp_input_method_manager_v2_interface,
                            version, id);
   if (!res)
     {
        wl_client_post_no_memory(client);
        return;
     }
   wl_resource_set_implementation(res, &_e_input_method_manager_interface, NULL,
                                  NULL);
}

static void
_e_text_input_focus_leave(void)
{
   Text_Input *ti;
   Eina_List *l;

   if (!_focus_surface) return;
   wl_list_remove(&_focus_surface_destroy.link);
   if (_active_ti && (wl_resource_get_client(_active_ti->res) == _focus_client))
     _e_input_method_deactivate();

   EINA_LIST_FOREACH(_text_inputs, l, ti)
     {
        if (!ti->entered) continue;
        if (wl_resource_get_client(ti->res) != _focus_client) continue;
        ti->entered = EINA_FALSE;
        _e_text_input_state_reset(&ti->pending);
        _e_text_input_state_reset(&ti->current);
        zwp_text_input_v3_send_leave(ti->res, _focus_surface);
     }
   _focus_surface = NULL;
   _focus_client = NULL;
}

/* E's surface destructor already moves the focus away, this only makes sure
 * _focus_surface never outlives the resource. */
static void
_e_text_input_focus_surface_destroy_cb(struct wl_listener *listener EINA_UNUSED, void *data EINA_UNUSED)
{
   _e_text_input_focus_leave();
}

EINTERN void
e_comp_wl_text_input_focus_set(E_Client *ec)
{
   struct wl_resource *surface = ec->comp_data->surface;
   Text_Input *ti;
   Eina_List *l;

   if (surface == _focus_surface) return;
   _e_text_input_focus_leave();

   _focus_surface = surface;
   _focus_client = wl_resource_get_client(surface);
   wl_resource_add_destroy_listener(surface, &_focus_surface_destroy);
   EINA_LIST_FOREACH(_text_inputs, l, ti)
     {
        if (wl_resource_get_client(ti->res) != _focus_client) continue;
        ti->entered = EINA_TRUE;
        zwp_text_input_v3_send_enter(ti->res, surface);
     }
}

/* The evas focus callbacks can come in either order, so a late focus out
 * must not take away the focus another surface already got. */
EINTERN void
e_comp_wl_text_input_focus_unset(E_Client *ec)
{
   if (ec->comp_data->surface != _focus_surface) return;
   _e_text_input_focus_leave();
}

EINTERN Eina_Bool
e_comp_wl_text_input_init(void)
{
   _focus_surface_destroy.notify = _e_text_input_focus_surface_destroy_cb;
   _text_input_manager_global =
     wl_global_create(e_comp_wl->wl.disp, &zwp_text_input_manager_v3_interface,
                      1, NULL, _e_text_input_manager_cb_bind);
   if (!_text_input_manager_global)
     {
        ERR("Could not add zwp_text_input_manager_v3 to wayland globals");
        return EINA_FALSE;
     }
   _input_method_manager_global =
     wl_global_create(e_comp_wl->wl.disp,
                      &zwp_input_method_manager_v2_interface, 1, NULL,
                      _e_input_method_manager_cb_bind);
   if (!_input_method_manager_global)
     {
        ERR("Could not add zwp_input_method_manager_v2 to wayland globals");
        return EINA_FALSE;
     }
   return EINA_TRUE;
}

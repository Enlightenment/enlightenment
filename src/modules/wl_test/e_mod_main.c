/* Test-only back door into the compositor.
 *
 * Built only with -Dtests=true. See src/protocol/wl-test.xml for what it
 * exposes and why.
 *
 * Everything here runs inside E's process, which is the whole point: it can
 * answer what E *believes* about a surface, not merely what E put on the
 * wire. It is also, for the same reason, coupled to E's internals and will
 * need updating when they move. Keep the surface area small.
 */
#include "e.h"
#include "wl-test-server-protocol.h"

E_API E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Wl_Test" };

typedef struct _Wl_Test_Client Wl_Test_Client;
typedef struct _Wl_Test_Touch Wl_Test_Touch;

struct _Wl_Test_Client
{
   struct wl_listener destroy;
   unsigned int serial;
};

struct _Wl_Test_Touch
{
   int id, x, y;
};

static struct wl_global *_wl_test_global = NULL;
static struct wl_listener _client_created_listener;
static Eina_List *_clients = NULL;
static unsigned int _next_serial = 0;

/* Window ids are assigned on first mention, as the module may be loaded
 * after windows exist. They are never reused, and 0 means no window. */
static Eina_Hash *_client_ids = NULL;
static unsigned int _next_client_id = 0;
static E_Client_Hook *_client_del_hook = NULL;

/* Pending sync requests, drained from an ecore job so that one main loop
 * iteration has run before sync_done goes out. */
static Eina_List *_pending_syncs = NULL;
static Ecore_Job *_sync_job = NULL;

static Eina_List *_input_handlers = NULL;

/* Touch points that are down, so touch_up lifts each where it last was. */
static Eina_List *_touches = NULL;

/* The last pointer position posted, while that motion is still queued. */
static int _posted_x = 0, _posted_y = 0;
static Eina_Bool _posted_pending = EINA_FALSE;

static void
_wl_test_cb_client_destroy(struct wl_listener *listener, void *data EINA_UNUSED)
{
   Wl_Test_Client *tc = wl_container_of(listener, tc, destroy);

   _clients = eina_list_remove(_clients, tc);
   free(tc);
}

static void
_wl_test_cb_client_created(struct wl_listener *listener EINA_UNUSED, void *data)
{
   struct wl_client *client = data;
   Wl_Test_Client *tc;

   tc = calloc(1, sizeof(*tc));
   if (!tc)
     {
        ERR("wl_test: out of memory numbering a client");
        return;
     }

   tc->serial = ++_next_serial;
   tc->destroy.notify = _wl_test_cb_client_destroy;
   wl_client_add_destroy_listener(client, &tc->destroy);
   _clients = eina_list_append(_clients, tc);
}

static unsigned int
_wl_test_client_serial_get(struct wl_client *client)
{
   struct wl_listener *listener;
   Wl_Test_Client *tc;

   listener = wl_client_get_destroy_listener(client, _wl_test_cb_client_destroy);
   if (!listener) return 0;
   tc = wl_container_of(listener, tc, destroy);
   return tc->serial;
}

static E_Client *
_wl_test_ec_get(struct wl_resource *surface)
{
   E_Client *ec;

   if (!surface) return NULL;
   ec = wl_resource_get_user_data(surface);
   if (!ec) return NULL;
   if (e_object_is_del(E_OBJECT(ec))) return NULL;
   return ec;
}

static unsigned int
_wl_test_client_id(E_Client *ec)
{
   unsigned int id;

   if (!ec) return 0;
   id = (unsigned int)(uintptr_t)eina_hash_find(_client_ids, &ec);
   if (id) return id;

   id = ++_next_client_id;
   eina_hash_add(_client_ids, &ec, (void *)(uintptr_t)id);
   return id;
}

static void
_wl_test_cb_client_del(void *data EINA_UNUSED, E_Client *ec)
{
   eina_hash_del_by_key(_client_ids, &ec);
}

static unsigned int
_wl_test_client_states(E_Client *ec)
{
   unsigned int states = 0;

   if (ec->focused) states |= WL_TEST_CLIENT_STATE_FOCUSED;
   /* Not ec->visible: an internal window whose canvas could not be created
    * keeps it set with nothing on screen. */
   if (ec->frame && evas_object_visible_get(ec->frame))
     states |= WL_TEST_CLIENT_STATE_VISIBLE;
   if (ec->iconic) states |= WL_TEST_CLIENT_STATE_ICONIFIED;
   if (ec->maximized & E_MAXIMIZE_HORIZONTAL)
     states |= WL_TEST_CLIENT_STATE_MAXIMIZED_H;
   if (ec->maximized & E_MAXIMIZE_VERTICAL)
     states |= WL_TEST_CLIENT_STATE_MAXIMIZED_V;
   if (ec->fullscreen) states |= WL_TEST_CLIENT_STATE_FULLSCREEN;
   if (ec->sticky) states |= WL_TEST_CLIENT_STATE_STICKY;
   if (ec->shaded) states |= WL_TEST_CLIENT_STATE_SHADED;
   if (ec->internal) states |= WL_TEST_CLIENT_STATE_INTERNAL;

   return states;
}

static void
_wl_test_send_client_info(struct wl_resource *resource, E_Client *ec)
{
   unsigned int output = 0;
   Eina_List *l;
   E_Zone *zone;
   int i = 0;

   EINA_LIST_FOREACH(e_comp->zones, l, zone)
     {
        if (zone == ec->zone) { output = i; break; }
        i++;
     }

   wl_test_send_client_info(resource,
                            _wl_test_client_id(ec),
                            ec->netwm.pid,
                            ec->icccm.title ?: "",
                            ec->icccm.class ?: "",
                            _wl_test_client_id(ec->parent),
                            ec->x, ec->y, ec->w, ec->h,
                            _wl_test_client_states(ec),
                            output);
}

static void
_wl_test_cb_get_clients(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   E_Client *ec;

   E_CLIENT_FOREACH(ec)
     {
        if (e_object_is_del(E_OBJECT(ec))) continue;
        _wl_test_send_client_info(resource, ec);
     }

   wl_test_send_clients_done(resource);
}

static E_Client *
_wl_test_client_by_id(struct wl_resource *resource, uint32_t id)
{
   E_Client *ec;

   if (id)
     {
        E_CLIENT_FOREACH(ec)
          {
             if (e_object_is_del(E_OBJECT(ec))) continue;
             if (_wl_test_client_id(ec) == id) return ec;
          }
     }

   wl_resource_post_error(resource, WL_TEST_ERROR_UNKNOWN_CLIENT,
                          "no window with id %u", id);
   return NULL;
}

static void
_wl_test_cb_client_action(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, uint32_t id, const char *name, const char *params)
{
   E_Client *ec;
   E_Action *act;

   ec = _wl_test_client_by_id(resource, id);
   if (!ec) return;

   act = e_action_find(name);
   if ((!act) || (!act->func.go))
     {
        wl_resource_post_error(resource, WL_TEST_ERROR_UNKNOWN_ACTION,
                               "no action named '%s'", name ?: "");
        return;
     }

   /* Actions tell NULL params, which selects their default, from "". */
   act->func.go(E_OBJECT(ec), (params && params[0]) ? params : NULL);
}

static void
_wl_test_cb_client_action_end(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, uint32_t id, const char *name, const char *params)
{
   E_Client *ec;
   E_Action *act;

   ec = _wl_test_client_by_id(resource, id);
   if (!ec) return;

   act = e_action_find(name);
   if ((!act) || (!act->func.end))
     {
        wl_resource_post_error(resource, WL_TEST_ERROR_UNKNOWN_ACTION,
                               "no action named '%s' with a second half",
                               name ?: "");
        return;
     }

   act->func.end(E_OBJECT(ec), (params && params[0]) ? params : NULL);
}

static void
_wl_test_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void
_wl_test_cb_get_client_serial(struct wl_client *client, struct wl_resource *resource)
{
   wl_test_send_client_serial(resource, _wl_test_client_serial_get(client));
}

static void
_wl_test_cb_get_surface_info(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, struct wl_resource *surface)
{
   E_Client *ec;

   ec = _wl_test_ec_get(surface);
   if (!ec)
     {
        wl_test_send_surface_unknown(resource, surface);
        return;
     }

   wl_test_send_surface_info(resource, surface,
                            ec->x, ec->y, ec->w, ec->h,
                            evas_object_visible_get(ec->frame) ? 1 : 0,
                            (e_client_focused_get() == ec) ? 1 : 0);
}

static void
_wl_test_cb_move_surface(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, struct wl_resource *surface, int32_t x, int32_t y)
{
   E_Client *ec;

   ec = _wl_test_ec_get(surface);
   if (!ec) return;

   evas_object_move(ec->frame, x, y);
}

static void
_wl_test_cb_zone_add(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, int32_t x, int32_t y, int32_t w, int32_t h)
{
   E_Zone *zone;
   int num = eina_list_count(e_comp->zones);
   char id[32];

   snprintf(id, sizeof(id), "e-test-%d", num);

   zone = e_zone_new(num, num, x, y, w, h);
   if (!zone)
     {
        ERR("wl_test: could not create zone");
        return;
     }

   /* e_comp_wl_output_init() finds the zone with e_zone_for_id_get(), which
    * matches on randr2_id. A zone made outside the randr path has none, so
    * set one or the output is silently dropped. */
   zone->randr2_id = strdup(id);

   if (!e_comp_wl_output_init(id, NULL, NULL, x, y, w, h, 0, 0, 0, 0, 0, num))
     ERR("wl_test: could not create wl_output for zone %d", num);
}

/* Input is posted as ecore events, the way a backend delivers it, rather than
 * fed to evas directly. Wayland clients are reached through evas callbacks
 * on their frame, but a good part of E listens on the ecore side of the same
 * event: wl_keyboard.key and key bindings, relative pointer motion, pointer
 * constraints, popup grab dismissal and the idle timer. Feeding evas would
 * skip all of it. The buffer canvas is registered with ecore_input_evas,
 * which carries the event on to evas after E's own handlers have run. */
static unsigned int
_input_timestamp(void)
{
   return (unsigned int)(ecore_time_get() * 1000.0);
}

static void
_input_mouse_post(int type, int device, int button, int x, int y)
{
   Ecore_Event_Mouse_Button *ev;

   ev = calloc(1, sizeof(*ev));
   if (!ev) return;

   ev->window = e_comp->ee_win;
   ev->event_window = e_comp->ee_win;
   ev->root_window = e_comp->ee_win;
   ev->timestamp = _input_timestamp();
   ev->same_screen = 1;
   ev->buttons = button;
   ev->x = ev->root.x = x;
   ev->y = ev->root.y = y;
   ev->multi.device = device;
   ev->multi.x = ev->multi.root.x = x;
   ev->multi.y = ev->multi.root.y = y;
   ev->multi.radius = 1;
   ev->multi.radius_x = 1;
   ev->multi.radius_y = 1;
   ev->multi.pressure = 1.0;

   ecore_event_add(type, ev, NULL, NULL);
}

static void
_input_move_post(int device, int x, int y)
{
   Ecore_Event_Mouse_Move *ev;

   ev = calloc(1, sizeof(*ev));
   if (!ev) return;

   ev->window = e_comp->ee_win;
   ev->event_window = e_comp->ee_win;
   ev->root_window = e_comp->ee_win;
   ev->timestamp = _input_timestamp();
   ev->same_screen = 1;
   ev->x = ev->root.x = x;
   ev->y = ev->root.y = y;
   ev->multi.device = device;
   ev->multi.x = ev->multi.root.x = x;
   ev->multi.y = ev->multi.root.y = y;
   ev->multi.radius = 1;
   ev->multi.radius_x = 1;
   ev->multi.radius_y = 1;
   ev->multi.pressure = 1.0;

   ecore_event_add(ECORE_EVENT_MOUSE_MOVE, ev, NULL, NULL);
}

/* ecore_event_add queues, so until a posted motion has been dispatched evas
 * still reports the position from before it. A button pressed in between
 * must use the posted position, or it clicks where the pointer used to be. */
static void
_pointer_xy_get(int *x, int *y)
{
   if (_posted_pending)
     {
        *x = _posted_x;
        *y = _posted_y;
        return;
     }
   evas_pointer_canvas_xy_get(e_comp->evas, x, y);
}

static Eina_Bool
_input_cb_mouse_move(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_Event_Mouse_Move *ev = event;

   if ((ev->window == e_comp->ee_win) && (!ev->multi.device))
     _posted_pending = EINA_FALSE;
   return ECORE_CALLBACK_PASS_ON;
}

static void
_pointer_motion(int x, int y)
{
   _posted_x = x;
   _posted_y = y;
   _posted_pending = EINA_TRUE;
   _input_move_post(0, x, y);
}

static void
_wl_test_cb_pointer_warp(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, int32_t x, int32_t y)
{
   _pointer_motion(x, y);
}

static void
_wl_test_cb_pointer_move(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, int32_t dx, int32_t dy)
{
   int px, py;

   _pointer_xy_get(&px, &py);
   _pointer_motion(px + dx, py + dy);
}

static void
_wl_test_cb_pointer_button(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, uint32_t button, uint32_t pressed)
{
   int b, x, y;

   /* evas numbers buttons from 1 in X11 order, where right and middle are
    * the other way round from linux/input.h. */
   switch (button)
     {
      case BTN_LEFT:   b = 1; break;
      case BTN_MIDDLE: b = 2; break;
      case BTN_RIGHT:  b = 3; break;
      default: b = (int)(button - BTN_LEFT) + 1; break;
     }

   _pointer_xy_get(&x, &y);
   _input_mouse_post(pressed ? ECORE_EVENT_MOUSE_BUTTON_DOWN
                             : ECORE_EVENT_MOUSE_BUTTON_UP, 0, b, x, y);
}

static void
_wl_test_cb_pointer_axis(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, uint32_t direction, int32_t steps)
{
   Ecore_Event_Mouse_Wheel *ev;
   int x, y;

   ev = calloc(1, sizeof(*ev));
   if (!ev) return;

   _pointer_xy_get(&x, &y);
   ev->window = e_comp->ee_win;
   ev->event_window = e_comp->ee_win;
   ev->root_window = e_comp->ee_win;
   ev->timestamp = _input_timestamp();
   ev->same_screen = 1;
   ev->direction = direction ? 1 : 0;
   ev->z = steps;
   ev->x = ev->root.x = x;
   ev->y = ev->root.y = y;

   ecore_event_add(ECORE_EVENT_MOUSE_WHEEL, ev, NULL, NULL);
}

/* ecore_input_evas turns an event with a non-zero multi.device into an evas
 * multi event, and E forwards that device number as the wl_touch id. Device
 * 0 is the pointer, which is why the protocol rejects touch id 0. */
static Eina_Bool
_wl_test_touch_id_check(struct wl_resource *resource, int32_t id)
{
   if (id > 0) return EINA_TRUE;
   wl_resource_post_error(resource, WL_TEST_ERROR_INVALID_TOUCH_ID,
                          "touch id %d is not positive", id);
   return EINA_FALSE;
}

static Wl_Test_Touch *
_wl_test_touch_find(int32_t id)
{
   Wl_Test_Touch *t;
   Eina_List *l;

   EINA_LIST_FOREACH(_touches, l, t)
     {
        if (t->id == id) return t;
     }
   return NULL;
}

static void
_wl_test_cb_touch_down(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int32_t id, int32_t x, int32_t y)
{
   Wl_Test_Touch *t;

   if (!_wl_test_touch_id_check(resource, id)) return;
   t = _wl_test_touch_find(id);
   if (!t)
     {
        t = calloc(1, sizeof(*t));
        if (!t) return;
        t->id = id;
        _touches = eina_list_append(_touches, t);
     }
   t->x = x;
   t->y = y;
   _input_mouse_post(ECORE_EVENT_MOUSE_BUTTON_DOWN, id, 1, x, y);
}

static void
_wl_test_cb_touch_move(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int32_t id, int32_t x, int32_t y)
{
   Wl_Test_Touch *t;

   if (!_wl_test_touch_id_check(resource, id)) return;
   t = _wl_test_touch_find(id);
   if (!t) return;
   t->x = x;
   t->y = y;
   _input_move_post(id, x, y);
}

static void
_wl_test_cb_touch_up(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, int32_t id)
{
   Wl_Test_Touch *t;

   if (!_wl_test_touch_id_check(resource, id)) return;
   t = _wl_test_touch_find(id);
   if (!t) return;
   _touches = eina_list_remove(_touches, t);
   _input_mouse_post(ECORE_EVENT_MOUSE_BUTTON_UP, id, 1, t->x, t->y);
   free(t);
}

/* E's own generator, the one behind the keyboard_send action: it maps the
 * name through the active keymap and posts an ecore key event. */
static void
_wl_test_cb_key(struct wl_client *client EINA_UNUSED, struct wl_resource *resource EINA_UNUSED, const char *keyname, uint32_t pressed)
{
   e_comp_wl_input_keyboard_event_generate(keyname, 0, !pressed);
}

static void
_wl_test_cb_sync_job(void *data EINA_UNUSED)
{
   struct wl_resource *resource;

   _sync_job = NULL;
   EINA_LIST_FREE(_pending_syncs, resource)
     wl_test_send_sync_done(resource);
}

static void
_wl_test_cb_sync(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   /* Answer from a job rather than inline. wl_display.sync only proves the
    * requests were read; most of what E does in response happens in a job or
    * an idler, and posted input is only dispatched on the next main loop
    * iteration. */
   _pending_syncs = eina_list_append(_pending_syncs, resource);
   if (!_sync_job) _sync_job = ecore_job_add(_wl_test_cb_sync_job, NULL);
}

static const struct wl_test_interface _wl_test_implementation =
{
   _wl_test_cb_destroy,
   _wl_test_cb_get_client_serial,
   _wl_test_cb_get_surface_info,
   _wl_test_cb_move_surface,
   _wl_test_cb_zone_add,
   _wl_test_cb_pointer_warp,
   _wl_test_cb_pointer_button,
   _wl_test_cb_pointer_axis,
   _wl_test_cb_touch_down,
   _wl_test_cb_touch_move,
   _wl_test_cb_touch_up,
   _wl_test_cb_key,
   _wl_test_cb_sync,
   _wl_test_cb_pointer_move,
   _wl_test_cb_get_clients,
   _wl_test_cb_client_action,
   _wl_test_cb_client_action_end,
};

static void
_wl_test_cb_unbind(struct wl_resource *resource)
{
   _pending_syncs = eina_list_remove(_pending_syncs, resource);
}

static void
_wl_test_cb_bind(struct wl_client *client, void *data EINA_UNUSED, uint32_t version, uint32_t id)
{
   struct wl_resource *resource;

   resource = wl_resource_create(client, &wl_test_interface, version, id);
   if (!resource)
     {
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(resource, &_wl_test_implementation, NULL,
                                  _wl_test_cb_unbind);
}

E_API void *
e_modapi_init(E_Module *m)
{
   if (!e_comp_wl)
     {
        ERR("wl_test: no wayland compositor data - wayland-only module");
        return NULL;
     }

   _wl_test_global = wl_global_create(e_comp_wl->wl.disp, &wl_test_interface, 4,
                                     NULL, _wl_test_cb_bind);
   if (!_wl_test_global)
     {
        ERR("wl_test: could not create global");
        return NULL;
     }

   _client_ids = eina_hash_pointer_new(NULL);
   _client_del_hook = e_client_hook_add(E_CLIENT_HOOK_DEL,
                                        _wl_test_cb_client_del, NULL);

   _client_created_listener.notify = _wl_test_cb_client_created;
   wl_display_add_client_created_listener(e_comp_wl->wl.disp,
                                          &_client_created_listener);

   /* Before E's own handler, which cancels a motion a pointer constraint
    * swallowed: this one must still see it to clear the posted position. */
   E_LIST_HANDLER_APPEND_PRE(_input_handlers, ECORE_EVENT_MOUSE_MOVE,
                             _input_cb_mouse_move, NULL);

   INF("wl_test: test interface active - this build is not for production");

   return m;
}

E_API int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   Wl_Test_Client *tc;

   E_FREE_LIST(_input_handlers, ecore_event_handler_del);
   E_FREE_LIST(_touches, free);

   if (_sync_job)
     {
        ecore_job_del(_sync_job);
        _sync_job = NULL;
     }
   _pending_syncs = eina_list_free(_pending_syncs);

   wl_global_destroy(_wl_test_global);
   _wl_test_global = NULL;

   wl_list_remove(&_client_created_listener.link);

   EINA_LIST_FREE(_clients, tc)
     {
        wl_list_remove(&tc->destroy.link);
        free(tc);
     }
   E_FREE_FUNC(_client_del_hook, e_client_hook_del);
   E_FREE_FUNC(_client_ids, eina_hash_free);

   return 1;
}

E_API int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   return 1;
}

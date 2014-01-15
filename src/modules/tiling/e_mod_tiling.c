#include "e_mod_tiling.h"

/* types {{{ */

#define TILING_OVERLAY_TIMEOUT 5.0
#define TILING_RESIZE_STEP 5
#define TILING_WRAP_SPEED 0.1

typedef enum {
    TILING_RESIZE,
    TILING_MOVE,
} tiling_change_t;

typedef enum {
    INPUT_MODE_NONE,
    INPUT_MODE_SWAPPING,
    INPUT_MODE_MOVING,
    INPUT_MODE_GOING,
    INPUT_MODE_TRANSITION,
} tiling_input_mode_t;

typedef enum {
    MOVE_UP,
    MOVE_DOWN,
    MOVE_LEFT,
    MOVE_RIGHT,

    MOVE_COUNT
} tiling_move_t;

typedef struct geom_t {
    int x, y, w, h;
} geom_t;

typedef struct overlay_t {
    Evas_Object *popup;
    Evas_Object *obj;
} overlay_t;

typedef struct transition_overlay_t {
    overlay_t overlay;
    int stack;
    char key[4];
    E_Client *ec;
} transition_overlay_t;

typedef struct Client_Extra {
    E_Client *client;
    geom_t expected;
    struct {
         geom_t geom;
         unsigned int layer;
         E_Stacking  stacking;
         E_Maximize  maximized;
         const char *bordername;
    } orig;
    overlay_t overlay;
    char key[4];
} Client_Extra;

struct tiling_g tiling_g = {
    .module = NULL,
    .config = NULL,
    .log_domain = -1,
    .default_keyhints = "asdfg;lkjh",
};

static void
_add_client(E_Client *ec);

/* }}} */
/* Globals {{{ */

static struct tiling_mod_main_g
{
    char                  edj_path[PATH_MAX];
    E_Config_DD          *config_edd,
                         *vdesk_edd;
    int                   currently_switching_desktop;
    Ecore_X_Window        action_input_win;
    Ecore_Event_Handler  *handler_key,
                         *handler_client_resize,
                         *handler_client_move,
                         *handler_client_add,
                         *handler_client_remove,
                         *handler_client_iconify,
                         *handler_client_uniconify,
                         *handler_client_stick,
                         *handler_client_unstick,
                         *handler_desk_show,
                         *handler_desk_before_show,
                         *handler_desk_set,
                         *handler_compositor_resize;
    E_Client_Hook        *pre_client_assign_hook;

    Tiling_Info          *tinfo;
    Eina_Hash            *info_hash;
    Eina_Hash            *client_extras;
    Eina_Hash            *overlays;

    E_Action             *act_togglefloat,
                         *act_addstack,
                         *act_removestack,
                         *act_tg_stack,
                         *act_swap,
                         *act_move,
                         *act_move_left,
                         *act_move_right,
                         *act_move_up,
                         *act_move_down,
                         *act_adjusttransitions,
                         *act_go,
                         *act_send_ne,
                         *act_send_nw,
                         *act_send_se,
                         *act_send_sw;

    int                   warp_x,
                          warp_y,
                          old_warp_x,
                          old_warp_y,
                          warp_to_x,
                          warp_to_y;
    Ecore_Timer          *warp_timer;

    overlay_t             move_overlays[MOVE_COUNT];
    transition_overlay_t *transition_overlay;
    Ecore_Timer          *action_timer;
    E_Client             *focused_ec;
    void (*action_cb)(E_Client *ec, Client_Extra *extra);

    tiling_input_mode_t   input_mode;
    char                  keys[4];
} _G = {
    .input_mode = INPUT_MODE_NONE,
};

/* }}} */
/* Utils {{{ */

/* I wonder why noone has implemented the following one yet? */
static E_Desk *
get_current_desk(void)
{
    E_Manager *m = e_manager_current_get();
    E_Comp *c = m->comp;
    E_Zone *z = e_zone_current_get(c);

    return e_desk_current_get(z);
}

static Tiling_Info *
_initialize_tinfo(const E_Desk *desk)
{
    Tiling_Info *tinfo;

    tinfo = E_NEW(Tiling_Info, 1);
    tinfo->desk = desk;
    eina_hash_direct_add(_G.info_hash, &tinfo->desk, tinfo);

    tinfo->conf = get_vdesk(tiling_g.config->vdesks, desk->x, desk->y,
                            desk->zone->num);

    return tinfo;
}

static void
check_tinfo(const E_Desk *desk)
{
    if (!_G.tinfo || _G.tinfo->desk != desk) {
        _G.tinfo = eina_hash_find(_G.info_hash, &desk);
        if (!_G.tinfo) {
            /* lazy init */
            _G.tinfo = _initialize_tinfo(desk);
        }
        if (!_G.tinfo->conf) {
            _G.tinfo->conf = get_vdesk(tiling_g.config->vdesks,
                                       desk->x, desk->y,
                                       desk->zone->num);
        }
    }
}

static int
is_floating_window(const E_Client *ec)
{
    return EINA_LIST_IS_IN(_G.tinfo->floating_windows, ec);
}

static int
is_tilable(const E_Client *ec)
{
    if (ec->icccm.min_h == ec->icccm.max_h
    &&  ec->icccm.max_h > 0)
        return false;

    if (ec->icccm.gravity == ECORE_X_GRAVITY_STATIC)
        return false;

    if (!tiling_g.config->tile_dialogs
            && ((ec->icccm.transient_for != 0)
                || (ec->netwm.type == E_WINDOW_TYPE_DIALOG)))
        return false;

    return true;
}

static void
change_window_border(E_Client   *ec,
                     const char *bordername)
{
    eina_stringshare_replace(&ec->bordername, bordername);
    ec->border.changed = true;
    ec->changes.border = true;
    ec->changed = true;

    DBG("%p -> border %s", ec, bordername);
}

static int
get_stack(const E_Client *ec)
{
    int i;
    for (i = 0; i < TILING_MAX_STACKS; i++) {
        if (EINA_LIST_IS_IN(_G.tinfo->stacks[i], ec))
            return i;
    }
    return -1;
}

static int
get_stack_count(void)
{
    int i;
    for (i = 0; i < TILING_MAX_STACKS; i++) {
        if (!_G.tinfo->stacks[i])
            return i;
    }
    return TILING_MAX_STACKS;
}

static int
get_window_count(void)
{
    int res = 0;
    int i;

    for (i = 0; i < TILING_MAX_STACKS; i++) {
        if (!_G.tinfo->stacks[i])
            break;
        res += eina_list_count(_G.tinfo->stacks[i]);
    }
    return res;
}

static int
get_transition_count(void)
{
    int res = 0;
    int i;

    for (i = 0; i < TILING_MAX_STACKS; i++) {
        if (!_G.tinfo->stacks[i])
            break;
        res += eina_list_count(_G.tinfo->stacks[i]);
    }
    if (_G.tinfo->stacks[0])
        res--;
    return res;
}

static void
_theme_edje_object_set_aux(Evas_Object *obj, const char *group)
{
    if (!e_theme_edje_object_set(obj, "base/theme/modules/tiling",
                                 group)) {
        edje_object_file_set(obj, _G.edj_path, group);
    }
}
#define _theme_edje_object_set(_obj, _group)                                 \
    if (!e_config->use_shaped_win)                                           \
        _theme_edje_object_set_aux(_obj, _group"/composite");                \
    else                                                                     \
        _theme_edje_object_set_aux(_obj, _group);

static Eina_Bool
_info_hash_update(const Eina_Hash *hash __UNUSED__, const void *key __UNUSED__,
                  void *data, void *fdata __UNUSED__)
{
    Tiling_Info *tinfo = data;

    if (tinfo->desk) {
        tinfo->conf = get_vdesk(tiling_g.config->vdesks,
                                tinfo->desk->x, tinfo->desk->y,
                                tinfo->desk->zone->num);
    } else {
        tinfo->conf = NULL;
    }

    return true;
}

void
e_tiling_update_conf(void)
{
    eina_hash_foreach(_G.info_hash, _info_hash_update, NULL);
}

static void
_e_client_move_resize(E_Client *ec,
                      int       x,
                      int       y,
                      int       w,
                      int       h)
{
    DBG("%p -> %dx%d+%d+%d", ec, w, h, x, y);
    evas_object_geometry_set(ec->frame, x, y, w, h);
}

static void
_e_client_move(E_Client *ec,
               int       x,
               int       y)
{
    DBG("%p -> +%d+%d", ec, x, y);
    evas_object_move(ec->frame, x, y);
}

static void
_e_client_resize(E_Client *ec,
                 int       w,
                 int       h)
{
    DBG("%p -> %dx%d", ec, w, h);
    evas_object_resize(ec->frame, w, h);
}

static void
_e_client_maximize(E_Client *ec, E_Maximize max)
{
    DBG("%p -> %s", ec,
        (max & E_MAXIMIZE_DIRECTION) == E_MAXIMIZE_NONE ? "NONE" :
        (max & E_MAXIMIZE_DIRECTION) == E_MAXIMIZE_VERTICAL ? "VERTICAL" :
        (max & E_MAXIMIZE_DIRECTION) == E_MAXIMIZE_HORIZONTAL ? "HORIZONTAL" :
        "BOTH");
    DBG("new_client:%s, ec->maximized=%x",
        ec->new_client? "true": "false",
        ec->maximized);
    e_client_maximize(ec, max);
}

static void
_e_client_unmaximize(E_Client *ec, E_Maximize max)
{
    DBG("%p -> %s", ec,
        (max & E_MAXIMIZE_DIRECTION) == E_MAXIMIZE_NONE ? "NONE" :
        (max & E_MAXIMIZE_DIRECTION) == E_MAXIMIZE_VERTICAL ? "VERTICAL" :
        (max & E_MAXIMIZE_DIRECTION) == E_MAXIMIZE_HORIZONTAL ? "HORIZONTAL" :
        "BOTH");
    e_client_unmaximize(ec, max);
}

static void
_restore_client(E_Client *ec)
{
    Client_Extra *extra;

    extra = eina_hash_find(_G.client_extras, &ec);
    if (!extra) {
         ERR("No extra for %p", ec);
         return;
    }
    _e_client_unmaximize(ec, E_MAXIMIZE_BOTH);
    _e_client_move_resize(ec,
                          extra->orig.geom.x,
                          extra->orig.geom.y,
                          extra->orig.geom.w,
                          extra->orig.geom.h);
    evas_object_layer_set(ec->frame, extra->orig.layer);
    if (extra->orig.maximized) {
        e_client_maximize(ec, extra->orig.maximized);
        ec->maximized = extra->orig.maximized;
    }

    DBG("Change window border back to %s for %p",
        extra->orig.bordername, ec);
    change_window_border(ec, (extra->orig.bordername)
                         ? extra->orig.bordername : "default");
}

static Client_Extra *
_get_or_create_client_extra(E_Client *ec)
{
    Client_Extra *extra;

    extra = eina_hash_find(_G.client_extras, &ec);
    if (!extra) {
        extra = E_NEW(Client_Extra, 1);
        *extra = (Client_Extra) {
            .client = ec,
            .expected = {
                .x = ec->x,
                .y = ec->y,
                .w = ec->w,
                .h = ec->h,
            },
            .orig = {
                .geom = {
                    .x = ec->x,
                    .y = ec->y,
                    .w = ec->w,
                    .h = ec->h,
                },
                .layer = ec->layer,
                .stacking = ec->netwm.state.stacking,
                .maximized = ec->maximized,
                .bordername = eina_stringshare_add(ec->bordername),
            },
        };
        eina_hash_direct_add(_G.client_extras, &extra->client, extra);
    } else {
        extra->expected = (geom_t) {
            .x = ec->x,
            .y = ec->y,
            .w = ec->w,
            .h = ec->h,
        };
    }

    return extra;
}

/* }}} */
/* Overlays {{{*/

static void
_overlays_free_cb(void *data)
{
    Client_Extra *extra = data;

    if (extra->overlay.obj) {
        evas_object_del(extra->overlay.obj);
        extra->overlay.obj = NULL;
    }
    if (extra->overlay.popup) {
        evas_object_hide(extra->overlay.popup);
        evas_object_del(extra->overlay.popup);
        extra->overlay.popup = NULL;
    }

    extra->key[0] = '\0';
}

static void
end_special_input(void)
{
    int i;

    if (_G.input_mode == INPUT_MODE_NONE)
        return;

    if (_G.overlays) {
        eina_hash_free(_G.overlays);
        _G.overlays = NULL;
    }

    if (_G.handler_key) {
        ecore_event_handler_del(_G.handler_key);
        _G.handler_key = NULL;
    }
    if (_G.action_input_win) {
        e_grabinput_release(_G.action_input_win, _G.action_input_win);
        ecore_x_window_free(_G.action_input_win);
        _G.action_input_win = 0;
    }
    if (_G.action_timer) {
        ecore_timer_del(_G.action_timer);
        _G.action_timer = NULL;
    }

    _G.focused_ec = NULL;
    _G.action_cb = NULL;

    switch(_G.input_mode) {
      case INPUT_MODE_MOVING:
        for (i = 0; i < MOVE_COUNT; i++) {
            overlay_t *overlay = &_G.move_overlays[i];

            if (overlay->obj) {
                evas_object_del(overlay->obj);
                overlay->obj = NULL;
            }
            if (overlay->popup) {
                evas_object_hide(overlay->popup);
                evas_object_del(overlay->popup);
                overlay->popup = NULL;
            }
        }
        break;
      case INPUT_MODE_TRANSITION:
        if (_G.transition_overlay) {
            if (_G.transition_overlay->overlay.obj) {
                evas_object_del(_G.transition_overlay->overlay.obj);
            }
            if (_G.transition_overlay->overlay.popup) {
                evas_object_hide(_G.transition_overlay->overlay.popup);
                evas_object_del(_G.transition_overlay->overlay.popup);
            }
            E_FREE(_G.transition_overlay);
            _G.transition_overlay = NULL;
        }
        break;
      default:
        break;
    }

    _G.input_mode = INPUT_MODE_NONE;
}

static Eina_Bool
overlay_key_down(void *data __UNUSED__,
		 int type __UNUSED__,
		 void *event)
{
    Ecore_Event_Key *ev = event;
    Client_Extra *extra;

    if (ev->event_window != _G.action_input_win)
        return ECORE_CALLBACK_PASS_ON;

    if (strcmp(ev->key, "Return") == 0)
        goto stop;
    if (strcmp(ev->key, "Escape") == 0)
        goto stop;
    if (strcmp(ev->key, "Backspace") == 0) {
        char *key = _G.keys;

        while (*key)
            key++;
        *key = '\0';
        return ECORE_CALLBACK_RENEW;
    }

    if (ev->key[0] && !ev->key[1] && strchr(tiling_g.config->keyhints,
                                            ev->key[1])) {
        char *key = _G.keys;

        while (*key)
            key++;
        *key++ = ev->key[0];
        *key = '\0';

        extra = eina_hash_find(_G.overlays, _G.keys);
        if (extra) {
            _G.action_cb(_G.focused_ec, extra);
        } else {
            return ECORE_CALLBACK_RENEW;
        }
    }

stop:
    end_special_input();
    return ECORE_CALLBACK_DONE;
}

static Eina_Bool
_timeout_cb(void *data __UNUSED__)
{
    end_special_input();
    return ECORE_CALLBACK_CANCEL;
}

static void
_do_overlay(E_Client *focused_ec,
            void (*action_cb)(E_Client *, Client_Extra *),
            tiling_input_mode_t input_mode)
{
    Ecore_X_Window parent;
    int nb_win;
    int hints_len;
    int key_len;
    int n = 0;
    int nmax;
    int i;

    end_special_input();

    nb_win = get_window_count();
    if (nb_win < 2) {
        return;
    }

    _G.input_mode = input_mode;

    _G.focused_ec = focused_ec;
    _G.action_cb = action_cb;

    _G.overlays = eina_hash_string_small_new(_overlays_free_cb);

    hints_len = strlen(tiling_g.config->keyhints);
    key_len = 1;
    nmax = hints_len;
    if (hints_len < nb_win) {
        key_len = 2;
        nmax *= hints_len;
        if (hints_len * hints_len < nb_win) {
            key_len = 3;
            nmax *= hints_len;
        }
    }

    for (i = 0; i < TILING_MAX_STACKS; i++) {
        Eina_List *l;
        E_Client *ec;

        if (!_G.tinfo->stacks[i])
            break;
        EINA_LIST_FOREACH(_G.tinfo->stacks[i], l, ec) {
            if (ec != focused_ec && n < nmax) {
                Client_Extra *extra;
                Evas_Coord ew, eh;

                extra = eina_hash_find(_G.client_extras, &ec);
                if (!extra) {
                    ERR("No extra for %p", ec);
                    continue;
                }

                extra->overlay.obj = edje_object_add(ec->comp->evas);
                extra->overlay.popup = e_comp_object_util_add(extra->overlay.obj, E_COMP_OBJECT_TYPE_POPUP);
                evas_object_layer_set(extra->overlay.popup, E_LAYER_CLIENT_NORMAL);
                e_theme_edje_object_set(extra->overlay.obj,
                                        "base/theme/borders",
                                        "e/widgets/border/default/resize");

                switch (key_len) {
                  case 1:
                    extra->key[0] = tiling_g.config->keyhints[n];
                    extra->key[1] = '\0';
                    break;
                  case 2:
                    extra->key[0] = tiling_g.config->keyhints[n / hints_len];
                    extra->key[1] = tiling_g.config->keyhints[n % hints_len];
                    extra->key[2] = '\0';
                    break;
                  case 3:
                    extra->key[0] = tiling_g.config->keyhints[n / hints_len / hints_len];
                    extra->key[0] = tiling_g.config->keyhints[n / hints_len];
                    extra->key[1] = tiling_g.config->keyhints[n % hints_len];
                    extra->key[2] = '\0';
                    break;
                }
                n++;

                eina_hash_add(_G.overlays, extra->key, extra);
                edje_object_part_text_set(extra->overlay.obj,
                                          "e.text.label",
                                          extra->key);
                edje_object_size_min_calc(extra->overlay.obj, &ew, &eh);

                evas_object_geometry_set(extra->overlay.popup,
                                    (ec->x - ec->zone->x) +
                                    ((ec->w - ew) / 2),
                                    (ec->y - ec->zone->y) +
                                    ((ec->h - eh) / 2),
                                    ew, eh);

                evas_object_show(extra->overlay.popup);
            }
        }
    }

    /* Get input */
    parent = _G.tinfo->desk->zone->comp->win;
    _G.action_input_win = ecore_x_window_input_new(parent, 0, 0, 1, 1);
    if (!_G.action_input_win) {
        end_special_input();
        return;
    }

    ecore_x_window_show(_G.action_input_win);
    if (!e_grabinput_get(_G.action_input_win, 0, _G.action_input_win)) {
        end_special_input();
        return;
    }
    _G.action_timer = ecore_timer_add(TILING_OVERLAY_TIMEOUT,
                                      _timeout_cb, NULL);

    _G.keys[0] = '\0';
    _G.handler_key = ecore_event_handler_add(ECORE_EVENT_KEY_DOWN,
                                             overlay_key_down, NULL);
}

/* }}} */
/* Reorganize Stacks {{{*/

static void
_reorganize_stack(int stack)
{
    Eina_List *l;

    if (stack < 0 || stack >= TILING_MAX_STACKS
        || !_G.tinfo->stacks[stack])
        return;

    if (_G.tinfo->stacks[stack]->next) {
        int zx, zy, zw, zh, i = 0, count;

        e_zone_useful_geometry_get(_G.tinfo->desk->zone, &zx, &zy, &zw, &zh);

        count = eina_list_count(_G.tinfo->stacks[stack]);

        if (_G.tinfo->conf->use_rows) {
            int y, w, h, cw;

            y = _G.tinfo->pos[stack];
            cw = 0;
            w = zw / count;
            h = _G.tinfo->size[stack];

            for (l = _G.tinfo->stacks[stack]; l; l = l->next, i++) {
                E_Client *ec = l->data;
                Client_Extra *extra;
                int d = (i * 2 * zw) % count
                    - (2 * cw) % count;

                extra = eina_hash_find(_G.client_extras, &ec);
                if (!extra) {
                    ERR("No extra for %p", ec);
                    continue;
                }

                if ((ec->maximized & E_MAXIMIZE_HORIZONTAL) && count != 1) {
                    _e_client_unmaximize(ec, E_MAXIMIZE_HORIZONTAL);
                }
                /* let's use a bresenham here */

                extra->expected.x = cw + zx;
                extra->expected.y = y;
                extra->expected.w = w + d;
                extra->expected.h = h;
                cw += extra->expected.w;

                _e_client_move_resize(ec,
                                      extra->expected.x,
                                      extra->expected.y,
                                      extra->expected.w,
                                      extra->expected.h);
            }
        } else {
            int x, w, h, ch;

            x = _G.tinfo->pos[stack];
            ch = 0;
            w = _G.tinfo->size[stack];
            h = zh / count;

            for (l = _G.tinfo->stacks[stack]; l; l = l->next, i++) {
                E_Client *ec = l->data;
                Client_Extra *extra;
                int d = (i * 2 * zh) % count
                    - (2 * ch) % count;

                extra = eina_hash_find(_G.client_extras, &ec);
                if (!extra) {
                    ERR("No extra for %p", ec);
                    continue;
                }

                if ((ec->maximized & E_MAXIMIZE_VERTICAL) && count != 1) {
                    _e_client_unmaximize(ec, E_MAXIMIZE_VERTICAL);
                }
                /* let's use a bresenham here */

                extra->expected.x = x;
                extra->expected.y = ch + zy;
                extra->expected.w = w;
                extra->expected.h = h + d;
                ch += extra->expected.h;

                _e_client_move_resize(ec,
                                      extra->expected.x,
                                      extra->expected.y,
                                      extra->expected.w,
                                      extra->expected.h);
            }
        }
    } else {
        Client_Extra *extra;
        E_Client *ec = _G.tinfo->stacks[stack]->data;

        extra = eina_hash_find(_G.client_extras, &ec);
        if (!extra) {
            ERR("No extra for %p", ec);
            return;
        }

        if (_G.tinfo->conf->use_rows) {
            int x, w;

            e_zone_useful_geometry_get(_G.tinfo->desk->zone,
                                       &x, NULL, &w, NULL);

            extra->expected.x = x;
            extra->expected.y = _G.tinfo->pos[stack];
            extra->expected.w = w;
            extra->expected.h = _G.tinfo->size[stack];

            _e_client_move_resize(ec,
                                  extra->expected.x,
                                  extra->expected.y,
                                  extra->expected.w,
                                  extra->expected.h);

            _e_client_maximize(ec, E_MAXIMIZE_EXPAND | E_MAXIMIZE_HORIZONTAL);
        } else {
            int y, h;

            e_zone_useful_geometry_get(_G.tinfo->desk->zone,
                                       NULL, &y, NULL, &h);

            extra->expected.x = _G.tinfo->pos[stack];
            extra->expected.y = y;
            extra->expected.w = _G.tinfo->size[stack];
            extra->expected.h = h;

            _e_client_move_resize(ec,
                                  extra->expected.x,
                                  extra->expected.y,
                                  extra->expected.w,
                                  extra->expected.h);

            _e_client_maximize(ec, E_MAXIMIZE_EXPAND | E_MAXIMIZE_VERTICAL);
        }
    }
}

static void
_move_resize_stack(int stack, int delta_pos, int delta_size)
{
    Eina_List *list = _G.tinfo->stacks[stack];
    Eina_List *l;

    for (l = list; l; l = l->next) {
        E_Client *ec = l->data;
        Client_Extra *extra;

        extra = eina_hash_find(_G.client_extras, &ec);
        if (!extra) {
            ERR("No extra for %p", ec);
            continue;
        }

        if (_G.tinfo->conf->use_rows) {
            extra->expected.y += delta_pos;
            extra->expected.h += delta_size;
        } else {
            extra->expected.x += delta_pos;
            extra->expected.w += delta_size;
        }

        _e_client_move_resize(ec,
                              extra->expected.x,
                              extra->expected.y,
                              extra->expected.w,
                              extra->expected.h);
    }

    _G.tinfo->pos[stack] += delta_pos;
    _G.tinfo->size[stack] += delta_size;
}

static void
_set_stack_geometry(int stack, int pos, int size)
{
    Eina_List *l;
    for (l = _G.tinfo->stacks[stack]; l; l = l->next) {
        E_Client *ec = l->data;
        Client_Extra *extra;

        extra = eina_hash_find(_G.client_extras, &ec);
        if (!extra) {
            ERR("No extra for %p", ec);
            continue;
        }
        DBG("expected: %dx%d+%d+%d (%p)",
            extra->expected.w,
            extra->expected.h,
            extra->expected.x,
            extra->expected.y,
            ec);

        if (_G.tinfo->conf->use_rows) {
            extra->expected.y = pos;
            extra->expected.h = size;

            if (ec->maximized) {
                if (l->next && (ec->maximized & E_MAXIMIZE_HORIZONTAL))
                    _e_client_unmaximize(ec, E_MAXIMIZE_HORIZONTAL);
                if (_G.tinfo->stacks[1] && (ec->maximized & E_MAXIMIZE_VERTICAL))
                    _e_client_unmaximize(ec, E_MAXIMIZE_VERTICAL);
            }
        } else {
            extra->expected.x = pos;
            extra->expected.w = size;

            if (ec->maximized) {
                if (l->next && (ec->maximized & E_MAXIMIZE_VERTICAL))
                    _e_client_unmaximize(ec, E_MAXIMIZE_VERTICAL);
                if (_G.tinfo->stacks[1] && (ec->maximized & E_MAXIMIZE_HORIZONTAL))
                    _e_client_unmaximize(ec, E_MAXIMIZE_HORIZONTAL);
            }
        }

        _e_client_move_resize(ec,
                              extra->expected.x,
                              extra->expected.y,
                              extra->expected.w,
                              extra->expected.h);
    }
    _G.tinfo->pos[stack] = pos;
    _G.tinfo->size[stack] = size;
}

static void
_add_stack(void)
{
    int nb_clients;
    Eina_List *l;
    int i;

    if (_G.tinfo->conf->nb_stacks == TILING_MAX_STACKS)
        return;

    _G.tinfo->conf->nb_stacks++;

    if (_G.tinfo->conf->nb_stacks == 1) {
        for (l = e_client_focus_stack_get(); l; l = l->next) {
            E_Client *ec;

            ec = l->data;
            if (ec->desk == _G.tinfo->desk)
                _add_client(ec);
        }
    }
    nb_clients = get_window_count();
    if (_G.tinfo->stacks[_G.tinfo->conf->nb_stacks - 2]
    &&  nb_clients >= _G.tinfo->conf->nb_stacks)
    {
        int nb_stacks = _G.tinfo->conf->nb_stacks - 1;
        int pos, s;
        /* Add stack */

        if (_G.tinfo->conf->use_rows)
            e_zone_useful_geometry_get(_G.tinfo->desk->zone,
                                       NULL, &pos, NULL, &s);
        else
            e_zone_useful_geometry_get(_G.tinfo->desk->zone,
                                       &pos, NULL, &s, NULL);

        for (i = 0; i <= nb_stacks; i++) {
            int size = 0;

            size = s / (nb_stacks + 1 - i);

            _set_stack_geometry(i, pos, size);

            s -= size;
            pos += size;
        }
        for (i = nb_stacks - 1; i >= 0; i--) {
            if (eina_list_count(_G.tinfo->stacks[i]) == 1) {
                _G.tinfo->stacks[i+1] = _G.tinfo->stacks[i];
                _reorganize_stack(i+1);
            } else {
                E_Client *ec = eina_list_last(_G.tinfo->stacks[i])->data;

                EINA_LIST_REMOVE(_G.tinfo->stacks[i], ec);
                _reorganize_stack(i);

                _G.tinfo->stacks[i+1] = NULL;
                EINA_LIST_APPEND(_G.tinfo->stacks[i+1], ec);
                _reorganize_stack(i+1);
                return;
            }
        }
    }
}

static void
_remove_stack(void)
{
    int i;
    Eina_List *l;

    if (!_G.tinfo->conf->nb_stacks)
        return;

    _G.tinfo->conf->nb_stacks--;

    if (!_G.tinfo->conf->nb_stacks) {
        for (i = 0; i < TILING_MAX_STACKS; i++) {
            for (l = _G.tinfo->stacks[i]; l; l = l->next) {
                E_Client *ec = l->data;

                _restore_client(ec);
            }
            eina_list_free(_G.tinfo->stacks[i]);
            _G.tinfo->stacks[i] = NULL;
        }
        e_place_zone_region_smart_cleanup(_G.tinfo->desk->zone);
    } else {
        int nb_stacks = _G.tinfo->conf->nb_stacks;
        int stack = _G.tinfo->conf->nb_stacks;
        int pos, s;

        if (_G.tinfo->stacks[stack]) {
            _G.tinfo->stacks[stack-1] = eina_list_merge(
                _G.tinfo->stacks[stack-1], _G.tinfo->stacks[stack]);
            _reorganize_stack(stack-1);
        }

        if (_G.tinfo->conf->use_rows) {
            e_zone_useful_geometry_get(_G.tinfo->desk->zone,
                                       NULL, &pos, NULL, &s);
        } else {
            e_zone_useful_geometry_get(_G.tinfo->desk->zone,
                                       &pos, NULL, &s, NULL);
        }
        for (i = 0; i < nb_stacks; i++) {
            int size = 0;

            size = s / (nb_stacks - i);

            _set_stack_geometry(i, pos, size);

            s -= size;
            pos += size;
        }
    }
}

static void
_toggle_rows_cols(void)
{
    int i;
#if 0
    Eina_List *wins = NULL;
    E_Client *ec;

    _G.tinfo->conf->use_rows = !_G.tinfo->conf->use_rows;
    for (i = 0; i < TILING_MAX_STACKS; i++) {
        EINA_LIST_FREE(_G.tinfo->stacks[i], ec) {
            EINA_LIST_APPEND(wins, ec);
            _restore_client(ec);
        }
        _G.tinfo->stacks[i] = NULL;
        _G.tinfo->pos[i] = 0;
        _G.tinfo->size[i] = 0;
    }

    DBG("reinsert (use_rows: %s)",
        _G.tinfo->conf->use_rows ? "true":"false");

    EINA_LIST_FREE(wins, ec) {
        _add_client(ec);
    }
#else
    int nb_stacks = get_stack_count();
    int pos, s;

    _G.tinfo->conf->use_rows = !_G.tinfo->conf->use_rows;

    if (_G.tinfo->conf->use_rows) {
        e_zone_useful_geometry_get(_G.tinfo->desk->zone,
                                   NULL, &pos, NULL, &s);
    } else {
        e_zone_useful_geometry_get(_G.tinfo->desk->zone,
                                   &pos, NULL, &s, NULL);
    }

    for (i = 0; i < nb_stacks; i++) {
        int size = 0;

        size = s / (nb_stacks - i);

        _set_stack_geometry(i, pos, size);

        s -= size;
        pos += size;
    }
    for (i = 0; i < nb_stacks; i++) {
        _reorganize_stack(i);
    }
#endif
}

void
change_desk_conf(struct _Config_vdesk *newconf)
{
    E_Manager *m;
    E_Comp *c;
    E_Zone *z;
    E_Desk *d;
    int old_nb_stacks = 0,
        new_nb_stacks = newconf->nb_stacks;
    int i;
    Eina_List *l;

    m = e_manager_current_get();
    if (!m) return;
    c = m->comp;
    z = e_comp_zone_number_get(c, newconf->zone_num);
    if (!z) return;
    d = e_desk_at_xy_get(z, newconf->x, newconf->y);
    if (!d) return;

    check_tinfo(d);
    if (_G.tinfo->conf) {
        old_nb_stacks = _G.tinfo->conf->nb_stacks;
        if (_G.tinfo->conf->use_rows != newconf->use_rows) {
            _G.tinfo->conf = newconf;
            _G.tinfo->conf->use_rows = !_G.tinfo->conf->use_rows;
            _toggle_rows_cols();
            return;
        }
    } else {
        newconf->nb_stacks = 0;
    }
    _G.tinfo->conf = newconf;
    _G.tinfo->conf->nb_stacks = old_nb_stacks;

    if (new_nb_stacks == old_nb_stacks)
        return;

    if (new_nb_stacks == 0) {
        for (i = 0; i < TILING_MAX_STACKS; i++) {
            for (l = _G.tinfo->stacks[i]; l; l = l->next) {
                E_Client *ec = l->data;

                _restore_client(ec);
            }
            eina_list_free(_G.tinfo->stacks[i]);
            _G.tinfo->stacks[i] = NULL;
        }
        e_place_zone_region_smart_cleanup(z);
    } else if (new_nb_stacks > old_nb_stacks) {
        for (i = new_nb_stacks; i > old_nb_stacks; i--) {
            _add_stack();
        }
    } else {
        for (i = new_nb_stacks; i < old_nb_stacks; i++) {
            _remove_stack();
        }
    }
    _G.tinfo->conf->nb_stacks = new_nb_stacks;
}

static void
_e_mod_action_add_stack_cb(E_Object   *obj __UNUSED__,
                           const char *params __UNUSED__)
{
    E_Desk *desk = get_current_desk();

    end_special_input();

    check_tinfo(desk);
    if (!_G.tinfo->conf)
        return;

    _add_stack();

    e_config_save_queue();
}

static void
_e_mod_action_remove_stack_cb(E_Object   *obj __UNUSED__,
                              const char *params __UNUSED__)
{
    E_Desk *desk = get_current_desk();

    end_special_input();

    check_tinfo(desk);
    if (!_G.tinfo->conf || !_G.tinfo->conf->nb_stacks)
        return;

    _remove_stack();

    e_config_save_queue();
}

static void
_e_mod_action_tg_stack_cb(E_Object   *obj __UNUSED__,
                          const char *params __UNUSED__)
{
    E_Desk *desk = get_current_desk();

    end_special_input();

    check_tinfo(desk);
    if (!_G.tinfo->conf || !_G.tinfo->conf->nb_stacks)
        return;

    _toggle_rows_cols();

    e_config_save_queue();
}


/* }}} */
/* Reorganize windows {{{*/

static void
_add_client(E_Client *ec)
{
    Client_Extra *extra;
    int stack;
    int i;

    if (!ec) {
        return;
    }
    if (is_floating_window(ec)) {
        return;
    }
    if (!is_tilable(ec)) {
        return;
    }
    if (ec->fullscreen) {
         return;
    }

    if (!_G.tinfo || !_G.tinfo->conf || !_G.tinfo->conf->nb_stacks) {
        return;
    }

    extra = _get_or_create_client_extra(ec);

    /* Stack tiled window below so that winlist doesn't mix up stacking */
    evas_object_layer_set(ec->frame, E_LAYER_CLIENT_BELOW);

    DBG("adding %p", ec);

    if (_G.tinfo->stacks[0]) {
        DBG("got stack 0");
        if (_G.tinfo->stacks[_G.tinfo->conf->nb_stacks - 1]) {
            DBG("using last stack");
            stack = _G.tinfo->conf->nb_stacks - 1;

            if (!_G.tinfo->stacks[stack]->next) {
                _e_client_unmaximize(_G.tinfo->stacks[stack]->data,
                                    E_MAXIMIZE_BOTH);
            }
            EINA_LIST_APPEND(_G.tinfo->stacks[stack], ec);
            _reorganize_stack(stack);
            if (ec->maximized)
                _e_client_unmaximize(ec, E_MAXIMIZE_BOTH);
        } else {
            /* Add stack */
            int nb_stacks = get_stack_count();
            int x, y, w, h;
            int pos, s, size = 0;

            DBG("add stack");

            assert((0 <= nb_stacks) && (nb_stacks < TILING_MAX_STACKS));
            e_zone_useful_geometry_get(ec->zone, &x, &y, &w, &h);

            if (_G.tinfo->conf->use_rows) {
                pos = y;
                s = h;
            } else {
                pos = x;
                s = w;
            }

            EINA_LIST_APPEND(_G.tinfo->stacks[nb_stacks], ec);

            for (i = 0; i < nb_stacks; i++) {

                size = s / (nb_stacks + 1 - i);

                _set_stack_geometry(i, pos, size);

                s -= size;
                pos += size;
            }

            _G.tinfo->pos[nb_stacks] = pos;
            _G.tinfo->size[nb_stacks] = size;
            if (_G.tinfo->conf->use_rows) {
                extra->expected.x = x;
                extra->expected.y = pos;
                extra->expected.w = w;
                extra->expected.h = size;
                _e_client_maximize(ec, E_MAXIMIZE_EXPAND |
                                      E_MAXIMIZE_HORIZONTAL);
            } else {
                extra->expected.x = pos;
                extra->expected.y = y;
                extra->expected.w = size;
                extra->expected.h = h;
                _e_client_maximize(ec, E_MAXIMIZE_EXPAND |
                                      E_MAXIMIZE_VERTICAL);
            }
            _e_client_move_resize(ec,
                                  extra->expected.x,
                                  extra->expected.y,
                                  extra->expected.w,
                                  extra->expected.h);

            stack = nb_stacks;
        }
    } else {
        DBG("lonely window");
        e_zone_useful_geometry_get(ec->zone,
                                   &extra->expected.x,
                                   &extra->expected.y,
                                   &extra->expected.w,
                                   &extra->expected.h);

        if (ec->maximized & E_MAXIMIZE_BOTH)
            _e_client_unmaximize(ec, E_MAXIMIZE_BOTH);
        _e_client_move_resize(ec,
                              extra->expected.x,
                              extra->expected.y,
                              extra->expected.w,
                              extra->expected.h);
        _e_client_maximize(ec, E_MAXIMIZE_EXPAND | E_MAXIMIZE_BOTH);
        EINA_LIST_APPEND(_G.tinfo->stacks[0], ec);
        if (_G.tinfo->conf->use_rows) {
            e_zone_useful_geometry_get(ec->zone,
                                       NULL, &_G.tinfo->pos[0],
                                       NULL, &_G.tinfo->size[0]);
        } else {
            e_zone_useful_geometry_get(ec->zone,
                                       &_G.tinfo->pos[0], NULL,
                                       &_G.tinfo->size[0], NULL);
        }
        stack = 0;
    }
    DBG("expected: %dx%d+%d+%d (%p)",
        extra->expected.w,
        extra->expected.h,
        extra->expected.x,
        extra->expected.y,
        ec);
}

static void
_remove_client(E_Client *ec)
{
    int stack;
    int nb_stacks;
    int i, j;

    nb_stacks = get_stack_count();

    stack = get_stack(ec);
    if (stack < 0)
        return;

    DBG("removing %p (%d%c)", ec, stack, _G.tinfo->conf->use_rows? 'r':'c');

    EINA_LIST_REMOVE(_G.tinfo->stacks[stack], ec);
    eina_hash_del(_G.client_extras, ec, NULL);

    if (_G.tinfo->stacks[stack]) {
        _reorganize_stack(stack);
    } else {
        int nb_clients = get_window_count();

        if (nb_stacks > nb_clients) {
            int pos, s;
            /* Remove stack */

            nb_stacks--;

            assert((0 <= nb_stacks) && (nb_stacks < TILING_MAX_STACKS - 1));

            for (i = stack; i < nb_stacks; i++) {
                _G.tinfo->stacks[i] = _G.tinfo->stacks[i+1];
            }
            _G.tinfo->stacks[nb_stacks] = NULL;
            if (_G.tinfo->conf->use_rows) {
                e_zone_useful_geometry_get(ec->zone,
                                           NULL, &pos, NULL, &s);
            } else {
                e_zone_useful_geometry_get(ec->zone,
                                           &pos, NULL, &s, NULL);
            }
            for (i = 0; i < nb_stacks; i++) {
                int size;

                size = s / (nb_stacks - i);

                _set_stack_geometry(i, pos, size);

                s -= size;
                pos += size;
            }
        } else {
            for (i = stack+1; i < nb_stacks; i++) {
                if (eina_list_count(_G.tinfo->stacks[i]) > 1) {
                    for (j = stack; j < i - 1; j++) {
                        _G.tinfo->stacks[j] = _G.tinfo->stacks[j+1];
                        _reorganize_stack(j);
                    }
                    ec = _G.tinfo->stacks[i]->data;
                    EINA_LIST_REMOVE(_G.tinfo->stacks[i], ec);
                    _reorganize_stack(i);

                    _G.tinfo->stacks[i-1] = NULL;
                    EINA_LIST_APPEND(_G.tinfo->stacks[i-1], ec);
                    _reorganize_stack(i-1);
                    return;
                }
            }
            for (i = stack-1; i >= 0; i--) {
                if (eina_list_count(_G.tinfo->stacks[i]) == 1) {
                    _G.tinfo->stacks[i+1] = _G.tinfo->stacks[i];
                    _reorganize_stack(i+1);
                } else {
                    ec = eina_list_last(_G.tinfo->stacks[i])->data;
                    EINA_LIST_REMOVE(_G.tinfo->stacks[i], ec);
                    _reorganize_stack(i);

                    _G.tinfo->stacks[i+1] = NULL;
                    EINA_LIST_APPEND(_G.tinfo->stacks[i+1], ec);
                    _reorganize_stack(i+1);
                    return;
                }
            }
        }
    }
}

static void
_move_resize_client_stack(E_Client *ec, Client_Extra *extra,
                          int stack, tiling_change_t change)
{
#define _MOVE_RESIZE_CLIENT_STACK(_pos, _size)                               \
    if (change == TILING_RESIZE) {                                           \
        if (stack == TILING_MAX_STACKS || !_G.tinfo->stacks[stack + 1]) {    \
            /* You're not allowed to resize */                               \
            ec->_size = extra->expected._size;                               \
        } else {                                                             \
            int delta = ec->_size - extra->expected._size;                   \
                                                                             \
            if (delta + 1 > _G.tinfo->size[stack + 1])                       \
                delta = _G.tinfo->size[stack + 1] - 1;                       \
                                                                             \
            _move_resize_stack(stack, 0, delta);                             \
            _move_resize_stack(stack + 1, delta, -delta);                    \
            extra->expected._size = ec->_size;                               \
        }                                                                    \
    } else {                                                                 \
        if (stack == 0) {                                                    \
            /* You're not allowed to move */                                 \
            ec->_pos = extra->expected._pos;                                 \
        } else {                                                             \
            int delta = ec->_pos - extra->expected._pos;                     \
                                                                             \
            if (delta + 1 > _G.tinfo->size[stack - 1])                       \
                delta = _G.tinfo->size[stack - 1] - 1;                       \
                                                                             \
            _move_resize_stack(stack, delta, -delta);                        \
            _move_resize_stack(stack - 1, 0, delta);                         \
            extra->expected._pos = ec->_pos;                                 \
        }                                                                    \
    }
    if (_G.tinfo->conf->use_rows) {
        _MOVE_RESIZE_CLIENT_STACK(y, h)
    } else {
        _MOVE_RESIZE_CLIENT_STACK(x, w)
    }
#undef _MOVE_RESIZE_CLIENT_STACK
}

static void
_move_resize_client_in_stack(E_Client *ec, Client_Extra *extra,
                              int stack, tiling_change_t change)
{
    Eina_List *l;

    l = eina_list_data_find_list(_G.tinfo->stacks[stack], ec);
    if (!l) {
        ERR("unable to ec %p in stack %d", ec, stack);
        return;
    }

    switch (change) {
      case TILING_RESIZE:
        if (!l->next) {
            if (l->prev) {
                E_Client *prevec = l->prev->data;
                Client_Extra *prevextra;

                prevextra = eina_hash_find(_G.client_extras, &prevec);
                if (!prevextra) {
                    ERR("No extra for %p", prevec);
                    return;
                }

                if (_G.tinfo->conf->use_rows) {
                    int delta;

                    delta = ec->w - extra->expected.w;
                    prevextra->expected.w -= delta;
                    extra->expected.x -= delta;
                    extra->expected.w = ec->w;
                } else {
                    int delta;

                    delta = ec->h - extra->expected.h;
                    prevextra->expected.h -= delta;
                    extra->expected.y -= delta;
                    extra->expected.h = ec->h;
                }

                _e_client_resize(prevec,
                                 prevextra->expected.w,
                                 prevextra->expected.h);
                _e_client_move(ec,
                               extra->expected.x,
                               extra->expected.y);
            } else {
                /* You're not allowed to resize */
                _e_client_resize(ec,
                                 extra->expected.w,
                                 extra->expected.h);
            }
        } else {
            E_Client *nextec = l->next->data;
            Client_Extra *nextextra;

            nextextra = eina_hash_find(_G.client_extras, &nextec);
            if (!nextextra) {
                ERR("No extra for %p", nextec);
                return;
            }

            if (_G.tinfo->conf->use_rows) {
                int min_width = MAX(nextec->icccm.base_w, 1);
                int delta;

                delta = ec->w - extra->expected.w;
                if (nextextra->expected.w - delta < min_width)
                    delta = nextextra->expected.w - min_width;

                nextextra->expected.x += delta;
                nextextra->expected.w -= delta;

                extra->expected.w += delta;
            } else {
                int min_height = MAX(nextec->icccm.base_h, 1);
                int delta;

                delta = ec->h - extra->expected.h;
                if (nextextra->expected.h - delta < min_height)
                    delta = nextextra->expected.h - min_height;

                nextextra->expected.y += delta;
                nextextra->expected.h -= delta;

                extra->expected.h += delta;
            }

            _e_client_move_resize(nextec,
                                  nextextra->expected.x,
                                  nextextra->expected.y,
                                  nextextra->expected.w,
                                  nextextra->expected.h);
            _e_client_move_resize(ec,
                                  extra->expected.x,
                                  extra->expected.y,
                                  extra->expected.w,
                                  extra->expected.h);
        }
        break;
      case TILING_MOVE:
        if (!l->prev) {
            /* You're not allowed to move */
            if (_G.tinfo->conf->use_rows) {
                ec->x = extra->expected.x;
            } else {
                ec->y = extra->expected.y;
            }
            _e_client_move(ec,
                           extra->expected.x,
                           extra->expected.y);
            DBG("trying to move %p, but !l->prev", ec);
        } else {
            E_Client *prevec = l->prev->data;
            Client_Extra *prevextra;

            prevextra = eina_hash_find(_G.client_extras, &prevec);
            if (!prevextra) {
                ERR("No extra for %p", prevec);
                return;
            }

            if (_G.tinfo->conf->use_rows) {
                int delta = ec->x - extra->expected.x;
                int min_width = MAX(prevec->icccm.base_w, 1);

                if (prevextra->expected.w - delta < min_width)
                    delta = prevextra->expected.w - min_width;

                prevextra->expected.w += delta;

                extra->expected.x += delta;
                extra->expected.w -= delta;
            } else {
                int delta = ec->y - extra->expected.y;
                int min_height = MAX(prevec->icccm.base_h, 1);

                if (prevextra->expected.h - delta < min_height)
                    delta = prevextra->expected.h - min_height;

                prevextra->expected.h += delta;

                extra->expected.y += delta;
                extra->expected.h -= delta;
            }

            _e_client_resize(prevec,
                             prevextra->expected.w,
                             prevextra->expected.h);
            _e_client_move_resize(ec,
                                  extra->expected.x,
                                  extra->expected.y,
                                  extra->expected.w,
                                  extra->expected.h);
        }
        break;
      default:
        ERR("invalid tiling change: %d", change);
    }
}

/* }}} */
/* Toggle Floating {{{ */

static void
toggle_floating(E_Client *ec)
{
    if (!ec)
        return;
    check_tinfo(ec->desk);
    if (!_G.tinfo->conf || !_G.tinfo->conf->nb_stacks)
        return;

    if (EINA_LIST_IS_IN(_G.tinfo->floating_windows, ec)) {
        EINA_LIST_REMOVE(_G.tinfo->floating_windows, ec);

        _add_client(ec);
    } else {
        _remove_client(ec);
        _restore_client(ec);
        EINA_LIST_APPEND(_G.tinfo->floating_windows, ec);
    }
}

static void
_e_mod_action_toggle_floating_cb(E_Object   *obj __UNUSED__,
                                 const char *params __UNUSED__)
{
    end_special_input();

    toggle_floating(e_client_focused_get());
}

/* }}} */
/* {{{ Swap */

static void
_action_swap(E_Client *ec_1,
             Client_Extra *extra_2)
{
    Client_Extra *extra_1;
    E_Client *ec_2 = extra_2->client;
    Eina_List *l_1 = NULL,
              *l_2 = NULL;
    geom_t gt;
    unsigned int ec_2_maximized;
    int i;

    extra_1 = eina_hash_find(_G.client_extras, &ec_1);
    if (!extra_1) {
        ERR("No extra for %p", ec_1);
        return;
    }

    for (i = 0; i < TILING_MAX_STACKS; i++) {
        if ((l_1 = eina_list_data_find_list(_G.tinfo->stacks[i], ec_1))) {
            break;
        }
    }
    for (i = 0; i < TILING_MAX_STACKS; i++) {
        if ((l_2 = eina_list_data_find_list(_G.tinfo->stacks[i], ec_2))) {
            break;
        }
    }

    if (!l_1 || !l_2) {
        return;
    }

    l_1->data = ec_2;
    l_2->data = ec_1;

    gt = extra_2->expected;
    extra_2->expected = extra_1->expected;
    extra_1->expected = gt;

    ec_2_maximized = ec_2->maximized;
    if (ec_2->maximized)
        _e_client_unmaximize(ec_2, E_MAXIMIZE_BOTH);
    if (ec_1->maximized) {
        _e_client_unmaximize(ec_1, E_MAXIMIZE_BOTH);
        _e_client_maximize(ec_2, ec_1->maximized);
    }
    if (ec_2_maximized) {
        _e_client_maximize(ec_1, ec_2_maximized);
    }
    _e_client_move_resize(ec_1,
                          extra_1->expected.x,
                          extra_1->expected.y,
                          extra_1->expected.w,
                          extra_1->expected.h);
    _e_client_move_resize(ec_2,
                          extra_2->expected.x,
                          extra_2->expected.y,
                          extra_2->expected.w,
                          extra_2->expected.h);
}

static void
_e_mod_action_swap_cb(E_Object   *obj __UNUSED__,
                      const char *params __UNUSED__)
{
    E_Desk *desk;
    E_Client *focused_ec;

    desk = get_current_desk();
    if (!desk)
        return;

    focused_ec = e_client_focused_get();
    if (!focused_ec || focused_ec->desk != desk)
        return;

    check_tinfo(desk);
    if (!_G.tinfo->conf || !_G.tinfo->conf->nb_stacks) {
        return;
    }

    _do_overlay(focused_ec, _action_swap, INPUT_MODE_SWAPPING);
}

/* }}} */
/* Move {{{*/

static void
_check_moving_anims(const E_Client *ec, const Client_Extra *extra, int stack)
{
    Eina_List *l = NULL;
    overlay_t *overlay;
    int nb_stacks = get_stack_count();

    if (stack < 0) {
        stack = get_stack(ec);
        if (stack < 0)
            return;
    }
    if (!extra) {
        extra = eina_hash_find(_G.client_extras, &ec);
        if (!extra) {
            ERR("No extra for %p", ec);
            return;
        }
    }
    l = eina_list_data_find_list(_G.tinfo->stacks[stack], ec);
    if (!l)
        return;

    /* move left */
    overlay = &_G.move_overlays[MOVE_LEFT];
    if ((_G.tinfo->conf->use_rows && l->prev)
    || (!_G.tinfo->conf->use_rows && !_G.tinfo->stacks[TILING_MAX_STACKS -1]
        && (ec != _G.tinfo->stacks[0]->data || _G.tinfo->stacks[0]->next)))
    {
        if (overlay->popup) {
            Evas_Coord ew, eh;

            edje_object_size_min_calc(overlay->obj, &ew, &eh);
            evas_object_move(_G.move_overlays[MOVE_LEFT].popup,
                         extra->expected.x - ew/2,
                         extra->expected.y + extra->expected.h/2 - eh/2);
        } else {
            Evas_Coord ew, eh;

            overlay->obj = edje_object_add(ec->comp->evas);
            _theme_edje_object_set(overlay->obj,
                                   "modules/tiling/move/left");
            edje_object_size_min_calc(overlay->obj, &ew, &eh);
            overlay->popup = e_comp_object_util_add(overlay->obj, E_COMP_OBJECT_TYPE_POPUP);
            evas_object_layer_set(overlay->popup, E_LAYER_CLIENT_NORMAL);
            evas_object_show(overlay->obj);
            evas_object_geometry_set(overlay->popup,
                                extra->expected.x - ew/2
                                                  - ec->zone->x,
                                extra->expected.y + extra->expected.h/2
                                                  - eh/2
                                                  - ec->zone->y,
                                ew,
                                eh);

            evas_object_show(overlay->popup);
        }
    } else if (overlay->popup) {
        if (overlay->popup) {
            evas_object_hide(overlay->popup);
            evas_object_del(overlay->popup);
            overlay->popup = NULL;
        }
    }

    /* move right */
    overlay = &_G.move_overlays[MOVE_RIGHT];
    if ((_G.tinfo->conf->use_rows && l->next)
    || (!_G.tinfo->conf->use_rows && (
            stack != TILING_MAX_STACKS - 1
            && ((stack == nb_stacks - 1 && _G.tinfo->stacks[stack]->next)
                || (stack != nb_stacks - 1))))) {
        if (overlay->popup) {
            Evas_Coord ew, eh;

            edje_object_size_min_calc(overlay->obj, &ew, &eh);
            evas_object_move(_G.move_overlays[MOVE_RIGHT].popup,
                         extra->expected.x + extra->expected.w - ew/2,
                         extra->expected.y + extra->expected.h/2 - eh/2);
        } else {
            Evas_Coord ew, eh;

            overlay->obj = edje_object_add(ec->comp->evas);
            overlay->popup = e_comp_object_util_add(overlay->obj, E_COMP_OBJECT_TYPE_POPUP);
            evas_object_layer_set(overlay->popup, E_LAYER_CLIENT_NORMAL);
            _theme_edje_object_set(overlay->obj,
                                   "modules/tiling/move/right");
            edje_object_size_min_calc(overlay->obj, &ew, &eh);
            evas_object_geometry_set(overlay->popup,
                                extra->expected.x + extra->expected.w - ew/2
                                                  - ec->zone->x,
                                extra->expected.y + extra->expected.h/2
                                                  - eh/2
                                                  - ec->zone->y,
                                ew,
                                eh);
            evas_object_show(overlay->popup);
        }
    } else if (overlay->popup) {
        if (overlay->popup) {
            evas_object_hide(overlay->popup);
            evas_object_del(overlay->popup);
            overlay->popup = NULL;
        }
    }

    /* move up */
    overlay = &_G.move_overlays[MOVE_UP];
    if ((!_G.tinfo->conf->use_rows && l->prev)
    ||  (_G.tinfo->conf->use_rows && !_G.tinfo->stacks[TILING_MAX_STACKS -1]
        && (ec != _G.tinfo->stacks[0]->data || _G.tinfo->stacks[0]->next)))
    {
        if (overlay->popup) {
            Evas_Coord ew, eh;

            edje_object_size_min_calc(overlay->obj, &ew, &eh);
            evas_object_move(_G.move_overlays[MOVE_UP].popup,
                         extra->expected.x + extra->expected.w/2 - ew/2,
                         extra->expected.y - eh/2);
        } else {
            Evas_Coord ew, eh;

            overlay->obj = edje_object_add(ec->comp->evas);
            overlay->popup = e_comp_object_util_add(overlay->obj, E_COMP_OBJECT_TYPE_POPUP);
            evas_object_layer_set(overlay->popup, E_LAYER_CLIENT_NORMAL);
            _theme_edje_object_set(overlay->obj, "modules/tiling/move/up");
            edje_object_size_min_calc(overlay->obj, &ew, &eh);
            evas_object_geometry_set(overlay->popup,
                                extra->expected.x + extra->expected.w/2
                                                  - ew/2
                                                  - ec->zone->x,
                                extra->expected.y - eh/2
                                                  - ec->zone->y,
                                ew,
                                eh);

            evas_object_show(overlay->popup);
        }
    } else if (overlay->popup) {
        if (overlay->popup) {
            evas_object_hide(overlay->popup);
            evas_object_del(overlay->popup);
            overlay->popup = NULL;
        }
    }

    /* move down */
    overlay = &_G.move_overlays[MOVE_DOWN];
    if ((!_G.tinfo->conf->use_rows && l->next)
    || (_G.tinfo->conf->use_rows && (
            stack != TILING_MAX_STACKS - 1
            && ((stack == nb_stacks - 1 && _G.tinfo->stacks[stack]->next)
                || (stack != nb_stacks - 1))))) {
        if (overlay->popup) {
            Evas_Coord ew, eh;

            edje_object_size_min_calc(overlay->obj, &ew, &eh);
            evas_object_move(_G.move_overlays[MOVE_DOWN].popup,
                         extra->expected.x + extra->expected.w/2 - ew/2,
                         extra->expected.y + extra->expected.h - eh/2);
        } else {
            Evas_Coord ew, eh;

            overlay->obj = edje_object_add(ec->comp->evas);
            overlay->popup = e_comp_object_util_add(overlay->obj, E_COMP_OBJECT_TYPE_POPUP);
            evas_object_layer_set(overlay->popup, E_LAYER_CLIENT_NORMAL);
            _theme_edje_object_set(overlay->obj,
                                   "modules/tiling/move/down");
            edje_object_size_min_calc(overlay->obj, &ew, &eh);
            evas_object_geometry_set(overlay->popup,
                                extra->expected.x + extra->expected.w/2
                                                  - ew/2
                                                  - ec->zone->x,
                                extra->expected.y + extra->expected.h - eh/2
                                                  - ec->zone->y,
                                ew,
                                eh);
            evas_object_show(overlay->popup);
        }
    } else if (overlay->popup) {
        if (overlay->popup) {
            evas_object_hide(overlay->popup);
            evas_object_del(overlay->popup);
            overlay->popup = NULL;
        }
    }
}

static void
_move_right_rows_or_down_cols(E_Client *ec, Eina_Bool check_moving_anims)
{
    E_Client *ec_1 = ec,
             *ec_2 = NULL;
    Client_Extra *extra_1 = NULL,
                 *extra_2 = NULL;
    Eina_List *l_1 = NULL,
              *l_2 = NULL;
    int stack;

    stack = get_stack(ec);
    if (stack < 0)
        return;

    l_1 = eina_list_data_find_list(_G.tinfo->stacks[stack], ec_1);
    if (!l_1 || !l_1->next)
        return;
    l_2 = l_1->next;
    ec_2 = l_2->data;

    extra_1 = eina_hash_find(_G.client_extras, &ec_1);
    if (!extra_1) {
        ERR("No extra for %p", ec_1);
        return;
    }
    extra_2 = eina_hash_find(_G.client_extras, &ec_2);
    if (!extra_2) {
        ERR("No extra for %p", ec_2);
        return;
    }

    l_1->data = ec_2;
    l_2->data = ec_1;

    if (_G.tinfo->conf->use_rows) {
        extra_2->expected.x = extra_1->expected.x;
        extra_1->expected.x += extra_2->expected.w;
    } else {
        extra_2->expected.y = extra_1->expected.y;
        extra_1->expected.y += extra_2->expected.h;
    }

    _e_client_move(ec_1,
                   extra_1->expected.x,
                   extra_1->expected.y);
    _e_client_move(ec_2,
                   extra_2->expected.x,
                   extra_2->expected.y);

    if (check_moving_anims)
        _check_moving_anims(ec_1, extra_1, stack);

    ecore_x_pointer_warp(_G.tinfo->desk->zone->comp->win,
                         extra_1->expected.x + extra_1->expected.w/2,
                         extra_1->expected.y + extra_1->expected.h/2);
}

static void
_move_left_rows_or_up_cols(E_Client *ec, Eina_Bool check_moving_anims)
{
    E_Client *ec_1 = ec,
             *ec_2 = NULL;
    Client_Extra *extra_1 = NULL,
                 *extra_2 = NULL;
    Eina_List *l_1 = NULL,
              *l_2 = NULL;
    int stack;

    stack = get_stack(ec);
    assert(stack >= 0);

    if (_G.tinfo->stacks[stack]->data == ec)
        return;

    l_1 = eina_list_data_find_list(_G.tinfo->stacks[stack], ec_1);
    if (!l_1 || !l_1->prev)
        return;
    l_2 = l_1->prev;
    ec_2 = l_2->data;

    extra_1 = eina_hash_find(_G.client_extras, &ec_1);
    if (!extra_1) {
        ERR("No extra for %p", ec_1);
        return;
    }
    extra_2 = eina_hash_find(_G.client_extras, &ec_2);
    if (!extra_2) {
        ERR("No extra for %p", ec_2);
        return;
    }

    l_1->data = ec_2;
    l_2->data = ec_1;

    if (_G.tinfo->conf->use_rows) {
        extra_1->expected.x = extra_2->expected.x;
        extra_2->expected.x += extra_1->expected.w;
    } else {
        extra_1->expected.y = extra_2->expected.y;
        extra_2->expected.y += extra_1->expected.h;
    }

    _e_client_move(ec_1,
                   extra_1->expected.x,
                   extra_1->expected.y);
    _e_client_move(ec_2,
                   extra_2->expected.x,
                   extra_2->expected.y);

    if (check_moving_anims)
        _check_moving_anims(ec_1, extra_1, stack);

    ecore_x_pointer_warp(_G.tinfo->desk->zone->comp->win,
                         extra_1->expected.x + extra_1->expected.w/2,
                         extra_1->expected.y + extra_1->expected.h/2);
}

static void
_move_up_rows_or_left_cols(E_Client *ec, Eina_Bool check_moving_anims)
{
    Client_Extra *extra;
    int stack;
    int i;
    int nb_stacks;

    stack = get_stack(ec);
    assert(stack >= 0);

    nb_stacks = get_stack_count();

    extra = eina_hash_find(_G.client_extras, &ec);
    if (!extra) {
        ERR("No extra for %p", ec);
        return;
    }

    if (stack == 0) {
        int x, y, w, h, s;
        int size = 0, pos;

        if (nb_stacks >= TILING_MAX_STACKS)
            return;
        if (_G.tinfo->stacks[0]->data == ec && !_G.tinfo->stacks[0]->next)
            return;

        EINA_LIST_REMOVE(_G.tinfo->stacks[0], ec);
        for (i = TILING_MAX_STACKS - 1; i > 0; i--) {
            _G.tinfo->stacks[i] = _G.tinfo->stacks[i-1];
        }
        _G.tinfo->stacks[0] = NULL;
        EINA_LIST_APPEND(_G.tinfo->stacks[0], ec);

        e_zone_useful_geometry_get(ec->zone, &x, &y, &w, &h);

        extra->expected.x = x;
        extra->expected.y = y;
        if (_G.tinfo->conf->use_rows) {
            s = h;
            pos = x;
            size = s / (nb_stacks + 1);
            extra->expected.w = w;
            extra->expected.h = size;
        } else {
            s = w;
            pos = y;
            size = s / (nb_stacks + 1);
            extra->expected.w = size;
            extra->expected.h = h;
        }
        _G.tinfo->pos[0] = pos;
        _G.tinfo->size[0] = size;

        s -= size;
        pos += size;

        for (i = 1; i <= nb_stacks; i++) {
            size = s / (nb_stacks + 1 - i);

            _set_stack_geometry(i, pos, size);

            s -= size;
            pos += size;
        }
        _reorganize_stack(1);

        _e_client_move_resize(ec,
                              extra->expected.x,
                              extra->expected.y,
                              extra->expected.w,
                              extra->expected.h);
        _e_client_maximize(ec, E_MAXIMIZE_EXPAND | E_MAXIMIZE_VERTICAL);

        if (nb_stacks + 1 > _G.tinfo->conf->nb_stacks) {
            _G.tinfo->conf->nb_stacks = nb_stacks + 1;
            e_config_save_queue();
        }
        if (check_moving_anims)
            _check_moving_anims(ec, extra, 0);
        ecore_x_pointer_warp(_G.tinfo->desk->zone->comp->win,
                             extra->expected.x + extra->expected.w/2,
                             extra->expected.y + extra->expected.h/2);
        return;
    }

    EINA_LIST_REMOVE(_G.tinfo->stacks[stack], ec);
    EINA_LIST_APPEND(_G.tinfo->stacks[stack - 1], ec);

    if (!_G.tinfo->stacks[stack]) {
        int pos, s;

        /* Remove stack */
        if (_G.tinfo->conf->use_rows) {
            e_zone_useful_geometry_get(ec->zone,
                                       NULL, &pos, NULL, &s);
        } else {
            e_zone_useful_geometry_get(ec->zone,
                                       &pos, NULL, &s, NULL);
        }

        nb_stacks--;

        assert((0 <= nb_stacks) && (nb_stacks < TILING_MAX_STACKS - 1));
        for (i = stack; i < nb_stacks; i++) {
            _G.tinfo->stacks[i] = _G.tinfo->stacks[i+1];
        }
        _G.tinfo->stacks[nb_stacks] = NULL;
        for (i = 0; i < nb_stacks; i++) {
            int size;

            size = s / (nb_stacks - i);

            _set_stack_geometry(i, pos, size);

            s -= size;
            pos += size;
        }
        _reorganize_stack(stack - 1);
    } else {
        _reorganize_stack(stack);
        _reorganize_stack(stack - 1);
    }

    if (check_moving_anims)
        _check_moving_anims(ec, extra, stack - 1);

    ecore_x_pointer_warp(_G.tinfo->desk->zone->comp->win,
                         extra->expected.x + extra->expected.w/2,
                         extra->expected.y + extra->expected.h/2);
}

static void
_move_down_rows_or_right_cols(E_Client *ec, Eina_Bool check_moving_anims)
{
    int stack;
    int nb_stacks;
    Client_Extra *extra;
    int i;

    stack = get_stack(ec);
    assert(stack >= 0);
    if (stack == TILING_MAX_STACKS - 1)
        return;

    nb_stacks = get_stack_count();
    assert((0 < nb_stacks) && (nb_stacks < TILING_MAX_STACKS));
    if (stack == nb_stacks - 1 && !_G.tinfo->stacks[stack]->next)
        return;

    extra = eina_hash_find(_G.client_extras, &ec);
    if (!extra) {
        ERR("No extra for %p", ec);
        return;
    }

    EINA_LIST_REMOVE(_G.tinfo->stacks[stack], ec);
    EINA_LIST_APPEND(_G.tinfo->stacks[stack + 1], ec);

    if (_G.tinfo->stacks[stack] && _G.tinfo->stacks[stack + 1]->next) {
        _reorganize_stack(stack);
        _reorganize_stack(stack + 1);
        if (check_moving_anims)
            _check_moving_anims(ec, extra, stack + 1);
    } else
    if (_G.tinfo->stacks[stack]) {
        /* Add stack */
        int x, y, w, h, s;
        int size = 0, pos;

        assert(nb_stacks < TILING_MAX_STACKS);

        _reorganize_stack(stack);

        e_zone_useful_geometry_get(ec->zone, &x, &y, &w, &h);

        if (_G.tinfo->conf->use_rows) {
            pos = y;
            s = h;
        } else {
            pos = x;
            s = w;
        }

        for (i = 0; i < nb_stacks; i++) {
            size = s / (nb_stacks + 1 - i);

            _set_stack_geometry(i, pos, size);

            s -= size;
            pos += size;
        }

        _G.tinfo->pos[nb_stacks] = pos;
        _G.tinfo->size[nb_stacks] = size;
        if (_G.tinfo->conf->use_rows) {
            extra->expected.x = x;
            extra->expected.y = pos;
            extra->expected.w = w;
            extra->expected.h = size;
        } else {
            extra->expected.x = pos;
            extra->expected.y = y;
            extra->expected.w = size;
            extra->expected.h = h;
        }
        _e_client_move_resize(ec,
                              extra->expected.x,
                              extra->expected.y,
                              extra->expected.w,
                              extra->expected.h);
        _e_client_maximize(ec, E_MAXIMIZE_EXPAND | E_MAXIMIZE_HORIZONTAL);

        if (nb_stacks + 1 > _G.tinfo->conf->nb_stacks) {
            _G.tinfo->conf->nb_stacks = nb_stacks + 1;
            e_config_save_queue();
        }
        if (check_moving_anims)
            _check_moving_anims(ec, extra, stack + 1);
    } else {
        int x, y, w, h, s, pos;

        /* Remove stack */
        e_zone_useful_geometry_get(ec->zone, &x, &y, &w, &h);

        nb_stacks--;

        assert((0 <= nb_stacks) && (nb_stacks < TILING_MAX_STACKS - 1));
        for (i = stack; i < nb_stacks; i++) {
             _G.tinfo->stacks[i] = _G.tinfo->stacks[i + 1];
        }

        if (_G.tinfo->conf->use_rows) {
            pos = y;
            s = h;
        } else {
            pos = x;
            s = w;
        }

        for (i = 0; i < nb_stacks; i++) {
            int size;

            size = s / (nb_stacks - i);

            _set_stack_geometry(i, pos, size);

            s -= size;
            pos += size;
        }
        _G.tinfo->stacks[nb_stacks] = NULL;
        _G.tinfo->pos[nb_stacks] = 0;
        _G.tinfo->size[nb_stacks] = 0;
        _reorganize_stack(stack);
        if (check_moving_anims)
            _check_moving_anims(ec, extra, stack);
    }

    ecore_x_pointer_warp(_G.tinfo->desk->zone->comp->win,
                         extra->expected.x + extra->expected.w/2,
                         extra->expected.y + extra->expected.h/2);
}

static Eina_Bool
move_key_down(void *data __UNUSED__,
              int type __UNUSED__,
              void *event)
{
    Ecore_Event_Key *ev = event;

    if (ev->event_window != _G.action_input_win)
        return ECORE_CALLBACK_PASS_ON;

    /* reset timer */
    ecore_timer_delay(_G.action_timer, TILING_OVERLAY_TIMEOUT
                      - ecore_timer_pending_get(_G.action_timer));

    if ((strcmp(ev->key, "Up") == 0)
    ||  (strcmp(ev->key, "k") == 0))
    {
        if (_G.tinfo->conf->use_rows)
            _move_up_rows_or_left_cols(_G.focused_ec, true);
        else
            _move_left_rows_or_up_cols(_G.focused_ec, true);
        return ECORE_CALLBACK_PASS_ON;
    } else if ((strcmp(ev->key, "Down") == 0)
           ||  (strcmp(ev->key, "j") == 0))
    {
        if (_G.tinfo->conf->use_rows)
            _move_down_rows_or_right_cols(_G.focused_ec, true);
        else
            _move_right_rows_or_down_cols(_G.focused_ec, true);
        return ECORE_CALLBACK_PASS_ON;
    } else if ((strcmp(ev->key, "Left") == 0)
           ||  (strcmp(ev->key, "h") == 0))
    {
        if (_G.tinfo->conf->use_rows)
            _move_left_rows_or_up_cols(_G.focused_ec, true);
        else
            _move_up_rows_or_left_cols(_G.focused_ec, true);
        return ECORE_CALLBACK_PASS_ON;
    } else if ((strcmp(ev->key, "Right") == 0)
           ||  (strcmp(ev->key, "l") == 0))
    {
        if (_G.tinfo->conf->use_rows)
            _move_right_rows_or_down_cols(_G.focused_ec, true);
        else
            _move_down_rows_or_right_cols(_G.focused_ec, true);
        return ECORE_CALLBACK_PASS_ON;
    }

    if (strcmp(ev->key, "Return") == 0)
        goto stop;
    if (strcmp(ev->key, "Escape") == 0)
        goto stop; /* TODO: fallback */

    return ECORE_CALLBACK_PASS_ON;
stop:
    end_special_input();
    return ECORE_CALLBACK_DONE;
}

static void
_e_mod_action_move_direct_cb(E_Object   *obj __UNUSED__,
                             const char *params)
{
    E_Desk *desk;
    E_Client *focused_ec;

    desk = get_current_desk();
    if (!desk)
        return;

    focused_ec = e_client_focused_get();
    if (!focused_ec || focused_ec->desk != desk)
        return;

    check_tinfo(desk);
    if (!_G.tinfo->conf || !_G.tinfo->conf->nb_stacks) {
        return;
    }


    assert(params != NULL);

    switch (params[0]) {
      case 'l': /* left */
        if (_G.tinfo->conf->use_rows)
            _move_left_rows_or_up_cols(focused_ec, false);
        else
            _move_up_rows_or_left_cols(focused_ec, false);
        break;
      case 'r': /* right */
        if (_G.tinfo->conf->use_rows)
            _move_right_rows_or_down_cols(focused_ec, false);
        else
            _move_down_rows_or_right_cols(focused_ec, false);
        break;
      case 'u': /* up */
        if (_G.tinfo->conf->use_rows)
            _move_up_rows_or_left_cols(focused_ec, false);
        else
            _move_left_rows_or_up_cols(focused_ec, false);
        break;
      case 'd': /* down */
        if (_G.tinfo->conf->use_rows)
            _move_down_rows_or_right_cols(focused_ec, false);
        else
            _move_right_rows_or_down_cols(focused_ec, false);
        break;
    }
}

static void
_e_mod_action_move_cb(E_Object   *obj __UNUSED__,
                      const char *params __UNUSED__)
{
    E_Desk *desk;
    E_Client *focused_ec;
    Ecore_X_Window parent;

    desk = get_current_desk();
    if (!desk)
        return;

    focused_ec = e_client_focused_get();
    if (!focused_ec || focused_ec->desk != desk)
        return;

    check_tinfo(desk);
    if (!_G.tinfo->conf || !_G.tinfo->conf->nb_stacks) {
        return;
    }

    _G.focused_ec = focused_ec;

    _G.input_mode = INPUT_MODE_MOVING;

    /* Get input */
    parent = focused_ec->zone->comp->win;
    _G.action_input_win = ecore_x_window_input_new(parent, 0, 0, 1, 1);
    if (!_G.action_input_win) {
        end_special_input();
        return;
    }

    ecore_x_window_show(_G.action_input_win);
    if (!e_grabinput_get(_G.action_input_win, 0, _G.action_input_win)) {
        end_special_input();
        return;
    }
    _G.action_timer = ecore_timer_add(TILING_OVERLAY_TIMEOUT,
                                      _timeout_cb, NULL);

    _G.handler_key = ecore_event_handler_add(ECORE_EVENT_KEY_DOWN,
                                             move_key_down, NULL);
    _check_moving_anims(focused_ec, NULL, -1);
}

/* }}} */
/* Adjust Transitions {{{ */

static void
_transition_overlays_free_cb(void *data)
{
    transition_overlay_t *trov = data;

    if (trov->overlay.popup) {
        evas_object_hide(trov->overlay.popup);
        evas_object_del(trov->overlay.popup);
        trov->overlay.popup = NULL;
    }
    if (trov != _G.transition_overlay) {
        E_FREE(trov);
    }
}

static void
_transition_move_cols(tiling_move_t direction)
{
    int delta = TILING_RESIZE_STEP;
    int stack;
    Evas_Object *popup = NULL;

    if (!_G.transition_overlay)
        return;

    stack = _G.transition_overlay->stack;

    if (_G.transition_overlay->ec) {
        Eina_List *l = NULL;
        E_Client *ec = _G.transition_overlay->ec,
                 *nextec = NULL;
        Client_Extra *extra = NULL,
                     *nextextra = NULL;
        int min_height = 0;
        int x, y;

        l = eina_list_data_find_list(_G.tinfo->stacks[stack], ec);
        if (!l) {
            ERR("unable to ec %p in stack %d", ec, stack);
            return;
        }

        extra = eina_hash_find(_G.client_extras, &ec);
        if (!extra) {
            ERR("No extra for %p", ec);
            return;
        }
        nextec = l->next->data;
        nextextra = eina_hash_find(_G.client_extras, &nextec);
        if (!nextextra) {
            ERR("No extra for %p", nextec);
            return;
        }

        if (direction == MOVE_UP) {
            delta *= -1;
        }

        nextec = l->next->data;
        min_height = MAX(nextec->icccm.base_h, 1);

        if (nextextra->expected.h - delta < min_height)
            delta = nextextra->expected.h - min_height;

        nextextra->expected.y += delta;
        nextextra->expected.h -= delta;
        _e_client_move_resize(nextec,
                              nextextra->expected.x,
                              nextextra->expected.y,
                              nextextra->expected.w,
                              nextextra->expected.h);

        extra->expected.h += delta;
        _e_client_move_resize(ec,
                              extra->expected.x,
                              extra->expected.y,
                              extra->expected.w,
                              extra->expected.h);

        popup = _G.transition_overlay->overlay.popup;
        evas_object_geometry_get(popup, &x, &y, NULL, NULL);
        evas_object_move(popup, x, y + delta);
    } else {
        int x, y;

        if (stack == TILING_MAX_STACKS || !_G.tinfo->stacks[stack + 1]) {
            return;
        }
        if (direction == MOVE_LEFT) {
            delta *= -1;
        }

        if (delta + 1 > _G.tinfo->size[stack + 1])
            delta = _G.tinfo->size[stack + 1] - 1;

        _move_resize_stack(stack, 0, delta);
        _move_resize_stack(stack+1, delta, -delta);

        popup = _G.transition_overlay->overlay.popup;
        evas_object_geometry_get(popup, &x, &y, NULL, NULL);
        evas_object_move(popup, x + delta, y);
    }
}

static void
_transition_move_rows(tiling_move_t direction)
{
    int delta = TILING_RESIZE_STEP;
    int stack;
    int x, y;
    Evas_Object *popup = NULL;

    if (!_G.transition_overlay)
        return;

    stack = _G.transition_overlay->stack;

    if (_G.transition_overlay->ec) {
        Eina_List *l = NULL;
        E_Client *ec = _G.transition_overlay->ec,
                 *nextec = NULL;
        Client_Extra *extra = NULL,
                     *nextextra = NULL;
        int min_width = 0;

        l = eina_list_data_find_list(_G.tinfo->stacks[stack], ec);
        if (!l) {
            ERR("unable to ec %p in stack %d", ec, stack);
            return;
        }

        extra = eina_hash_find(_G.client_extras, &ec);
        if (!extra) {
            ERR("No extra for %p", ec);
            return;
        }
        nextec = l->next->data;
        nextextra = eina_hash_find(_G.client_extras, &nextec);
        if (!nextextra) {
            ERR("No extra for %p", nextec);
            return;
        }

        if (direction == MOVE_LEFT) {
            delta *= -1;
        }

        nextec = l->next->data;
        min_width = MAX(nextec->icccm.base_w, 1);

        if (nextextra->expected.w - delta < min_width)
            delta = nextextra->expected.w - min_width;

        nextextra->expected.x += delta;
        nextextra->expected.w -= delta;
        _e_client_move_resize(nextec,
                              nextextra->expected.x,
                              nextextra->expected.y,
                              nextextra->expected.w,
                              nextextra->expected.h);

        extra->expected.w += delta;
        _e_client_move_resize(ec,
                              extra->expected.x,
                              extra->expected.y,
                              extra->expected.w,
                              extra->expected.h);

        popup = _G.transition_overlay->overlay.popup;
        evas_object_geometry_get(popup, &x, &y, NULL, NULL);
        evas_object_move(popup, x + delta, y);
    } else {

        if (stack == TILING_MAX_STACKS || !_G.tinfo->stacks[stack + 1]) {
            return;
        }
        if (direction == MOVE_UP) {
            delta *= -1;
        }

        if (delta + 1 > _G.tinfo->size[stack + 1])
            delta = _G.tinfo->size[stack + 1] - 1;

        _move_resize_stack(stack, 0, delta);
        _move_resize_stack(stack+1, delta, -delta);

        popup = _G.transition_overlay->overlay.popup;
        evas_object_geometry_get(popup, &x, &y, NULL, NULL);
        evas_object_move(popup, x, y + delta);
    }
}

static Eina_Bool
_transition_overlay_key_down(void *data __UNUSED__,
                             int type __UNUSED__,
                             void *event)
{
    Ecore_Event_Key *ev = event;

    if (ev->event_window != _G.action_input_win)
        return ECORE_CALLBACK_PASS_ON;

    if (strcmp(ev->key, "Return") == 0)
        goto stop;
    if (strcmp(ev->key, "Escape") == 0)
        goto stop;

    /* reset timer */
    ecore_timer_delay(_G.action_timer, TILING_OVERLAY_TIMEOUT
                      - ecore_timer_pending_get(_G.action_timer));

    if (_G.transition_overlay) {
        DBG("ev->key='%s'; %p %d", ev->key,
            _G.transition_overlay->ec, _G.tinfo->conf->use_rows);
        if ((strcmp(ev->key, "Up") == 0)
            ||  (strcmp(ev->key, "k") == 0))
        {
            if (_G.transition_overlay->ec && !_G.tinfo->conf->use_rows) {
                _transition_move_cols(MOVE_UP);
                return ECORE_CALLBACK_PASS_ON;
            } else
            if (!_G.transition_overlay->ec && _G.tinfo->conf->use_rows) {
                _transition_move_rows(MOVE_UP);
                return ECORE_CALLBACK_PASS_ON;
            }
        } else
        if ((strcmp(ev->key, "Down") == 0)
        ||  (strcmp(ev->key, "j") == 0))
        {
            if (_G.transition_overlay->ec && !_G.tinfo->conf->use_rows) {
                _transition_move_cols(MOVE_DOWN);
                return ECORE_CALLBACK_PASS_ON;
            } else
            if (!_G.transition_overlay->ec && _G.tinfo->conf->use_rows) {
                _transition_move_rows(MOVE_DOWN);
                return ECORE_CALLBACK_PASS_ON;
            }
        } else
        if ((strcmp(ev->key, "Left") == 0)
        ||  (strcmp(ev->key, "h") == 0))
        {
            if (!_G.transition_overlay->ec && !_G.tinfo->conf->use_rows) {
                _transition_move_cols(MOVE_LEFT);
                return ECORE_CALLBACK_PASS_ON;
            } else
            if (_G.transition_overlay->ec && _G.tinfo->conf->use_rows) {
                _transition_move_rows(MOVE_LEFT);
                return ECORE_CALLBACK_PASS_ON;
            }
        } else
        if ((strcmp(ev->key, "Right") == 0)
        ||  (strcmp(ev->key, "l") == 0))
        {
            if (!_G.transition_overlay->ec && !_G.tinfo->conf->use_rows) {
                _transition_move_cols(MOVE_RIGHT);
                return ECORE_CALLBACK_PASS_ON;
            } else
            if (_G.transition_overlay->ec && _G.tinfo->conf->use_rows) {
                _transition_move_rows(MOVE_RIGHT);
                return ECORE_CALLBACK_PASS_ON;
            }
        }

        return ECORE_CALLBACK_RENEW;
    } else {
        if (strcmp(ev->key, "Backspace") == 0) {
            char *key = _G.keys;

            while (*key)
                key++;
            *key = '\0';
            return ECORE_CALLBACK_RENEW;
        }
        if (ev->key[0] && !ev->key[1] && strchr(tiling_g.config->keyhints,
                                                ev->key[1]))
        {
            transition_overlay_t *trov = NULL;
            E_Client *ec = NULL;
            Client_Extra *extra = NULL;
            Evas_Coord ew, eh;
            char *key = _G.keys;

            while (*key)
                key++;
            *key++ = ev->key[0];
            *key = '\0';

            trov = eina_hash_find(_G.overlays, _G.keys);
            if (!trov) {
                return ECORE_CALLBACK_RENEW;
            }
            ec = trov->ec;

            _G.transition_overlay = trov;
            eina_hash_free(_G.overlays);
            _G.overlays = NULL;

            if (ec) {
                extra = eina_hash_find(_G.client_extras, &ec);
                if (!extra) {
                    ERR("No extra for %p", ec);
                    goto stop;
                }
            }
            if (!trov->overlay.obj) {
                trov->overlay.obj =
                    edje_object_add(e_comp_get(_G.tinfo->desk)->evas);
            }
            if (!trov->overlay.popup) {
                trov->overlay.popup = e_comp_object_util_add(trov->overlay.obj, E_COMP_OBJECT_TYPE_POPUP);
                evas_object_layer_set(trov->overlay.popup, E_LAYER_CLIENT_NORMAL);
            }
            if ((ec && !_G.tinfo->conf->use_rows)
            ||  (!ec && _G.tinfo->conf->use_rows)) {
                _theme_edje_object_set(trov->overlay.obj,
                                       "modules/tiling/transition/horizontal");
            } else {
                _theme_edje_object_set(trov->overlay.obj,
                                       "modules/tiling/transition/vertical");
            }

            edje_object_size_min_calc(trov->overlay.obj, &ew, &eh);
            if (ec) {
                if (_G.tinfo->conf->use_rows) {
                    evas_object_geometry_set(trov->overlay.popup,
                        (extra->expected.x - trov->ec->zone->x +
                            extra->expected.w - (ew / 2)),
                        (extra->expected.y - trov->ec->zone->y +
                            ((extra->expected.h - eh) / 2)),
                        ew, eh);
                } else {
                    evas_object_geometry_set(trov->overlay.popup,
                        (extra->expected.x - trov->ec->zone->x +
                            ((extra->expected.w - ew) / 2)),
                        (extra->expected.y - trov->ec->zone->y +
                            extra->expected.h - (eh / 2)),
                        ew, eh);
                }
            } else {
                if (_G.tinfo->conf->use_rows) {
                    evas_object_geometry_set(trov->overlay.popup,
                                        (trov->ec->zone->w/2 - ew/2),
                                        (_G.tinfo->pos[trov->stack]
                                         + _G.tinfo->size[trov->stack]
                                         - trov->ec->zone->y - eh/2),
                                        ew, eh);
                } else {
                    evas_object_geometry_set(trov->overlay.popup,
                                        (_G.tinfo->pos[trov->stack]
                                         + _G.tinfo->size[trov->stack]
                                         - trov->ec->zone->x - ew/2),
                                        (trov->ec->zone->h/2 - eh/2),
                                        ew, eh);
                }
            }
            evas_object_show(trov->overlay.popup);

            return ECORE_CALLBACK_RENEW;
        }
    }

stop:
    end_special_input();
    return ECORE_CALLBACK_DONE;
}

static void
_do_transition_overlay(void)
{
    int nb_transitions;
    Ecore_X_Window parent;
    int hints_len;
    int key_len;
    int n = 0;
    int nmax;
    int i;

    end_special_input();

    nb_transitions = get_transition_count();
    if (nb_transitions < 1) {
        return;
    }

    _G.input_mode = INPUT_MODE_TRANSITION;

    _G.overlays = eina_hash_string_small_new(_transition_overlays_free_cb);
    hints_len = strlen(tiling_g.config->keyhints);
    key_len = 1;
    nmax = hints_len;
    if (hints_len < nb_transitions) {
        key_len = 2;
        nmax *= hints_len;
        if (hints_len * hints_len < nb_transitions) {
            key_len = 3;
            nmax *= hints_len;
        }
    }


    for (i = 0; i < TILING_MAX_STACKS; i++) {
        Eina_List *l;
        E_Client *ec;

        if (!_G.tinfo->stacks[i])
            break;
        EINA_LIST_FOREACH(_G.tinfo->stacks[i], l, ec) {
            if (l->next && n < nmax) {
                Client_Extra *extra;
                Evas_Coord ew, eh;
                transition_overlay_t *trov;

                extra = eina_hash_find(_G.client_extras, &ec);
                if (!extra) {
                    ERR("No extra for %p", ec);
                    continue;
                }

                trov = E_NEW(transition_overlay_t, 1);

                trov->overlay.obj = edje_object_add(ec->comp->evas);
                trov->overlay.popup = e_comp_object_util_add(trov->overlay.obj, E_COMP_OBJECT_TYPE_POPUP);
                evas_object_layer_set(trov->overlay.popup, E_LAYER_CLIENT_NORMAL);
                e_theme_edje_object_set(trov->overlay.obj,
                                        "base/theme/borders",
                                        "e/widgets/border/default/resize");

                switch (key_len) {
                  case 1:
                    trov->key[0] = tiling_g.config->keyhints[n];
                    trov->key[1] = '\0';
                    break;
                  case 2:
                    trov->key[0] = tiling_g.config->keyhints[n / hints_len];
                    trov->key[1] = tiling_g.config->keyhints[n % hints_len];
                    trov->key[2] = '\0';
                    break;
                  case 3:
                    trov->key[0] = tiling_g.config->keyhints[n / hints_len / hints_len];
                    trov->key[0] = tiling_g.config->keyhints[n / hints_len];
                    trov->key[1] = tiling_g.config->keyhints[n % hints_len];
                    trov->key[2] = '\0';
                    break;
                }
                n++;
                trov->stack = i;
                trov->ec = ec;

                eina_hash_add(_G.overlays, trov->key, trov);
                edje_object_part_text_set(trov->overlay.obj,
                                          "e.text.label",
                                          trov->key);
                edje_object_size_min_calc(trov->overlay.obj, &ew, &eh);

                if (_G.tinfo->conf->use_rows) {
                    evas_object_geometry_set(trov->overlay.popup,
                        (extra->expected.x - trov->ec->zone->x +
                            extra->expected.w - (ew / 2)),
                        (extra->expected.y - trov->ec->zone->y +
                            ((extra->expected.h - eh) / 2)),
                        ew, eh);
                } else {
                    evas_object_geometry_set(trov->overlay.popup,
                        (extra->expected.x - trov->ec->zone->x +
                            ((extra->expected.w - ew) / 2)),
                        (extra->expected.y - trov->ec->zone->y +
                            extra->expected.h - (eh / 2)),
                        ew, eh);
                }

                evas_object_show(trov->overlay.popup);
            }
        }
        if (i != (TILING_MAX_STACKS - 1) &&
            _G.tinfo->stacks[i+1] && n < nmax)
        {
            Evas_Coord ew, eh;
            transition_overlay_t *trov;

            trov = E_NEW(transition_overlay_t, 1);

            trov->overlay.obj = edje_object_add(e_comp_get(_G.tinfo->desk)->evas);
            trov->overlay.popup = e_comp_object_util_add(trov->overlay.obj, E_COMP_OBJECT_TYPE_POPUP);
            evas_object_layer_set(trov->overlay.popup, E_LAYER_CLIENT_NORMAL);
            e_theme_edje_object_set(trov->overlay.obj,
                                    "base/theme/borders",
                                    "e/widgets/border/default/resize");

            switch (key_len) {
              case 1:
                trov->key[0] = tiling_g.config->keyhints[n];
                trov->key[1] = '\0';
                break;
              case 2:
                trov->key[0] = tiling_g.config->keyhints[n / hints_len];
                trov->key[1] = tiling_g.config->keyhints[n % hints_len];
                trov->key[2] = '\0';
                break;
              case 3:
                trov->key[0] = tiling_g.config->keyhints[n / hints_len / hints_len];
                trov->key[0] = tiling_g.config->keyhints[n / hints_len];
                trov->key[1] = tiling_g.config->keyhints[n % hints_len];
                trov->key[2] = '\0';
                break;
            }
            n++;
            trov->stack = i;
            trov->ec = NULL;

            eina_hash_add(_G.overlays, trov->key, trov);
            edje_object_part_text_set(trov->overlay.obj,
                                      "e.text.label",
                                      trov->key);
            edje_object_size_min_calc(trov->overlay.obj, &ew, &eh);

            if (_G.tinfo->conf->use_rows) {
                evas_object_geometry_set(trov->overlay.popup,
                                    (trov->ec->zone->w/2 - ew/2),
                                    (_G.tinfo->pos[trov->stack]
                                     + _G.tinfo->size[trov->stack]
                                     - trov->ec->zone->y - eh/2),
                                    ew, eh);
            } else {
                evas_object_geometry_set(trov->overlay.popup,
                                    (_G.tinfo->pos[trov->stack]
                                     + _G.tinfo->size[trov->stack]
                                     - trov->ec->zone->x - ew/2),
                                    (trov->ec->zone->h/2 - eh/2),
                                    ew, eh);
            }

            evas_object_show(trov->overlay.popup);
        }
    }

    /* Get input */
    parent = _G.tinfo->desk->zone->comp->win;
    _G.action_input_win = ecore_x_window_input_new(parent, 0, 0, 1, 1);
    if (!_G.action_input_win) {
        end_special_input();
        return;
    }

    ecore_x_window_show(_G.action_input_win);
    if (!e_grabinput_get(_G.action_input_win, 0, _G.action_input_win)) {
        end_special_input();
        return;
    }
    _G.action_timer = ecore_timer_add(TILING_OVERLAY_TIMEOUT,
                                      _timeout_cb, NULL);

    _G.keys[0] = '\0';
    _G.handler_key = ecore_event_handler_add(ECORE_EVENT_KEY_DOWN,
                                             _transition_overlay_key_down,
                                             NULL);
}

static void
_e_mod_action_adjust_transitions(E_Object   *obj __UNUSED__,
                                 const char *params __UNUSED__)
{
    E_Desk *desk;

    desk = get_current_desk();
    if (!desk)
        return;

    check_tinfo(desk);
    if (!_G.tinfo->conf || !_G.tinfo->conf->nb_stacks) {
        return;
    }

    _do_transition_overlay();
}

/* }}} */
/* Go {{{ */

static Eina_Bool
_warp_timer(void *data __UNUSED__)
{
    if (_G.warp_timer) {
        double spd = TILING_WRAP_SPEED;

        _G.old_warp_x = _G.warp_x;
        _G.old_warp_y = _G.warp_y;
        _G.warp_x = (_G.warp_x * (1.0 - spd)) + (_G.warp_to_x * spd);
        _G.warp_y = (_G.warp_y * (1.0 - spd)) + (_G.warp_to_y * spd);

        ecore_x_pointer_warp(_G.tinfo->desk->zone->comp->win,
                             _G.warp_x, _G.warp_y);

        if (abs(_G.warp_x - _G.old_warp_x) <= 1
        &&  abs(_G.warp_y - _G.old_warp_y) <= 1) {
            _G.warp_timer = NULL;
            return ECORE_CALLBACK_CANCEL;
        }

        return ECORE_CALLBACK_RENEW;
    }
    _G.warp_timer = NULL;
    return ECORE_CALLBACK_CANCEL;
}

static void
_action_go(E_Client *data __UNUSED__,
           Client_Extra *extra_2)
{
    E_Client *ec = extra_2->client;

    _G.warp_to_x = ec->x + (ec->w / 2);
    _G.warp_to_y = ec->y + (ec->h / 2);
    ecore_x_pointer_xy_get(_G.tinfo->desk->zone->comp->win,
                           &_G.warp_x, &_G.warp_y);
    e_client_focus_latest_set(ec);
    _G.warp_timer = ecore_timer_add(0.01, _warp_timer, NULL);
}

static void
_e_mod_action_go_cb(E_Object   *obj __UNUSED__,
                    const char *params __UNUSED__)
{
    E_Desk *desk;

    desk = get_current_desk();
    if (!desk)
        return;

    check_tinfo(desk);
    if (!_G.tinfo->conf || !_G.tinfo->conf->nb_stacks) {
        return;
    }

    _do_overlay(NULL, _action_go, INPUT_MODE_GOING);
}

/* }}} */
/* Send to a corner {{{ */

static void
_e_mod_action_send_cb(E_Object   *obj __UNUSED__,
                      const char *params) EINA_ARG_NONNULL(2)
{
    E_Desk *desk;
    E_Client *ec;
    int x, y, w, h;
    geom_t g;

    assert(params != NULL);

    desk = get_current_desk();
    if (!desk)
        return;

    ec = e_client_focused_get();
    if (!ec || ec->desk != desk)
        return;

    if (!is_tilable(ec))
        return;

    check_tinfo(desk);
    if (!_G.tinfo->conf)
        return;

    /* Fill initial values if not already done */
    _get_or_create_client_extra(ec);

    if (!tiling_g.config->show_titles) {
        if ((ec->bordername && strcmp(ec->bordername, "pixel"))
        ||  !ec->bordername)
        {
            change_window_border(ec, "pixel");
        }
    }

    if (ec->maximized)
        _e_client_unmaximize(ec, E_MAXIMIZE_BOTH);

    /* add to floating */
    if (!EINA_LIST_IS_IN(_G.tinfo->floating_windows, ec)) {
        _remove_client(ec);
        EINA_LIST_APPEND(_G.tinfo->floating_windows, ec);
    }

    e_zone_useful_geometry_get(ec->zone, &x, &y, &w, &h);
    g.w = w / 2;
    g.h = h / 2;

    if (params[0] == 'n') {
        g.y = 0;
        if (params[1] == 'w') {
            /* NW */
            g.x = 0;
        } else {
            /* NE */
            g.x = w / 2;
        }
    } else {
        g.y = h / 2;
        if (params[1] == 'w') {
            /* SW */
            g.x = 0;
        } else {
            /* SE */
            g.x = w / 2;
        }
    }
    _e_client_move_resize(ec, g.x, g.y, g.w, g.h);
}

/* }}} */
/* Hooks {{{*/

static void
_pre_client_assign_hook(void *data __UNUSED__,
                        E_Client *ec)
{
    if (tiling_g.config->show_titles)
        return;

    if (!ec) {
        return;
    }

    check_tinfo(ec->desk);
    if (!_G.tinfo->conf || !_G.tinfo->conf->nb_stacks) {
        return;
    }

    if (is_floating_window(ec)) {
        return;
    }

    if (!is_tilable(ec)) {
        return;
    }

    if (ec->fullscreen) {
         return;
    }

    /* Fill initial values if not already done */
    _get_or_create_client_extra(ec);

    if ((ec->bordername && strcmp(ec->bordername, "pixel"))
    ||  !ec->bordername)
    {
        change_window_border(ec, "pixel");
    }
}

static void _move_or_resize(E_Client *ec)
{
    Client_Extra *extra;
    int stack = -1;

    if (!ec) {
        return;
    }

    check_tinfo(ec->desk);
    if (!_G.tinfo->conf || !_G.tinfo->conf->nb_stacks) {
        return;
    }

    stack = get_stack(ec);
    if (stack < 0) {
        return;
    }

    DBG("Resize: %p / '%s' / '%s', (%d,%d), changes(size=%d, position=%d, client=%d)"
        " g:%dx%d+%d+%d ecname:'%s' (stack:%d%c) maximized:%s fs:%s",
        ec, ec->icccm.title, ec->netwm.name,
        ec->desk->x, ec->desk->y,
        ec->changes.size, ec->changes.pos, ec->changes.border,
        ec->w, ec->h, ec->x, ec->y, ec->bordername,
        stack, _G.tinfo->conf->use_rows? 'r':'c',
        (ec->maximized & E_MAXIMIZE_DIRECTION) == E_MAXIMIZE_NONE ? "NONE" :
        (ec->maximized & E_MAXIMIZE_DIRECTION) == E_MAXIMIZE_VERTICAL ? "VERTICAL" :
        (ec->maximized & E_MAXIMIZE_DIRECTION) == E_MAXIMIZE_HORIZONTAL ? "HORIZONTAL" :
        "BOTH", ec->fullscreen? "true": "false");

    extra = eina_hash_find(_G.client_extras, &ec);
    if (!extra) {
        ERR("No extra for %p", ec);
        return;
    }

    DBG("expected: %dx%d+%d+%d",
        extra->expected.w,
        extra->expected.h,
        extra->expected.x,
        extra->expected.y);
    DBG("delta:%dx%d,%d,%d. step:%dx%d. base:%dx%d",
        ec->w - extra->expected.w, ec->h - extra->expected.h,
        ec->x - extra->expected.x, ec->y - extra->expected.y,
        ec->icccm.step_w, ec->icccm.step_h,
        ec->icccm.base_w, ec->icccm.base_h);

    if (stack == 0 && !_G.tinfo->stacks[1] && !_G.tinfo->stacks[0]->next) {
        if (ec->maximized) {
            extra->expected.x = ec->x;
            extra->expected.y = ec->y;
            extra->expected.w = ec->w;
            extra->expected.h = ec->h;
        } else {
            /* TODO: what if a window doesn't want to be maximized? */
            _e_client_unmaximize(ec, E_MAXIMIZE_BOTH);
            _e_client_maximize(ec, E_MAXIMIZE_EXPAND | E_MAXIMIZE_BOTH);
        }
    }
    if (ec->x == extra->expected.x && ec->y == extra->expected.y
    &&  ec->w == extra->expected.w && ec->h == extra->expected.h)
    {
        return;
    }
    if (ec->maximized) {

        if (_G.tinfo->conf->use_rows) {
            if (ec->maximized & E_MAXIMIZE_VERTICAL) {
                 _e_client_unmaximize(ec, E_MAXIMIZE_VERTICAL);
                 _e_client_move_resize(ec,
                                       extra->expected.x,
                                       extra->expected.y,
                                       extra->expected.w,
                                       extra->expected.h);
            }
            if (ec->maximized & E_MAXIMIZE_HORIZONTAL
            && eina_list_count(_G.tinfo->stacks[stack]) > 1) {
                 _e_client_unmaximize(ec, E_MAXIMIZE_HORIZONTAL);
                 _e_client_move_resize(ec,
                                       extra->expected.x,
                                       extra->expected.y,
                                       extra->expected.w,
                                       extra->expected.h);
            }
        } else {
            if (ec->maximized & E_MAXIMIZE_HORIZONTAL) {
                 _e_client_unmaximize(ec, E_MAXIMIZE_HORIZONTAL);
                 _e_client_move_resize(ec,
                                       extra->expected.x,
                                       extra->expected.y,
                                       extra->expected.w,
                                       extra->expected.h);
            }
            if (ec->maximized & E_MAXIMIZE_VERTICAL
            && eina_list_count(_G.tinfo->stacks[stack]) > 1) {
                 _e_client_unmaximize(ec, E_MAXIMIZE_VERTICAL);
                 _e_client_move_resize(ec,
                                       extra->expected.x,
                                       extra->expected.y,
                                       extra->expected.w,
                                       extra->expected.h);
            }
        }
    }

    if (abs(extra->expected.w - ec->w) >= MAX(ec->icccm.step_w, 1)) {
        if (_G.tinfo->conf->use_rows)
            _move_resize_client_in_stack(ec, extra, stack, TILING_RESIZE);
        else
            _move_resize_client_stack(ec, extra, stack, TILING_RESIZE);
    }
    if (abs(extra->expected.h - ec->h) >= MAX(ec->icccm.step_h, 1)) {
        if (_G.tinfo->conf->use_rows)
            _move_resize_client_stack(ec, extra, stack, TILING_RESIZE);
        else
            _move_resize_client_in_stack(ec, extra, stack, TILING_RESIZE);
    }
    if (extra->expected.x != ec->x) {
        if (_G.tinfo->conf->use_rows)
            _move_resize_client_in_stack(ec, extra, stack, TILING_MOVE);
        else
            _move_resize_client_stack(ec, extra, stack, TILING_MOVE);
    }
    if (extra->expected.y != ec->y) {
        if (_G.tinfo->conf->use_rows)
            _move_resize_client_stack(ec, extra, stack, TILING_MOVE);
        else
            _move_resize_client_in_stack(ec, extra, stack, TILING_MOVE);
    }

    if (_G.input_mode == INPUT_MODE_MOVING
    &&  ec == _G.focused_ec) {
        _check_moving_anims(ec, extra, stack);
    }
}

static Eina_Bool
_resize_hook(void *data __UNUSED__, int type __UNUSED__, E_Event_Client *event)
{
    E_Client *ec = event->ec;

    _move_or_resize(ec);

    return true;
}

static Eina_Bool
_move_hook(void *data __UNUSED__, int type __UNUSED__, E_Event_Client*event)
{
    E_Client *ec = event->ec;

    _move_or_resize(ec);

    return true;
}

static Eina_Bool
_add_hook(void *data __UNUSED__, int type __UNUSED__, E_Event_Client *event)
{
    E_Client *ec = event->ec;
    int stack = -1;

    if (e_client_util_ignored_get(ec)) return ECORE_CALLBACK_RENEW;
    if (_G.input_mode != INPUT_MODE_NONE
    &&  _G.input_mode != INPUT_MODE_MOVING
    &&  _G.input_mode != INPUT_MODE_TRANSITION)
    {
        end_special_input();
    }

    check_tinfo(ec->desk);
    if (!_G.tinfo->conf || !_G.tinfo->conf->nb_stacks) {
        return true;
    }

    if (!is_tilable(ec)) {
        return true;
    }

    stack = get_stack(ec);
    if (stack >= 0) {
        return true;
    }

    DBG("Add: %p / '%s' / '%s', (%d,%d), changes(size=%d, position=%d, client=%d)"
        " g:%dx%d+%d+%d ecname:'%s' (stack:%d%c) maximized:%s fs:%s",
        ec, ec->icccm.title, ec->netwm.name,
        ec->desk->x, ec->desk->y,
        ec->changes.size, ec->changes.pos, ec->changes.border,
        ec->w, ec->h, ec->x, ec->y, ec->bordername,
        stack, _G.tinfo->conf->use_rows? 'r':'c',
        (ec->maximized & E_MAXIMIZE_DIRECTION) == E_MAXIMIZE_NONE ? "NONE" :
        (ec->maximized & E_MAXIMIZE_DIRECTION) == E_MAXIMIZE_VERTICAL ? "VERTICAL" :
        (ec->maximized & E_MAXIMIZE_DIRECTION) == E_MAXIMIZE_HORIZONTAL ? "HORIZONTAL" :
        "BOTH", ec->fullscreen? "true": "false");

    _add_client(ec);

    return true;
}

static Eina_Bool
_remove_hook(void *data __UNUSED__, int type __UNUSED__, E_Event_Client *event)
{
    E_Client *ec = event->ec;

    if (e_client_util_ignored_get(ec)) return ECORE_CALLBACK_RENEW;
    end_special_input();

    if (_G.currently_switching_desktop)
        return true;

    check_tinfo(ec->desk);
    if (!_G.tinfo->conf)
        return true;

    if (EINA_LIST_IS_IN(_G.tinfo->floating_windows, ec)) {
        EINA_LIST_REMOVE(_G.tinfo->floating_windows, ec);
        return true;
    }

    _remove_client(ec);

    return true;
}

static bool
_iconify_hook(void *data __UNUSED__, int type __UNUSED__, E_Event_Client *event)
{
    E_Client *ec = event->ec;

    DBG("iconify hook: %p", ec);

    end_special_input();

    if (ec->deskshow)
        return true;

    check_tinfo(ec->desk);
    if (!_G.tinfo->conf)
        return true;

    if (EINA_LIST_IS_IN(_G.tinfo->floating_windows, ec)) {
        return true;
    }

    _remove_client(ec);

    return true;
}

static bool
_uniconify_hook(void *data __UNUSED__, int type __UNUSED__, E_Event_Client *event)
{
    E_Client *ec = event->ec;
    int stack = -1;

    if (_G.input_mode != INPUT_MODE_NONE
    &&  _G.input_mode != INPUT_MODE_MOVING
    &&  _G.input_mode != INPUT_MODE_TRANSITION)
    {
        end_special_input();
    }

    if (ec->deskshow)
        return true;

    check_tinfo(ec->desk);
    if (!_G.tinfo->conf || !_G.tinfo->conf->nb_stacks) {
        return true;
    }

    if (!is_tilable(ec)) {
        return true;
    }

    stack = get_stack(ec);
    if (stack >= 0) {
        return true;
    }
    _add_client(ec);

    return true;
}

static Eina_Bool
_stick_hook(void *data __UNUSED__, int type __UNUSED__, void *event __UNUSED__)
{
    DBG("TODO");
    return true;
}

static Eina_Bool
_unstick_hook(void *data __UNUSED__, int type __UNUSED__, void *event __UNUSED__)
{
    DBG("TODO");
    return true;
}

static Eina_Bool
_desk_show_hook(void *data __UNUSED__, int type __UNUSED__, void *event __UNUSED__)
{
    _G.currently_switching_desktop = 0;

    end_special_input();

    return true;
}

static Eina_Bool
_desk_before_show_hook(void *data __UNUSED__, int type __UNUSED__, void *event __UNUSED__)
{
    end_special_input();

    _G.currently_switching_desktop = 1;

    return true;
}

static bool
_desk_set_hook(void *data __UNUSED__, int type __UNUSED__, E_Event_Client_Desk_Set *ev)
{
    DBG("%p: from (%d,%d) to (%d,%d)", ev->ec,
        ev->desk->x, ev->desk->y,
        ev->ec->desk->x, ev->ec->desk->y);

    end_special_input();

    check_tinfo(ev->desk);
    if (!_G.tinfo->conf) {
        return true;
    }

    if (is_floating_window(ev->ec)) {
        EINA_LIST_REMOVE(_G.tinfo->floating_windows, ev->ec);
    } else {
        if (get_stack(ev->ec) >= 0) {
            _remove_client(ev->ec);
            _restore_client(ev->ec);
        }
    }

    check_tinfo(ev->ec->desk);
    if (!_G.tinfo->conf) {
        return true;
    }

    if (get_stack(ev->ec) < 0)
        _add_client(ev->ec);

    return true;
}

static bool
_compositor_resize_hook(void *data __UNUSED__, int type __UNUSED__, E_Event_Compositor_Resize *ev)
{
    Eina_List *l;
    E_Zone *zone;
    int x, y, i;

    EINA_LIST_FOREACH(ev->comp->zones, l, zone) {
        for (x = 0; x < zone->desk_x_count; x++) {
            for (y = 0; y < zone->desk_y_count; y++) {
                E_Desk *desk = zone->desks[x + (y * zone->desk_x_count)];
                Eina_List *wins = NULL;
                E_Client *ec;

                check_tinfo(desk);
                if (!_G.tinfo->conf || !_G.tinfo->conf->nb_stacks) {
                    continue;
                }

                for (i = 0; i < TILING_MAX_STACKS; i++) {
                    EINA_LIST_FREE(_G.tinfo->stacks[i], ec) {
                        EINA_LIST_APPEND(wins, ec);
                        _restore_client(ec);
                    }
                    _G.tinfo->stacks[i] = NULL;
                    _G.tinfo->pos[i] = 0;
                    _G.tinfo->size[i] = 0;
                }

                EINA_LIST_FREE(wins, ec) {
                    _add_client(ec);
                }
            }
        }
    }

    return true;
}

/* }}} */
/* Module setup {{{*/

static void
_clear_info_hash(void *data)
{
    Tiling_Info *ti = data;
    int i;

    eina_list_free(ti->floating_windows);
    for (i = 0; i < TILING_MAX_STACKS; i++) {
        eina_list_free(ti->stacks[i]);
        ti->stacks[i] = NULL;
    }
    E_FREE(ti);
}

static void
_clear_border_extras(void *data)
{
    Client_Extra *extra = data;

    eina_stringshare_del(extra->orig.bordername);

    E_FREE(extra);
}

EAPI E_Module_Api e_modapi =
{
    E_MODULE_API_VERSION,
    "Tiling"
};

EAPI void *
e_modapi_init(E_Module *m)
{
    E_Desk *desk;
    Eina_List *l;

    tiling_g.module = m;

    if (tiling_g.log_domain < 0) {
        tiling_g.log_domain = eina_log_domain_register("tiling", NULL);
        if (tiling_g.log_domain < 0) {
            EINA_LOG_CRIT("could not register log domain 'tiling'");
        }
    }

    _G.info_hash = eina_hash_pointer_new(_clear_info_hash);
    _G.client_extras = eina_hash_pointer_new(_clear_border_extras);

    _G.pre_client_assign_hook = e_client_hook_add(
        E_CLIENT_HOOK_EVAL_PRE_FRAME_ASSIGN, _pre_client_assign_hook, NULL);

#define HANDLER(_h, _e, _f)                                   \
    _h = ecore_event_handler_add(E_EVENT_##_e,                \
                                 (Ecore_Event_Handler_Cb) _f, \
                                 NULL);

    HANDLER(_G.handler_client_resize, CLIENT_RESIZE, _resize_hook);
    HANDLER(_G.handler_client_move, CLIENT_MOVE, _move_hook);
    HANDLER(_G.handler_client_add, CLIENT_ADD, _add_hook);
    HANDLER(_G.handler_client_remove, CLIENT_REMOVE, _remove_hook);

    HANDLER(_G.handler_client_iconify, CLIENT_ICONIFY, _iconify_hook);
    HANDLER(_G.handler_client_uniconify, CLIENT_UNICONIFY, _uniconify_hook);
    HANDLER(_G.handler_client_stick, CLIENT_STICK, _stick_hook);
    HANDLER(_G.handler_client_unstick, CLIENT_UNSTICK, _unstick_hook);

    HANDLER(_G.handler_desk_show, DESK_SHOW, _desk_show_hook);
    HANDLER(_G.handler_desk_before_show, DESK_BEFORE_SHOW, _desk_before_show_hook);
    HANDLER(_G.handler_desk_set, CLIENT_DESK_SET, _desk_set_hook);
    HANDLER(_G.handler_compositor_resize, COMPOSITOR_RESIZE, _compositor_resize_hook);
#undef HANDLER

#define ACTION_ADD(_act, _cb, _title, _value, _params, _example, _editable)  \
    {                                                                        \
        E_Action *_action = _act;                                            \
        const char *_name = _value;                                          \
        if ((_action = e_action_add(_name))) {                               \
            _action->func.go = _cb;                                          \
            e_action_predef_name_set(N_("Tiling"), _title, _name,            \
                                     _params, _example, _editable);          \
        }                                                                    \
    }

    /* Module's actions */
    ACTION_ADD(_G.act_togglefloat, _e_mod_action_toggle_floating_cb,
               N_("Toggle floating"), "toggle_floating",
               NULL, NULL, 0);
    ACTION_ADD(_G.act_addstack, _e_mod_action_add_stack_cb,
               N_("Add a stack"), "add_stack",
               NULL, NULL, 0);
    ACTION_ADD(_G.act_removestack, _e_mod_action_remove_stack_cb,
               N_("Remove a stack"), "remove_stack",
               NULL, NULL, 0);
    ACTION_ADD(_G.act_tg_stack, _e_mod_action_tg_stack_cb,
               N_("Toggle between rows and columns"), "tg_cols_rows",
               NULL, NULL, 0);
    ACTION_ADD(_G.act_swap, _e_mod_action_swap_cb,
               N_("Swap a window with an other"), "swap",
               NULL, NULL, 0);

    ACTION_ADD(_G.act_move, _e_mod_action_move_cb,
               N_("Move window"), "move",
               NULL, NULL, 0);
    ACTION_ADD(_G.act_move_left, _e_mod_action_move_direct_cb,
               N_("Move window to the left"), "move_left",
               "left", NULL, 0);
    ACTION_ADD(_G.act_move_right, _e_mod_action_move_direct_cb,
               N_("Move window to the right"), "move_right",
               "right", NULL, 0);
    ACTION_ADD(_G.act_move_up, _e_mod_action_move_direct_cb,
               N_("Move window up"), "move_up",
               "up", NULL, 0);
    ACTION_ADD(_G.act_move_down, _e_mod_action_move_direct_cb,
               N_("Move window down"), "move_down",
               "down", NULL, 0);

    ACTION_ADD(_G.act_adjusttransitions, _e_mod_action_adjust_transitions,
               N_("Adjust transitions"), "adjust_transitions",
               NULL, NULL, 0);
    ACTION_ADD(_G.act_go, _e_mod_action_go_cb,
               N_("Focus a particular window"), "go",
               NULL, NULL, 0);

    ACTION_ADD(_G.act_send_ne, _e_mod_action_send_cb,
               N_("Send to upper right corner"), "send_ne",
               "ne", NULL, 0);
    ACTION_ADD(_G.act_send_nw, _e_mod_action_send_cb,
               N_("Send to upper left corner"), "send_nw",
               "nw", NULL, 0);
    ACTION_ADD(_G.act_send_se, _e_mod_action_send_cb,
               N_("Send to lower right corner"), "send_se",
               "se", NULL, 0);
    ACTION_ADD(_G.act_send_sw, _e_mod_action_send_cb,
               N_("Send to lower left corner"), "send_sw",
               "sw", NULL, 0);
#undef ACTION_ADD

    /* Configuration entries */
    snprintf(_G.edj_path, sizeof(_G.edj_path),"%s/e-module-tiling.edj",
             e_module_dir_get(m));
    e_configure_registry_category_add("windows", 50, _("Windows"), NULL,
                                      "preferences-system-windows");
    e_configure_registry_item_add("windows/tiling", 150, _("Tiling"),
                                  NULL, _G.edj_path,
                                  e_int_config_tiling_module);

    /* Configuration itself */
    _G.config_edd = E_CONFIG_DD_NEW("Tiling_Config", Config);
    _G.vdesk_edd = E_CONFIG_DD_NEW("Tiling_Config_VDesk",
                                   struct _Config_vdesk);
    E_CONFIG_VAL(_G.config_edd, Config, tile_dialogs, INT);
    E_CONFIG_VAL(_G.config_edd, Config, show_titles, INT);
    E_CONFIG_VAL(_G.config_edd, Config, keyhints, STR);

    E_CONFIG_LIST(_G.config_edd, Config, vdesks, _G.vdesk_edd);
    E_CONFIG_VAL(_G.vdesk_edd, struct _Config_vdesk, x, INT);
    E_CONFIG_VAL(_G.vdesk_edd, struct _Config_vdesk, y, INT);
    E_CONFIG_VAL(_G.vdesk_edd, struct _Config_vdesk, zone_num, INT);
    E_CONFIG_VAL(_G.vdesk_edd, struct _Config_vdesk, nb_stacks, INT);
    E_CONFIG_VAL(_G.vdesk_edd, struct _Config_vdesk, use_rows, INT);

    tiling_g.config = e_config_domain_load("module.tiling", _G.config_edd);
    if (!tiling_g.config) {
        tiling_g.config = E_NEW(Config, 1);
        tiling_g.config->tile_dialogs = 1;
        tiling_g.config->show_titles = 1;
    }
    if (!tiling_g.config->keyhints)
        tiling_g.config->keyhints = strdup(tiling_g.default_keyhints);
    else
        tiling_g.config->keyhints = strdup(tiling_g.config->keyhints);

    E_CONFIG_LIMIT(tiling_g.config->tile_dialogs, 0, 1);
    E_CONFIG_LIMIT(tiling_g.config->show_titles, 0, 1);

    for (l = tiling_g.config->vdesks; l; l = l->next) {
        struct _Config_vdesk *vd;

        vd = l->data;

        E_CONFIG_LIMIT(vd->nb_stacks, 0, TILING_MAX_STACKS);
        E_CONFIG_LIMIT(vd->use_rows, 0, 1);
    }

    desk = get_current_desk();
    _G.tinfo = _initialize_tinfo(desk);

    _G.input_mode = INPUT_MODE_NONE;
    _G.currently_switching_desktop = 0;
    _G.action_cb = NULL;

    return m;
}

static void
_disable_desk(E_Desk *desk)
{
    Eina_List *l;
    int i;

    check_tinfo(desk);
    if (!_G.tinfo->conf || !_G.tinfo->conf->nb_stacks) {
        return;
    }

    for (i = 0; i < TILING_MAX_STACKS; i++) {
        for (l = _G.tinfo->stacks[i]; l; l = l->next) {
            E_Client *ec = l->data;

            _restore_client(ec);
        }
        eina_list_free(_G.tinfo->stacks[i]);
        _G.tinfo->stacks[i] = NULL;
    }
}

static void
_disable_all_tiling(void)
{
    const Eina_List *l, *ll;
    E_Comp *comp;
    E_Zone *zone;
    E_Desk *desk;
    int x, y;

    EINA_LIST_FOREACH(e_comp_list(), l, comp) {
        EINA_LIST_FOREACH(comp->zones, ll, zone) {
            for (x = 0; x < zone->desk_x_count; x++) {
                for (y = 0; y < zone->desk_y_count; y++) {
                    desk = zone->desks[x + (y * zone->desk_x_count)];

                    _disable_desk(desk);
                }
            }
            e_place_zone_region_smart_cleanup(zone);
        }
    }
}

EAPI int
e_modapi_shutdown(E_Module *m __UNUSED__)
{
    _disable_all_tiling();

    if (tiling_g.log_domain >= 0) {
        eina_log_domain_unregister(tiling_g.log_domain);
        tiling_g.log_domain = -1;
    }

    if (_G.pre_client_assign_hook) {
        e_client_hook_del(_G.pre_client_assign_hook);
        _G.pre_client_assign_hook = NULL;
    }

#define FREE_HANDLER(x)              \
    if (x) {                         \
        ecore_event_handler_del(x);  \
        x = NULL;                    \
    }
    FREE_HANDLER(_G.handler_client_resize);
    FREE_HANDLER(_G.handler_client_move);
    FREE_HANDLER(_G.handler_client_add);
    FREE_HANDLER(_G.handler_client_remove);

    FREE_HANDLER(_G.handler_client_iconify);
    FREE_HANDLER(_G.handler_client_uniconify);
    FREE_HANDLER(_G.handler_client_stick);
    FREE_HANDLER(_G.handler_client_unstick);

    FREE_HANDLER(_G.handler_desk_show);
    FREE_HANDLER(_G.handler_desk_before_show);
    FREE_HANDLER(_G.handler_desk_set);
#undef FREE_HANDLER


#define ACTION_DEL(act, title, value)                        \
    if (act) {                                               \
        e_action_predef_name_del("Tiling", title);           \
        e_action_del(value);                                 \
        act = NULL;                                          \
    }
    ACTION_DEL(_G.act_togglefloat, "Toggle floating", "toggle_floating");
    ACTION_DEL(_G.act_addstack, "Add a stack", "add_stack");
    ACTION_DEL(_G.act_removestack, "Remove a stack", "remove_stack");
    ACTION_DEL(_G.act_tg_stack, "Toggle between rows and columns", "tg_cols_rows");
    ACTION_DEL(_G.act_swap, "Swap a window with an other", "swap");

    ACTION_DEL(_G.act_move, "Move window", "move");
    ACTION_DEL(_G.act_move_left, "Move window to the left", "move_left");
    ACTION_DEL(_G.act_move_right, "Move window to the right", "move_right");
    ACTION_DEL(_G.act_move_up, "Move window up", "move_up");
    ACTION_DEL(_G.act_move_down, "Move window down", "move_down");

    ACTION_DEL(_G.act_adjusttransitions, "Adjust transitions",
               "adjust_transitions");
    ACTION_DEL(_G.act_go, "Focus a particular window", "go");

    ACTION_DEL(_G.act_send_ne, "Send to upper right corner", "send_ne");
    ACTION_DEL(_G.act_send_nw, "Send to upper left corner", "send_nw");
    ACTION_DEL(_G.act_send_se, "Send to lower right corner", "send_se");
    ACTION_DEL(_G.act_send_sw, "Send to lower left corner", "send_sw");
#undef ACTION_DEL

    e_configure_registry_item_del("windows/tiling");
    e_configure_registry_category_del("windows");

    end_special_input();

    free(tiling_g.config->keyhints);
    E_FREE(tiling_g.config);
    E_CONFIG_DD_FREE(_G.config_edd);
    E_CONFIG_DD_FREE(_G.vdesk_edd);

    tiling_g.module = NULL;

    eina_hash_free(_G.info_hash);
    _G.info_hash = NULL;

    eina_hash_free(_G.client_extras);
    _G.client_extras = NULL;

    _G.tinfo = NULL;

    return 1;
}

EAPI int
e_modapi_save(E_Module *m __UNUSED__)
{
    e_config_domain_save("module.tiling", _G.config_edd, tiling_g.config);

    return true;
}
/* }}} */

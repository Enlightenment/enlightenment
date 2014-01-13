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
                         *act_swap,
                         *act_toggle_split_mode;

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
    Tiling_Split_Type     split_type;

    tiling_input_mode_t   input_mode;
    char                  keys[4];
} _G = {
    .input_mode = INPUT_MODE_NONE,
    .split_type = TILING_SPLIT_HORIZONTAL,
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

static Eina_Bool
desk_should_tile_check(const E_Desk *desk)
{
   check_tinfo(desk);
   return (_G.tinfo->conf &&_G.tinfo->conf->nb_stacks);
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
//    e_client_maximize(ec, max);
}

static void
_e_client_unmaximize(E_Client *ec, E_Maximize max)
{
    DBG("%p -> %s", ec,
        (max & E_MAXIMIZE_DIRECTION) == E_MAXIMIZE_NONE ? "NONE" :
        (max & E_MAXIMIZE_DIRECTION) == E_MAXIMIZE_VERTICAL ? "VERTICAL" :
        (max & E_MAXIMIZE_DIRECTION) == E_MAXIMIZE_HORIZONTAL ? "HORIZONTAL" :
        "BOTH");
//    e_client_unmaximize(ec, max);
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
_reapply_tree(void)
{
     int zx, zy, zw, zh;

     if (_G.tinfo->tree)
       {
          e_zone_useful_geometry_get(_G.tinfo->desk->zone, &zx, &zy, &zw, &zh);

          tiling_window_tree_apply(_G.tinfo->tree, zx, zy, zw, zh);
       }
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
    }
    _G.tinfo->conf->nb_stacks = new_nb_stacks;
}

/* }}} */
/* Reorganize windows {{{*/

static void
_add_client(E_Client *ec)
{
    /* Should I need to check that the client is not already added? */
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

    _get_or_create_client_extra(ec);

    /* Stack tiled window below so that winlist doesn't mix up stacking */
    evas_object_layer_set(ec->frame, E_LAYER_CLIENT_BELOW);

    DBG("adding %p", ec);

    if (ec->maximized)
       _e_client_unmaximize(ec, E_MAXIMIZE_BOTH);

    /* Window tree updating. */
      {
         /* If focused is NULL, it should return the root. */
         Window_Tree *parent = tiling_window_tree_client_find(_G.tinfo->tree,
               e_client_focused_get());
         Window_Tree *new_node;
         if (!parent)
           {
              if (_G.tinfo->tree)
                {
                   ERR("Couldn't find tree item for focused client %p. Using root..",
                         e_client_focused_get());
                }
              parent = _G.tinfo->tree;
           }

         new_node = tiling_window_tree_add(parent, ec, _G.split_type);
         if (!_G.tinfo->tree)
            _G.tinfo->tree = new_node;
      }

    _reapply_tree();
}

static void
_remove_client(E_Client *ec)
{
    DBG("removing %p", ec);

    eina_hash_del(_G.client_extras, ec, NULL);

    /* Window tree updating. */
      {
         /* If focused is NULL, it should return the root. */
         Window_Tree *item = tiling_window_tree_client_find(_G.tinfo->tree, ec);
         if (!item)
           {
              ERR("Couldn't find tree item for focused client %p!", ec);
              return;
           }

         _G.tinfo->tree = tiling_window_tree_remove(_G.tinfo->tree, item);
      }

    _reapply_tree();
}

/* }}} */
/* Toggle Floating {{{ */

static void
toggle_floating(E_Client *ec)
{
    if (!ec)
        return;
    if (!desk_should_tile_check(ec->desk))
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

    if (!desk_should_tile_check(desk))
        return;

    _do_overlay(focused_ec, _action_swap, INPUT_MODE_SWAPPING);
}

/* }}} */
/* Toggle split mode {{{ */

static void
_e_mod_action_toggle_split_mode(E_Object   *obj __UNUSED__,
                                 const char *params __UNUSED__)
{
    E_Desk *desk;

    desk = get_current_desk();
    if (!desk)
        return;

    if (!desk_should_tile_check(desk))
        return;

    _G.split_type = (_G.split_type == TILING_SPLIT_VERTICAL) ?
       TILING_SPLIT_HORIZONTAL : TILING_SPLIT_VERTICAL;
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

    if (!desk_should_tile_check(ec->desk))
        return;

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

    if (!desk_should_tile_check(ec->desk))
        return;

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

    _reapply_tree();
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

    if (e_client_util_ignored_get(ec)) return ECORE_CALLBACK_RENEW;
    if (_G.input_mode != INPUT_MODE_NONE
    &&  _G.input_mode != INPUT_MODE_MOVING
    &&  _G.input_mode != INPUT_MODE_TRANSITION)
    {
        end_special_input();
    }

    DBG("Add: %p / '%s' / '%s', (%d,%d), changes(size=%d, position=%d, client=%d)"
        " g:%dx%d+%d+%d ecname:'%s' maximized:%s fs:%s",
        ec, ec->icccm.title, ec->netwm.name,
        ec->desk->x, ec->desk->y,
        ec->changes.size, ec->changes.pos, ec->changes.border,
        ec->w, ec->h, ec->x, ec->y, ec->bordername,
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

    if (!desk_should_tile_check(ec->desk))
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

    if (!desk_should_tile_check(ec->desk))
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

    if (_G.input_mode != INPUT_MODE_NONE
    &&  _G.input_mode != INPUT_MODE_MOVING
    &&  _G.input_mode != INPUT_MODE_TRANSITION)
    {
        end_special_input();
    }

    if (ec->deskshow)
        return true;

    if (!desk_should_tile_check(ec->desk))
        return true;

    if (!is_tilable(ec)) {
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

    if (!desk_should_tile_check(ev->desk))
        return true;

    if (is_floating_window(ev->ec)) {
        EINA_LIST_REMOVE(_G.tinfo->floating_windows, ev->ec);
    } else {
         if (tiling_window_tree_client_find(_G.tinfo->tree, ev->ec)) {
            _remove_client(ev->ec);
            _restore_client(ev->ec);
        }
    }

    if (!desk_should_tile_check(ev->ec->desk))
        return true;

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

                if (!desk_should_tile_check(desk))
                   continue;

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
    ACTION_ADD(_G.act_swap, _e_mod_action_swap_cb,
               N_("Swap a window with an other"), "swap",
               NULL, NULL, 0);

    ACTION_ADD(_G.act_toggle_split_mode, _e_mod_action_toggle_split_mode,
               N_("Toggle split mode"), "toggle_split_mode",
               NULL, NULL, 0);

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

   if (!desk_should_tile_check(desk))
      return;

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
    ACTION_DEL(_G.act_swap, "Swap a window with an other", "swap");

    ACTION_DEL(_G.act_toggle_split_mode, "Toggle split mode",
          "toggle_split_mode");
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

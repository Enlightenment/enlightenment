#define EXECUTIVE_MODE_ENABLED
#define E_COMP_X
#include "e.h"

#define RANDR_VERSION_1_3 ((1 << 16) | 3)
#define RANDR_VERSION_1_4 ((1 << 16) | 4)

#define GRAV_SET(ec, grav)                                                         \
  ecore_x_window_gravity_set(e_client_util_pwin_get(ec), grav);                          \
  if (_e_comp_x_client_data_get(ec)->lock_win) ecore_x_window_gravity_set(_e_comp_x_client_data_get(ec)->lock_win, grav);

#ifdef HAVE_WAYLAND
# define E_COMP_X_PIXMAP_CHECK if ((e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_X) && (!e_client_has_xwindow(ec))) return
#else
# define E_COMP_X_PIXMAP_CHECK if (!e_pixmap_is_x(ec->pixmap)) return
#endif
/* maybe make configurable?
 * not sure why anyone would want to change it...
 */
#define MOVE_COUNTER_LIMIT 50

EINTERN void _e_main_cb_x_fatal(void *data EINA_UNUSED);

typedef struct _Frame_Extents Frame_Extents;

struct _Frame_Extents
{
   int l, r, t, b;
};

struct _E_Comp_X_Data
{
   Ecore_X_Window lock_win;
   Ecore_X_Window lock_grab_break_wnd;
   Ecore_Event_Handler *lock_key_handler;

   Eina_List *retry_clients;
   Ecore_Timer *retry_timer;
   Eina_Bool restack : 1;
};

static unsigned int focus_time = 0;
static Ecore_Timer *focus_timer;
static E_Client *mouse_client;
static Eina_List *handlers = NULL;
static Eina_Hash *clients_win_hash = NULL;
static Eina_Hash *damages_hash = NULL;
static Eina_Hash *frame_extents = NULL;
static Eina_Hash *alarm_hash = NULL;

static Ecore_Idle_Enterer *_e_comp_x_post_client_idler = NULL;
static Ecore_Idle_Enterer *_x_idle_flush = NULL;
static Eina_List *post_clients = NULL;

static int _e_comp_x_mapping_change_disabled = 0;

static Ecore_X_Randr_Screen_Size screen_size = { -1, -1 };
static int screen_size_index = -1;

static Ecore_Timer *screensaver_idle_timer = NULL;
static Eina_Bool screensaver_dimmed = EINA_FALSE;

static Ecore_X_Atom backlight_atom = 0;
extern double e_bl_val;

static void _e_comp_x_hook_client_pre_frame_assign(void *d EINA_UNUSED, E_Client *ec);

static inline E_Comp_X_Client_Data *
_e_comp_x_client_data_get(const E_Client *ec)
{
#ifdef HAVE_WAYLAND
   if (!e_pixmap_is_x(ec->pixmap))
     return e_comp_wl_client_xwayland_data(ec);
#endif
   return ec->comp_data;
}

static Eina_Bool
_e_comp_x_flusher(void *data EINA_UNUSED)
{
   ecore_x_flush();
   return ECORE_CALLBACK_RENEW;
}

static inline Ecore_X_Window
_e_comp_x_client_window_get(const E_Client *ec)
{
   E_Comp_X_Client_Data *cd = _e_comp_x_client_data_get(ec);
   if (cd && cd->reparented)
     return e_client_util_pwin_get(ec);
   return e_client_util_win_get(ec);
}

static void
_e_comp_x_client_damage_add(E_Client *ec)
{
   Ecore_X_Window win;

   if (_e_comp_x_client_data_get(ec)->damage) return;
   win = _e_comp_x_client_window_get(ec);
   _e_comp_x_client_data_get(ec)->damage = ecore_x_damage_new(win, ECORE_X_DAMAGE_REPORT_DELTA_RECTANGLES);
   eina_hash_add(damages_hash, &_e_comp_x_client_data_get(ec)->damage, ec);
}

static void
_e_comp_x_focus_check(void)
{
   E_Client *focused;

   if (stopping || e_comp->nocomp) return;
   focused = e_client_focused_get();
   /* if there is no new focused or it is a non-X client,
    * focus comp canvas on focus-out */
   if ((!focused) || (e_pixmap_type_get(focused->pixmap) != E_PIXMAP_TYPE_X))
     e_grabinput_focus(e_comp->ee_win, E_FOCUS_METHOD_PASSIVE);
}

static void
_e_comp_x_client_frame_update(E_Client *ec, int l, int r, int t, int b)
{
   ecore_x_netwm_frame_size_set(e_client_util_win_get(ec), l, r, t, b);
   ecore_x_e_frame_size_set(e_client_util_win_get(ec), l, r, t, b);
   _e_comp_x_client_data_get(ec)->frame_update = 0;
}

static void
_e_comp_x_client_event_free(void *d EINA_UNUSED, void *e)
{
   E_Event_Client *ev = e;

   UNREFD(ev->ec, 1);
   e_object_unref(E_OBJECT(ev->ec));
   free(ev);
}

static void
_e_comp_x_print_win(Ecore_X_Window win)
{
   int x, y, w, h;
   Eina_Bool vis;

   ecore_x_window_geometry_get(win, &x, &y, &w, &h);
   vis = ecore_x_window_visible_get(win);
   fprintf(stderr, "%s 0x%x: %d,%d @ %dx%d\n", vis ? "VIS" : "HID", win, x, y, w, h);
}

static void
_e_comp_x_focus_grab(E_Client *ec)
{
   if (ec->internal_elm_win) return;
   ecore_x_window_button_grab(e_client_util_win_get(ec), 1,
                              ECORE_X_EVENT_MASK_MOUSE_DOWN |
                              ECORE_X_EVENT_MASK_MOUSE_UP |
                              ECORE_X_EVENT_MASK_MOUSE_MOVE, 0, 1);
   ecore_x_window_button_grab(e_client_util_win_get(ec), 2,
                              ECORE_X_EVENT_MASK_MOUSE_DOWN |
                              ECORE_X_EVENT_MASK_MOUSE_UP |
                              ECORE_X_EVENT_MASK_MOUSE_MOVE, 0, 1);
   ecore_x_window_button_grab(e_client_util_win_get(ec), 3,
                              ECORE_X_EVENT_MASK_MOUSE_DOWN |
                              ECORE_X_EVENT_MASK_MOUSE_UP |
                              ECORE_X_EVENT_MASK_MOUSE_MOVE, 0, 1);
   _e_comp_x_client_data_get(ec)->button_grabbed = 1;
}

static void
_e_comp_x_focus_init(E_Client *ec)
{
   if (_e_comp_x_client_data_get(ec)->button_grabbed) return;
   if (!((e_client_focus_policy_click(ec)) ||
       (e_config->always_click_to_raise) ||
       (e_config->always_click_to_focus))) return;
   _e_comp_x_focus_grab(ec);
}

static void
_e_comp_x_focus_setup(E_Client *ec)
{
   if (_e_comp_x_client_data_get(ec)->button_grabbed) return;
   if ((!e_client_focus_policy_click(ec)) ||
       (e_config->always_click_to_raise) ||
       (e_config->always_click_to_focus)) return;
   _e_comp_x_focus_grab(ec);
}


static void
_e_comp_x_focus_setdown(E_Client *ec)
{
   Ecore_X_Window win;

   if (!_e_comp_x_client_data_get(ec)->button_grabbed) return;
   if ((!e_client_focus_policy_click(ec)) ||
       (e_config->always_click_to_raise) ||
       (e_config->always_click_to_focus)) return;
   win = e_client_util_win_get(ec);
   e_bindings_mouse_ungrab(E_BINDING_CONTEXT_WINDOW, win);
   e_bindings_wheel_ungrab(E_BINDING_CONTEXT_WINDOW, win);
   ecore_x_window_button_ungrab(win, 1, 0, 1);
   ecore_x_window_button_ungrab(win, 2, 0, 1);
   ecore_x_window_button_ungrab(win, 3, 0, 1);
   e_bindings_mouse_grab(E_BINDING_CONTEXT_WINDOW, win);
   e_bindings_wheel_grab(E_BINDING_CONTEXT_WINDOW, win);
   _e_comp_x_client_data_get(ec)->button_grabbed = 0;
}

static Eina_Bool
_e_comp_x_client_new_helper(E_Client *ec)
{
   Ecore_X_Window win = e_client_util_win_get(ec);
   int at_num = 0, i;
   Ecore_X_Atom *atoms;

   if (!ecore_x_window_attributes_get(win, &ec->comp_data->initial_attributes))
     {
        //CRI("OUCH! FIX THIS!");
        DELD(ec, 1);
        e_object_del(E_OBJECT(ec));
        return EINA_FALSE;
     }
   if (ec->re_manage && (!ec->comp_data->initial_attributes.visible))
     {
        /* ain't gonna be no hidden clients on my watch! */
        DELD(ec, 1);
        e_object_del(E_OBJECT(ec));
        return EINA_FALSE;
     }
   ec->depth = ec->comp_data->initial_attributes.depth;
   ec->override = ec->comp_data->initial_attributes.override;
   ec->placed |= ec->override;
   ec->input_only = ec->comp_data->initial_attributes.input_only;
   ec->border_size = ec->comp_data->initial_attributes.border;
   ec->icccm.accepts_focus = (!ec->override) && (!ec->input_only);
   //INF("NEW CLIENT: %d,%d -> %d,%d", ec->x, ec->y, ec->comp_data->initial_attributes.x, ec->comp_data->initial_attributes.y);
   ec->x = ec->client.x = ec->comp_data->initial_attributes.x;
   ec->y = ec->client.y = ec->comp_data->initial_attributes.y;
   if (ec->override && ((ec->x == -77) && (ec->y == -777)))
     {
        /* this is the ecore-x private window :/ */
        e_comp_ignore_win_add(E_PIXMAP_TYPE_X, win);
        DELD(ec, 1);
        e_object_del(E_OBJECT(ec));
        return EINA_FALSE;
     }
   if ((!e_client_util_ignored_get(ec)) && (!ec->internal) && (!ec->internal_elm_win))
     {
        ec->comp_data->need_reparent = 1;
        EC_CHANGED(ec);
        ec->take_focus = !starting;
     }
   ec->new_client ^= ec->override;
   if (!ec->new_client)
     e_comp->new_clients--;

   ec->w = ec->client.w = ec->comp_data->initial_attributes.w;
   ec->h = ec->client.h = ec->comp_data->initial_attributes.h;
   if (!ec->internal)
     {
        ec->comp_data->pw = ec->w;
        ec->comp_data->ph = ec->h;
     }
   ec->changes.size = 1;
   

   e_pixmap_visual_cmap_set(ec->pixmap, ec->comp_data->initial_attributes.visual, ec->comp_data->initial_attributes.colormap);
   if (ec->override && (!ec->internal))
     ecore_x_window_shape_events_select(win, 1);
   if (ec->override && (!(ec->comp_data->initial_attributes.event_mask.mine & ECORE_X_EVENT_MASK_WINDOW_PROPERTY)))
     ecore_x_event_mask_set(win, ECORE_X_EVENT_MASK_WINDOW_PROPERTY);

   atoms = ecore_x_window_prop_list(win, &at_num);
   ec->icccm.fetch.command = 1;

   if (atoms)
     {
        Eina_Bool video_parent = EINA_FALSE;
        Eina_Bool video_position = EINA_FALSE;

        /* icccm */
        for (i = 0; i < at_num; i++)
          {
             if (atoms[i] == ECORE_X_ATOM_WM_NAME)
               ec->icccm.fetch.title = 1;
             else if (atoms[i] == ECORE_X_ATOM_WM_CLASS)
               ec->icccm.fetch.name_class = 1;
             else if (atoms[i] == ECORE_X_ATOM_WM_ICON_NAME)
               ec->icccm.fetch.icon_name = 1;
             else if (atoms[i] == ECORE_X_ATOM_WM_CLIENT_MACHINE)
               ec->icccm.fetch.machine = 1;
             else if (atoms[i] == ECORE_X_ATOM_WM_HINTS)
               ec->icccm.fetch.hints = 1;
             else if (atoms[i] == ECORE_X_ATOM_WM_NORMAL_HINTS)
               ec->icccm.fetch.size_pos_hints = 1;
             else if (atoms[i] == ECORE_X_ATOM_WM_PROTOCOLS)
               ec->icccm.fetch.protocol = 1;
             else if (atoms[i] == ECORE_X_ATOM_MOTIF_WM_HINTS)
               ec->mwm.fetch.hints = 1;
             else if (atoms[i] == ECORE_X_ATOM_WM_TRANSIENT_FOR)
               {
                  ec->icccm.fetch.transient_for = 1;
                  ec->netwm.fetch.type = 1;
               }
             else if (atoms[i] == ECORE_X_ATOM_WM_CLIENT_LEADER)
               ec->icccm.fetch.client_leader = 1;
             else if (atoms[i] == ECORE_X_ATOM_WM_WINDOW_ROLE)
               ec->icccm.fetch.window_role = 1;
             else if (atoms[i] == ECORE_X_ATOM_WM_STATE)
               ec->icccm.fetch.state = 1;
          }
        /* netwm, loop again, netwm will ignore some icccm, so we
         * have to be sure that netwm is checked after */
        for (i = 0; i < at_num; i++)
          {
             if (atoms[i] == ECORE_X_ATOM_NET_WM_NAME)
               {
                  /* Ignore icccm */
                  ec->icccm.fetch.title = 0;
                  ec->netwm.fetch.name = 1;
               }
             else if (atoms[i] == ECORE_X_ATOM_NET_WM_ICON_NAME)
               {
                  /* Ignore icccm */
                  ec->icccm.fetch.icon_name = 0;
                  ec->netwm.fetch.icon_name = 1;
               }
             else if (atoms[i] == ECORE_X_ATOM_NET_WM_ICON)
               {
                  ec->netwm.fetch.icon = 1;
               }
             else if (atoms[i] == ECORE_X_ATOM_NET_WM_USER_TIME)
               {
                  ec->netwm.fetch.user_time = 1;
               }
             else if (atoms[i] == ECORE_X_ATOM_NET_WM_STRUT)
               {
                  DBG("ECORE_X_ATOM_NET_WM_STRUT");
                  ec->netwm.fetch.strut = 1;
               }
             else if (atoms[i] == ECORE_X_ATOM_NET_WM_STRUT_PARTIAL)
               {
                  DBG("ECORE_X_ATOM_NET_WM_STRUT_PARTIAL");
                  ec->netwm.fetch.strut = 1;
               }
             else if (atoms[i] == ECORE_X_ATOM_NET_WM_WINDOW_TYPE)
               {
                  /* Ignore mwm
                     ec->mwm.fetch.hints = 0;
                   */
                  ec->netwm.fetch.type = 1;
               }
             else if (atoms[i] == ECORE_X_ATOM_NET_WM_STATE)
               {
                  ec->netwm.fetch.state = 1;
               }
             else if (atoms[i] == ECORE_X_ATOM_NET_WM_WINDOW_OPACITY)
               ec->netwm.fetch.opacity = 1;
          }
        /* other misc atoms */
        for (i = 0; i < at_num; i++)
          {
             /* loop to check for own atoms */
             if (atoms[i] == E_ATOM_WINDOW_STATE)
               {
                  ec->e.fetch.state = 1;
               }
             /* loop to check for qtopia atoms */
             if (atoms[i] == ATM__QTOPIA_SOFT_MENU)
               ec->qtopia.fetch.soft_menu = 1;
             else if (atoms[i] == ATM__QTOPIA_SOFT_MENUS)
               ec->qtopia.fetch.soft_menus = 1;
             /* loop to check for vkbd atoms */
             else if (atoms[i] == ECORE_X_ATOM_E_VIRTUAL_KEYBOARD_STATE)
               ec->vkbd.fetch.state = 1;
             else if (atoms[i] == ECORE_X_ATOM_E_VIRTUAL_KEYBOARD)
               ec->vkbd.fetch.vkbd = 1;
             /* loop to check for illume atoms */
             else if (atoms[i] == ECORE_X_ATOM_E_ILLUME_CONFORMANT)
               ec->comp_data->illume.conformant.fetch.conformant = 1;
             else if (atoms[i] == ECORE_X_ATOM_E_ILLUME_QUICKPANEL_STATE)
               ec->comp_data->illume.quickpanel.fetch.state = 1;
             else if (atoms[i] == ECORE_X_ATOM_E_ILLUME_QUICKPANEL)
               ec->comp_data->illume.quickpanel.fetch.quickpanel = 1;
             else if (atoms[i] == ECORE_X_ATOM_E_ILLUME_QUICKPANEL_PRIORITY_MAJOR)
               ec->comp_data->illume.quickpanel.fetch.priority.major = 1;
             else if (atoms[i] == ECORE_X_ATOM_E_ILLUME_QUICKPANEL_PRIORITY_MINOR)
               ec->comp_data->illume.quickpanel.fetch.priority.minor = 1;
             else if (atoms[i] == ECORE_X_ATOM_E_ILLUME_QUICKPANEL_ZONE)
               ec->comp_data->illume.quickpanel.fetch.zone = 1;
             else if (atoms[i] == ECORE_X_ATOM_E_ILLUME_DRAG_LOCKED)
               ec->comp_data->illume.drag.fetch.locked = 1;
             else if (atoms[i] == ECORE_X_ATOM_E_ILLUME_DRAG)
               ec->comp_data->illume.drag.fetch.drag = 1;
             else if (atoms[i] == ECORE_X_ATOM_E_ILLUME_WINDOW_STATE)
               ec->comp_data->illume.win_state.fetch.state = 1;
             else if (atoms[i] == ECORE_X_ATOM_E_VIDEO_PARENT)
               video_parent = EINA_TRUE;
             else if (atoms[i] == ECORE_X_ATOM_E_VIDEO_POSITION)
               video_position = EINA_TRUE;
             /* loop to check for window profile list atom */
             else if (atoms[i] == ECORE_X_ATOM_E_WINDOW_PROFILE_SUPPORTED)
               ec->e.fetch.profile = 1;
          }
        if (video_position && video_parent)
          {
             ec->e.state.video = 1;
             ec->e.fetch.video_parent = 1;
             ec->e.fetch.video_position = 1;
             fprintf(stderr, "We found a video window \\o/ %x\n", win);
          }
        free(atoms);
     }

   return EINA_TRUE;
}

static E_Client *
_e_comp_x_client_find_by_damage(Ecore_X_Damage damage)
{
   return eina_hash_find(damages_hash, &damage);
}

static E_Client *
_e_comp_x_client_find_by_window(Ecore_X_Window win)
{
   E_Client *ec;

   ec = eina_hash_find(clients_win_hash, &win);
   if (ec && e_object_is_del(E_OBJECT(ec))) ec = NULL;
   return ec;
}

/*
static E_Client *
_e_comp_x_client_find_all_by_window(Ecore_X_Window win)
{
   return eina_hash_find(clients_win_hash, &win);
}
*/

static void
_e_comp_x_add_fail_job(void *d EINA_UNUSED)
{
   e_util_dialog_internal
     (_("Compositor Warning"),
      _("Your display driver does not support OpenGL, GLSL<br>"
        "shaders or no OpenGL engines were compiled or installed<br>"
        "for Evas or Ecore-Evas. Falling back to software engine.<br>"
        "<br>"
        "You will need an OpenGL 2.0 (or OpenGL ES 2.0) capable<br>"
        "GPU to use OpenGL with compositing."));
}

static void
_pri_adj(int pid, int set, int adj, Eina_Bool use_adj, Eina_Bool adj_children, Eina_Bool do_children)
{
   int newpri = set;

   if (use_adj) newpri = getpriority(PRIO_PROCESS, pid) + adj;
   setpriority(PRIO_PROCESS, pid, newpri);
// shouldn't need to do this as default ionice class is "none" (0), and
// this inherits io priority FROM nice level
//        ioprio_set(IOPRIO_WHO_PROCESS, pid,
//                   IOPRIO_PRIO_VALUE(2, 5));
   if (do_children)
     {
        Eina_List *files;
        char *file, buf[PATH_MAX];
        FILE *f;
        int pid2, ppid;

        // yes - this is /proc specific... so this may not work on some
        // os's - works on linux. too bad for others.
        files = ecore_file_ls("/proc");
        EINA_LIST_FREE(files, file)
          {
             if (isdigit(file[0]))
               {
                  snprintf(buf, sizeof(buf), "/proc/%s/stat", file);
                  f = fopen(buf, "r");
                  if (f)
                    {
                       pid2 = -1;
                       ppid = -1;
                       if (fscanf(f, "%i %*s %*s %i %*s", &pid2, &ppid) == 2)
                         {
                            fclose(f);
                            if (ppid == pid)
                              {
                                 if (adj_children)
                                   _pri_adj(pid2, set, adj, EINA_TRUE,
                                            adj_children, do_children);
                                 else
                                   _pri_adj(pid2, set, adj, use_adj,
                                            adj_children, do_children);
                              }
                         }
                       else fclose(f);
                    }
               }
             free(file);
          }
     }
}

static E_Client *
_e_comp_x_client_find_by_alarm(Ecore_X_Sync_Alarm al)
{
   return eina_hash_find(alarm_hash, &al);
}

static void
_e_comp_x_client_move_resize_send(E_Client *ec)
{
   if (ec->internal_elm_win)
     ecore_evas_managed_move(e_win_ee_get(ec->internal_elm_win), ec->client.x - ec->x, ec->client.y - ec->y);

   ecore_x_icccm_move_resize_send(e_client_util_win_get(ec), ec->client.x, ec->client.y, ec->client.w, ec->client.h);
}

static Eina_Bool
_e_comp_x_post_client_idler_cb(void *d EINA_UNUSED)
{
   E_Client *ec;

   //INF("POST IDLER");
   EINA_LIST_FREE(post_clients, ec)
     {
        Ecore_X_Window win, twin;

        if (e_object_is_del(E_OBJECT(ec)) || (!_e_comp_x_client_data_get(ec))) continue;
        win = _e_comp_x_client_window_get(ec);
        if (ec->post_move)
          {
             E_Client *tmp;
             Eina_List *l;

             EINA_LIST_FOREACH(ec->e.state.video_child, l, tmp)
               {
                  twin = _e_comp_x_client_window_get(tmp);
                  ecore_x_window_move(twin,
                                      ec->client.x +
                                      tmp->e.state.video_position.x,
                                      ec->client.y +
                                      tmp->e.state.video_position.y);
               }
          }
        if (ec->e.state.video)
          {
             E_Client *parent;

             parent = ec->e.state.video_parent_client;
             twin = _e_comp_x_client_window_get(parent);
             ecore_x_window_move(twin,
                                 parent->client.x + 
                                 ec->e.state.video_position.x,
                                 parent->client.y +
                                 ec->e.state.video_position.y);
          }
        else if (ec->shading)
          {
             // do nothing
          }
        else if ((ec->post_move) && (ec->post_resize))
          {
             //INF("X MVRSZ");
             ecore_x_window_move_resize(win,
                                        ec->client.x,
                                        ec->client.y,
                                        ec->client.w,
                                        ec->client.h);
             if (_e_comp_x_client_data_get(ec)->reparented)
               ecore_x_window_move_resize(e_client_util_win_get(ec), 0, 0,
                                          ec->client.w,
                                          ec->client.h);
          }
        else if (ec->post_move)
          {
             //INF("X MV");
             ecore_x_window_move(win, ec->client.x, ec->client.y);
          }
        else if (ec->post_resize)
          {
             //INF("X RSZ: %dx%d (REAL: %dx%d)", ec->client.w, ec->client.h, ec->w, ec->h);
             ecore_x_window_resize(win,
                                   ec->client.w, ec->client.h);
             if (_e_comp_x_client_data_get(ec)->reparented)
               ecore_x_window_move_resize(e_client_util_win_get(ec), 0, 0,
                                          ec->client.w, ec->client.h);
          }
        if ((!ec->shading) && (!ec->shaded))
          {
             if (ec->post_resize)
               {
                  if (ec->netwm.sync.request && (e_comp->comp_type == E_PIXMAP_TYPE_X))
                    {
                       //INF("NETWM SYNC: %p", ec);
                       ec->netwm.sync.wait++;
                       ecore_x_netwm_sync_request_send(e_client_util_win_get(ec),
                                                       ec->netwm.sync.serial++);
                    }
               }
             
             if (ec->post_move || ec->post_resize)
               _e_comp_x_client_move_resize_send(ec);
          }

        if (ec->e.state.video)
          {
             fprintf(stderr, "VIDEO %p: [%i, %i] [%i, %i]\n",
                     ec,
                     ec->e.state.video_parent_client->client.x +
                     ec->e.state.video_position.x,
                     ec->e.state.video_parent_client->client.y +
                     ec->e.state.video_position.y,
                     ec->w, ec->h);
          }
        if (ec->netwm.opacity_changed)
          {
             unsigned int opacity;
             int op;

             evas_object_color_get(ec->frame, NULL, NULL, NULL, &op);
             ec->netwm.opacity = op;
             opacity = (op << 24);
             ecore_x_window_prop_card32_set(e_client_util_win_get(ec), ECORE_X_ATOM_NET_WM_WINDOW_OPACITY, &opacity, 1);
             /* flag gets unset in property cb to avoid fetching opacity after we just set it */
          }
        if (e_pixmap_is_x(ec->pixmap))
          {
             if (ec->post_resize)
               e_pixmap_dirty(ec->pixmap);
             e_comp_object_render_update_del(ec->frame);
             if (!ec->internal)
               {
                  _e_comp_x_client_data_get(ec)->pw = ec->client.w;
                  _e_comp_x_client_data_get(ec)->ph = ec->client.h;
               }
          }
        ec->post_move = 0;
        ec->post_resize = 0;
     }
   if (e_comp->x_comp_data->restack && (!e_comp->new_clients))
     {
        e_hints_client_stacking_set();
        e_comp->x_comp_data->restack = 0;
     }
   _e_comp_x_post_client_idler = NULL;
   return EINA_FALSE;
}

static void
_e_comp_x_post_client_idler_add(E_Client *ec)
{
   if (!_e_comp_x_post_client_idler)
     _e_comp_x_post_client_idler = ecore_idle_enterer_add(_e_comp_x_post_client_idler_cb, NULL);
   if (!ec) CRI("ACK!");
   //INF("POST IDLE: %p", ec);
   if (!eina_list_data_find(post_clients, ec))
     post_clients = eina_list_append(post_clients, ec);
}

static void
_e_comp_x_client_stack(E_Client *ec)
{
   E_Client *ec2;
   Ecore_X_Window_Stack_Mode mode = ECORE_X_WINDOW_STACK_BELOW;
   Ecore_X_Window cwin, win = 0;
   Eina_List *l;

   if (ec->override && (!ec->internal)) return; //can't restack these
   if (e_client_is_stacking(ec)) return;
   if (!_e_comp_x_client_data_get(ec)) return;
   if (_e_comp_x_client_data_get(ec)->unredirected_single) return;

   cwin = _e_comp_x_client_window_get(ec);

   ecore_x_window_shadow_tree_flush();

   /* try stacking below */
   if (e_comp->nocomp_ec && (ec != e_comp->nocomp_ec))
     win = _e_comp_x_client_window_get(e_comp->nocomp_ec);
   else
     {
        ec2 = ec;
        do
          {
             ec2 = e_client_above_get(ec2);
             if (ec2 && e_client_has_xwindow(ec2) && (e_client_is_stacking(ec2) || ((!ec2->override) || ec2->internal)))
               {
                  if (ec2->layer != ec->layer) break;
                  if (_e_comp_x_client_data_get(ec2)->need_reparent && (!_e_comp_x_client_data_get(ec2)->reparented)) continue;
                  win = _e_comp_x_client_window_get(ec2);
               }
          } while (ec2 && (!win));
     }

   /* try stacking above */
   if (!win)
     {
        ec2 = ec;
        do
          {
             ec2 = e_client_below_get(ec2);
             if (ec2 && e_client_has_xwindow(ec2) && (e_client_is_stacking(ec2) || ((!ec2->override) || ec2->internal)))
               {
                  if (ec2->layer != ec->layer) break;
                  if (_e_comp_x_client_data_get(ec2)->need_reparent && (!_e_comp_x_client_data_get(ec2)->reparented)) continue;
                  win = _e_comp_x_client_window_get(ec2);
                  mode = ECORE_X_WINDOW_STACK_ABOVE;
               }
          } while (ec2 && (!win));
     }

   /* just layer stack */
   if (!win)
     {
        win = e_comp->layers[e_comp_canvas_layer_map(ec->layer)].win;
        mode = ECORE_X_WINDOW_STACK_BELOW;
     }
   ecore_x_window_configure(cwin,
                            ECORE_X_WINDOW_CONFIGURE_MASK_SIBLING |
                            ECORE_X_WINDOW_CONFIGURE_MASK_STACK_MODE,
                            0, 0, 0, 0, 0, win, mode);
   _e_comp_x_post_client_idler_add(ec);
   e_comp->x_comp_data->restack = 1;
   EINA_LIST_FOREACH(ec->e.state.video_child, l, ec2)
     evas_object_stack_below(ec2->frame, ec->frame);
}

static E_Client *
_e_comp_x_client_new(Ecore_X_Window win, Eina_Bool first)
{
   E_Pixmap *cp;
   E_Client *ec;

   cp = e_pixmap_new(E_PIXMAP_TYPE_X, win);
   EINA_SAFETY_ON_NULL_RETURN_VAL(cp, NULL);

   ec = e_client_new(cp, first, 0);
   return ec;
}

static void
_e_comp_x_client_pri_raise(E_Client *ec)
{
   if (ec->netwm.pid <= 0) return;
   if (ec->netwm.pid == getpid()) return;
   _pri_adj(ec->netwm.pid,
            e_config->priority - 1, -1, EINA_FALSE,
//            EINA_TRUE, EINA_TRUE);
            EINA_TRUE, EINA_FALSE);
//   printf("WIN: pid %i, title %s (HI!!!!!!!!!!!!!!!!!!)\n",
//          ec->netwm.pid, e_client_util_name_get(ec));
}

static void
_e_comp_x_client_pri_norm(E_Client *ec)
{
   if (ec->netwm.pid <= 0) return;
   if (ec->netwm.pid == getpid()) return;
   _pri_adj(ec->netwm.pid,
            e_config->priority, 1, EINA_FALSE,
//            EINA_TRUE, EINA_TRUE);
            EINA_TRUE, EINA_FALSE);
//   printf("WIN: pid %i, title %s (NORMAL)\n",
//          ec->netwm.pid, e_client_util_name_get(ec));
}

static void
_e_comp_x_client_shape_input_rectangle_set(E_Client *ec)
{
   Ecore_X_Window win = e_client_util_pwin_get(ec);

   if (ec->override || (!_e_comp_x_client_data_get(ec)->reparented)) return;

   if (ec->visible && (!ec->comp_hidden))
     ecore_x_composite_window_events_enable(win);
   else
     ecore_x_composite_window_events_disable(win);
}

static void
_e_comp_x_evas_color_set_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   E_Client *ec = data;
   int a;

   if (!_e_comp_x_client_data_get(ec)) return;
   evas_object_color_get(obj, NULL, NULL, NULL, &a);
   if (a == ec->netwm.opacity) return;
   ec->netwm.opacity_changed = 1;
   _e_comp_x_post_client_idler_add(ec);
}

static void
_e_comp_x_evas_ping_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Client *ec = data;

   if (!_e_comp_x_client_data_get(ec)) return;
   ecore_x_netwm_ping_send(e_client_util_win_get(ec));
}

static void
_e_comp_x_evas_kill_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Client *ec = data;

   if (!_e_comp_x_client_data_get(ec)) return;
   ecore_x_kill(e_client_util_win_get(ec));
}

static void
_e_comp_x_evas_delete_request_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Client *ec = data;

   if (!_e_comp_x_client_data_get(ec)) return;
   if (ec->netwm.ping)
     e_client_ping(ec);
   ecore_x_window_delete_request_send(e_client_util_win_get(ec));
}

static void
_e_comp_x_evas_comp_hidden_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Client *tmp, *ec = data;
   Eina_List *l;
   Ecore_X_Window win;

   if (!_e_comp_x_client_data_get(ec)) return;
   if (_e_comp_x_client_data_get(ec)->need_reparent) return;
   win = _e_comp_x_client_window_get(ec);

   EINA_LIST_FOREACH(ec->e.state.video_child, l, tmp)
     {
        Ecore_X_Window cwin;

        cwin = _e_comp_x_client_window_get(tmp);
        if (ec->comp_hidden)
          ecore_x_window_hide(cwin);
        else
          ecore_x_window_show(cwin);
     }

   if (ec->comp_hidden)
     ecore_x_composite_window_events_disable(win);
   else
     ecore_x_composite_window_events_enable(win);
    ecore_x_window_ignore_set(win, ec->comp_hidden);
}

static void
_e_comp_x_evas_shade_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Client *ec = data;
   Eina_List *l;
   E_Client *tmp;

   if (!_e_comp_x_client_data_get(ec)) return;
   EINA_LIST_FOREACH(ec->e.state.video_child, l, tmp)
     ecore_x_window_hide(e_client_util_pwin_get(tmp));

   ecore_x_window_shadow_tree_flush();
}

static void
_e_comp_x_evas_frame_recalc_cb(void *data, Evas_Object *obj, void *event_info)
{
   E_Client *ec = data;
   E_Comp_Object_Frame *fr = event_info;

   if (!_e_comp_x_client_data_get(ec)) return;
   if (evas_object_visible_get(obj))
     _e_comp_x_client_frame_update(ec, fr->l, fr->r, fr->t, fr->b);
   else
     _e_comp_x_client_data_get(ec)->frame_update = 1;
   ec->post_move = ec->post_resize = 1;
   _e_comp_x_post_client_idler_add(ec);
}

static void
_e_comp_x_evas_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Client *ec = data;

   if (!_e_comp_x_client_data_get(ec)) return;
   if (_e_comp_x_client_data_get(ec)->moving && (!_e_comp_x_client_data_get(ec)->unredirected_single))
     {
        if (_e_comp_x_client_data_get(ec)->move_counter++ < MOVE_COUNTER_LIMIT) return;
        _e_comp_x_client_data_get(ec)->move_counter = 0;
     }

   ecore_x_window_shadow_tree_flush();
   ec->post_move = 1;
   _e_comp_x_post_client_idler_add(ec);
}

static void
_e_comp_x_evas_resize_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Client *ec = data;

   if (!_e_comp_x_client_data_get(ec)) return;
   if (ec->shading || ec->shaded) return;
   if (!e_pixmap_size_changed(ec->pixmap, ec->client.w, ec->client.h)) return;

   ecore_x_window_shadow_tree_flush();

   if (ec->e.state.video)
     {
        if (ec->e.state.video_position.updated)
          {
             ecore_x_window_move(e_client_util_pwin_get(ec),
                                 ec->e.state.video_parent_client->client.x +
                                 ec->e.state.video_position.x,
                                 ec->e.state.video_parent_client->client.y +
                                 ec->e.state.video_position.y);
             ec->e.state.video_position.updated = 0;
          }
     }
   else if (ec->changes.pos)
     {
        E_Client *tmp;
        Eina_List *l;

        EINA_LIST_FOREACH(ec->e.state.video_child, l, tmp)
          ecore_x_window_move(e_client_util_pwin_get(tmp),
                              ec->client.x + tmp->e.state.video_position.x,
                              ec->client.y + tmp->e.state.video_position.y);
     }

   ec->post_resize = 1;
   if (e_pixmap_is_x(ec->pixmap))
     e_comp_object_render_update_del(ec->frame);
   _e_comp_x_post_client_idler_add(ec);
}

static void
_e_comp_x_evas_hide_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Client *ec = data, *tmp;
   unsigned int visible = 0;
   Eina_List *l;

   if (!_e_comp_x_client_data_get(ec)) return; // already deleted, happens with internal wins
   ecore_x_window_shadow_tree_flush();
   if ((!ec->iconic) && (!ec->override))
     ecore_x_window_prop_card32_set(e_client_util_win_get(ec), E_ATOM_MAPPED, &visible, 1);

   EINA_LIST_FOREACH(ec->e.state.video_child, l, tmp)
     evas_object_hide(tmp->frame);

   if (ec->unredirected_single)
     ecore_x_window_hide(_e_comp_x_client_window_get(ec));

   if (e_comp_config_get()->send_flush)
     ecore_x_e_comp_flush_send(e_client_util_win_get(ec));
   if (e_comp_config_get()->send_dump)
     ecore_x_e_comp_dump_send(e_client_util_win_get(ec));
}

static void
_e_comp_x_evas_show_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Client *ec = data, *tmp;
   unsigned int visible = 1;
   Ecore_X_Window win;
   Eina_List *l;

   if (!_e_comp_x_client_data_get(ec)) return;
   win = e_client_util_win_get(ec);
   ecore_x_window_shadow_tree_flush();
   if (!_e_comp_x_client_data_get(ec)->need_reparent)
     ecore_x_window_show(win);
   if (ec->unredirected_single)
     ecore_x_window_show(_e_comp_x_client_window_get(ec));
   if (!ec->override)
     e_hints_window_visible_set(ec);

   ecore_x_window_prop_card32_set(win, E_ATOM_MAPPED, &visible, 1);
   ecore_x_window_prop_card32_set(win, E_ATOM_MANAGED, &visible, 1);

   if (_e_comp_x_client_data_get(ec)->frame_update)
     {
        int ll, r, t, b;

        e_comp_object_frame_geometry_get(obj, &ll, &r, &t, &b);
        _e_comp_x_client_frame_update(ec, ll, r, t, b);
     }

   EINA_LIST_FOREACH(ec->e.state.video_child, l, tmp)
     {
        evas_object_show(tmp->frame);
        ecore_x_window_show(e_client_util_pwin_get(tmp));
     }
}

static void
_e_comp_x_evas_stack_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   if (evas_object_data_get(obj, "client_restack"))
     _e_comp_x_client_stack(data);
}

static void
_e_comp_x_evas_unfullscreen_zoom_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   if ((screen_size.width != -1) && (screen_size.height != -1))
     {
        ecore_x_randr_screen_primary_output_size_set(e_comp->root,
                                                     screen_size_index);
        screen_size.width = -1;
        screen_size.height = -1;
     }
}

static void
_e_comp_x_evas_fullscreen_zoom_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Client *ec = data;
   Ecore_X_Randr_Screen_Size_MM *sizes;
   int num_sizes, i, best_size_index = 0;

   ecore_x_randr_screen_primary_output_current_size_get(e_comp->root,
                                                        &screen_size.width,
                                                        &screen_size.height,
                                                        NULL, NULL, NULL);
   sizes = ecore_x_randr_screen_primary_output_sizes_get(e_comp->root,
                                                         &num_sizes);
   if (sizes)
     {
        Ecore_X_Randr_Screen_Size best_size = { -1, -1 };
        int best_dist = INT_MAX, dist;

        for (i = 0; i < num_sizes; i++)
          {
             if ((sizes[i].width > ec->w) && (sizes[i].height > ec->h))
               {
                  dist = (sizes[i].width * sizes[i].height) - (ec->w * ec->h);
                  if (dist < best_dist)
                    {
                       best_size.width = sizes[i].width;
                       best_size.height = sizes[i].height;
                       best_dist = dist;
                       best_size_index = i;
                    }
               }
          }
        if (((best_size.width != -1) && (best_size.height != -1)) &&
            ((best_size.width != screen_size.width) ||
             (best_size.height != screen_size.height)))
          {
             if (ecore_x_randr_screen_primary_output_size_set(e_comp->root,
                                                              best_size_index))
               screen_size_index = best_size_index;
             evas_object_geometry_set(ec->frame, 0, 0, best_size.width, best_size.height);
          }
        else
          {
             screen_size.width = -1;
             screen_size.height = -1;
             evas_object_geometry_set(ec->frame, 0, 0, ec->zone->w, ec->zone->h);
          }
        free(sizes);
     }
   else
     evas_object_geometry_set(ec->frame, ec->zone->x, ec->zone->y, ec->zone->w, ec->zone->h);
}

static Eina_Bool
_e_comp_x_destroy(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Destroy *ev)
{
   E_Client *ec;

   //INF("X DESTROY: %u", ev->win);
   e_comp_ignore_win_del(E_PIXMAP_TYPE_X, ev->win);
   ec = _e_comp_x_client_find_by_window(ev->win);
   if (!ec)
     {
        if (!e_comp->x_comp_data->retry_clients) return ECORE_CALLBACK_RENEW;
        e_comp->x_comp_data->retry_clients = eina_list_remove(e_comp->x_comp_data->retry_clients, (uintptr_t*)(unsigned long)ev->win);
        if (!e_comp->x_comp_data->retry_clients)
          E_FREE_FUNC(e_comp->x_comp_data->retry_timer, ecore_timer_del);
        return ECORE_CALLBACK_PASS_ON;
     }
   if (_e_comp_x_client_data_get(ec))
     {
        if (_e_comp_x_client_data_get(ec)->reparented)
          e_client_comp_hidden_set(ec, 1);
        evas_object_pass_events_set(ec->frame, 1);
        evas_object_hide(ec->frame);
        _e_comp_x_client_data_get(ec)->deleted = 1;
        DELD(ec, 2);
        e_object_del(E_OBJECT(ec));
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_x_resize_request(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Resize_Request *ev)
{
   E_Client *ec;
   int w, h;

   ec = _e_comp_x_client_find_by_window(ev->win);
   if (!ec)
     {
        if (!e_comp_find_by_window(ev->win)) ecore_x_window_resize(ev->win, ev->w, ev->h);
        return ECORE_CALLBACK_PASS_ON;
     }
   w = ev->w, h = ev->h;
   if (ec->zone && (e_config->geometry_auto_resize_limit == 1))
     {
        int zx, zy, zw, zh;

        e_zone_useful_geometry_get(ec->zone, &zx, &zy, &zw, &zh);
        if (w > zw)
          w = zw;

        if (h > zh)
          h = zh;
     }
   if ((w != ec->w) || (h != ec->h))
     evas_object_resize(ec->frame, w, h);
   return ECORE_CALLBACK_RENEW;
}

static void
_e_comp_x_client_evas_init(E_Client *ec)
{
   if (_e_comp_x_client_data_get(ec)->evas_init) return;
   _e_comp_x_client_data_get(ec)->evas_init = 1;
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_RESTACK, _e_comp_x_evas_stack_cb, ec);
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_SHOW, _e_comp_x_evas_show_cb, ec);
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_HIDE, _e_comp_x_evas_hide_cb, ec);
   if (!ec->override)
     {
        evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_MOVE, _e_comp_x_evas_move_cb, ec);
        evas_object_smart_callback_add(ec->frame, "client_resize", _e_comp_x_evas_resize_cb, ec);
     }
   evas_object_smart_callback_add(ec->frame, "frame_recalc_done", _e_comp_x_evas_frame_recalc_cb, ec);
   evas_object_smart_callback_add(ec->frame, "shade_done", _e_comp_x_evas_shade_cb, ec);
   evas_object_smart_callback_add(ec->frame, "comp_hidden", _e_comp_x_evas_comp_hidden_cb, ec);
   evas_object_smart_callback_add(ec->frame, "delete_request", _e_comp_x_evas_delete_request_cb, ec);
   evas_object_smart_callback_add(ec->frame, "kill_request", _e_comp_x_evas_kill_cb, ec);
   evas_object_smart_callback_add(ec->frame, "ping", _e_comp_x_evas_ping_cb, ec);
   evas_object_smart_callback_add(ec->frame, "color_set", _e_comp_x_evas_color_set_cb, ec);
   evas_object_smart_callback_add(ec->frame, "fullscreen_zoom", _e_comp_x_evas_fullscreen_zoom_cb, ec);
   evas_object_smart_callback_add(ec->frame, "unfullscreen_zoom", _e_comp_x_evas_unfullscreen_zoom_cb, ec);
   /* force apply this since we haven't set up our smart cb previously */
   _e_comp_x_evas_comp_hidden_cb(ec, NULL, NULL);
}

static Eina_Bool
_e_comp_x_object_add(void *d EINA_UNUSED, int t EINA_UNUSED, E_Event_Comp_Object *ev)
{
   E_Client *ec;

   ec = e_comp_object_client_get(ev->comp_object);
   if ((!ec) || e_object_is_del(E_OBJECT(ec)) || ec->re_manage) return ECORE_CALLBACK_RENEW;
   E_COMP_X_PIXMAP_CHECK ECORE_CALLBACK_RENEW;
   _e_comp_x_client_evas_init(ec);
   _e_comp_x_client_stack(ec);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_e_comp_x_show_request(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Show_Request *ev)
{
   E_Client *ec;

   //INF("X SHOW REQ: %u", ev->win);
   ec = _e_comp_x_client_find_by_window(ev->win);
   if (e_comp_ignore_win_find(ev->win) ||
     (ec && (ec->ignored || ec->override)) ||
     (!e_comp_find_by_window(ev->parent)) ||
     (ev->parent != e_comp->root))
     {
        ecore_x_window_show(ev->win);
        return ECORE_CALLBACK_RENEW;
     }
   if (!ec)
     ec = _e_comp_x_client_new(ev->win, 0);
   if (!ec)
     {
        ecore_x_window_show(ev->win);
        return ECORE_CALLBACK_RENEW;
     }
   if ((e_comp->comp_type != E_PIXMAP_TYPE_X) && ec->ignored)
     {
        ec->visible = 1;
        if (ec->comp_data->need_reparent)
          _e_comp_x_hook_client_pre_frame_assign(NULL, ec);
        else
          ecore_x_window_show(ev->win);
        return ECORE_CALLBACK_RENEW;
     }

   if ((!_e_comp_x_client_data_get(ec)->reparented) && (!e_client_util_ignored_get(ec)))
     {
        if (!ec->override)
          _e_comp_x_client_data_get(ec)->need_reparent = 1;
        ec->visible = 1;
        EC_CHANGED(ec);
        return ECORE_CALLBACK_RENEW;
     }
   if (ec->iconic)
     {
        if (!ec->lock_client_iconify)
          e_client_uniconify(ec);
     }
   else
     {
        /* FIXME: make client "urgent" for a bit - it wants attention */
        /*	e_client_show(ec); */
        if (!ec->lock_client_stacking)
          evas_object_raise(ec->frame);
     }
   return ECORE_CALLBACK_RENEW;
}

static void
_e_comp_x_show_helper(E_Client *ec)
{
   if ((!ec->override) && (!ec->re_manage) && (!_e_comp_x_client_data_get(ec)->first_map) &&
       (!_e_comp_x_client_data_get(ec)->reparented) && (!_e_comp_x_client_data_get(ec)->need_reparent))
     {
        /* this is most likely an internal window */
        _e_comp_x_client_data_get(ec)->need_reparent = 1;
        ec->visible = 1;
        if (ec->internal_elm_win)
          ec->take_focus = 1;
        EC_CHANGED(ec);
     }
   else if (ec->override)
     {
        if (e_comp->comp_type == E_PIXMAP_TYPE_WL)
          {
             ec->visible = 1;
             return;
          }
        ec->redirected = !ec->input_only;
        ec->changes.visible = ec->visible = 1;
        EC_CHANGED(ec);
        if (!_e_comp_x_client_data_get(ec)->first_map)
          _e_comp_x_client_evas_init(ec);
     }
   if (!ec->input_only)
     _e_comp_x_client_damage_add(ec);
   if (!_e_comp_x_client_data_get(ec)->need_reparent)
     {
        e_pixmap_usable_set(ec->pixmap, 1);
        if (ec->hidden || ec->iconic)
          {
             evas_object_hide(ec->frame);
             e_comp_object_damage(ec->frame, 0, 0, ec->w, ec->h);
          }
        else
          evas_object_show(ec->frame);
        _e_comp_x_client_data_get(ec)->first_map = 1;
        if (ec->internal_elm_win)
          {
             _e_comp_x_post_client_idler_add(ec);
             ec->post_move = 1;
             ec->post_resize = 1;
             e_comp->x_comp_data->restack = 1;
          }
     }
}

static Eina_Bool
_e_comp_x_show_retry(void *data EINA_UNUSED)
{
   uintptr_t *win;

   EINA_LIST_FREE(e_comp->x_comp_data->retry_clients, win)
     {
        E_Client *ec;

        ec = _e_comp_x_client_new((Ecore_X_Window)(uintptr_t)win, 0);
        if (ec) _e_comp_x_show_helper(ec);
     }

   e_comp->x_comp_data->retry_timer = NULL;
   return EINA_FALSE;
}

static Eina_Bool
_e_comp_x_show(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Show *ev)
{
   E_Client *ec;

   //INF("X SHOW: %u", ev->win);
   ec = _e_comp_x_client_find_by_window(ev->win);
   if (!ec)
     {
        E_Comp *c;

        if (e_comp_ignore_win_find(ev->win)) return ECORE_CALLBACK_RENEW;
        c = e_comp_find_by_window(ev->event_win);
        if (!c) return ECORE_CALLBACK_RENEW;
        if (ev->event_win != e_comp->root) return ECORE_CALLBACK_RENEW;
        if ((c->win == ev->win) || (c->ee_win == ev->win) ||
            (c->root == ev->win) || (c->cm_selection == ev->win)) return ECORE_CALLBACK_RENEW;
        /* some window which we haven't made a client for yet but need to */
        ec = _e_comp_x_client_new(ev->win, 0);
        if (!ec)
          {
             if (c->x_comp_data->retry_timer)
               ecore_timer_reset(c->x_comp_data->retry_timer);
             else
               c->x_comp_data->retry_timer = ecore_timer_add(0.02, _e_comp_x_show_retry, c);
             c->x_comp_data->retry_clients = eina_list_append(c->x_comp_data->retry_clients, (uintptr_t*)(unsigned long)ev->win);
             return ECORE_CALLBACK_RENEW;
          }
     }
   else if (e_object_is_del(E_OBJECT(ec)) || ec->already_unparented)
     return ECORE_CALLBACK_RENEW;
   _e_comp_x_show_helper(ec);
   if (ec->internal || (!ec->override))
     _e_comp_x_client_stack(ec);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_e_comp_x_hide(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Hide *ev)
{
   E_Client *ec;
   Eina_Bool hid = EINA_FALSE;

   //INF("X HIDE: %u", ev->win);
   ec = _e_comp_x_client_find_by_window(ev->win);
   if (!ec)
     {
        if (!e_comp->x_comp_data->retry_clients) return ECORE_CALLBACK_RENEW;
        e_comp->x_comp_data->retry_clients = eina_list_remove(e_comp->x_comp_data->retry_clients, (uintptr_t*)(unsigned long)ev->win);
        if (!e_comp->x_comp_data->retry_clients)
          E_FREE_FUNC(e_comp->x_comp_data->retry_timer, ecore_timer_del);
        return ECORE_CALLBACK_PASS_ON;
     }
   if ((!ec->visible) || (ec->hidden && ec->unredirected_single))
     {
        //INF("IGNORED");
        return ECORE_CALLBACK_PASS_ON;
     }

   if (ev->win != ev->event_win)
     {
        if ((!ec->override) && (!ev->send_event)) return ECORE_CALLBACK_PASS_ON;
     }
   if (ec->ignore_first_unmap > 0)
     {
        ec->ignore_first_unmap--;
        return ECORE_CALLBACK_PASS_ON;
     }
   /* Don't delete hidden or iconified windows */
   if (ec->iconic)
     {
        /* Only hide the border if it is visible */
        hid = EINA_TRUE;
        evas_object_hide(ec->frame);
     }
   else
     {
        hid = EINA_TRUE;
        evas_object_hide(ec->frame);
        if (!ec->internal)
          {
             if (ec->exe_inst && ec->exe_inst->exe)
               ec->exe_inst->phony = 0;
             DELD(ec, 3);
             e_object_del(E_OBJECT(ec));
          }
     }
   if (hid)
     _e_comp_x_focus_check();
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_x_reparent(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Reparent *ev)
{
   E_Client *ec;

   ec = _e_comp_x_client_find_by_window(ev->win);
   if ((!ec) || (ev->win == e_client_util_pwin_get(ec))) return ECORE_CALLBACK_PASS_ON;
   DBG("== repar [%u] to [%u]", ev->win, ev->parent);
   /* FIXME: this is almost definitely wrong */
   if (ev->parent != e_client_util_pwin_get(ec))
     {
        e_pixmap_parent_window_set(ec->pixmap, ev->parent);
        if (!e_object_is_del(E_OBJECT(ec)))
          e_comp_object_damage(ec->frame, 0, 0, ec->w, ec->h);
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_x_configure(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Configure *ev)
{
   E_Client *ec;
   Eina_Bool move, resize;

   if (e_comp_find_by_window(ev->win))
     {
        // do not handle this here - use randr events
        //e_comp_canvas_resize(ev->w, ev->h);
        return ECORE_CALLBACK_RENEW;
     }
   ec = _e_comp_x_client_find_by_window(ev->win);
   if ((!ec) || (!ec->override) || (!ev->override) || e_client_is_stacking(ec)) return ECORE_CALLBACK_PASS_ON;

   //INF("configure %p: %d,%d %dx%d", ec, ev->x, ev->y, ev->w, ev->h);
   if (ev->abovewin == 0)
     {
        if (e_client_below_get(ec))
          evas_object_lower(ec->frame);
     }
   else
     {
        E_Client *ec2 = _e_comp_x_client_find_by_window(ev->abovewin);

        if (ec2)
          {
             /* client(ec) wants to be above a layer client(ec2) */
             if (e_client_is_stacking(ec2)) //is it a stacking placeholder window?
               {
                  E_Client *ec3;

                  ec3 = e_client_above_get(ec2);
                  /* stack under next client(ec3) */
                  if (ec3)
                    {
                       evas_object_layer_set(ec->frame, ec3->layer);
                       if (!e_client_is_stacking(ec3))
                         evas_object_stack_below(ec->frame, ec3->frame);
                    }
                  else //force override to obey our stacking
                    evas_object_layer_set(ec->frame, ec2->layer);
               }
             else
               {
                  evas_object_layer_set(ec->frame, ec2->layer);
                  evas_object_stack_above(ec->frame, ec2->frame);
               }
          }
        else
          evas_object_layer_set(ec->frame, E_LAYER_CLIENT_PRIO);
     }
   move = (ec->client.x != ev->x) || (ec->client.y != ev->y);
   resize = (ec->client.w != ev->w) || (ec->client.h != ev->h);
   if (!ec->internal)
     _e_comp_x_client_data_get(ec)->pw = ev->w, _e_comp_x_client_data_get(ec)->ph = ev->h;
   EINA_RECTANGLE_SET(&ec->client, ev->x, ev->y, ev->w, ev->h);
   if (move)
     evas_object_move(ec->frame, ev->x, ev->y);
   if (resize && e_pixmap_is_x(ec->pixmap))
     {
        e_pixmap_dirty(ec->pixmap);
        evas_object_resize(ec->frame, ev->w, ev->h);
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_e_comp_x_configure_request(void *data  EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Configure_Request *ev)
{
   E_Client *ec;
   Eina_Bool move = EINA_FALSE, resize = EINA_FALSE;
   int ox, oy, ow, oh;
   int x, y, w, h;

   if (e_comp_find_by_window(ev->win)) return ECORE_CALLBACK_RENEW;
   ec = _e_comp_x_client_find_by_window(ev->win);

   /* pass through requests for windows we haven't/won't reparent yet */
   if (ec && (!_e_comp_x_client_data_get(ec)->need_reparent) && (!_e_comp_x_client_data_get(ec)->reparented))
     {
        _e_comp_x_client_data_get(ec)->initial_attributes.x = ev->x;
        _e_comp_x_client_data_get(ec)->initial_attributes.y = ev->y;
        ec->w = ec->client.w = _e_comp_x_client_data_get(ec)->initial_attributes.w = ev->w;
        ec->h = ec->client.h = _e_comp_x_client_data_get(ec)->initial_attributes.h = ev->h;
        ec->border_size = ev->border;
        ec->changes.size = 1;
        ec = NULL;
     }
   if (!ec)
     {
        ecore_x_window_configure(ev->win, ev->value_mask,
                                 ev->x, ev->y, ev->w, ev->h, ev->border,
                                 ev->abovewin, ev->detail);
        return ECORE_CALLBACK_PASS_ON;
     }

   x = ox = ec->x;
   y = oy = ec->y;
   w = ow = ec->client.w;
   h = oh = ec->client.h;

   if ((ev->value_mask & ECORE_X_WINDOW_CONFIGURE_MASK_X) ||
       (ev->value_mask & ECORE_X_WINDOW_CONFIGURE_MASK_Y))
     {
        int zx, zy, zw, zh;

        e_zone_useful_geometry_get(ec->zone, &zx, &zy, &zw, &zh);
        if (ev->value_mask & ECORE_X_WINDOW_CONFIGURE_MASK_X)
          {
             _e_comp_x_client_data_get(ec)->initial_attributes.x = ev->x;
             if (e_config->screen_limits == E_CLIENT_OFFSCREEN_LIMIT_ALLOW_NONE)
               x = E_CLAMP(ev->x, zx, zx + zw - ec->w);
             else
               x = ev->x;
          }
        if (ev->value_mask & ECORE_X_WINDOW_CONFIGURE_MASK_Y)
          {
             _e_comp_x_client_data_get(ec)->initial_attributes.y = ev->y;
             if (e_config->screen_limits == E_CLIENT_OFFSCREEN_LIMIT_ALLOW_NONE)
               y = E_CLAMP(ev->y, zy, zy + zh - ec->h);
             else
               y = ev->y;
          }
     }
   
   if ((ev->value_mask & ECORE_X_WINDOW_CONFIGURE_MASK_W) ||
       (ev->value_mask & ECORE_X_WINDOW_CONFIGURE_MASK_H))
     {
        if (ev->value_mask & ECORE_X_WINDOW_CONFIGURE_MASK_W)
          w = ev->w, _e_comp_x_client_data_get(ec)->initial_attributes.w = ev->w;
        if (ev->value_mask & ECORE_X_WINDOW_CONFIGURE_MASK_H)
          h = ev->h, _e_comp_x_client_data_get(ec)->initial_attributes.h = ev->h;
     }
   
   e_comp_object_frame_wh_adjust(ec->frame, w, h, &w, &h);

   move = (x != ec->x) || (y != ec->y);
   resize = (w != ec->w) || (h != ec->h);

   if (move && (!ec->lock_client_location))
     {
        if ((ec->maximized & E_MAXIMIZE_TYPE) != E_MAXIMIZE_NONE)
          {
             E_Zone *zone;

             ec->saved.x = x;
             ec->saved.y = y;

             zone = e_comp_zone_xy_get(x, y);
             if (zone && (zone->x || zone->y))
               {
                  ec->saved.x -= zone->x;
                  ec->saved.y -= zone->y;
               }
          }
        else
          {
             /* client is completely outside the screen, policy does not allow */
             if (((!E_INTERSECTS(x, y, ec->w, ec->h, 0, 0, e_comp->w - 5, e_comp->h - 5)) &&
                 (e_config->screen_limits != E_CLIENT_OFFSCREEN_LIMIT_ALLOW_FULL)) ||
                 /* client is partly outside the zone, policy does not allow */
                 (((!E_INSIDE(x, y, 0, 0, e_comp->w - 5, e_comp->h - 5)) &&
                  (!E_INSIDE(x + ec->w, y, 0, 0, e_comp->w - 5, e_comp->h - 5)) &&
                  (!E_INSIDE(x, y + ec->h, 0, 0, e_comp->w - 5, e_comp->h - 5)) &&
                  (!E_INSIDE(x + ec->w, y + ec->h, 0, 0, e_comp->w - 5, e_comp->h - 5))) &&
                  (e_config->screen_limits == E_CLIENT_OFFSCREEN_LIMIT_ALLOW_NONE))
                )
               e_comp_object_util_center(ec->frame);
             else
               {
                  evas_object_move(ec->frame, x, y);
               }
          }
     }

   if (resize && (!ec->lock_client_size) && (move || ((!ec->maximized) && (!ec->fullscreen))))
     {
        if ((ec->maximized & E_MAXIMIZE_TYPE) != E_MAXIMIZE_NONE)
          {
             ec->saved.w = w;
             ec->saved.h = h;
          }
        else
          {
             evas_object_resize(ec->frame, w, h);
          }
     }
   if (!ec->lock_client_stacking)
     {
        if ((ev->value_mask & ECORE_X_WINDOW_CONFIGURE_MASK_STACK_MODE) &&
            (ev->value_mask & ECORE_X_WINDOW_CONFIGURE_MASK_SIBLING))
          {
             E_Client *oec;

             if (ev->detail == ECORE_X_WINDOW_STACK_ABOVE)
               {
                  oec = _e_comp_x_client_find_by_window(ev->abovewin);
                  if (oec)
                    {
                       evas_object_stack_above(ec->frame, oec->frame);
                    }
                  else
                    {
                       ecore_x_window_configure(e_client_util_pwin_get(ec),
                                                ECORE_X_WINDOW_CONFIGURE_MASK_SIBLING |
                                                ECORE_X_WINDOW_CONFIGURE_MASK_STACK_MODE,
                                                0, 0, 0, 0, 0,
                                                ev->abovewin, ECORE_X_WINDOW_STACK_ABOVE);
                    }
               }
             else if (ev->detail == ECORE_X_WINDOW_STACK_BELOW)
               {
                  oec = _e_comp_x_client_find_by_window(ev->abovewin);
                  if (oec)
                    {
                       evas_object_stack_below(ec->frame, oec->frame);
                    }
                  else
                    {
                       ecore_x_window_configure(e_client_util_pwin_get(ec),
                                                ECORE_X_WINDOW_CONFIGURE_MASK_SIBLING |
                                                ECORE_X_WINDOW_CONFIGURE_MASK_STACK_MODE,
                                                0, 0, 0, 0, 0,
                                                ev->abovewin, ECORE_X_WINDOW_STACK_BELOW);
                    }
               }
             else if (ev->detail == ECORE_X_WINDOW_STACK_TOP_IF)
               {
                  /* FIXME: do */
               }
             else if (ev->detail == ECORE_X_WINDOW_STACK_BOTTOM_IF)
               {
                  /* FIXME: do */
               }
             else if (ev->detail == ECORE_X_WINDOW_STACK_OPPOSITE)
               {
                  /* FIXME: do */
               }
          }
        else if (ev->value_mask & ECORE_X_WINDOW_CONFIGURE_MASK_STACK_MODE)
          {
             if (ev->detail == ECORE_X_WINDOW_STACK_ABOVE)
               {
                  evas_object_raise(ec->frame);
               }
             else if (ev->detail == ECORE_X_WINDOW_STACK_BELOW)
               {
                  evas_object_lower(ec->frame);
               }
             else if (ev->detail == ECORE_X_WINDOW_STACK_TOP_IF)
               {
                  /* FIXME: do */
               }
             else if (ev->detail == ECORE_X_WINDOW_STACK_BOTTOM_IF)
               {
                  /* FIXME: do */
               }
             else if (ev->detail == ECORE_X_WINDOW_STACK_OPPOSITE)
               {
                  /* FIXME: do */
               }
          }
     }

   /* FIXME: need to send synthetic stacking event to, as well as move/resize */
   if ((((ec->maximized & E_MAXIMIZE_TYPE) != E_MAXIMIZE_NONE) && (move || resize)) ||
       ((!move) && (!resize)))
     _e_comp_x_client_move_resize_send(ec);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_x_stack_request(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Stack_Request *ev)
{
   E_Client *ec;

   ec = _e_comp_x_client_find_by_window(ev->win);
   if (!ec) return ECORE_CALLBACK_RENEW;
   if (ev->detail == ECORE_X_WINDOW_STACK_ABOVE)
     evas_object_raise(ec->frame);
   else if (ev->detail == ECORE_X_WINDOW_STACK_BELOW)
     evas_object_lower(ec->frame);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_x_stack(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Stack *ev)
{
   E_Client *ec = _e_comp_x_client_find_by_window(ev->win);
   if (!ec) return ECORE_CALLBACK_PASS_ON;
   if (ev->detail == ECORE_X_WINDOW_STACK_ABOVE) evas_object_raise(ec->frame);
   else evas_object_lower(ec->frame);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_x_property(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Property *ev)
{
   E_Client *ec;

   ec = _e_comp_x_client_find_by_window(ev->win);
   if (!ec) return ECORE_CALLBACK_RENEW;

   if (ev->atom == ECORE_X_ATOM_WM_NAME)
     {
        if ((!ec->netwm.name) &&
            (!ec->netwm.fetch.name))
          {
             ec->icccm.fetch.title = 1;
             EC_CHANGED(ec);
          }
     }
   else if (ev->atom == ECORE_X_ATOM_NET_WM_NAME)
     {
        ec->netwm.fetch.name = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_WM_CLASS)
     {
        ec->icccm.fetch.name_class = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_WM_ICON_NAME)
     {
        if ((!ec->netwm.icon_name) &&
            (!ec->netwm.fetch.icon_name))
          {
             ec->icccm.fetch.icon_name = 1;
             EC_CHANGED(ec);
          }
     }
   else if (ev->atom == ECORE_X_ATOM_NET_WM_ICON_NAME)
     {
        ec->netwm.fetch.icon_name = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_WM_CLIENT_MACHINE)
     {
        ec->icccm.fetch.machine = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_WM_PROTOCOLS)
     {
        ec->icccm.fetch.protocol = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_WM_HINTS)
     {
        ec->icccm.fetch.hints = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_WM_NORMAL_HINTS)
     {
        if (_e_comp_x_client_data_get(ec)->internal_props_set)
          _e_comp_x_client_data_get(ec)->internal_props_set--;
        else
          {
             ec->icccm.fetch.size_pos_hints = 1;
             EC_CHANGED(ec);
          }
     }
   else if (ev->atom == ECORE_X_ATOM_MOTIF_WM_HINTS)
     {
        /*
           if ((ec->netwm.type == E_WINDOW_TYPE_UNKNOWN) &&
            (!ec->netwm.fetch.type))
           {
         */
        ec->mwm.fetch.hints = 1;
        EC_CHANGED(ec);
        /*
           }
         */
     }
   else if (ev->atom == ECORE_X_ATOM_WM_TRANSIENT_FOR)
     {
        ec->icccm.fetch.transient_for = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_WM_CLIENT_LEADER)
     {
        ec->icccm.fetch.client_leader = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_WM_WINDOW_ROLE)
     {
        ec->icccm.fetch.window_role = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_NET_WM_ICON)
     {
        ec->netwm.fetch.icon = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ATM__QTOPIA_SOFT_MENU)
     {
        ec->qtopia.fetch.soft_menu = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ATM__QTOPIA_SOFT_MENUS)
     {
        ec->qtopia.fetch.soft_menus = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_E_VIRTUAL_KEYBOARD_STATE)
     {
        ec->vkbd.fetch.state = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_E_VIRTUAL_KEYBOARD)
     {
        ec->vkbd.fetch.vkbd = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_E_ILLUME_CONFORMANT)
     {
        _e_comp_x_client_data_get(ec)->illume.conformant.fetch.conformant = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_E_ILLUME_QUICKPANEL_STATE)
     {
        _e_comp_x_client_data_get(ec)->illume.quickpanel.fetch.state = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_E_ILLUME_QUICKPANEL)
     {
        _e_comp_x_client_data_get(ec)->illume.quickpanel.fetch.quickpanel = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_E_ILLUME_QUICKPANEL_PRIORITY_MAJOR)
     {
        _e_comp_x_client_data_get(ec)->illume.quickpanel.fetch.priority.major = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_E_ILLUME_QUICKPANEL_PRIORITY_MINOR)
     {
        _e_comp_x_client_data_get(ec)->illume.quickpanel.fetch.priority.minor = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_E_ILLUME_QUICKPANEL_ZONE)
     {
        _e_comp_x_client_data_get(ec)->illume.quickpanel.fetch.zone = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_E_ILLUME_DRAG_LOCKED)
     {
        _e_comp_x_client_data_get(ec)->illume.drag.fetch.locked = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_E_ILLUME_DRAG)
     {
        _e_comp_x_client_data_get(ec)->illume.drag.fetch.drag = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_E_ILLUME_WINDOW_STATE)
     {
        _e_comp_x_client_data_get(ec)->illume.win_state.fetch.state = 1;
        EC_CHANGED(ec);
     }
   /*
      else if (ev->atom == ECORE_X_ATOM_NET_WM_USER_TIME)
      {
        ec->netwm.fetch.user_time = 1;
        EC_CHANGED(ec);
      }
      else if (ev->atom == ECORE_X_ATOM_NET_WM_STRUT)
      {
        ec->netwm.fetch.strut = 1;
        EC_CHANGED(ec);
      }
      else if (ev->atom == ECORE_X_ATOM_NET_WM_STRUT_PARTIAL)
      {
        ec->netwm.fetch.strut = 1;
        EC_CHANGED(ec);
      }
    */
   else if (ev->atom == ECORE_X_ATOM_NET_WM_SYNC_REQUEST_COUNTER)
     {
        //printf("ECORE_X_ATOM_NET_WM_SYNC_REQUEST_COUNTER\n");
     }
   else if (ev->atom == ECORE_X_ATOM_E_VIDEO_POSITION)
     {
        ec->e.fetch.video_position = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_E_VIDEO_PARENT)
     {
        ec->e.fetch.video_parent = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_NET_WM_STATE)
     {
        ec->netwm.fetch.state = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_NET_WM_WINDOW_OPACITY)
     {
        if (ec->netwm.opacity_changed)
          ec->netwm.opacity_changed = 0;
        else
          {
             ec->netwm.fetch.opacity = 1;
             EC_CHANGED(ec);
          }
     }
   else if (ev->atom == ECORE_X_ATOM_E_WINDOW_PROFILE_SUPPORTED)
     {
        ec->e.fetch.profile = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_E_WINDOW_PROFILE_AVAILABLE_LIST)
     {
        ec->e.fetch.profile = 1;
        EC_CHANGED(ec);
     }

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_e_comp_x_message(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Client_Message *ev)
{
   E_Client *ec;

   ec = _e_comp_x_client_find_by_window(ev->win);
   if (!ec)
     {
        DBG("missed client message '%s' for %u", ecore_x_atom_name_get(ev->message_type), ev->win);
        return ECORE_CALLBACK_RENEW;
     }
   if (ev->message_type == ECORE_X_ATOM_NET_WM_WINDOW_OPACITY)
     {
        ec->netwm.fetch.opacity = 1;
        EC_CHANGED(ec);
     }
   else if (ev->message_type == ECORE_X_ATOM_E_WINDOW_PROFILE_CHANGE)
     {
        if (ec->e.state.profile.use)
          {
             char *p = ecore_x_atom_name_get(ev->data.l[1]);
             if (e_client_desk_window_profile_available_check(ec, p))
               {
                  if (e_util_strcmp(p, ec->desk->window_profile))
                    {
                       E_Desk *desk = e_comp_desk_window_profile_get(p);
                       if (desk) e_client_desk_set(ec, desk);
                    }
               }
             free(p);
          }
     }
   else if (ev->message_type == ECORE_X_ATOM_NET_ACTIVE_WINDOW)
     {
#if 0 /* notes */
        if (e->data.l[0] == 0 /* 0 == old, 1 == client, 2 == pager */)
          {
          }
        timestamp = e->data.l[1];
        requestor_id e->data.l[2];
#endif
        switch (e_config->window_activehint_policy)
          {
           case 0: break;

           case 1:
             e_client_urgent_set(ec, 1);
             break;

           default:
             if (e_client_action_get() || e_comp->nocomp)
               {
                  /* be helpful and ignore activates during window actions, but still set urgent */
                  e_client_urgent_set(ec, 1);
                  break;
               }
             if ((!starting) && (!ec->focused))
               {
                  if ((e_config->window_activehint_policy == E_ACTIVEHINT_POLICY_ACTIVATE_EXCLUDE) &&
                      (!ec->desk->visible))
                    e_client_urgent_set(ec, 1);
                  else
                    e_client_activate(ec, EINA_TRUE);
               }
             else
               evas_object_raise(ec->frame);
          }
     }
   else if (ev->message_type == ECORE_X_ATOM_E_WINDOW_PROFILE_CHANGE_DONE)
     {
        if ((ec->e.state.profile.use) &&
            (ec->e.state.profile.wait_for_done))
          {
             char *p = ecore_x_atom_name_get(ev->data.l[1]);
             if (!e_util_strcmp(ec->e.state.profile.wait, p))
               {
                  eina_stringshare_replace(&ec->e.state.profile.wait, NULL);
                  ec->e.state.profile.wait_for_done = 0;
                  E_Desk *desk = ec->e.state.profile.wait_desk;
                  if ((desk) && (desk != ec->desk))
                    {
                       eina_stringshare_replace(&ec->e.state.profile.name,
                                                desk->window_profile);
                       e_client_desk_set(ec, desk);
                    }
                  e_client_desk_window_profile_wait_desk_set(ec, NULL);
               }
             free(p);
          }
     }
   else if (ev->message_type == ATM_NETWM_SHOW_WINDOW_MENU)
     {
       /*
         message_type = _NET_WM_SHOW_WINDOW_MENU
         window = window for which the menu should be shown
         format = 32
         data.l[0] = xinput2_device_id
         data.l[1] = root_x
         data.l[2] = root_y
         other data.l[] elements = 0
        */

        int x, y;

        x = ev->data.l[1];
        y = ev->data.l[2];
        e_int_client_menu_show(ec,
          e_comp_canvas_x_root_adjust(x),
          e_comp_canvas_y_root_adjust(y),
          0, 0);
     }
   else if (ev->message_type == ATM_NETWM_PERFORM_BUTTON_ACTION)
     {
        char emission[128];
       /*
         message_type = _NET_WM_PERFORM_BUTTON_ACTION
         window = window for which the action should be performed
         format = 32
         data.l[0] = xinput2_device_id
         data.l[1] = root_x
         data.l[2] = root_y
         data.l[3] = button
         data.l[4] = timestamp
        */
        if (e_dnd_active() || ec->iconic) return ECORE_CALLBACK_RENEW;
        switch (ev->data.l[3])
          {
           case 1:
           case 2:
           case 3:
             snprintf(emission, sizeof(emission), "mouse,down,%d", (int)ev->data.l[3]);
             e_bindings_signal_handle(E_BINDING_CONTEXT_WINDOW, E_OBJECT(ec), emission, "e.event.titlebar");
             break;
           case 4:
             e_bindings_signal_handle(E_BINDING_CONTEXT_WINDOW, E_OBJECT(ec), "mouse,wheel,?,-1", "e.event.titlebar");
             break;
           case 5:
             e_bindings_signal_handle(E_BINDING_CONTEXT_WINDOW, E_OBJECT(ec), "mouse,wheel,?,1", "e.event.titlebar");
             break;
          }
     }
#ifdef HAVE_WAYLAND
   else if (ev->message_type == WL_SURFACE_ID)
     {
        void *res;
        E_Client *wc = NULL;

        if (e_comp->comp_type != E_PIXMAP_TYPE_WL) return ECORE_CALLBACK_RENEW;
        res = wl_client_get_object(e_comp->wl_comp_data->xwl_client, ev->data.l[0]);
        if (res)
          wc = wl_resource_get_user_data(res);
        if (wc)
          e_comp_x_xwayland_client_setup(ec, wc);
        else
          {
             ec->comp_data->surface_id = ev->data.l[0];
             e_comp_wl_xwayland_client_queue(ec);
          }
     }
#endif
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_x_state_request(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_State_Request *ev)
{
   int i;
   E_Client *ec;

   ec = _e_comp_x_client_find_by_window(ev->win);
   if (!ec) return ECORE_CALLBACK_RENEW;

   for (i = 0; i < 2; i++)
     e_hints_window_state_update(ec, ev->state[i], ev->action);

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_e_comp_x_mapping_change(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Mapping_Change *ev EINA_UNUSED)
{
   const Eina_List *l;
   E_Client *ec;

   if (_e_comp_x_mapping_change_disabled) return ECORE_CALLBACK_RENEW;
   e_comp_canvas_keys_ungrab();
   EINA_LIST_FOREACH(e_comp->clients, l, ec)
     {
        Ecore_X_Window win;

        if (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_X) continue;
        win = e_client_util_win_get(ec);
        if ((!_e_comp_x_client_data_get(ec)->first_map) || (!_e_comp_x_client_data_get(ec)->reparented)) continue;
        if (ec->focused)
          {
             _e_comp_x_focus_setup(ec);
             _e_comp_x_focus_setdown(ec);
          }
        else
          {
             _e_comp_x_focus_setdown(ec);
             _e_comp_x_focus_setup(ec);
             e_bindings_mouse_grab(E_BINDING_CONTEXT_WINDOW, win);
             e_bindings_wheel_grab(E_BINDING_CONTEXT_WINDOW, win);
          }
     }
   e_comp_canvas_keys_grab();
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_x_mouse_in(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Mouse_In *ev)
{
   E_Client *ec;

   ec = _e_comp_x_client_find_by_window(ev->win);
   if (!ec) return ECORE_CALLBACK_RENEW;
   if (_e_comp_x_client_data_get(ec)->deleted) return ECORE_CALLBACK_RENEW;
   mouse_client = ec;
   e_client_mouse_in(ec, e_comp_canvas_x_root_adjust(ev->root.x), e_comp_canvas_x_root_adjust(ev->root.y));
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_e_comp_x_mouse_out(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Mouse_In *ev)
{
   E_Client *ec;

   if ((ev->mode == ECORE_X_EVENT_MODE_UNGRAB) &&
       (ev->detail == ECORE_X_EVENT_DETAIL_INFERIOR))
     return ECORE_CALLBACK_PASS_ON;
   if (ev->mode == ECORE_X_EVENT_MODE_GRAB)
     return ECORE_CALLBACK_PASS_ON;
   if ((ev->mode == ECORE_X_EVENT_MODE_NORMAL) &&
       (ev->detail == ECORE_X_EVENT_DETAIL_INFERIOR))
     return ECORE_CALLBACK_PASS_ON;
   ec = _e_comp_x_client_find_by_window(ev->win);
   if (!ec) return ECORE_CALLBACK_RENEW;
   if (_e_comp_x_client_data_get(ec)->deleted) return ECORE_CALLBACK_RENEW;
   if (mouse_client == ec) mouse_client = NULL;
   e_client_mouse_out(ec, e_comp_canvas_x_root_adjust(ev->root.x), e_comp_canvas_x_root_adjust(ev->root.y));
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_e_comp_x_mouse_wheel(void *d EINA_UNUSED, int t EINA_UNUSED, Ecore_Event_Mouse_Wheel *ev)
{
   E_Client *ec;
   E_Binding_Event_Wheel ev2;

   //if (action_input_win)
     //ec = action_border;
   //else
     {
        ec = _e_comp_x_client_find_by_window(ev->window);
        if (!ec) return ECORE_CALLBACK_RENEW;
     }
   if (_e_comp_x_client_data_get(ec)->deleted) return ECORE_CALLBACK_RENEW;
   e_bindings_ecore_event_mouse_wheel_convert(ev, &ev2);
   e_client_mouse_wheel(ec, (Evas_Point*)&ev->root, &ev2);
   return ECORE_CALLBACK_RENEW;
}


static Eina_Bool
_e_comp_x_mouse_up(void *d EINA_UNUSED, int t EINA_UNUSED, Ecore_Event_Mouse_Button *ev)
{
   E_Client *ec;
   E_Binding_Event_Mouse_Button ev2;

   //if (action_input_win)
     //ec = action_border;
   //else
     {
        ec = _e_comp_x_client_find_by_window(ev->window);
        if ((!ec) && (ev->window != ev->event_window))
          ec = _e_comp_x_client_find_by_window(ev->event_window);
        if (!ec)
          {
             if (e_client_comp_grabbed_get())
               ec = e_client_action_get();
             if (!ec) return ECORE_CALLBACK_RENEW;
          }
        if ((!ec) || e_client_util_ignored_get(ec)) return ECORE_CALLBACK_RENEW;
     }
   if (_e_comp_x_client_data_get(ec)->deleted) return ECORE_CALLBACK_RENEW;
   e_bindings_ecore_event_mouse_button_convert(ev, &ev2);
   e_client_mouse_up(ec, ev->buttons, (Evas_Point*)&ev->root, &ev2);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_e_comp_x_mouse_down(void *d EINA_UNUSED, int t EINA_UNUSED, Ecore_Event_Mouse_Button *ev)
{
   E_Client *ec;
   E_Binding_Event_Mouse_Button ev2;

   if (e_client_action_get()) return ECORE_CALLBACK_RENEW; //block extra mouse buttons during action
   //if (action_input_win)
     //ec = action_border;
   //else
     {
        ec = _e_comp_x_client_find_by_window(ev->window);
        if ((!ec) && (ev->window != ev->event_window))
          ec = _e_comp_x_client_find_by_window(ev->event_window);
        if ((!ec) || e_client_util_ignored_get(ec)) return ECORE_CALLBACK_RENEW;
     }
   if (_e_comp_x_client_data_get(ec)->deleted) return ECORE_CALLBACK_RENEW;
   e_bindings_ecore_event_mouse_button_convert(ev, &ev2);
   e_client_mouse_down(ec, ev->buttons, (Evas_Point*)&ev->root, &ev2);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_e_comp_x_mouse_move(void *d EINA_UNUSED, int t EINA_UNUSED, Ecore_Event_Mouse_Move *ev)
{
   E_Client *ec;

   ec = e_client_action_get();
   if (!ec)
     {
        ec = _e_comp_x_client_find_by_window(ev->window);
        if ((!ec) && (ev->window != ev->event_window))
          ec = _e_comp_x_client_find_by_window(ev->event_window);
        if ((!ec) || e_client_util_ignored_get(ec)) return ECORE_CALLBACK_RENEW;
        if (!ec->mouse.in)
          e_client_mouse_in(ec, e_comp_canvas_x_root_adjust(ev->root.x), e_comp_canvas_x_root_adjust(ev->root.y));
        return ECORE_CALLBACK_RENEW;
     }
   E_COMP_X_PIXMAP_CHECK ECORE_CALLBACK_RENEW;
   if (_e_comp_x_client_data_get(ec)->deleted || e_client_util_ignored_get(ec)) return ECORE_CALLBACK_RENEW;
   if (e_client_util_resizing_get(ec) && (e_comp->comp_type == E_PIXMAP_TYPE_X) &&
       ec->netwm.sync.request &&
       _e_comp_x_client_data_get(ec)->alarm
      )
     {
        if ((ecore_loop_time_get() - ec->netwm.sync.send_time) > 0.5)
          {
             if (ec->pending_resize)
               {
                  ec->changes.pos = 1;
                  ec->changes.size = 1;
                  EC_CHANGED(ec);
                  _e_comp_x_client_move_resize_send(ec);
               }
             E_FREE_LIST(ec->pending_resize, free);

             ec->netwm.sync.wait = 0;
             /* sync.wait is incremented when resize_handle sends
              * sync-request and decremented by sync-alarm cb. so
              * we resize here either on initial resize, timeout or
              * when no new resize-request was added by sync-alarm cb.
              */
          }
     }
   e_client_mouse_move(ec, (Evas_Point*)&ev->root);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_e_comp_x_focus_out(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Focus_Out *ev)
{
   E_Client *ec;

   ec = _e_comp_x_client_find_by_window(ev->win);
   if (!ec) return ECORE_CALLBACK_RENEW;

   _e_comp_x_client_pri_norm(ec);
   if (ev->mode == ECORE_X_EVENT_MODE_NORMAL)
     {
        if (ev->detail == ECORE_X_EVENT_DETAIL_INFERIOR)
          return ECORE_CALLBACK_PASS_ON;
        else if (ev->detail == ECORE_X_EVENT_DETAIL_NON_LINEAR)
          return ECORE_CALLBACK_PASS_ON;
        else if (ev->detail == ECORE_X_EVENT_DETAIL_NON_LINEAR_VIRTUAL)
          return ECORE_CALLBACK_PASS_ON;
     }
   else if (ev->mode == ECORE_X_EVENT_MODE_GRAB)
     {
        if (ev->detail == ECORE_X_EVENT_DETAIL_NON_LINEAR)
          return ECORE_CALLBACK_PASS_ON;
        else if (ev->detail == ECORE_X_EVENT_DETAIL_INFERIOR)
          return ECORE_CALLBACK_PASS_ON;
        else if (ev->detail == ECORE_X_EVENT_DETAIL_NON_LINEAR_VIRTUAL)
          return ECORE_CALLBACK_PASS_ON;
        else if (ev->detail == ECORE_X_EVENT_DETAIL_ANCESTOR)
          return ECORE_CALLBACK_PASS_ON;
        else if (ev->detail == ECORE_X_EVENT_DETAIL_VIRTUAL)
          return ECORE_CALLBACK_PASS_ON;
     }
   else if (ev->mode == ECORE_X_EVENT_MODE_UNGRAB)
     {
        /* for firefox/thunderbird (xul) menu walking */
        /* NB: why did i disable this before? */
        if (ev->detail == ECORE_X_EVENT_DETAIL_INFERIOR)
          return ECORE_CALLBACK_PASS_ON;
        else if (ev->detail == ECORE_X_EVENT_DETAIL_POINTER)
          return ECORE_CALLBACK_PASS_ON;
     }
   else if (ev->mode == ECORE_X_EVENT_MODE_WHILE_GRABBED)
     {
        if (ev->detail == ECORE_X_EVENT_DETAIL_ANCESTOR)
          return ECORE_CALLBACK_PASS_ON;
        else if (ev->detail == ECORE_X_EVENT_DETAIL_INFERIOR)
          return ECORE_CALLBACK_PASS_ON;
     }
   evas_object_focus_set(ec->frame, 0);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_x_client_property(void *d EINA_UNUSED, int t EINA_UNUSED, E_Event_Client_Property *ev)
{
   if ((ev->property & E_CLIENT_PROPERTY_ICON) && ev->ec->desktop)
     ecore_x_window_prop_string_set(e_client_util_win_get(ev->ec), E_ATOM_DESKTOP_FILE,
                                    ev->ec->desktop->orig_path);
   return ECORE_CALLBACK_RENEW;
}

static void
_e_comp_x_client_zone_geometry_set(E_Client *ec)
{
   unsigned int zgeom[4];

   E_COMP_X_PIXMAP_CHECK;
   zgeom[0] = ec->zone->x;
   zgeom[1] = ec->zone->y;
   zgeom[2] = ec->zone->w;
   zgeom[3] = ec->zone->h;
   ecore_x_window_prop_card32_set(e_client_util_win_get(ec), E_ATOM_ZONE_GEOMETRY, zgeom, 4);
}

static Eina_Bool
_e_comp_x_client_zone_set(void *data EINA_UNUSED, int type EINA_UNUSED, E_Event_Client *ev)
{
   E_Client *ec = ev->ec;

   if (e_object_is_del(E_OBJECT(ec))) return ECORE_CALLBACK_RENEW;
   E_COMP_X_PIXMAP_CHECK ECORE_CALLBACK_RENEW;
   ecore_x_window_prop_card32_set(e_client_util_win_get(ev->ec), E_ATOM_ZONE, &ev->ec->zone->num, 1);
   _e_comp_x_client_zone_geometry_set(ev->ec);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_e_comp_x_sync_alarm(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Sync_Alarm *ev)
{
   unsigned int serial;
   E_Client *ec;
   Eina_Bool resize = EINA_FALSE;

   ec = _e_comp_x_client_find_by_alarm(ev->alarm);
   if (!ec) return ECORE_CALLBACK_RENEW;

   if (ec->netwm.sync.wait)
     ec->netwm.sync.wait--;

   if (ecore_x_sync_counter_query(_e_comp_x_client_data_get(ec)->sync_counter, &serial))
     {
        E_Client_Pending_Resize *pnd = NULL;

        /* skip pending for which we didn't get a reply */
        while (ec->pending_resize)
          {
             pnd = ec->pending_resize->data;
             ec->pending_resize = eina_list_remove(ec->pending_resize, pnd);

             if (serial == pnd->serial)
               break;

             E_FREE(pnd);
          }

        if (pnd)
          {
             resize = ((ec->w != pnd->w) || (ec->h != pnd->h));
             e_comp_object_frame_wh_adjust(ec->frame, pnd->w, pnd->h, &ec->w, &ec->h);
             E_FREE(pnd);
          }
     }

   if (resize)
     {
        evas_object_resize(ec->frame, ec->w, ec->h);
        if (ec->internal_elm_win)
          evas_object_resize(ec->internal_elm_win, ec->client.w, ec->client.h);
     }

   ecore_x_pointer_xy_get(e_comp->root,
                          &ec->mouse.current.mx,
                          &ec->mouse.current.my);

   ec->netwm.sync.send_time = ecore_loop_time_get();
   if (resize)
     e_client_mouse_move(ec, &(Evas_Point){ec->mouse.current.mx, ec->mouse.current.my});
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_e_comp_x_desktop_change(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Desktop_Change *ev)
{
   E_Client *ec;

   ec = _e_comp_x_client_find_by_window(ev->win);
   if (ec)
     {
        if (ev->desk == 0xffffffff)
          e_client_stick(ec);
        else if ((int)ev->desk < (ec->zone->desk_x_count * ec->zone->desk_y_count))
          {
             E_Desk *desk;

             desk = e_desk_at_pos_get(ec->zone, ev->desk);
             if (desk)
               e_client_desk_set(ec, desk);
          }
     }
   else
     ecore_x_netwm_desktop_set(ev->win, ev->desk);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_e_comp_x_move_resize_request(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Move_Resize_Request *ev)
{
   E_Client *ec;

   ec = _e_comp_x_client_find_by_window(ev->win);
   if (!ec) return ECORE_CALLBACK_RENEW;

   if ((ec->shaded) || (ec->shading) ||
       (ec->fullscreen) || (ec->moving) ||
       (ec->resize_mode != E_POINTER_RESIZE_NONE))
     return ECORE_CALLBACK_PASS_ON;

   if ((ev->button >= 1) && (ev->button <= 3))
     {
        ec->mouse.last_down[ev->button - 1].mx = ev->x;
        ec->mouse.last_down[ev->button - 1].my = ev->y;
        ec->mouse.last_down[ev->button - 1].x = ec->x;
        ec->mouse.last_down[ev->button - 1].y = ec->y;
        ec->mouse.last_down[ev->button - 1].w = ec->w;
        ec->mouse.last_down[ev->button - 1].h = ec->h;
     }
   else
     {
        ec->moveinfo.down.x = ec->x;
        ec->moveinfo.down.y = ec->y;
        ec->moveinfo.down.w = ec->w;
        ec->moveinfo.down.h = ec->h;
     }
   ec->mouse.current.mx = ev->x;
   ec->mouse.current.my = ev->y;
   ec->moveinfo.down.button = ev->button;
   ec->moveinfo.down.mx = ev->x;
   ec->moveinfo.down.my = ev->y;

   if (!ec->lock_user_stacking)
     evas_object_raise(ec->frame);

   if (ev->direction == E_POINTER_MOVE)
     {
        ec->cur_mouse_action = e_action_find("window_move");
        if (ec->cur_mouse_action)
          {
             if ((!ec->cur_mouse_action->func.end_mouse) &&
                 (!ec->cur_mouse_action->func.end))
               ec->cur_mouse_action = NULL;
             if (ec->cur_mouse_action)
               {
                  e_object_ref(E_OBJECT(ec->cur_mouse_action));
                  ec->cur_mouse_action->func.go(E_OBJECT(ec), NULL);
               }
          }
        return ECORE_CALLBACK_PASS_ON;
     }

   ec->cur_mouse_action = e_action_find("window_resize");
   if (ec->cur_mouse_action)
     {
        if ((!ec->cur_mouse_action->func.end_mouse) &&
            (!ec->cur_mouse_action->func.end))
          ec->cur_mouse_action = NULL;
     }
   if (!ec->cur_mouse_action) return ECORE_CALLBACK_RENEW;
   e_object_ref(E_OBJECT(ec->cur_mouse_action));
   ec->cur_mouse_action->func.go(E_OBJECT(ec), NULL);

   switch (ev->direction)
     {
      case E_POINTER_RESIZE_TL:
        ec->resize_mode = E_POINTER_RESIZE_TL;
        GRAV_SET(ec, ECORE_X_GRAVITY_SE);
        break;

      case E_POINTER_RESIZE_T:
        ec->resize_mode = E_POINTER_RESIZE_T;
        GRAV_SET(ec, ECORE_X_GRAVITY_S);
        break;

      case E_POINTER_RESIZE_TR:
        ec->resize_mode = E_POINTER_RESIZE_TR;
        GRAV_SET(ec, ECORE_X_GRAVITY_SW);
        break;

      case E_POINTER_RESIZE_R:
        ec->resize_mode = E_POINTER_RESIZE_R;
        GRAV_SET(ec, ECORE_X_GRAVITY_W);
        break;

      case E_POINTER_RESIZE_BR:
        ec->resize_mode = E_POINTER_RESIZE_BR;
        GRAV_SET(ec, ECORE_X_GRAVITY_NW);
        break;

      case E_POINTER_RESIZE_B:
        ec->resize_mode = E_POINTER_RESIZE_B;
        GRAV_SET(ec, ECORE_X_GRAVITY_N);
        break;

      case E_POINTER_RESIZE_BL:
        ec->resize_mode = E_POINTER_RESIZE_BL;
        GRAV_SET(ec, ECORE_X_GRAVITY_NE);
        break;

      case E_POINTER_RESIZE_L:
        ec->resize_mode = E_POINTER_RESIZE_L;
        GRAV_SET(ec, ECORE_X_GRAVITY_E);
        break;

      default:
        return ECORE_CALLBACK_PASS_ON;
     }

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_e_comp_x_focus_timer_cb(void *d EINA_UNUSED)
{
   E_Client *focused;

   /* if mouse-based focus policy clients exist for [focused] and [mouse_client],
    * [mouse_client] should have focus here.
    * race conditions in x11 focus setting can result in a scenario such that:
    * -> set focus on A
    * -> set focus on B
    * -> receive focus event on A
    * -> re-set focus on A
    * -> receive focus event on B
    * -> receive focus event on A
    * ...
    * where the end state is that the cursor is over a client which does not have focus.
    * this timer is triggered only when such eventing occurs in order to adjust the focused
    * client as necessary
    */
   focused = e_client_focused_get();
   if (mouse_client && focused && (!e_client_focus_policy_click(focused)) && (mouse_client != focused))
     {
        int x, y;

        ecore_evas_pointer_xy_get(e_comp->ee, &x, &y);
        if (E_INSIDE(x, y, mouse_client->x, mouse_client->y, mouse_client->w, mouse_client->h))
          {
             if (!_e_comp_x_client_data_get(mouse_client)->deleted)
               e_client_mouse_in(mouse_client, x, y);
          }
     }
   focus_timer = NULL;
   return EINA_FALSE;
}

static void
_e_comp_x_focus_timer(void)
{
   if (focus_timer)
     ecore_timer_reset(focus_timer);
   else /* focus has changed twice in .002 seconds; .01 seconds should be more than enough delay */
     focus_timer = ecore_timer_add(0.01, _e_comp_x_focus_timer_cb, NULL);
}

static Eina_Bool
_e_comp_x_focus_in(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Focus_In *ev)
{
   E_Client *ec, *focused;

   ec = _e_comp_x_client_find_by_window(ev->win);
   if (!ec) return ECORE_CALLBACK_RENEW;

   /* block refocus attempts on iconic clients
    * these result from iconifying a client during a grab */
   if (ec->iconic) return ECORE_CALLBACK_RENEW;

   focused = e_client_focused_get();

   _e_comp_x_client_pri_raise(ec);
   if (ev->mode == ECORE_X_EVENT_MODE_GRAB)
     {
        if (ev->detail == ECORE_X_EVENT_DETAIL_POINTER) return ECORE_CALLBACK_PASS_ON;
     }
   else if (ev->mode == ECORE_X_EVENT_MODE_UNGRAB)
     {
        /* this seems to break winlist...
        if (ev->detail == ECORE_X_EVENT_DETAIL_POINTER)
        */

        /* we're getting a non-linear focus re-set on a visible
         * window on current desk when no other client is focused;
         * this is probably when we're trying to reapply focus after
         * unfocusing some comp canvas widget
         */
        if ((ev->detail != ECORE_X_EVENT_DETAIL_NON_LINEAR) ||
            focused ||
            (!e_client_util_desk_visible(ec, e_desk_current_get(ec->zone))))
        return ECORE_CALLBACK_PASS_ON;
     }

   /* ignore focus in from !take_focus windows, we just gave it em */
   /* if (!ec->icccm.take_focus)
    *   return ECORE_CALLBACK_PASS_ON; */

   /* should be equal, maybe some clients don't reply with the proper timestamp ? */
   if (ev->time >= focus_time)
     evas_object_focus_set(ec->frame, 1);
   /* handle case of someone trying to benchmark focus handling */
   if ((!e_client_focus_policy_click(ec)) && (focused && (!e_client_focus_policy_click(focused))) &&
       (ev->time - focus_time <= 2))
     _e_comp_x_focus_timer();
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_x_shape(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Shape *ev)
{
   E_Client *ec;
   //const char *txt[] =
   //{
      //[ECORE_X_SHAPE_BOUNDING] = "BOUNDING",
      //[ECORE_X_SHAPE_INPUT] = "INPUT",
      //[ECORE_X_SHAPE_CLIP] = "CLIP",
   //};

   ec = _e_comp_x_client_find_by_window(ev->win);
   if (!ec) return ECORE_CALLBACK_RENEW;
   //INF("%p(%s): %d,%d %dx%d || %d", ec, txt[ev->type], ev->x, ev->y, ev->w, ev->h, ev->shaped);
   if (ev->win == e_client_util_win_get(ec))
     {
        if (ev->type == ECORE_X_SHAPE_INPUT)
          {
             ec->changes.shape_input = 1;
             ec->need_shape_merge = 1;
          }
        else
          {
             if ((_e_comp_x_client_data_get(ec)->shape.x != ev->x) ||
                 (_e_comp_x_client_data_get(ec)->shape.y != ev->y) ||
                 (_e_comp_x_client_data_get(ec)->shape.w != ev->w) ||
                 (_e_comp_x_client_data_get(ec)->shape.h != ev->h))
               {
                  /* bounding box changed, need export for rendering */
                  EINA_RECTANGLE_SET(&_e_comp_x_client_data_get(ec)->shape, ev->x, ev->y, ev->w, ev->h);
                  ec->need_shape_export = !ec->override;
               }
             ec->changes.shape = 1;
          }
     }
   else if (ec->shaped) //shape change on parent window only valid if window is known to be shaped
     ec->need_shape_export = 1;
   if (ec->changes.shape)
     {
        if (ev->type == ECORE_X_SHAPE_BOUNDING)
          e_comp_object_damage(ec->frame, 0, 0, ec->w, ec->h);
     }
   EC_CHANGED(ec);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_x_damage(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Damage *ev)
{
   E_Client *ec;

   ec = _e_comp_x_client_find_by_damage(ev->damage);
   if ((!ec) || e_object_is_del(E_OBJECT(ec))) return ECORE_CALLBACK_PASS_ON;
   if (_e_comp_x_client_data_get(ec)->damage)
     {
        Ecore_X_Region parts;

        parts = ecore_x_region_new(NULL, 0);
        ecore_x_damage_subtract(_e_comp_x_client_data_get(ec)->damage, 0, parts);
        ecore_x_region_free(parts);
     }
   //WRN("DAMAGE %p: %dx%d", ec, ev->area.width, ev->area.height);

   if (e_comp->nocomp)
     e_pixmap_dirty(ec->pixmap);
   /* discard unwanted xy position of first damage
    * to avoid wrong composition for override redirect window */
   else if ((ec->override) && (!_e_comp_x_client_data_get(ec)->first_damage))
     e_comp_object_damage(ec->frame, 0, 0, ev->area.width, ev->area.height);
   else
     e_comp_object_damage(ec->frame, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
   if ((!ec->re_manage) && (!ec->override) && (!_e_comp_x_client_data_get(ec)->first_damage))
     e_comp_object_render_update_del(ec->frame);
   else
     E_FREE_FUNC(_e_comp_x_client_data_get(ec)->first_draw_delay, ecore_timer_del);
   _e_comp_x_client_data_get(ec)->first_damage = 1;
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_e_comp_x_damage_win(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Damage *ev)
{
   if (e_comp_find_by_window(ev->win)) e_comp_render_queue();
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_x_grab_replay(void *data EINA_UNUSED, int type, void *event)
{
   Ecore_Event_Mouse_Button *ev = event;
   E_Binding_Event_Mouse_Button ev2;
   E_Client *ec;

   if (type != ECORE_EVENT_MOUSE_BUTTON_DOWN) return ECORE_CALLBACK_DONE;
   if ((!e_config->pass_click_on) && 
       (!e_config->always_click_to_raise) && // this works even if not on click-to-focus
       (!e_config->always_click_to_focus) // this works even if not on click-to-focus
       )
     return ECORE_CALLBACK_DONE;
   ec = _e_comp_x_client_find_by_window(ev->event_window);
   if (!ec) return ECORE_CALLBACK_DONE;
   if (ec->cur_mouse_action) return ECORE_CALLBACK_DONE;
   if (ev->event_window != e_client_util_win_get(ec)) return ECORE_CALLBACK_DONE;
   e_bindings_ecore_event_mouse_button_convert(ev, &ev2);
   return !e_bindings_mouse_button_find(E_BINDING_CONTEXT_WINDOW,
                                        &ev2, NULL);
}

static void
_e_comp_x_hook_client_eval_end(void *d EINA_UNUSED, E_Client *ec)
{
   E_COMP_X_PIXMAP_CHECK;
   if (e_comp->x_comp_data->restack && (!e_comp->new_clients))
     {
        e_hints_client_stacking_set();
        e_comp->x_comp_data->restack = 0;
     }
}

static Eina_Bool
_e_comp_x_client_shape_rects_check(E_Client *ec, Ecore_X_Rectangle *rects, int num)
{
   Eina_Bool changed = 1;
   Ecore_X_Rectangle *orects;

   if (((unsigned int)num == ec->shape_rects_num) && (ec->shape_rects))
     {
        int i;

        orects = (Ecore_X_Rectangle*)ec->shape_rects;
        changed = 0;
        for (i = 0; i < num; i++)
          {
             E_RECTS_CLIP_TO_RECT(rects[i].x, rects[i].y,
               rects[i].width, rects[i].height, 0, 0, ec->client.w, ec->client.h);

             if ((orects[i].x != rects[i].x) ||
                 (orects[i].y != rects[i].y) ||
                 (orects[i].width != rects[i].width) ||
                 (orects[i].height != rects[i].height))
               {
                  changed = 1;
                  break;
               }
          }
     }
   if (changed)
     {
        E_FREE(ec->shape_rects);
        ec->shape_rects = (Eina_Rectangle*)rects;
        ec->shape_rects_num = num;
        ec->shape_changed = 1;
        e_comp_shape_queue();
     }
   else
     free(rects);
   return changed;
}

static void
_e_comp_x_hook_client_post_new_client(void *d EINA_UNUSED, E_Client *ec)
{
   E_COMP_X_PIXMAP_CHECK;
   if (ec->changes.reset_gravity)
     {
        GRAV_SET(ec, ECORE_X_GRAVITY_NW);
     }

   if (ec->need_shape_merge)
     {
        _e_comp_x_client_shape_input_rectangle_set(ec);
        if ((!ec->shaped) && _e_comp_x_client_data_get(ec)->reparented)
          ecore_x_window_shape_mask_set(e_client_util_pwin_get(ec), 0);
        ec->need_shape_merge = 0;
     }

   if (ec->changes.internal_state)
     {
        Ecore_X_Atom state[1];
        int num = 0;

        if (e_win_centered_get(ec->internal_elm_win))
          state[num++] = E_ATOM_WINDOW_STATE_CENTERED;

        if (num)
          ecore_x_window_prop_card32_set(elm_win_window_id_get(ec->internal_elm_win), E_ATOM_WINDOW_STATE, state, num);
        else
          ecore_x_window_prop_property_del(elm_win_window_id_get(ec->internal_elm_win), E_ATOM_WINDOW_STATE);
        ec->changes.internal_state = 0;
     }

   if (ec->need_shape_export)
     {
        Ecore_X_Rectangle *rects;
        int num;

        rects = ecore_x_window_shape_rectangles_get(e_client_util_win_get(ec), &num);
        if (rects)
          _e_comp_x_client_shape_rects_check(ec, rects, num);
        else
          {
             E_FREE(ec->shape_rects);
             ec->shape_rects_num = 0;
          }
        ec->need_shape_export = 0;
     }
}

static void
_e_comp_x_hook_client_pre_frame_assign(void *d EINA_UNUSED, E_Client *ec)
{
   Ecore_X_Window win, pwin;
   int w, h;
   E_Pixmap *ep;
   E_Comp_X_Client_Data *cd;

   E_COMP_X_PIXMAP_CHECK;
   ep = e_comp_x_client_pixmap_get(ec);
   win = e_client_util_win_get(ec);
   cd = _e_comp_x_client_data_get(ec);
   if (!cd->need_reparent) return;
   w = MAX(ec->client.w, 1);
   h = MAX(ec->client.h, 1);
   /* match ec parent argbness */
   if (ec->argb)
     pwin = ecore_x_window_manager_argb_new(e_comp->root, ec->client.x, ec->client.y, w, h);
   else
     {
        pwin = ecore_x_window_override_new(e_comp->root, ec->client.x, ec->client.y, w, h);
        ecore_x_window_shape_events_select(pwin, !ec->internal); //let's just agree never to do this with our own windows...
     }

   if (ec->client.w && ec->client.h)
     /* force a resize here (no-op most of the time)
      * prevents mismatch between remembered window size and current size
      */
     ecore_x_window_resize(win, ec->client.w, ec->client.h);
   ecore_x_window_container_manage(pwin);
   if (!ec->internal) ecore_x_window_client_manage(win);
   ecore_x_window_configure(pwin,
                            ECORE_X_WINDOW_CONFIGURE_MASK_SIBLING |
                            ECORE_X_WINDOW_CONFIGURE_MASK_STACK_MODE,
                            0, 0, 0, 0, 0,
                            win, ECORE_X_WINDOW_STACK_ABOVE);
   ecore_x_event_mask_set(pwin, ECORE_X_EVENT_MASK_KEY_DOWN | ECORE_X_EVENT_MASK_KEY_UP |
                                ECORE_X_EVENT_MASK_MOUSE_MOVE | ECORE_X_EVENT_MASK_MOUSE_DOWN |
                                ECORE_X_EVENT_MASK_MOUSE_UP |
                                ECORE_X_EVENT_MASK_MOUSE_IN | ECORE_X_EVENT_MASK_MOUSE_OUT);
   ecore_x_window_border_width_set(win, 0);
   ec->border_size = 0;

   if (ec->e.state.video)
     {
        ecore_x_window_lower(pwin);
        ecore_x_composite_window_events_disable(pwin);
        ecore_x_window_ignore_set(pwin, EINA_TRUE);
     }
   if (!ec->internal)
     ecore_x_window_save_set_add(win);
   ecore_x_window_reparent(win, pwin, 0, 0);

   {
      unsigned int managed = 1;
      ecore_x_window_prop_card32_set(win, E_ATOM_MANAGED, &managed, 1);
   }
   e_pixmap_parent_window_set(ep, pwin);
   ec->border.changed = 1;
   if (!ec->shaped)
     ecore_x_window_shape_mask_set(pwin, 0);
   if (ec->shaped_input)
     ecore_x_window_shape_input_rectangles_set(pwin, (Ecore_X_Rectangle*)ec->shape_input_rects, ec->shape_input_rects_num);
   ec->changes.shape = 1;
   ec->changes.shape_input = 1;
   if (ec->internal_elm_win)
     {
        ecore_x_window_gravity_set(win, ECORE_X_GRAVITY_NW);
        ec->changes.reset_gravity = 1;
     }
   EC_CHANGED(ec);

   eina_hash_add(clients_win_hash, &pwin, ec);

   if (ec->visible)
     {
        if (cd->set_win_type && ec->internal_elm_win)
          {
             if (ec->dialog)
               ecore_x_netwm_window_type_set(win, ECORE_X_WINDOW_TYPE_DIALOG);
             else
               ecore_x_netwm_window_type_set(win, ECORE_X_WINDOW_TYPE_NORMAL);
             cd->set_win_type = 0;
          }
     }
   if (ec->re_manage || ec->visible)
     {
        ecore_x_window_show(win);
        ecore_x_window_show(pwin);
     }

   _e_comp_x_focus_init(ec);
   e_bindings_mouse_grab(E_BINDING_CONTEXT_WINDOW, win);
   e_bindings_wheel_grab(E_BINDING_CONTEXT_WINDOW, win);
   _e_comp_x_client_evas_init(ec);
   if (ec->netwm.ping && (!ec->ping_poller))
     e_client_ping(ec);
   if (ec->visible) evas_object_show(ec->frame);
   cd->need_reparent = 0;
   ec->redirected = 1;
   if (cd->change_icon)
     {
        ec->changes.icon = 1;
        EC_CHANGED(ec);
     }
   cd->change_icon = 0;
   cd->reparented = 1;
   _e_comp_x_evas_comp_hidden_cb(ec, NULL, NULL);
}

static void
_e_comp_x_hook_client_fetch(void *d EINA_UNUSED, E_Client *ec)
{
   Ecore_X_Window win, pwin;
   int rem_change = 0;
   Eina_Bool need_desk_set = EINA_FALSE;
   E_Comp_X_Client_Data *cd;

   E_COMP_X_PIXMAP_CHECK;
   win = e_client_util_win_get(ec);
   pwin = e_client_util_pwin_get(ec);
   cd = _e_comp_x_client_data_get(ec);
   if (ec->changes.visible)
     _e_comp_x_client_shape_input_rectangle_set(ec);
   /* fetch any info queued to be fetched */
   if (ec->changes.prop || ec->netwm.fetch.state)
     {
        e_hints_window_state_get(ec);
        ec->netwm.fetch.state = 0;
        rem_change = 1;
     }
   if (ec->icccm.fetch.client_leader)
     {
        /* TODO: What do to if the client leader isn't mapped yet? */
        E_Client *ec_leader = NULL;

        ec->icccm.client_leader = ecore_x_icccm_client_leader_get(win);
        if (ec->icccm.client_leader)
          ec_leader = _e_comp_x_client_find_by_window(ec->icccm.client_leader);
        if (ec->leader)
          {
             if (ec->leader != ec_leader)
               {
                  ec->leader->group = eina_list_remove(ec->leader->group, ec);
                  if (ec->leader->modal == ec) ec->leader->modal = NULL;
                  ec->leader = NULL;
               }
             else
               ec_leader = NULL;
          }
        /* If this client is the leader of the group, don't register itself */
        if ((ec_leader) && (ec_leader != ec))
          {
             ec_leader->group = eina_list_append(ec_leader->group, ec);
             ec->leader = ec_leader;
             /* Only set the window modal to the leader it there is no parent */
             if ((ec->netwm.state.modal) &&
                 ((!ec->parent) || (ec->parent->modal != ec)))
               {
                  ec->leader->modal = ec;
                  if (ec->leader->focused)
                    evas_object_focus_set(ec->frame, 1);
                  else
                    {
                       Eina_List *l;
                       E_Client *child;

                       EINA_LIST_FOREACH(ec->leader->group, l, child)
                         {
                            if ((child != ec) && (child->focused))
                              evas_object_focus_set(ec->frame, 1);
                         }
                    }
               }
          }
        ec->icccm.fetch.client_leader = 0;
        rem_change = 1;
     }
   if (ec->icccm.fetch.title)
     {
        char *title = ecore_x_icccm_title_get(win);
        eina_stringshare_replace(&ec->icccm.title, title);
        free(title);

        e_comp_object_frame_title_set(ec->frame, ec->icccm.title);

        ec->icccm.fetch.title = 0;
        rem_change = 1;
     }
   if (ec->netwm.fetch.name)
     {
        char *name;
        ecore_x_netwm_name_get(win, &name);
        eina_stringshare_replace(&ec->netwm.name, name);
        free(name);

        ec->hacks.iconic_shading =
          ((ec->netwm.icon_name == ec->netwm.name) &&
           (!e_util_strcmp(ec->netwm.name, "QEMU")));

        e_comp_object_frame_title_set(ec->frame, ec->netwm.name);

        ec->netwm.fetch.name = 0;
        rem_change = 1;
     }

   if (ec->icccm.fetch.name_class)
     {
        const char *pname, *pclass;
        char *nname, *nclass;

        ecore_x_icccm_name_class_get(win, &nname, &nclass);
        pname = ec->icccm.name;
        pclass = ec->icccm.class;
        ec->icccm.name = eina_stringshare_add(nname);
        ec->icccm.class = eina_stringshare_add(nclass);
        ec->hacks.mapping_change =
          ((!e_util_strcasecmp(ec->icccm.class, "vmplayer")) || 
           (!e_util_strcasecmp(ec->icccm.class, "vmware")));
        if (ec->hacks.mapping_change)
          _e_comp_x_mapping_change_disabled++;
        _e_comp_x_mapping_change_disabled = MIN(_e_comp_x_mapping_change_disabled, 0);
        free(nname);
        free(nclass);

        if (!((ec->icccm.name == pname) &&
              (ec->icccm.class == pclass)))
          {
             ec->changes.icon = 1;
             rem_change = 1;
          }

        eina_stringshare_del(pname);
        eina_stringshare_del(pclass);
        ec->icccm.fetch.name_class = 0;
     }
   if (ec->changes.prop || ec->icccm.fetch.state)
     {
        ec->icccm.state = ecore_x_icccm_state_get(win);
        ec->icccm.fetch.state = 0;
        rem_change = 1;
     }
   if (ec->changes.prop || ec->e.fetch.state)
     {
        e_hints_window_e_state_get(ec);
        ec->e.fetch.state = 0;
        rem_change = 1;
     }
   if (ec->e.fetch.profile)
     {
        const char **list = NULL;
        int n, i, res;
        unsigned int use;

        ec->e.state.profile.use = 0;

        if (ec->e.state.profile.name)
          {
             eina_stringshare_del(ec->e.state.profile.name);
             ec->e.state.profile.name = NULL;
          }

        if (ec->e.state.profile.available_list)
          {
             for (i = 0; i < ec->e.state.profile.num; i++)
               {
                  if (ec->e.state.profile.available_list[i])
                    {
                       eina_stringshare_del(ec->e.state.profile.available_list[i]);
                       ec->e.state.profile.available_list[i] = NULL;
                    }
               }
             E_FREE(ec->e.state.profile.available_list);
             ec->e.state.profile.available_list = NULL;
          }
        ec->e.state.profile.num = 0;

        res = ecore_x_window_prop_card32_get(win,
                                             ECORE_X_ATOM_E_WINDOW_PROFILE_SUPPORTED,
                                             &use,
                                             1);
        if ((res == 1) && (use == 1))
          {
             Ecore_X_Atom val;
             res = ecore_x_window_prop_atom_get(win,
                                                ECORE_X_ATOM_E_WINDOW_PROFILE_CHANGE,
                                                &val, 1);
             if (res == 1)
               {
                  char *name = ecore_x_atom_name_get(val);
                  if (name)
                    {
                       ec->e.state.profile.set = eina_stringshare_add(name);
                       free(name);
                    }
               }

             if (ecore_x_e_window_available_profiles_get(win, &list, &n))
               {
                  ec->e.state.profile.available_list = E_NEW(const char *, n);
                  for (i = 0; i < n; i++)
                    ec->e.state.profile.available_list[i] = eina_stringshare_add(list[i]);
                  ec->e.state.profile.num = n;
               }
             need_desk_set = EINA_TRUE;
             ec->e.state.profile.use = 1;
             free(list);
          }

        ec->e.fetch.profile = 0;
     }
   if (ec->changes.prop || ec->netwm.fetch.type)
     {
        unsigned int type = ec->netwm.type;
        e_hints_window_type_get(ec);
        if (((!ec->lock_border) || (!ec->border.name) || (type != ec->netwm.type)) && (cd->reparented || ec->internal))
          {
             ec->border.changed = 1;
             EC_CHANGED(ec);
          }

        if ((ec->netwm.type == E_WINDOW_TYPE_DOCK) || ec->tooltip)
          {
             if (!ec->netwm.state.skip_pager)
               {
                  ec->netwm.state.skip_pager = 1;
                  ec->netwm.update.state = 1;
               }
             if (!ec->netwm.state.skip_taskbar)
               {
                  ec->netwm.state.skip_taskbar = 1;
                  ec->netwm.update.state = 1;
               }
          }
        else if (ec->netwm.type == E_WINDOW_TYPE_DESKTOP)
          {
             ec->focus_policy_override = E_FOCUS_CLICK;
             _e_comp_x_focus_setdown(ec);
             _e_comp_x_focus_setup(ec);
             e_bindings_mouse_grab(E_BINDING_CONTEXT_WINDOW, win);
             e_bindings_wheel_grab(E_BINDING_CONTEXT_WINDOW, win);
             if (!ec->netwm.state.skip_pager)
               {
                  ec->netwm.state.skip_pager = 1;
                  ec->netwm.update.state = 1;
               }
             if (!ec->netwm.state.skip_taskbar)
               {
                  ec->netwm.state.skip_taskbar = 1;
                  ec->netwm.update.state = 1;
               }
             if (!e_client_util_ignored_get(ec))
               ec->border.changed = ec->borderless = 1;
          }
        if (ec->tooltip)
          {
             ec->icccm.accepts_focus = 0;
             eina_stringshare_replace(&ec->bordername, "borderless");
          }
        {
           E_Event_Client_Property *ev;

           ev = E_NEW(E_Event_Client_Property, 1);
           ev->ec = ec;
           REFD(ec, 1);
           e_object_ref(E_OBJECT(ec));
           ev->property = E_CLIENT_PROPERTY_NETWM_STATE;
           ecore_event_add(E_EVENT_CLIENT_PROPERTY, ev, _e_comp_x_client_event_free, NULL);
        }
        ec->netwm.fetch.type = 0;
     }
   if (ec->icccm.fetch.machine)
     {
        char *machine = ecore_x_icccm_client_machine_get(win);

        if ((!machine) && (ec->icccm.client_leader))
          machine = ecore_x_icccm_client_machine_get(ec->icccm.client_leader);

        eina_stringshare_replace(&ec->icccm.machine, machine);
        free(machine);

        ec->icccm.fetch.machine = 0;
        rem_change = 1;
     }
   if (ec->icccm.fetch.command)
     {
        if ((ec->icccm.command.argc > 0) && (ec->icccm.command.argv))
          {
             int i;

             for (i = 0; i < ec->icccm.command.argc; i++)
               free(ec->icccm.command.argv[i]);
             free(ec->icccm.command.argv);
          }
        ec->icccm.command.argc = 0;
        ec->icccm.command.argv = NULL;
        ecore_x_icccm_command_get(win,
                                  &(ec->icccm.command.argc),
                                  &(ec->icccm.command.argv));
        if ((ec->icccm.client_leader) &&
            (!ec->icccm.command.argv))
          ecore_x_icccm_command_get(ec->icccm.client_leader,
                                    &(ec->icccm.command.argc),
                                    &(ec->icccm.command.argv));
        ec->icccm.fetch.command = 0;
        rem_change = 1;
     }
   if (ec->changes.prop || ec->icccm.fetch.hints)
     {
        Eina_Bool accepts_focus, is_urgent;

        accepts_focus = EINA_TRUE;
        is_urgent = EINA_FALSE;
        ec->icccm.initial_state = ECORE_X_WINDOW_STATE_HINT_NORMAL;
        if (ecore_x_icccm_hints_get(win,
                                    &accepts_focus,
                                    &ec->icccm.initial_state,
                                    &ec->icccm.icon_pixmap,
                                    &ec->icccm.icon_mask,
                                    (Ecore_X_Window*)&ec->icccm.icon_window,
                                    (Ecore_X_Window*)&ec->icccm.window_group,
                                    &is_urgent))
          {
             ec->icccm.accepts_focus = accepts_focus;
             ec->icccm.urgent = is_urgent;
             e_client_urgent_set(ec, is_urgent);

             /* If this is a new window, set the state as requested. */
             if ((ec->new_client) &&
                 (ec->icccm.initial_state == ECORE_X_WINDOW_STATE_HINT_ICONIC))
               e_client_iconify(ec);
          }
        ec->icccm.fetch.hints = 0;
        rem_change = 1;
     }
   if (ec->changes.prop || ec->icccm.fetch.size_pos_hints)
     {
        Eina_Bool request_pos;

        request_pos = EINA_FALSE;
        if (ecore_x_icccm_size_pos_hints_get(win,
                                             &request_pos,
                                             &ec->icccm.gravity,
                                             &ec->icccm.min_w,
                                             &ec->icccm.min_h,
                                             &ec->icccm.max_w,
                                             &ec->icccm.max_h,
                                             &ec->icccm.base_w,
                                             &ec->icccm.base_h,
                                             &ec->icccm.step_w,
                                             &ec->icccm.step_h,
                                             &ec->icccm.min_aspect,
                                             &ec->icccm.max_aspect))
          {
             ec->icccm.request_pos = request_pos;
             if (request_pos && (!ec->placed) && (!ec->re_manage))
               {
                  Ecore_X_Window_Attributes *att;
                  int bw;
                  int l, r, t, b;
                  int zx = 0, zy = 0, zw = 0, zh = 0;

                  if (ec->zone)
                    e_zone_useful_geometry_get(ec->zone, &zx, &zy, &zw, &zh);
                  e_comp_object_frame_geometry_get(ec->frame, &l, &r, &t, &b);
                  att = &cd->initial_attributes;
                  bw = att->border * 2;
                  switch (ec->icccm.gravity)
                    {
                     case ECORE_X_GRAVITY_N:
                       ec->x = (att->x - (bw / 2)) - (l / 2);
                       ec->y = att->y;
                       break;

                     case ECORE_X_GRAVITY_NE:
                       ec->x = (att->x - (bw)) - (l);
                       ec->y = att->y;
                       break;

                     case ECORE_X_GRAVITY_E:
                       ec->x = (att->x - (bw)) - (l);
                       ec->y = (att->y - (bw / 2)) - (t / 2);
                       break;

                     case ECORE_X_GRAVITY_SE:
                       ec->x = (att->x - (bw)) - (l);
                       ec->y = (att->y - (bw)) - (t);
                       break;

                     case ECORE_X_GRAVITY_S:
                       ec->x = (att->x - (bw / 2)) - (l / 2);
                       ec->y = (att->y - (bw)) - (t);
                       break;

                     case ECORE_X_GRAVITY_SW:
                       ec->x = att->x;
                       ec->y = (att->y - (bw)) - (t);
                       break;

                     case ECORE_X_GRAVITY_W:
                       ec->x = att->x;
                       ec->y = (att->y - (bw)) - (t);
                       break;

                     case ECORE_X_GRAVITY_CENTER:
                       ec->x = (att->x - (bw / 2)) - (l / 2);
                       ec->y = (att->y - (bw / 2)) - (t / 2);
                       break;

                     case ECORE_X_GRAVITY_NW:
                     default:
                       ec->x = att->x;
                       ec->y = att->y;
                    }

                  /*
                   * This ensures that windows that like to open with a x/y
                   * position smaller than returned by e_zone_useful_geometry_get()
                   * are moved to useful positions.
                   */
                  // ->
                  if (e_config->geometry_auto_move)
                    {
                       if (ec->x < zx)
                         ec->x = zx;

                       if (ec->y < zy)
                         ec->y = zy;

                       /* ensure we account for windows which already have client_inset;
                        * fixes lots of wine placement issues
                        */
                       if (ec->x - l >= zx)
                         ec->x -= l;
                       if (ec->y - t >= zy)
                         ec->y -= t;

                       if (ec->x + ec->w > zx + zw)
                         ec->x = zx + zw - ec->w;

                       if (ec->y + ec->h > zy + zh)
                         ec->y = zy + zh - ec->h;

                       // <--
                       if (e_comp_zone_xy_get(ec->x, ec->y))
                         {
                            if (!E_INSIDE(ec->x, ec->y, ec->zone->x, ec->zone->y, ec->zone->w, ec->zone->h))
                              {
                                 ec->x = E_CLAMP(ec->x, ec->zone->x, ec->zone->x + ec->zone->w);
                                 ec->y = E_CLAMP(ec->y, ec->zone->y, ec->zone->y + ec->zone->h);
                              }
                            /* some application failing to correctly center a window */
                            if (eina_list_count(e_comp->zones) > 1)
                              {
                                 if (abs((e_comp->w / 2) - ec->x - (ec->w / 2)) < 3)
                                   ec->x = ((ec->zone->x + ec->zone->w) / 2) - (ec->w / 2);
                                 if (abs((e_comp->h / 2) - ec->y - (ec->h / 2)) < 3)
                                   ec->y = ((ec->zone->y + ec->zone->h) / 2) - (ec->h / 2);
                              }
                            ec->changes.pos = 1;
                            ec->placed = 1;
                         }
                    }
                  else
                    {
                       ec->changes.pos = 1;
                       ec->placed = 1;
                    }
               }
          }
        else
          {
          }
        if (ec->icccm.min_w > 32767) ec->icccm.min_w = 32767;
        if (ec->icccm.min_h > 32767) ec->icccm.min_h = 32767;
        if (ec->icccm.max_w > 32767) ec->icccm.max_w = 32767;
        if (ec->icccm.max_h > 32767) ec->icccm.max_h = 32767;
        if (ec->icccm.base_w > 32767) ec->icccm.base_w = 32767;
        if (ec->icccm.base_h > 32767) ec->icccm.base_h = 32767;
        //	if (ec->icccm.step_w < 1) ec->icccm.step_w = 1;
        //	if (ec->icccm.step_h < 1) ec->icccm.step_h = 1;
        // if doing a resize, fix it up
        if (e_client_util_resizing_get(ec))
          {
             int x, y, w, h, new_w, new_h;

             x = ec->x;
             y = ec->y;
             w = ec->w;
             h = ec->h;
             new_w = w;
             new_h = h;
             e_client_resize_limit(ec, &new_w, &new_h);
             if ((ec->resize_mode == E_POINTER_RESIZE_TL) ||
                 (ec->resize_mode == E_POINTER_RESIZE_L) ||
                 (ec->resize_mode == E_POINTER_RESIZE_BL))
               x += (w - new_w);
             if ((ec->resize_mode == E_POINTER_RESIZE_TL) ||
                 (ec->resize_mode == E_POINTER_RESIZE_T) ||
                 (ec->resize_mode == E_POINTER_RESIZE_TR))
               y += (h - new_h);
             evas_object_geometry_set(ec->frame, x, y, new_w, new_h);
          }
        ec->icccm.fetch.size_pos_hints = 0;
        rem_change = 1;
     }
   if (ec->icccm.fetch.protocol)
     {
        int i, num;
        Ecore_X_WM_Protocol *proto;

        proto = ecore_x_window_prop_protocol_list_get(win, &num);
        if (proto)
          {
             for (i = 0; i < num; i++)
               {
                  if (proto[i] == ECORE_X_WM_PROTOCOL_DELETE_REQUEST)
                    ec->icccm.delete_request = 1;
                  else if (proto[i] == ECORE_X_WM_PROTOCOL_TAKE_FOCUS)
                    ec->icccm.take_focus = 1;
                  else if (proto[i] == ECORE_X_NET_WM_PROTOCOL_PING)
                    ec->netwm.ping = 1;
                  else if (proto[i] == ECORE_X_NET_WM_PROTOCOL_SYNC_REQUEST)
                    {
                       ec->netwm.sync.request = 1;
                       if (!ecore_x_netwm_sync_counter_get(win,
                                                           &cd->sync_counter))
                         ec->netwm.sync.request = 0;
                    }
               }
             free(proto);
          }
        if (ec->netwm.ping && cd->evas_init)
          e_client_ping(ec);
        else
          E_FREE_FUNC(ec->ping_poller, ecore_poller_del);
        ec->icccm.fetch.protocol = 0;
     }
   if (ec->icccm.fetch.transient_for)
     {
        /* TODO: What do to if the transient for isn't mapped yet? */
        E_Client *ec_parent = NULL;

        ec->icccm.transient_for = ecore_x_icccm_transient_for_get(win);
        if (ec->icccm.transient_for)
          ec_parent = _e_comp_x_client_find_by_window(ec->icccm.transient_for);
        /* If we already have a parent, remove it */
        if (ec->parent)
          {
             if (ec_parent != ec->parent)
               {
                  ec->parent->transients = eina_list_remove(ec->parent->transients, ec);
                  if (ec->parent->modal == ec) ec->parent->modal = NULL;
                  ec->parent = NULL;
               }
             else
               ec_parent = NULL;
          }
        if ((ec_parent) && (ec_parent != ec) &&
            (eina_list_data_find(ec->transients, ec_parent) != ec_parent))
          {
             ec_parent->transients = eina_list_append(ec_parent->transients, ec);
             ec->parent = ec_parent;
          }
        if (ec->parent && (!e_client_util_ignored_get(ec)))
          {
             evas_object_layer_set(ec->frame, ec->parent->layer);
             if (ec->netwm.state.modal)
               {
                  E_Comp_X_Client_Data *pcd;

                  ec->parent->modal = ec;
                  ec->parent->lock_close = 1;
                  pcd = _e_comp_x_client_data_get(ec->parent);
                  if (!pcd->lock_win)
                    {
                       eina_hash_add(clients_win_hash, &pcd->lock_win, ec->parent);
                       pcd->lock_win = ecore_x_window_input_new(e_client_util_pwin_get(ec->parent), 0, 0, ec->parent->w, ec->parent->h);
                       e_comp_ignore_win_add(E_PIXMAP_TYPE_X, pcd->lock_win);
                       ecore_x_window_show(pcd->lock_win);
                       ecore_x_icccm_name_class_set(pcd->lock_win, "comp_data->lock_win", "comp_data->lock_win");
                    }
               }

             if (e_config->focus_setting == E_FOCUS_NEW_DIALOG ||
                 (ec->parent->focused && (e_config->focus_setting == E_FOCUS_NEW_DIALOG_IF_OWNER_FOCUSED)))
               ec->take_focus = 1;
          }
        ec->icccm.fetch.transient_for = 0;
        rem_change = 1;
     }
   if (ec->icccm.fetch.window_role)
     {
        char *role = ecore_x_icccm_window_role_get(win);
        eina_stringshare_replace(&ec->icccm.window_role, role);
        free(role);

        ec->icccm.fetch.window_role = 0;
        rem_change = 1;
     }
   if (ec->icccm.fetch.icon_name)
     {
        char *icon_name = ecore_x_icccm_icon_name_get(win);
        eina_stringshare_replace(&ec->icccm.icon_name, icon_name);
        free(icon_name);

        ec->hacks.iconic_shading =
          ((ec->netwm.icon_name == ec->netwm.name) &&
           (!e_util_strcmp(ec->netwm.icon_name, "QEMU")));

        ec->icccm.fetch.icon_name = 0;
        rem_change = 1;
     }
   if (ec->netwm.fetch.icon_name)
     {
        char *icon_name;
        ecore_x_netwm_icon_name_get(win, &icon_name);
        eina_stringshare_replace(&ec->netwm.icon_name, icon_name);
        free(icon_name);

        ec->netwm.fetch.icon_name = 0;
        rem_change = 1;
     }
   if (ec->netwm.fetch.opacity)
     {
        unsigned int val;

        if (ecore_x_window_prop_card32_get(win, ECORE_X_ATOM_NET_WM_WINDOW_OPACITY, &val, 1) > 0)
          {
             val = (val >> 24);
             if (ec->netwm.opacity != val)
               {
                  ec->netwm.opacity = val;
                  rem_change = 1;
               }
          }
     }
   if (ec->netwm.fetch.icon)
     {
        int i;
        if (ec->netwm.icons)
          {
             for (i = 0; i < ec->netwm.num_icons; i++)
               {
                  free(ec->netwm.icons[i].data);
                  ec->netwm.icons[i].data = NULL;
               }
             free(ec->netwm.icons);
          }
        ec->netwm.icons = NULL;
        ec->netwm.num_icons = 0;
        if (ecore_x_netwm_icons_get(win,
                                    &ec->netwm.icons,
                                    &ec->netwm.num_icons))
          {
             // unless the rest of e uses border icons OTHER than icon #0
             // then free the rest that we don't need anymore.
             for (i = 1; i < ec->netwm.num_icons; i++)
               {
                  free(ec->netwm.icons[i].data);
                  ec->netwm.icons[i].data = NULL;
               }
             ec->netwm.num_icons = 1;
             ec->changes.icon = 1;
          }
        ec->netwm.fetch.icon = 0;
     }
   if (ec->netwm.fetch.user_time)
     {
        ecore_x_netwm_user_time_get(win, &ec->netwm.user_time);
        ec->netwm.fetch.user_time = 0;
     }
   if (ec->netwm.fetch.strut)
     {
        if (!ecore_x_netwm_strut_partial_get(win,
                                             &ec->netwm.strut.left,
                                             &ec->netwm.strut.right,
                                             &ec->netwm.strut.top,
                                             &ec->netwm.strut.bottom,
                                             &ec->netwm.strut.left_start_y,
                                             &ec->netwm.strut.left_end_y,
                                             &ec->netwm.strut.right_start_y,
                                             &ec->netwm.strut.right_end_y,
                                             &ec->netwm.strut.top_start_x,
                                             &ec->netwm.strut.top_end_x,
                                             &ec->netwm.strut.bottom_start_x,
                                             &ec->netwm.strut.bottom_end_x))
          {
             ecore_x_netwm_strut_get(win,
                                     &ec->netwm.strut.left, &ec->netwm.strut.right,
                                     &ec->netwm.strut.top, &ec->netwm.strut.bottom);

             ec->netwm.strut.left_start_y = 0;
             ec->netwm.strut.left_end_y = 0;
             ec->netwm.strut.right_start_y = 0;
             ec->netwm.strut.right_end_y = 0;
             ec->netwm.strut.top_start_x = 0;
             ec->netwm.strut.top_end_x = 0;
             ec->netwm.strut.bottom_start_x = 0;
             ec->netwm.strut.bottom_end_x = 0;
          }
        ec->netwm.fetch.strut = 0;
     }
   if (ec->qtopia.fetch.soft_menu)
     {
        e_hints_window_qtopia_soft_menu_get(ec);
        ec->qtopia.fetch.soft_menu = 0;
        rem_change = 1;
     }
   if (ec->qtopia.fetch.soft_menus)
     {
        e_hints_window_qtopia_soft_menus_get(ec);
        ec->qtopia.fetch.soft_menus = 0;
        rem_change = 1;
     }
   if (ec->vkbd.fetch.state)
     {
        e_hints_window_virtual_keyboard_state_get(ec);
        ec->vkbd.fetch.state = 0;
        rem_change = 1;
     }
   if (ec->vkbd.fetch.vkbd)
     {
        e_hints_window_virtual_keyboard_get(ec);
        ec->vkbd.fetch.vkbd = 0;
        rem_change = 1;
     }
   if (cd->illume.conformant.fetch.conformant)
     {
        cd->illume.conformant.conformant =
          ecore_x_e_illume_conformant_get(win);
        cd->illume.conformant.fetch.conformant = 0;
     }
   if (cd->illume.quickpanel.fetch.state)
     {
        cd->illume.quickpanel.state =
          ecore_x_e_illume_quickpanel_state_get(win);
        cd->illume.quickpanel.fetch.state = 0;
     }
   if (cd->illume.quickpanel.fetch.quickpanel)
     {
        cd->illume.quickpanel.quickpanel =
          ecore_x_e_illume_quickpanel_get(win);
        cd->illume.quickpanel.fetch.quickpanel = 0;
     }
   if (cd->illume.quickpanel.fetch.priority.major)
     {
        cd->illume.quickpanel.priority.major =
          ecore_x_e_illume_quickpanel_priority_major_get(win);
        cd->illume.quickpanel.fetch.priority.major = 0;
     }
   if (cd->illume.quickpanel.fetch.priority.minor)
     {
        cd->illume.quickpanel.priority.minor =
          ecore_x_e_illume_quickpanel_priority_minor_get(win);
        cd->illume.quickpanel.fetch.priority.minor = 0;
     }
   if (cd->illume.quickpanel.fetch.zone)
     {
        cd->illume.quickpanel.zone =
          ecore_x_e_illume_quickpanel_zone_get(win);
        cd->illume.quickpanel.fetch.zone = 0;
     }
   if (cd->illume.drag.fetch.drag)
     {
        cd->illume.drag.drag =
          ecore_x_e_illume_drag_get(win);
        cd->illume.drag.fetch.drag = 0;
     }
   if (cd->illume.drag.fetch.locked)
     {
        cd->illume.drag.locked =
          ecore_x_e_illume_drag_locked_get(win);
        cd->illume.drag.fetch.locked = 0;
     }
   if (cd->illume.win_state.fetch.state)
     {
        cd->illume.win_state.state =
          ecore_x_e_illume_window_state_get(win);
        cd->illume.win_state.fetch.state = 0;
     }
   if (ec->changes.shape)
     {
        Ecore_X_Rectangle *rects;
        int num;
        Eina_Bool pshaped = ec->shaped;

        ec->changes.shape = 0;
        rects = ecore_x_window_shape_rectangles_get(win, &num);
        if (rects && num)
          {
             int cw = 0, ch = 0;

             /* This doesn't fix the race, but makes it smaller. we detect
              * this and if cw and ch != client w/h then mark this as needing
              * a shape change again to fixup next event loop.
              */
             ecore_x_window_size_get(win, &cw, &ch);
             /* normalize for client window dead zone */
             if (ec->border_size && (num == 1))
               {
                  rects[0].x += ec->border_size;
                  rects[0].y += ec->border_size;
                  rects[0].width -= ec->border_size;
                  rects[0].height -= ec->border_size;
               }
             if ((cw != ec->client.w) || (ch != ec->client.h))
               {
                  ec->changes.shape = 1;
                  EC_CHANGED(ec);
               }
             if ((num == 1) &&
                 (rects[0].x == 0) &&
                 (rects[0].y == 0) &&
                 ((int)rects[0].width == cw) &&
                 ((int)rects[0].height == ch))
               {
                  if (ec->shaped)
                    {
                       ec->shaped = 0;
                       if (cd->reparented && (!ec->bordername))
                         {
                            ec->border.changed = 1;
                            EC_CHANGED(ec);
                         }
                    }
               }
             else
               {
                  if (cd->reparented)
                    {
                       ecore_x_window_shape_rectangles_set(pwin, rects, num);
                       if ((!ec->shaped) && (!ec->bordername))
                         {
                            ec->border.changed = 1;
                            EC_CHANGED(ec);
                         }
                    }
                  else
                    {
                       if (_e_comp_x_client_shape_rects_check(ec, rects, num))
                         e_comp_object_damage(ec->frame, 0, 0, ec->w, ec->h);
                    }
                  ec->shaped = 1;
                  ec->changes.shape_input = 0;
                  E_FREE(ec->shape_input_rects);
                  ec->shape_input_rects_num = 0;
               }
             if (cd->reparented || (!ec->shaped))
               free(rects);
             if (ec->shape_changed)
               e_comp_object_frame_theme_set(ec->frame, E_COMP_OBJECT_FRAME_RESHADOW);
             evas_object_pass_events_set(ec->frame, 0);
          }
        else
          {
             ec->shaped = 1;
             E_FREE(ec->shape_rects);
             E_FREE(ec->shape_input_rects);
             ec->shape_input_rects_num = 0;
             e_comp_object_frame_theme_set(ec->frame, E_COMP_OBJECT_FRAME_RESHADOW);
             if (ec->shaped && cd->reparented && (!ec->bordername))
               {
                  ec->border.changed = 1;
                  EC_CHANGED(ec);
               }
             evas_object_pass_events_set(ec->frame, 1);
          }
        if (ec->shaped != pshaped)
          {
             _e_comp_x_client_shape_input_rectangle_set(ec);
             if ((!ec->shaped) && cd->reparented)
               ecore_x_window_shape_mask_set(pwin, 0);
          }
        ec->need_shape_merge = 1;
     }
   if (ec->changes.shape_input)
     {
        Ecore_X_Rectangle *rects;
        Eina_Bool pshaped = ec->shaped_input, changed = EINA_FALSE;
        int num;

        ec->changes.shape_input = 0;
        rects = ecore_x_window_shape_input_rectangles_get(win, &num);
        if (rects && num)
          {
             int cw = 0, ch = 0;

             /* This doesn't fix the race, but makes it smaller. we detect
              * this and if cw and ch != client w/h then mark this as needing
              * a shape change again to fixup next event loop.
              */
             ecore_x_window_size_get(win, &cw, &ch);
             /* normalize for client window dead zone */
             if (ec->border_size && (num == 1))
               {
                  rects[0].x += ec->border_size;
                  rects[0].y += ec->border_size;
                  rects[0].width -= ec->border_size;
                  rects[0].height -= ec->border_size;
               }
             if ((cw != ec->client.w) || (ch != ec->client.h))
               ec->changes.shape_input = 1;
             if (num == 1)
               E_RECTS_CLIP_TO_RECT(rects[0].x, rects[0].y, rects[0].width, rects[0].height, 0, 0, cw, ch);
             if ((num == 1) &&
                 (rects[0].x == 0) &&
                 (rects[0].y == 0) &&
                 ((int)rects[0].width == cw) &&
                 ((int)rects[0].height == ch))
               {
                  ec->shaped_input = 0;
                  free(rects);
               }
             else
               {
                  ec->shaped_input = 1;
                  if (cd->reparented)
                    ecore_x_window_shape_input_rectangles_set(pwin, rects, num);
                  changed = EINA_TRUE;
                  free(ec->shape_input_rects);
                  ec->shape_input_rects = (Eina_Rectangle*)rects;
                  ec->shape_input_rects_num = num;
               }
             evas_object_pass_events_set(ec->frame, 0);
          }
        else
          {
             ec->shaped_input = 1;
             if (cd->reparented)
               ecore_x_window_shape_input_rectangles_set(pwin, rects, num);
             changed = EINA_TRUE;
             evas_object_pass_events_set(ec->frame, 1);
             free(ec->shape_input_rects);
             ec->shape_input_rects = NULL;
             ec->shape_input_rects_num = 0;
          }
        if (changed || (pshaped != ec->shaped_input))
          {
             ec->need_shape_merge = 1;
             e_comp_shape_queue();
          }
     }
   if (ec->changes.prop || ec->mwm.fetch.hints)
     {
        int pb;

        ec->mwm.exists =
          ecore_x_mwm_hints_get(win,
                                &ec->mwm.func,
                                &ec->mwm.decor,
                                &ec->mwm.input);
        pb = ec->mwm.borderless;
        ec->mwm.borderless = 0;
        if (ec->mwm.exists)
          {
             if ((!(ec->mwm.decor & ECORE_X_MWM_HINT_DECOR_ALL)) &&
                 (!(ec->mwm.decor & ECORE_X_MWM_HINT_DECOR_TITLE)) &&
                 (!(ec->mwm.decor & ECORE_X_MWM_HINT_DECOR_BORDER)))
               ec->mwm.borderless = 1;
          }
        if (ec->mwm.borderless != pb)
          {
             if ((ec->internal || cd->reparented) && ((!ec->lock_border) || (!ec->border.name)))
               {
                  ec->border.changed = 1;
                  EC_CHANGED(ec);
               }
          }
        ec->mwm.fetch.hints = 0;
        rem_change = 1;
     }
   if (ec->e.fetch.video_parent)
     {
        /* unlinking child/parent */
        if (ec->e.state.video_parent_client != NULL)
          {
             ec->e.state.video_parent_client->e.state.video_child =
               eina_list_remove(ec->e.state.video_parent_client->e.state.video_child, ec);
          }

        ecore_x_window_prop_card32_get(win,
                                       ECORE_X_ATOM_E_VIDEO_PARENT,
                                       (Ecore_X_Window*)&ec->e.state.video_parent, 1);

        /* linking child/parent */
        if (ec->e.state.video_parent != 0)
          {
             E_Client *tmp;

             tmp = _e_comp_x_client_find_by_window(ec->e.state.video_parent);
             if (tmp)
               {
                  /* fprintf(stderr, "child added to parent \\o/\n"); */
                  ec->e.state.video_parent_client = tmp;
                  tmp->e.state.video_child = eina_list_append(tmp->e.state.video_child, ec);
                  if (ec->desk != tmp->desk)
                    e_client_desk_set(ec, tmp->desk);
               }
          }

        /* fprintf(stderr, "new parent %x => %p\n", ec->e.state.video_parent, ec->e.state.video_parent_client); */

        if (ec->e.state.video_parent_client) ec->e.fetch.video_parent = 0;
        rem_change = 1;
     }
   if (ec->e.fetch.video_position && ec->e.fetch.video_parent == 0)
     {
        unsigned int xy[2];

        ecore_x_window_prop_card32_get(win,
                                       ECORE_X_ATOM_E_VIDEO_POSITION,
                                       xy, 2);
        ec->e.state.video_position.x = xy[0];
        ec->e.state.video_position.y = xy[1];
        ec->e.state.video_position.updated = 1;
        ec->e.fetch.video_position = 0;
        ec->x = ec->e.state.video_position.x;
        ec->y = ec->e.state.video_position.y;

        fprintf(stderr, "internal position has been updated [%i, %i]\n", ec->e.state.video_position.x, ec->e.state.video_position.y);
     }
   if (ec->changes.prop || ec->netwm.update.state)
     {
        e_hints_window_state_set(ec);
        /* Some stats might change the border, like modal */
        if (((!ec->lock_border) || (!ec->border.name)) &&
            (!(((ec->maximized & E_MAXIMIZE_TYPE) == E_MAXIMIZE_FULLSCREEN))) &&
            (ec->internal || cd->reparented))
          {
             ec->border.changed = 1;
             EC_CHANGED(ec);
          }
        if (ec->parent)
          {
             if (ec->netwm.state.modal)
               {
                  ec->parent->modal = ec;
                  if (ec->parent->focused)
                    evas_object_focus_set(ec->frame, 1);
               }
          }
        else if (ec->leader)
          {
             if (ec->netwm.state.modal)
               {
                  ec->leader->modal = ec;
                  if (ec->leader->focused)
                    evas_object_focus_set(ec->frame, 1);
                  else
                    {
                       Eina_List *l;
                       E_Client *child;

                       EINA_LIST_FOREACH(ec->leader->group, l, child)
                         {
                            if ((child != ec) && (child->focused))
                              evas_object_focus_set(ec->frame, 1);
                         }
                    }
               }
          }
        ec->netwm.update.state = 0;
     }

   if (cd->fetch_exe)
     {
        E_Exec_Instance *inst;

        if (((!ec->lock_border) || (!ec->border.name)) && 
            (ec->internal || cd->reparented))
          {
             ec->border.changed = 1;
             EC_CHANGED(ec);
          }

        {
           char *str = NULL;

           if ((!ec->internal) &&
               ((ecore_x_netwm_startup_id_get(win, &str) && (str)) ||
               ((ec->icccm.client_leader > 0) &&
                ecore_x_netwm_startup_id_get(ec->icccm.client_leader, &str) && (str)))
               )
             {
                if (!strncmp(str, "E_START|", 8))
                  {
                     int id;

                     id = atoi(str + 8);
                     if (id > 0) ec->netwm.startup_id = id;
                  }
                free(str);
             }
        }
        /* It's ok not to have fetch flag, should only be set on startup
         * and not changed. */
        if (!ecore_x_netwm_pid_get(win, &ec->netwm.pid))
          {
             if (ec->icccm.client_leader)
               {
                  if (!ecore_x_netwm_pid_get(ec->icccm.client_leader, &ec->netwm.pid))
                    ec->netwm.pid = -1;
               }
             else
               ec->netwm.pid = -1;
          }

        if (!e_client_util_ignored_get(ec))
          {
             inst = e_exec_startup_id_pid_instance_find(ec->netwm.startup_id,
                                                        ec->netwm.pid);
             if (inst)
               {
                  Eina_Bool found;

                  if (inst->used == 0)
                    {
                       inst->used++;
                       if ((!ec->remember) || !(ec->remember->apply & E_REMEMBER_APPLY_DESKTOP))
                         {
                            E_Zone *zone;
                            E_Desk *desk;

                            zone = e_comp_zone_number_get(inst->screen);
                            if (zone) e_client_zone_set(ec, zone);
                            desk = e_desk_at_xy_get(ec->zone, inst->desk_x,
                                                    inst->desk_y);
                            if (desk) e_client_desk_set(ec, desk);
                         }
                       if (ec->netwm.pid != ecore_exe_pid_get(inst->exe))
                         {
                            /* most likely what has happened here is that the .desktop launcher
                             * has spawned a process which then created this border, meaning the
                             * E_Exec instance will be deleted in a moment, and we will be unable to track it.
                             * to prevent this, we convert our instance to a phony
                             */
                             inst->phony = 1;
                         }
                    }
                  found = !!inst->clients;
                  e_exec_instance_client_add(inst, ec);
                  if (!found)
                    e_exec_instance_found(inst);
               }

             if (e_config->window_grouping) // FIXME: We may want to make the border "urgent" so that the user knows it appeared.
               {
                  E_Client *ecl = NULL;

                  ecl = ec->parent;
                  if (!ecl)
                    ecl = ec->leader;
                  if (!ecl)
                    {
                       E_Client *child = e_client_bottom_get();

                       do
                         {
                            if (child == ec) continue;
                            if (e_object_is_del(E_OBJECT(child))) continue;
                            if ((ec->icccm.client_leader) &&
                                (child->icccm.client_leader ==
                                 ec->icccm.client_leader))
                              {
                                 ecl = child;
                                 break;
                              }
                            child = e_client_above_get(child);
                         } while (child);
                    }
                  if (ecl)
                    {
                       if (ecl->zone)
                         e_client_zone_set(ec, ecl->zone);
                       if (ecl->desk)
                         e_client_desk_set(ec, ecl->desk);
                       else
                         e_client_stick(ec);
                    }
               }
          }
        cd->fetch_exe = 0;
     }

   if ((e_config->use_desktop_window_profile) && (need_desk_set))
     {
        E_Desk *desk = NULL;
        const char *p, *p2;
        Eina_Bool set = EINA_FALSE;
        int i;

        /* set desktop using given initial profile */
        p = ec->e.state.profile.set;
        if (p)
          {
             if (!e_util_strcmp(p, ec->desk->window_profile))
               {
                  e_client_desk_window_profile_wait_desk_set(ec, ec->desk);
                  set = EINA_TRUE;
               }
             else
               {
                  desk = e_comp_desk_window_profile_get(p);
                  if (desk)
                    {
                       e_client_desk_set(ec, desk);
                       set = EINA_TRUE;
                    }
               }
          }

        if (!set)
          {
             /* try to find desktop using available profile list */
             for (i = 0; i < ec->e.state.profile.num; i++)
               {
                  p2 = ec->e.state.profile.available_list[i];
                  if (!e_util_strcmp(p2, ec->desk->window_profile))
                    {
                       eina_stringshare_replace(&ec->e.state.profile.set, p2);
                       e_client_desk_window_profile_wait_desk_set(ec, ec->desk);
                       set = EINA_TRUE;
                       break;
                    }
               }
             if (!set)
               {
                  for (i = 0; i < ec->e.state.profile.num; i++)
                    {
                       p2 = ec->e.state.profile.available_list[i];
                       desk = e_comp_desk_window_profile_get(p2);
                       if (desk)
                         {
                            e_client_desk_set(ec, desk);
                            set = EINA_TRUE;
                            break;
                         }
                    }
               }
          }

        if ((!set) && ec->desk)
          {
             eina_stringshare_replace(&ec->e.state.profile.set, ec->desk->window_profile);
             e_client_desk_window_profile_wait_desk_set(ec, ec->desk);
          }
     }

   if (ec->e.state.profile.set)
     {
        ecore_x_e_window_profile_change_request_send(win,
                                                     ec->e.state.profile.set);
        eina_stringshare_replace(&ec->e.state.profile.wait, ec->e.state.profile.set);
        ec->e.state.profile.wait_for_done = 1;
        eina_stringshare_replace(&ec->e.state.profile.set, NULL);
     }
   ec->changes.prop = 0;
   if (rem_change) e_remember_update(ec);
   if ((!cd->reparented) && (!ec->internal)) ec->changes.border = 0;
   if (ec->changes.icon)
     {
        /* don't create an icon until we need it */
        if (cd->reparented) return;
        cd->change_icon = 1;
        ec->changes.icon = 0;
     }
}

static Eina_Bool
_e_comp_x_first_draw_delay_cb(void *data)
{
   E_Client *ec = data;

   _e_comp_x_client_data_get(ec)->first_draw_delay = NULL;
   e_comp_object_damage(ec->frame, 0, 0, ec->w, ec->h);
   return EINA_FALSE;
}

static void
_e_comp_x_hook_client_new(void *d EINA_UNUSED, E_Client *ec)
{
   Ecore_X_Window win;

   E_COMP_X_PIXMAP_CHECK;
   win = e_pixmap_window_get(ec->pixmap);
   ec->ignored = e_comp_ignore_win_find(win);

   ec->comp_data = E_NEW(E_Comp_Client_Data, 1);
   ec->comp_data->set_win_type = ec->comp_data->fetch_exe = 1;

   /* FIXME: ewww - round trip */
   ec->argb = ecore_x_window_argb_get(win);
   ec->changes.shape = 1;
   ec->changes.shape_input = 1;

   ec->netwm.type = E_WINDOW_TYPE_UNKNOWN;
   ec->icccm.state = ECORE_X_WINDOW_STATE_HINT_NONE;

   if (!_e_comp_x_client_new_helper(ec)) return;
   ec->ignored |= e_comp->comp_type == E_PIXMAP_TYPE_WL;

   ec->comp_data->first_damage = ec->internal;

   eina_hash_add(clients_win_hash, &win, ec);
   if (!ec->input_only)
     ec->comp_data->first_draw_delay = ecore_timer_add(e_comp_config_get()->first_draw_delay, _e_comp_x_first_draw_delay_cb, ec);
}

static void
_e_comp_x_hook_client_focus_unset(void *d EINA_UNUSED, E_Client *ec)
{
   E_COMP_X_PIXMAP_CHECK;
   _e_comp_x_focus_setup(ec);
   _e_comp_x_focus_check();
}

static void
_e_comp_x_hook_client_focus_set(void *d EINA_UNUSED, E_Client *ec)
{
   focus_time = ecore_x_current_time_get();
   if (!e_client_has_xwindow(ec))
     {
        e_grabinput_focus(e_comp->ee_win, E_FOCUS_METHOD_PASSIVE);
        return;
     }
   _e_comp_x_focus_setdown(ec);

   if ((ec->icccm.take_focus) && (ec->icccm.accepts_focus))
     {
        e_grabinput_focus(e_client_util_win_get(ec), E_FOCUS_METHOD_LOCALLY_ACTIVE);
        /* TODO what if the client didn't take focus ? */
     }
   else if (!ec->icccm.accepts_focus)
     {
        e_grabinput_focus(e_client_util_win_get(ec), E_FOCUS_METHOD_GLOBALLY_ACTIVE);
     }
   else if (!ec->icccm.take_focus)
     {
        e_grabinput_focus(e_client_util_win_get(ec), E_FOCUS_METHOD_PASSIVE);
     }
}

static void
_e_comp_x_hook_client_redirect(void *d EINA_UNUSED, E_Client *ec)
{
   E_COMP_X_PIXMAP_CHECK;
   if (_e_comp_x_client_data_get(ec)->unredirected_single)
     {
        ecore_x_composite_redirect_window(_e_comp_x_client_window_get(ec), ECORE_X_COMPOSITE_UPDATE_MANUAL);
        _e_comp_x_client_data_get(ec)->unredirected_single = 0;
     }
   else if (e_comp->nocomp)
     {
        /* first window */
        e_comp_x_nocomp_end();
        ecore_x_window_reparent(_e_comp_x_client_window_get(ec), e_comp->root, ec->client.x, ec->client.y);
        _e_comp_x_client_stack(ec);
     }
   if (!_e_comp_x_client_data_get(ec)->damage)
     _e_comp_x_client_damage_add(ec);
}

static void
_e_comp_x_hook_client_unredirect(void *d EINA_UNUSED, E_Client *ec)
{
   Ecore_X_Region parts;

   E_COMP_X_PIXMAP_CHECK;
   eina_hash_del(damages_hash, &_e_comp_x_client_data_get(ec)->damage, ec);
   parts = ecore_x_region_new(NULL, 0);
   ecore_x_damage_subtract(_e_comp_x_client_data_get(ec)->damage, 0, parts);
   ecore_x_region_free(parts);
   ecore_x_damage_free(_e_comp_x_client_data_get(ec)->damage);
   _e_comp_x_client_data_get(ec)->damage = 0;

   if (ec->unredirected_single && (!_e_comp_x_client_data_get(ec)->unredirected_single))
     {
        ecore_x_composite_unredirect_window(_e_comp_x_client_window_get(ec), ECORE_X_COMPOSITE_UPDATE_MANUAL);
        ecore_x_window_reparent(_e_comp_x_client_window_get(ec), e_comp->win, ec->client.x, ec->client.y);
        ecore_x_window_raise(_e_comp_x_client_window_get(ec));
        _e_comp_x_client_data_get(ec)->unredirected_single = 1;
     }
   if (!e_comp->nocomp) return; //wait for it...
   ecore_x_composite_unredirect_subwindows(e_comp->root, ECORE_X_COMPOSITE_UPDATE_MANUAL);
   ecore_x_window_hide(e_comp->win);
}

static void
_e_comp_x_hook_client_del(void *d EINA_UNUSED, E_Client *ec)
{
   Ecore_X_Window win, pwin;
   unsigned int visible = 0;
   E_Comp_X_Client_Data *cd;

   E_COMP_X_PIXMAP_CHECK;
   win = e_client_util_win_get(ec);
   pwin = e_client_util_pwin_get(ec);
   cd = _e_comp_x_client_data_get(ec);

   if (mouse_client == ec) mouse_client = NULL;
   if ((!stopping) && cd && (!cd->deleted))
     ecore_x_window_prop_card32_set(win, E_ATOM_MANAGED, &visible, 1);
   if ((!ec->already_unparented) && cd && cd->reparented)
     {
        _e_comp_x_focus_setdown(ec);
        e_bindings_mouse_ungrab(E_BINDING_CONTEXT_WINDOW, win);
        e_bindings_wheel_ungrab(E_BINDING_CONTEXT_WINDOW, win);
        if (!cd->deleted)
          {
             if (stopping)
               {
                  ecore_x_window_reparent(win, e_comp->root,
                                          ec->client.x, ec->client.y);
                  ecore_x_window_configure(win,
                                           ECORE_X_WINDOW_CONFIGURE_MASK_SIBLING |
                                           ECORE_X_WINDOW_CONFIGURE_MASK_STACK_MODE,
                                           0, 0, 0, 0, 0, pwin,
                                           ECORE_X_WINDOW_STACK_BELOW);
               }

             else
               /* put the window back where we found it to prevent annoying dancing windows */
               ecore_x_window_reparent(win, e_comp->root,
                                       cd->initial_attributes.x,
                                       cd->initial_attributes.y);
             if (!ec->internal)
               ecore_x_window_save_set_del(win);
          }
     }
   ec->already_unparented = 1;
   if (cd)
     eina_hash_del_by_key(clients_win_hash, &win);
   if (cd && cd->damage)
     {
        eina_hash_del(damages_hash, &cd->damage, ec);
        ecore_x_damage_free(cd->damage);
        cd->damage = 0;
     }
   if (cd && cd->reparented)
     {
        eina_hash_del_by_key(clients_win_hash, &pwin);
        e_pixmap_parent_window_set(e_comp_x_client_pixmap_get(ec), 0);
        ecore_x_window_free(pwin);
     }

   if (ec->hacks.mapping_change)
     _e_comp_x_mapping_change_disabled--;
   if (ec->parent && (ec->parent->modal == ec))
     {
        E_Comp_X_Client_Data *pcd;

        pcd = _e_comp_x_client_data_get(ec->parent);
        if (pcd->lock_win)
          {
             eina_hash_del_by_key(clients_win_hash, &pcd->lock_win);
             ecore_x_window_hide(pcd->lock_win);
             ecore_x_window_free(pcd->lock_win);
             pcd->lock_win = 0;
          }
        ec->parent->lock_close = 0;
        ec->parent->modal = NULL;
     }
   if (cd)
     E_FREE_FUNC(cd->first_draw_delay, ecore_timer_del);
#ifdef HAVE_WAYLAND
   if (e_comp->comp_type == E_PIXMAP_TYPE_WL)
     {
        if (e_pixmap_is_x(ec->pixmap))
          e_comp_wl_client_xwayland_setup(ec, NULL, NULL);
        else
          {
             free(cd);
             e_pixmap_free(e_comp_x_client_pixmap_get(ec));
          }
     }
#endif
   if (post_clients)
     post_clients = eina_list_remove(post_clients, ec);

   _e_comp_x_focus_check();
}

static void
_e_comp_x_hook_client_move_end(void *d EINA_UNUSED, E_Client *ec)
{
   E_COMP_X_PIXMAP_CHECK;
   _e_comp_x_client_data_get(ec)->moving = 0;
   if (!_e_comp_x_client_data_get(ec)->move_counter) return;
   _e_comp_x_client_data_get(ec)->move_counter = 0;
   ec->post_move = 1;
   _e_comp_x_post_client_idler_add(ec);
}

static void
_e_comp_x_hook_client_move_begin(void *d EINA_UNUSED, E_Client *ec)
{
   E_COMP_X_PIXMAP_CHECK;
   _e_comp_x_client_data_get(ec)->moving = 1;
}

static void
_e_comp_x_hook_client_resize_end(void *d EINA_UNUSED, E_Client *ec)
{
   E_COMP_X_PIXMAP_CHECK;
   if (_e_comp_x_client_data_get(ec)->alarm)
     {
        eina_hash_del_by_key(alarm_hash, &_e_comp_x_client_data_get(ec)->alarm);
        ecore_x_sync_alarm_free(_e_comp_x_client_data_get(ec)->alarm);
        _e_comp_x_client_data_get(ec)->alarm = 0;
     }
   ec->netwm.sync.alarm = 0;
   /* resize to last geometry if sync alarm for it was not yet handled */
   if (ec->pending_resize)
     {
        EC_CHANGED(ec);
        ec->changes.pos = 1;
        ec->changes.size = 1;
     }

   E_FREE_LIST(ec->pending_resize, free);
}

static void
_e_comp_x_hook_client_resize_begin(void *d EINA_UNUSED, E_Client *ec)
{
   E_COMP_X_PIXMAP_CHECK;
   if (!ec->netwm.sync.request) return;
   if (e_comp->comp_type == E_PIXMAP_TYPE_X)
     {
        _e_comp_x_client_data_get(ec)->alarm = ecore_x_sync_alarm_new(_e_comp_x_client_data_get(ec)->sync_counter);
        eina_hash_add(alarm_hash, &_e_comp_x_client_data_get(ec)->alarm, ec);
     }
   ec->netwm.sync.alarm = ec->netwm.sync.serial = 1;
   ec->netwm.sync.wait = 0;
   ec->netwm.sync.send_time = ecore_loop_time_get();
}

static void
_e_comp_x_hook_client_desk_set(void *d EINA_UNUSED, E_Client *ec)
{
   unsigned int num[2];
   int x, y;

   E_COMP_X_PIXMAP_CHECK;
   e_desk_xy_get(ec->desk, &x, &y);
   num[0] = x, num[1] = y;
   ecore_x_window_prop_card32_set(e_client_util_win_get(ec), E_ATOM_DESK, num, 2);
}

static Eina_Bool
_e_comp_x_cb_ping(void *data EINA_UNUSED, int ev_type EINA_UNUSED, Ecore_X_Event_Ping *ev)
{
   E_Client *ec;

   ec = _e_comp_x_client_find_by_window(ev->event_win);
   if (ec) ec->ping_ok = 1;
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_x_screensaver_idle_timer_cb(void *d EINA_UNUSED)
{
   ecore_event_add(E_EVENT_SCREENSAVER_ON, NULL, NULL, NULL);
   screensaver_idle_timer = NULL;
   return EINA_FALSE;
}

static Ecore_Timer *screensaver_eval_timer = NULL;
static Eina_Bool saver_on = EINA_FALSE;

static Eina_Bool
_e_comp_x_screensaver_eval_cb(void *d EINA_UNUSED)
{
   if (saver_on)
     {
        if (e_config->backlight.idle_dim)
          {
             double t = e_config->screensaver_timeout -
               e_config->backlight.timer;

             if (t < 1.0) t = 1.0;
             E_FREE_FUNC(screensaver_idle_timer, ecore_timer_del);
             if (e_config->screensaver_enable)
               screensaver_idle_timer = ecore_timer_add
                   (t, _e_comp_x_screensaver_idle_timer_cb, NULL);
             if (e_backlight_mode_get(NULL) != E_BACKLIGHT_MODE_DIM)
               {
                  e_backlight_mode_set(NULL, E_BACKLIGHT_MODE_DIM);
                  screensaver_dimmed = EINA_TRUE;
               }
          }
        else
          {
             if (!e_screensaver_on_get())
               ecore_event_add(E_EVENT_SCREENSAVER_ON, NULL, NULL, NULL);
          }
     }
   else
     {
        if (screensaver_idle_timer)
          {
             E_FREE_FUNC(screensaver_idle_timer, ecore_timer_del);
             if (e_config->backlight.idle_dim)
               {
                  if (e_backlight_mode_get(NULL) != E_BACKLIGHT_MODE_NORMAL)
                    e_backlight_mode_set(NULL, E_BACKLIGHT_MODE_NORMAL);
               }
          }
        else
          {
             if (screensaver_dimmed)
               {
                  if (e_backlight_mode_get(NULL) != E_BACKLIGHT_MODE_NORMAL)
                    e_backlight_mode_set(NULL, E_BACKLIGHT_MODE_NORMAL);
                  screensaver_dimmed = EINA_FALSE;
               }
             if (e_screensaver_on_get())
               ecore_event_add(E_EVENT_SCREENSAVER_OFF, NULL, NULL, NULL);
          }
     }
   screensaver_eval_timer = NULL;
   return EINA_FALSE;
}

static Eina_Bool
_e_comp_x_screensaver_notify_cb(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Screensaver_Notify *ev)
{
   if ((ev->on) && (!saver_on))
     {
        saver_on = EINA_TRUE;
	E_FREE_FUNC(screensaver_eval_timer, ecore_timer_del);
	screensaver_eval_timer = ecore_timer_add(0.3, _e_comp_x_screensaver_eval_cb, NULL);
     }
   else if ((!ev->on) && (saver_on))
     {
        saver_on = EINA_FALSE;
	E_FREE_FUNC(screensaver_eval_timer, ecore_timer_del);
	screensaver_eval_timer = ecore_timer_add(0.3, _e_comp_x_screensaver_eval_cb, NULL);
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_comp_x_backlight_notify_cb(void *data EINA_UNUSED, int t EINA_UNUSED, Ecore_X_Event_Randr_Output_Property_Notify *ev)
{
   double x_bl;

   if (ev->property != backlight_atom) return ECORE_CALLBACK_RENEW;
   x_bl = ecore_x_randr_output_backlight_level_get(0, ev->output);

   if (x_bl >= 0.0)
     {
        if (fabs(e_bl_val - x_bl) < DBL_EPSILON)
          ecore_event_add(E_EVENT_BACKLIGHT_CHANGE, NULL, NULL, NULL);;
        e_bl_val = x_bl;
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_e_comp_x_cb_frame_extents_request(void *data EINA_UNUSED, int ev_type EINA_UNUSED, Ecore_X_Event_Frame_Extents_Request *ev)
{
   Ecore_X_Window_Type type;
   Ecore_X_MWM_Hint_Decor decor;
   Ecore_X_Window_State *state;
   Ecore_X_Window win;
   Frame_Extents *extents;
   const char *border, *sig, *key;
   int ok;
   unsigned int i, num;

   win = ecore_x_window_parent_get(ev->win);
   if (win != e_comp->root) return ECORE_CALLBACK_PASS_ON;

   /* TODO:
    * * We need to check if we remember this window, and border locking is set
    */
   border = "default";
   key = border;
   ok = ecore_x_mwm_hints_get(ev->win, NULL, &decor, NULL);
   if ((ok) &&
       (!(decor & ECORE_X_MWM_HINT_DECOR_ALL)) &&
       (!(decor & ECORE_X_MWM_HINT_DECOR_TITLE)) &&
       (!(decor & ECORE_X_MWM_HINT_DECOR_BORDER)))
     {
        border = "borderless";
        key = border;
     }

   ok = ecore_x_netwm_window_type_get(ev->win, &type);
   if ((ok) &&
       ((type == ECORE_X_WINDOW_TYPE_DESKTOP) ||
        (type == ECORE_X_WINDOW_TYPE_DOCK)))
     {
        border = "borderless";
        key = border;
     }

   sig = NULL;
   ecore_x_netwm_window_state_get(ev->win, &state, &num);
   if (state)
     {
        int maximized = 0;

        for (i = 0; i < num; i++)
          {
             switch (state[i])
               {
                case ECORE_X_WINDOW_STATE_MAXIMIZED_VERT:
                  maximized++;
                  break;

                case ECORE_X_WINDOW_STATE_MAXIMIZED_HORZ:
                  maximized++;
                  break;

                case ECORE_X_WINDOW_STATE_FULLSCREEN:
                  border = "borderless";
                  key = border;
                  break;

                case ECORE_X_WINDOW_STATE_SHADED:
                case ECORE_X_WINDOW_STATE_SKIP_TASKBAR:
                case ECORE_X_WINDOW_STATE_SKIP_PAGER:
                case ECORE_X_WINDOW_STATE_HIDDEN:
                case ECORE_X_WINDOW_STATE_ICONIFIED:
                case ECORE_X_WINDOW_STATE_MODAL:
                case ECORE_X_WINDOW_STATE_STICKY:
                case ECORE_X_WINDOW_STATE_ABOVE:
                case ECORE_X_WINDOW_STATE_BELOW:
                case ECORE_X_WINDOW_STATE_DEMANDS_ATTENTION:
                case ECORE_X_WINDOW_STATE_UNKNOWN:
                  break;
               }
          }
        if ((maximized == 2) &&
            (e_config->maximize_policy == E_MAXIMIZE_FULLSCREEN))
          {
             sig = "ev,action,maximize,fullscreen";
             key = "maximize,fullscreen";
          }
        free(state);
     }

   extents = eina_hash_find(frame_extents, key);
   if (!extents)
     {
        Evas_Object *o;
        char buf[1024];

        extents = E_NEW(Frame_Extents, 1);
        if (!extents) return ECORE_CALLBACK_RENEW;

        o = edje_object_add(e_comp->evas);
        snprintf(buf, sizeof(buf), "ev/widgets/border/%s/border", border);
        ok = e_theme_edje_object_set(o, "base/theme/borders", buf);
        if (ok)
          {
             Evas_Coord x, y, w, h;

             if (sig)
               {
                  edje_object_signal_emit(o, sig, "ev");
                  edje_object_message_signal_process(o);
               }

             evas_object_resize(o, 1000, 1000);
             edje_object_calc_force(o);
             edje_object_part_geometry_get(o, "ev.swallow.client",
                                           &x, &y, &w, &h);
             extents->l = x;
             extents->r = 1000 - (x + w);
             extents->t = y;
             extents->b = 1000 - (y + h);
          }
        else
          {
             extents->l = 0;
             extents->r = 0;
             extents->t = 0;
             extents->b = 0;
          }
        evas_object_del(o);
        eina_hash_add(frame_extents, key, extents);
     }

   if (extents)
     ecore_x_netwm_frame_size_set(ev->win, extents->l, extents->r, extents->t, extents->b);

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_e_comp_x_randr_change(void *data EINA_UNUSED, int ev_type EINA_UNUSED, void *event_info EINA_UNUSED)
{
   if ((e_comp->w != e_randr2->w) ||
       (e_comp->h != e_randr2->h))
     {
        if (e_comp->comp_type == E_PIXMAP_TYPE_X)
          e_comp_canvas_resize(e_randr2->w, e_randr2->h);
     }
   else
     {
        E_Client *ec;

        ecore_x_netwm_desk_size_set(e_comp->root, e_comp->w, e_comp->h);
        if (e_comp->comp_type == E_PIXMAP_TYPE_X)
          e_randr2_screens_setup(e_comp->w, e_comp->h);

        if (e_comp->comp_type == E_PIXMAP_TYPE_X)
          e_comp_canvas_update();
        E_CLIENT_FOREACH(ec)
          {
             if (!e_client_util_ignored_get(ec))
               _e_comp_x_client_zone_geometry_set(ec);
          }
     }
   return ECORE_CALLBACK_RENEW;
}

static void
_e_comp_x_ee_resize(Ecore_Evas *ee EINA_UNUSED)
{
   E_Client *ec;

   ecore_x_netwm_desk_size_set(e_comp->root, e_comp->w, e_comp->h);
   e_randr2_screens_setup(e_comp->w, e_comp->h);

   e_comp_canvas_update();
   E_CLIENT_FOREACH(ec)
     {
        if (!e_client_util_ignored_get(ec))
          _e_comp_x_client_zone_geometry_set(ec);
     }
}

static void
_e_comp_x_del(E_Comp *c)
{
   unsigned int i;

   ecore_x_window_key_ungrab(c->root, "F", ECORE_EVENT_MODIFIER_SHIFT |
                             ECORE_EVENT_MODIFIER_CTRL |
                             ECORE_EVENT_MODIFIER_ALT, 0);
   ecore_x_window_key_ungrab(c->root, "Home", ECORE_EVENT_MODIFIER_SHIFT |
                             ECORE_EVENT_MODIFIER_CTRL |
                             ECORE_EVENT_MODIFIER_ALT, 0);
   if (c->grabbed)
     {
        c->grabbed = 0;
        ecore_x_ungrab();
     }

   for (i = e_comp_canvas_layer_map(E_LAYER_CLIENT_DESKTOP); i <= e_comp_canvas_layer_map(E_LAYER_CLIENT_PRIO); i++)
     ecore_x_window_free(c->layers[i].win);

   ecore_x_composite_unredirect_subwindows
     (c->root, ECORE_X_COMPOSITE_UPDATE_MANUAL);
   if (c->block_win) ecore_x_window_free(c->block_win);
   ecore_x_composite_render_window_disable(c->win);
   e_alert_composite_win(c->root, 0);

   ecore_x_window_free(c->cm_selection);
   ecore_x_screen_is_composited_set(0, 0);

   eina_list_free(c->x_comp_data->retry_clients);
   ecore_timer_del(c->x_comp_data->retry_timer);
   free(c->x_comp_data);
}

static void
_e_comp_x_manage_windows(void)
{
   Ecore_X_Window *windows;
   int wnum;
   int i;
   const char *atom_names[] =
   {
      "_XEMBED_INFO",
      "_KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR",
      "KWM_DOCKWINDOW"
   };
   Ecore_X_Atom atoms[3];
   Ecore_X_Atom atom_xmbed, atom_kde_netwm_systray, atom_kwm_dockwindow;
   int count;

   /* a manager is designated for each root. lets get all the windows in
      the managers root */
   windows = ecore_x_window_children_get(e_comp->root, &wnum);
   if (!windows) return;

   ecore_x_atoms_get(atom_names, 3, atoms);
   atom_xmbed = atoms[0];
   atom_kde_netwm_systray = atoms[1];
   atom_kwm_dockwindow = atoms[2];
   for (i = 0; i < wnum; i++)
     {
        unsigned int ret_val, deskxy[2];
        unsigned char *data = NULL;
        int ret;
        E_Client *ec = NULL;

        if ((e_comp->win == windows[i]) || (e_comp->ee_win == windows[i]) ||
            (e_comp->root == windows[i]) || (e_comp->cm_selection == windows[i]))
          continue;
        if (_e_comp_x_client_find_by_window(windows[i]))
          continue;
        if (!ecore_x_window_prop_property_get(windows[i],
                                              atom_kde_netwm_systray,
                                              atom_xmbed, 32,
                                              &data, &count))
          data = NULL;

        if (!data)
          {
             if (!ecore_x_window_prop_property_get(windows[i],
                                                   atom_kwm_dockwindow,
                                                   atom_kwm_dockwindow, 32,
                                                   &data, &count))
               data = NULL;
          }
        if (data)
          {
             E_FREE(data);
             continue;
          }
        ret = ecore_x_window_prop_card32_get(windows[i],
                                             E_ATOM_MANAGED,
                                             &ret_val, 1);

        /* we have seen this window before */
        if ((ret > -1) && (ret_val == 1))
          {
             E_Zone *zone = NULL;
             E_Desk *desk = NULL;
             unsigned int id;
             char *path;
             Efreet_Desktop *desktop = NULL;

             /* get all information from window before it is
              * reset by e_client_new */
             ret = ecore_x_window_prop_card32_get(windows[i],
                                                  E_ATOM_ZONE,
                                                  &id, 1);
             if (ret == 1)
               zone = e_comp_zone_number_get(id);
             if (!zone)
               zone = e_zone_current_get();
             ret = ecore_x_window_prop_card32_get(windows[i],
                                                  E_ATOM_DESK,
                                                  deskxy, 2);
             if (ret == 2)
               desk = e_desk_at_xy_get(zone,
                                       deskxy[0],
                                       deskxy[1]);

             path = ecore_x_window_prop_string_get(windows[i],
                                                   E_ATOM_DESKTOP_FILE);
             if (path)
               {
                  desktop = efreet_desktop_get(path);
                  free(path);
               }

             {
                ec = _e_comp_x_client_new(windows[i], 1);
                if (ec)
                  {
                     if (desk) e_client_desk_set(ec, desk);
                     ec->desktop = desktop;
                  }
             }
          }
        else
          {
             /* We have not seen this window, and X tells us it
              * should be seen */
             ec = _e_comp_x_client_new(windows[i], 1);
          }
        if (ec && (!_e_comp_x_client_data_get(ec)->initial_attributes.visible))
          {
             DELD(ec, 3);
             E_FREE_FUNC(ec, e_object_del);
          }
        if (ec)
          {
             if (ec->override)
               {
                  _e_comp_x_client_evas_init(ec);
                  if (!ec->input_only)
                    _e_comp_x_client_damage_add(ec);
                  e_pixmap_usable_set(ec->pixmap, 1);
                  _e_comp_x_client_data_get(ec)->first_map = 1;
                  evas_object_geometry_set(ec->frame, ec->client.x, ec->client.y, ec->client.w, ec->client.h);
               }
             ec->ignore_first_unmap = 1;
             evas_object_show(ec->frame);
             _e_comp_x_client_stack(ec);
          }
     }
   free(windows);
}

static void
_e_comp_x_bindings_grab_cb(void)
{
   Eina_List *l;
   E_Client *ec;

   EINA_LIST_FOREACH(e_comp->clients, l, ec)
     {
        if (e_client_util_ignored_get(ec)) continue;
        _e_comp_x_focus_init(ec);
        if (ec->focused)
          _e_comp_x_focus_setdown(ec);
        else
          {
             _e_comp_x_focus_setup(ec);
             e_bindings_mouse_grab(E_BINDING_CONTEXT_WINDOW, e_client_util_win_get(ec));
             e_bindings_wheel_grab(E_BINDING_CONTEXT_WINDOW, e_client_util_win_get(ec));
          }
     }
}

static void
_e_comp_x_bindings_ungrab_cb(void)
{
   Eina_List *l;
   E_Client *ec;

   EINA_LIST_FOREACH(e_comp->clients, l, ec)
     {
        Ecore_X_Window win;

        if (e_client_util_ignored_get(ec)) continue;
        win = e_client_util_win_get(ec);
        ecore_x_window_button_ungrab(win, 1, 0, 1);
        ecore_x_window_button_ungrab(win, 2, 0, 1);
        ecore_x_window_button_ungrab(win, 3, 0, 1);
        e_bindings_mouse_ungrab(E_BINDING_CONTEXT_WINDOW, win);
        e_bindings_wheel_ungrab(E_BINDING_CONTEXT_WINDOW, win);
        _e_comp_x_client_data_get(ec)->button_grabbed = 0;
     }
}

static Eina_Bool
_e_comp_x_desklock_key_down(void *d EINA_UNUSED, int t EINA_UNUSED, Ecore_Event_Key *ev)
{
   return (ev->window == e_comp->x_comp_data->lock_win);
}

static void
_e_comp_x_desklock_hide(void)
{
   if (e_comp->x_comp_data->lock_win)
     {
        e_grabinput_release(e_comp->x_comp_data->lock_win, e_comp->x_comp_data->lock_win);
        ecore_x_window_free(e_comp->x_comp_data->lock_win);
        e_comp->x_comp_data->lock_win = 0;
     }

   if (e_comp->x_comp_data->lock_grab_break_wnd)
     ecore_x_window_show(e_comp->x_comp_data->lock_grab_break_wnd);
   e_comp->x_comp_data->lock_grab_break_wnd = 0;
   E_FREE_FUNC(e_comp->x_comp_data->lock_key_handler, ecore_event_handler_del);
   e_comp_override_del();
}

static Eina_Bool
_e_comp_x_desklock_show(void)
{
   Ecore_X_Window win;

   win = e_comp->x_comp_data->lock_win =
     ecore_x_window_input_new(e_comp->root, 0, 0, 1, 1);
   ecore_x_window_show(win);
   if (!e_grabinput_get(win, 0, win))
     {
        Ecore_X_Window *windows;
        int wnum, i;

        windows = ecore_x_window_children_get(e_comp->root, &wnum);
        if (!windows) goto fail;
        for (i = 0; i < wnum; i++)
          {
             Ecore_X_Window_Attributes att;

             memset(&att, 0, sizeof(Ecore_X_Window_Attributes));
             ecore_x_window_attributes_get(windows[i], &att);
             if (att.visible)
               {
                  ecore_x_window_hide(windows[i]);
                  if (e_grabinput_get(win, 0, win))
                    {
                       e_comp->x_comp_data->lock_grab_break_wnd = windows[i];
                       free(windows);
                       goto works;
                    }
                  ecore_x_window_show(windows[i]);
               }
          }
        free(windows);
     }
works:
   e_comp_override_add();
   e_comp_ignore_win_add(E_PIXMAP_TYPE_X, e_comp->x_comp_data->lock_win);
   e_comp->x_comp_data->lock_key_handler =
     ecore_event_handler_add(ECORE_EVENT_KEY_DOWN, (Ecore_Event_Handler_Cb)_e_comp_x_desklock_key_down, e_comp);

   return EINA_TRUE;
fail:
   /* everything failed - can't lock */
   e_util_dialog_show(_("Lock Failed"),
                      _("Locking the desktop failed because some application<br>"
                        "has grabbed either the keyboard or the mouse or both<br>"
                        "and their grab is unable to be broken."));
   return EINA_FALSE;
}

static Eina_Bool
_e_comp_x_setup(Ecore_X_Window root, int w, int h)
{
   Ecore_X_Window_Attributes att;
   Eina_Bool res;
   unsigned int i;

   res = ecore_x_screen_is_composited(0);
   if (res)
     {
        ERR(_("Another compositor is already running on your display server."));
        return EINA_FALSE;
     }
   if (!ecore_x_window_manage(root)) return EINA_FALSE;

   E_OBJECT_DEL_SET(e_comp, _e_comp_x_del);
   e_comp->x_comp_data = E_NEW(E_Comp_X_Data, 1);
   ecore_x_e_window_profile_supported_set(root, e_config->use_desktop_window_profile);
   e_comp->cm_selection = ecore_x_window_input_new(root, 0, 0, 1, 1);
   if (!e_comp->cm_selection) return EINA_FALSE;
   ecore_x_icccm_name_class_set(e_comp->cm_selection, "comp", "cm_selection");
   e_comp_ignore_win_add(E_PIXMAP_TYPE_X, e_comp->cm_selection);
   e_hints_init(root, e_comp->cm_selection);
   ecore_x_window_background_color_set(root, 0, 0, 0);
   ecore_x_screen_is_composited_set(0, e_comp->cm_selection);
   ecore_x_selection_owner_set(e_comp->cm_selection, ecore_x_atom_get("WM_S0"), ecore_x_current_time_get());

   e_comp->win = ecore_x_composite_render_window_enable(root);
   if (!e_comp->win)
     {
        ERR(_("Your display server does not support the compositor overlay window.\n"
              "This is needed for Enlightenment to function."));
        return EINA_FALSE;
     }
   e_comp->root = root;

   memset((&att), 0, sizeof(Ecore_X_Window_Attributes));
   ecore_x_window_attributes_get(e_comp->win, &att);

   if ((att.depth != 24) && (att.depth != 32))
     {
        /*
                  e_util_dialog_internal
                  (_("Compositor Error"), _("Your screen is not in 24/32bit display mode.<br>"
                  "This is required to be your default depth<br>"
                  "setting for the compositor to work properly."));
                  ecore_x_composite_render_window_disable(e_comp->win);
                  free(e_comp);
                  return NULL;
         */
     }
   e_comp->depth = att.depth;

   e_alert_composite_win(root, e_comp->win);

   if (!e_comp_x_randr_canvas_new(e_comp->win, w, h))
     ecore_job_add(_e_comp_x_add_fail_job, NULL);

   ecore_x_composite_redirect_subwindows(root, ECORE_X_COMPOSITE_UPDATE_MANUAL);

   ecore_x_window_key_grab(root, "Home", ECORE_EVENT_MODIFIER_SHIFT |
                           ECORE_EVENT_MODIFIER_CTRL |
                           ECORE_EVENT_MODIFIER_ALT, 0);
   ecore_x_window_key_grab(root, "F", ECORE_EVENT_MODIFIER_SHIFT |
                           ECORE_EVENT_MODIFIER_CTRL |
                           ECORE_EVENT_MODIFIER_ALT, 0);

   ecore_evas_callback_resize_set(e_comp->ee, _e_comp_x_ee_resize);
   ecore_evas_data_set(e_comp->ee, "comp", e_comp);
   e_comp->bindings_grab_cb = _e_comp_x_bindings_grab_cb;
   e_comp->bindings_ungrab_cb = _e_comp_x_bindings_ungrab_cb;

   if (e_comp->comp_type == E_PIXMAP_TYPE_NONE)
     {
        if (!e_comp_canvas_init(w, h)) return EINA_FALSE;
     }

   e_grabinput_focus(e_comp->ee_win, E_FOCUS_METHOD_PASSIVE);

   /* init layers */
   for (i = e_comp_canvas_layer_map(E_LAYER_CLIENT_DESKTOP); i <= e_comp_canvas_layer_map(E_LAYER_CLIENT_PRIO); i++)
     {
        char buf[64];
        E_Client *ec;

        e_comp->layers[i].win = ecore_x_window_input_new(root, 0, 0, 1, 1);
        ecore_x_window_show(e_comp->layers[i].win);
        snprintf(buf, sizeof(buf), "%d", e_comp_canvas_layer_map_to(i));
        ecore_x_icccm_name_class_set(e_comp->layers[i].win, buf, "e_layer_win");

        if (i >= e_comp_canvas_layer_map(E_LAYER_CLIENT_ABOVE))
          ecore_x_window_raise(e_comp->layers[i].win);
        ec = _e_comp_x_client_new(e_comp->layers[i].win, 0);
        evas_object_name_set(ec->frame, "layer_obj");
        ec->lock_client_stacking = 1;
        ec->internal = 1;
        ec->visible = 1;
        evas_object_del(e_comp->layers[i].obj);
        e_comp->layers[i].obj = ec->frame;
        evas_object_layer_set(ec->frame, e_comp_canvas_layer_map_to(i));
        evas_object_pass_events_set(ec->frame, 1);
        evas_object_show(ec->frame);
     }
   for (i = e_comp_canvas_layer_map(E_LAYER_CLIENT_NORMAL); i < e_comp_canvas_layer_map(E_LAYER_CLIENT_ABOVE); i--)
     ecore_x_window_lower(e_comp->layers[i].win);

   ecore_evas_lower(e_comp->ee);
   if (e_comp->comp_type == E_PIXMAP_TYPE_NONE)
     {
        e_comp->pointer = e_pointer_window_new(e_comp->root, 0);
        e_comp->pointer->color = ecore_x_cursor_color_supported_get();
        e_pointer_type_push(e_comp->pointer, e_comp->pointer, "default");
        ecore_x_icccm_state_set(ecore_evas_window_get(e_comp->ee), ECORE_X_WINDOW_STATE_HINT_NORMAL);
     }
   else
     e_pointer_window_add(e_comp->pointer, e_comp->root);
   _e_comp_x_manage_windows();

   {
      E_Client *ec;

      E_CLIENT_REVERSE_FOREACH(ec)
        if (!e_client_util_ignored_get(ec))
          {
             ec->want_focus = ec->take_focus = 1;
             break;
          }
   }

   return !!e_comp->bg_blank_object;
}

static Eina_Bool
_e_comp_x_screens_setup(void)
{
   Ecore_X_Window root;
   int rw, rh;

   if (e_comp->comp_type == E_PIXMAP_TYPE_NONE)
     {
        e_comp_x_randr_screen_iface_set();
        if (!e_randr2_init()) return 0;
     }
   root = ecore_x_window_root_first_get();
   if (!root)
     {
        e_error_message_show("X reports there are no root windows!\n");
        return 0;
     }
   ecore_x_window_size_get(root, &rw, &rh);
   if (e_comp->comp_type == E_PIXMAP_TYPE_NONE)
     e_randr2_screens_setup(rw, rh);
   return _e_comp_x_setup(root, rw, rh);
}

E_API Eina_Bool
e_comp_x_init(void)
{
   if (!ecore_x_init(NULL))
     {
        e_error_message_show(_("Enlightenment cannot initialize Ecore_X!\n"));
        return EINA_FALSE;
     }

   ecore_x_io_error_handler_set(_e_main_cb_x_fatal, NULL);

   if (!ecore_x_composite_query())
     {
        _e_comp_x_print_win(0);
        ecore_x_shutdown();
        e_error_message_show
          (_("Your display server does not support XComposite, "
             "or Ecore-X was built without XComposite support. "
             "Note that for composite support you will also need "
             "XRender and XFixes support in X11 and Ecore."));
        return EINA_FALSE;
     }
   if (!ecore_x_damage_query())
     {
        e_error_message_show
          (_("Your display server does not support XDamage "
             "or Ecore was built without XDamage support."));
        return EINA_FALSE;
     }

   ecore_x_screensaver_event_listen_set(1);

   clients_win_hash = eina_hash_int32_new(NULL);
   damages_hash = eina_hash_int32_new(NULL);
   alarm_hash = eina_hash_int32_new(NULL);
   frame_extents = eina_hash_string_superfast_new(free);

   E_LIST_HANDLER_APPEND(handlers, E_EVENT_COMP_OBJECT_ADD, _e_comp_x_object_add, NULL);

   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_DESTROY, _e_comp_x_destroy, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_SHOW, _e_comp_x_show, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_SHOW_REQUEST, _e_comp_x_show_request, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_RESIZE_REQUEST, _e_comp_x_resize_request, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_HIDE, _e_comp_x_hide, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_REPARENT, _e_comp_x_reparent, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_CONFIGURE, _e_comp_x_configure, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_CONFIGURE_REQUEST, _e_comp_x_configure_request, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_STATE_REQUEST, _e_comp_x_state_request, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_STACK, _e_comp_x_stack, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_STACK_REQUEST, _e_comp_x_stack_request, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_CLIENT_MESSAGE, _e_comp_x_message, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_PROPERTY, _e_comp_x_property, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_SHAPE, _e_comp_x_shape, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_DAMAGE_NOTIFY, _e_comp_x_damage, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_DAMAGE, _e_comp_x_damage_win, NULL);

   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_MAPPING_CHANGE, _e_comp_x_mapping_change, NULL);

   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_FOCUS_IN, _e_comp_x_focus_in, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_FOCUS_OUT, _e_comp_x_focus_out, NULL);

   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_MOVE_RESIZE_REQUEST,
                         _e_comp_x_move_resize_request, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_DESKTOP_CHANGE,
                         _e_comp_x_desktop_change, NULL);
   if (e_comp->comp_type != E_PIXMAP_TYPE_WL)
     E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_SYNC_ALARM,
                           _e_comp_x_sync_alarm, NULL);

   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_MOUSE_BUTTON_DOWN,
                         _e_comp_x_mouse_down, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_MOUSE_BUTTON_UP,
                         _e_comp_x_mouse_up, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_MOUSE_WHEEL,
                         _e_comp_x_mouse_wheel, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_MOUSE_MOVE,
                         _e_comp_x_mouse_move, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_MOUSE_IN,
                         _e_comp_x_mouse_in, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_MOUSE_OUT,
                         _e_comp_x_mouse_out, NULL);

   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_ZONE_SET,
                         _e_comp_x_client_zone_set, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_PROPERTY,
                         _e_comp_x_client_property, NULL);

   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_FRAME_EXTENTS_REQUEST,
                         _e_comp_x_cb_frame_extents_request, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_PING,
                         _e_comp_x_cb_ping, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_SCREENSAVER_NOTIFY, _e_comp_x_screensaver_notify_cb, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_RANDR_OUTPUT_PROPERTY_NOTIFY, _e_comp_x_backlight_notify_cb, NULL);
   if (ecore_x_randr_version_get() >= RANDR_VERSION_1_3)
     backlight_atom = ecore_x_atom_get("Backlight");

   if (!backlight_atom)
     backlight_atom = ecore_x_atom_get("BACKLIGHT");

   ecore_x_screensaver_custom_blanking_enable();

   e_screensaver_attrs_set(ecore_x_screensaver_timeout_get(),
                           ecore_x_screensaver_blank_get(),
                           ecore_x_screensaver_expose_get());
   ecore_x_passive_grab_replay_func_set(_e_comp_x_grab_replay, NULL);

   e_client_hook_add(E_CLIENT_HOOK_DESK_SET, _e_comp_x_hook_client_desk_set, NULL);
   e_client_hook_add(E_CLIENT_HOOK_RESIZE_BEGIN, _e_comp_x_hook_client_resize_begin, NULL);
   e_client_hook_add(E_CLIENT_HOOK_RESIZE_END, _e_comp_x_hook_client_resize_end, NULL);
   e_client_hook_add(E_CLIENT_HOOK_MOVE_BEGIN, _e_comp_x_hook_client_move_begin, NULL);
   e_client_hook_add(E_CLIENT_HOOK_MOVE_END, _e_comp_x_hook_client_move_end, NULL);
   e_client_hook_add(E_CLIENT_HOOK_DEL, _e_comp_x_hook_client_del, NULL);
   e_client_hook_add(E_CLIENT_HOOK_NEW_CLIENT, _e_comp_x_hook_client_new, NULL);
   e_client_hook_add(E_CLIENT_HOOK_EVAL_FETCH, _e_comp_x_hook_client_fetch, NULL);
   e_client_hook_add(E_CLIENT_HOOK_EVAL_PRE_FRAME_ASSIGN, _e_comp_x_hook_client_pre_frame_assign, NULL);
   e_client_hook_add(E_CLIENT_HOOK_UNREDIRECT, _e_comp_x_hook_client_unredirect, NULL);
   e_client_hook_add(E_CLIENT_HOOK_REDIRECT, _e_comp_x_hook_client_redirect, NULL);
   e_client_hook_add(E_CLIENT_HOOK_EVAL_POST_NEW_CLIENT, _e_comp_x_hook_client_post_new_client, NULL);
   e_client_hook_add(E_CLIENT_HOOK_FOCUS_SET, _e_comp_x_hook_client_focus_set, NULL);
   e_client_hook_add(E_CLIENT_HOOK_FOCUS_UNSET, _e_comp_x_hook_client_focus_unset, NULL);
   e_client_hook_add(E_CLIENT_HOOK_EVAL_END, _e_comp_x_hook_client_eval_end, NULL);

   e_desklock_show_hook_add(_e_comp_x_desklock_show);
   e_desklock_hide_hook_add(_e_comp_x_desklock_hide);

   if (!e_atoms_init()) return 0;
   if (!_e_comp_x_screens_setup()) return EINA_FALSE;
   if (!e_xsettings_init())
     e_error_message_show(_("Enlightenment cannot initialize the XSettings system.\n"));
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_RANDR_CHANGE, _e_comp_x_randr_change, NULL);

   ecore_x_sync();
   _x_idle_flush = ecore_idle_enterer_add(_e_comp_x_flusher, NULL);

   return EINA_TRUE;
}

E_API void
e_comp_x_shutdown(void)
{
   E_FREE_LIST(handlers, ecore_event_handler_del);
   E_FREE_FUNC(clients_win_hash, eina_hash_free);
   E_FREE_FUNC(damages_hash, eina_hash_free);
   E_FREE_FUNC(alarm_hash, eina_hash_free);
   E_FREE_FUNC(frame_extents, eina_hash_free);
   e_xsettings_shutdown();
   ecore_x_screensaver_custom_blanking_disable();
   if (x_fatal) return;
   e_atoms_shutdown();
   e_randr2_shutdown();
   /* ecore_x_ungrab(); */
   ecore_x_focus_reset();
   ecore_x_events_allow_all();
   ecore_x_shutdown();
}

EINTERN void
e_comp_x_nocomp_end(void)
{
   e_comp->nocomp = 0;
   ecore_x_window_show(e_comp->win);
   ecore_x_composite_redirect_subwindows(e_comp->root, ECORE_X_COMPOSITE_UPDATE_MANUAL);
   _e_comp_x_focus_check();
}

#ifdef HAVE_WAYLAND
EINTERN void
e_comp_x_xwayland_client_setup(E_Client *ec, E_Client *wc)
{
   Ecore_X_Window win, pwin;
   E_Comp_X_Client_Data *cd;

   win = e_client_util_win_get(ec);
   pwin = e_client_util_pwin_get(ec);
   cd = ec->comp_data;
   e_comp_wl_client_xwayland_setup(wc, cd, e_pixmap_ref(ec->pixmap));
   eina_hash_del(damages_hash, &cd->damage, ec);
   ecore_x_damage_free(cd->damage);
   E_FREE_FUNC(cd->first_draw_delay, ecore_timer_del);
   cd->damage = 0;
   ec->comp_data = NULL;
   cd->evas_init = 0;
   _e_comp_x_client_evas_init(wc);
   wc->borderless = ec->borderless;
   wc->border.changed = 1;
   EC_CHANGED(wc);
   wc->depth = ec->depth;
   wc->override = ec->override;
   wc->placed = ec->placed;
   wc->input_only = ec->input_only;
   wc->border_size = ec->border_size;
   memcpy(&wc->icccm, &ec->icccm, sizeof(ec->icccm));
   memcpy(&wc->netwm, &ec->netwm, sizeof(ec->netwm));
   memcpy(&wc->e, &ec->e, sizeof(ec->e));
   ec->new_client = 1;
   e_comp->new_clients++;

   eina_hash_set(clients_win_hash, &win, wc);
   if (pwin)
     eina_hash_set(clients_win_hash, &pwin, wc);
   wc->visible = 1;
   wc->ignored = 0;
   if (ec->override)
     e_client_focus_stack_set(eina_list_remove(e_client_focus_stack_get(), wc));
   evas_object_name_set(wc->frame, evas_object_name_get(ec->frame));
   wc->x = ec->x, wc->y = ec->y;
   wc->client.x = ec->client.x, wc->client.y = ec->client.y;
   wc->w = ec->w, wc->h = ec->h;
   wc->client.w = ec->client.w, wc->client.h = ec->client.h;
   wc->layer = ec->layer;
   wc->netwm.sync.request = 1;
   evas_object_layer_set(wc->frame, evas_object_layer_get(ec->frame));
   evas_object_geometry_set(wc->frame, ec->x, ec->y, ec->w, ec->h);
   evas_object_show(wc->frame);
   e_object_del(E_OBJECT(ec));
   e_hints_window_visible_set(wc);
   _e_comp_x_client_stack(wc);
}
#endif

E_API inline E_Pixmap *
e_comp_x_client_pixmap_get(const E_Client *ec)
{
#ifdef HAVE_WAYLAND
   if (!e_pixmap_is_x(ec->pixmap))
     return e_comp_wl_client_xwayland_pixmap(ec);
#endif
   return ec->pixmap;
}

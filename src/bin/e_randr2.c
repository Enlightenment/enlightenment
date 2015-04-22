#include "e.h"

#define E_RANDR_CONFIG_VERSION 1

/////////////////////////////////////////////////////////////////////////
static Eina_Bool               _screen_closed(E_Randr2_Screen *s);
static void                    _animated_apply_abort(void);
static Eina_Bool               _cb_delay_timer(void *data);
static Eina_Bool               _cb_fade_animator(void *data);
static void                    _animated_apply(void);
static void                    _do_apply(void);
static void                    _info_free(E_Randr2 *r);
static E_Config_Randr2        *_config_load(void);
static void                    _config_free(E_Config_Randr2 *cfg);
static Eina_Bool               _config_save(E_Randr2 *r, E_Config_Randr2 *cfg);
static void                    _config_update(E_Randr2 *r, E_Config_Randr2 *cfg);
static void                    _config_apply(E_Randr2 *r, E_Config_Randr2 *cfg);
static int                     _config_screen_match_count(E_Randr2 *r, E_Config_Randr2 *cfg);
static char                   *_screens_fingerprint(E_Randr2 *r);
static Eina_Bool               _screens_differ(E_Randr2 *r1, E_Randr2 *r2);
static Eina_Bool               _cb_screen_change_delay(void *data);
static E_Randr2_Screen        *_screen_output_find(const char *out);
static E_Randr2_Screen        *_screen_id_find(const char *id);
static void                    _screen_config_takeover(void);
static void                    _screen_config_do(E_Randr2_Screen *s);
static void                    _screen_config_eval(void);
static void                    _screen_config_maxsize(void);

/////////////////////////////////////////////////////////////////////////
static E_Config_DD   *_e_randr2_cfg_edd = NULL;
static E_Config_DD   *_e_randr2_cfg_screen_edd = NULL;
static Eina_List     *_ev_handlers = NULL;
static Ecore_Timer   *_screen_delay_timer = NULL;
static Eina_Bool      event_screen = EINA_FALSE;
static Eina_Bool      event_ignore = EINA_FALSE;

/////////////////////////////////////////////////////////////////////////
EAPI E_Config_Randr2 *e_randr2_cfg = NULL;
EAPI E_Randr2        *e_randr2 = NULL;

EAPI int              E_EVENT_RANDR_CHANGE = 0;

/////////////////////////////////////////////////////////////////////////
// X11 backend
static Eina_Bool _output_init(void);
static void _output_shutdown(void);
static void _output_events_listen(void);
static void _output_events_unlisten(void);
static char *_output_screen_get(Ecore_X_Window root, Ecore_X_Randr_Output o);
static Ecore_X_Randr_Edid_Display_Interface_Type _output_conn_type_get(Ecore_X_Window root, Ecore_X_Randr_Output o);
static char *_output_name_get(Ecore_X_Window root, Ecore_X_Randr_Output o);
static Eina_Bool _is_lid_name(const char *name);
static E_Randr2 *_info_get(void);
static Eina_Bool _cb_screen_change(void *data, int type, void *event);
static Eina_Bool _cb_crtc_change(void *data, int type, void *event);
static Eina_Bool _cb_output_change(void *data, int type, void *event);
static Eina_Bool _output_name_find(Ecore_X_Window root, const char *name, Ecore_X_Randr_Output *outputs, int outputs_num, Ecore_X_Randr_Output *out_ret);
static Eina_Bool _output_exists(Ecore_X_Randr_Output out, Ecore_X_Randr_Crtc_Info *info);
static Eina_Bool _rotation_exists(int rot, Ecore_X_Randr_Crtc_Info *info);
static Ecore_X_Randr_Mode _mode_screen_find(Ecore_X_Window root, E_Randr2_Screen *s, Ecore_X_Randr_Output out);
static void _screen_config_apply(void);

/////////////////////////////////////////////////////////////////////////
EINTERN Eina_Bool
e_randr2_init(void)
{
   if (!_output_init()) return EINA_FALSE;
   // create data descriptors for config storage
   _e_randr2_cfg_screen_edd =
     E_CONFIG_DD_NEW("E_Config_Randr2_Screen", E_Config_Randr2_Screen);
#undef T
#undef D
#define T E_Config_Randr2_Screen
#define D _e_randr2_cfg_screen_edd
   E_CONFIG_VAL(D, T, id, STR);
   E_CONFIG_VAL(D, T, rel_to, STR);
   E_CONFIG_VAL(D, T, rel_align, DOUBLE);
   E_CONFIG_VAL(D, T, mode_refresh, DOUBLE);
   E_CONFIG_VAL(D, T, mode_w, UINT);
   E_CONFIG_VAL(D, T, mode_h, INT);
   E_CONFIG_VAL(D, T, rotation, INT);
   E_CONFIG_VAL(D, T, priority, INT);
   E_CONFIG_VAL(D, T, rel_mode, UCHAR);
   E_CONFIG_VAL(D, T, enabled, UCHAR);

   _e_randr2_cfg_edd = E_CONFIG_DD_NEW("E_Config_Randr2", E_Config_Randr2);
#undef T
#undef D
#define T E_Config_Randr2
#define D _e_randr2_cfg_edd
   E_CONFIG_VAL(D, T, version, INT);
   E_CONFIG_LIST(D, T, screens, _e_randr2_cfg_screen_edd);
   E_CONFIG_VAL(D, T, restore, UCHAR);
   E_CONFIG_VAL(D, T, ignore_hotplug_events, UCHAR);
   E_CONFIG_VAL(D, T, ignore_acpi_events, UCHAR);

   if (!E_EVENT_RANDR_CHANGE) E_EVENT_RANDR_CHANGE = ecore_event_type_new();
   // set up events from the driver
   _output_events_listen();
   // get current screen info
   e_randr2 = _info_get();
   // from screen info calculate screen max dimensions
   _screen_config_maxsize();
   // load config and apply it
   e_randr2_cfg = _config_load();
   // only apply if restore is set AND at least one configured screen
   // matches one we have
   if ((e_randr2_cfg->restore) &&
       (_config_screen_match_count(e_randr2, e_randr2_cfg) > 0))
     {
        _do_apply();
     }
   else
     {
        _config_update(e_randr2, e_randr2_cfg);
        e_randr2_config_save();
     }
   return EINA_TRUE;
}

EINTERN int
e_randr2_shutdown(void)
{
   _animated_apply_abort();
   // nuke any screen config delay handler
   if (_screen_delay_timer) ecore_timer_del(_screen_delay_timer);
   _screen_delay_timer = NULL;
   // stop listening to driver info
   _output_events_unlisten();
   // clear up all event handlers
   E_FREE_LIST(_ev_handlers, ecore_event_handler_del);
   // free up screen info
   _info_free(e_randr2);
   e_randr2 = NULL;
   _config_free(e_randr2_cfg);
   e_randr2_cfg = NULL;
   // free up data descriptors
   E_CONFIG_DD_FREE(_e_randr2_cfg_edd);
   E_CONFIG_DD_FREE(_e_randr2_cfg_screen_edd)
   _output_shutdown();
   return 1;
}

EAPI Eina_Bool
e_randr2_config_save(void)
{
   // save our config
   return _config_save(e_randr2, e_randr2_cfg);
}

EAPI void
e_randr2_config_apply(void)
{
   _animated_apply();
}

EAPI void
e_randr2_screeninfo_update(void)
{
   // re-fetch/update current screen info
   _info_free(e_randr2);
   e_randr2 = _info_get();
   _screen_config_maxsize();
}

/////////////////////////////////////////////////////////////////////////
static double _start_time = 0.0;
static Ecore_Animator *_fade_animator = NULL;
static Ecore_Timer *_apply_delay = NULL;
Eina_Bool _applying = EINA_FALSE;
static int _target_from = 0;
static int _target_to = 0;
static Evas_Object *_fade_obj = NULL;

static Eina_Bool
_screen_closed(E_Randr2_Screen *s)
{
   printf("RRR: check lid for %s...\n", s->info.name);
   if (!e_acpi_lid_is_closed()) return EINA_FALSE;
   if (s->info.is_lid)
     {
        printf("RRR:   is closed lid\n");
        return EINA_TRUE;
     }
   return EINA_FALSE;
}

static void
_animated_apply_abort(void)
{
   if (_apply_delay) ecore_timer_del(_apply_delay);
   if (_fade_animator) ecore_animator_del(_fade_animator);
   _apply_delay = NULL;
   _fade_animator = NULL;
   _applying = EINA_FALSE;
   _fade_obj = NULL;
}

static Eina_Bool
_cb_delay_timer(void *data EINA_UNUSED)
{
   _apply_delay = NULL;
   _target_from = 255;
   _target_to = 0;
   _start_time = ecore_loop_time_get();
   _fade_animator = ecore_animator_add(_cb_fade_animator, NULL);
   return EINA_FALSE;
}

static Eina_Bool
_cb_fade_animator(void *data EINA_UNUSED)
{
   double t = ecore_loop_time_get() - _start_time;
   int v;

   t = t / 0.5;
   if (t < 0.0) t = 0.0;
   v = _target_from + ((_target_to - _target_from) * t);
   if (t >= 1.0) v = _target_to;
   evas_object_color_set(_fade_obj, 0, 0, 0, v);
   if (v == _target_to)
     {
        if (_target_to == 255)
          {
             _apply_delay = ecore_timer_add(3.0, _cb_delay_timer, NULL);
             _do_apply();
          }
        else
          {
             evas_object_del(_fade_obj);
             _fade_obj = NULL;
             _applying = EINA_FALSE;
          }
        _fade_animator = NULL;
        return EINA_FALSE;
     }
   return EINA_TRUE;
}

static void
_animated_apply(void)
{
   Evas *e;

   // fade out, config, wait 3 seconds, fade back in
   if (_applying) return;
   _applying = EINA_TRUE;
   _start_time = ecore_loop_time_get();
   e = e_comp->evas;
   _fade_obj = evas_object_rectangle_add(e);
   evas_object_pass_events_set(_fade_obj, EINA_TRUE);
   evas_object_color_set(_fade_obj, 0, 0, 0, 0);
   evas_object_move(_fade_obj, 0, 0);
   evas_object_resize(_fade_obj, 999999, 999999);
   evas_object_layer_set(_fade_obj, EVAS_LAYER_MAX);
   evas_object_show(_fade_obj);
   _target_from = 0;
   _target_to = 255;
   _fade_animator = ecore_animator_add(_cb_fade_animator, NULL);
}

static void
_do_apply(void)
{
   // take current screen config and apply it to the driver
   printf("RRR: re-get info before applying..\n");
   _info_free(e_randr2);
   e_randr2 = _info_get();
   _screen_config_maxsize();
   printf("RRR: apply config...\n");
   _config_apply(e_randr2, e_randr2_cfg);
   printf("RRR: takeover config...\n");
   _screen_config_takeover();
   printf("RRR: eval config...\n");
   _screen_config_eval();
   printf("RRR: really apply config...\n");
   _screen_config_apply();
   printf("RRR: done config...\n");
}

static void
_info_free(E_Randr2 *r)
{
   E_Randr2_Screen *s;
   E_Randr2_Mode *m;

   if (!r) return;
   // free up our randr screen data
   EINA_LIST_FREE(r->screens, s)
     {
        free(s->id);
        free(s->info.screen);
        free(s->info.name);
        free(s->info.edid);
        EINA_LIST_FREE(s->info.modes, m) free(m);
        free(s->config.relative.to);
        free(s);
     }
   free(r);
}

static E_Config_Randr2 *
_config_load(void)
{
   E_Config_Randr2 *cfg;

   // load config and check if version is up to date
   cfg = e_config_domain_load("e_randr2", _e_randr2_cfg_edd);
   if (cfg)
     {
        if (cfg->version < E_RANDR_CONFIG_VERSION)
          {
             _config_free(cfg);
             cfg = NULL;
          }
        else
          {
             printf("RRR: loaded existing config\n");
             return cfg;
          }
     }

   // need new config
   cfg = calloc(1, sizeof(E_Config_Randr2));
   cfg->version = E_RANDR_CONFIG_VERSION;
   cfg->screens = NULL;
   cfg->restore = 1;
   cfg->ignore_hotplug_events = 0;
   cfg->ignore_acpi_events = 0;
   printf("RRR: fresh config\n");
   return cfg;
}

static void
_config_free(E_Config_Randr2 *cfg)
{
   E_Config_Randr2_Screen *cs;

   if (!cfg) return;
   // free config data
   EINA_LIST_FREE(cfg->screens, cs) free(cs);
   free(cfg);
}

static Eina_Bool
_config_save(E_Randr2 *r, E_Config_Randr2 *cfg)
{
   if ((!r) || (!cfg)) return EINA_FALSE;
   // save config struct to cfg file
   return e_config_domain_save("e_randr2", _e_randr2_cfg_edd, cfg);
}

static void
_config_update(E_Randr2 *r, E_Config_Randr2 *cfg)
{
   Eina_List *l;
   E_Randr2_Screen *s;
   E_Config_Randr2_Screen *cs;

   printf("--------------------------------------------------\n");
   EINA_LIST_FOREACH(r->screens, l, s)
     {
        printf("RRR: out id=%s:  connected=%i\n", s->id, s->info.connected);
        if ((!s->id) || (!s->info.connected) || (_screen_closed(s))) continue;
        cs = e_randr2_config_screen_find(s, cfg);
        if (!cs)
          {
             cs = calloc(1, sizeof(E_Config_Randr2_Screen));
             if (cs)
               {
                  cs->id = strdup(s->id);
                  cfg->screens = eina_list_prepend(cfg->screens, cs);
               }
          }
        if (cs)
          {
             if (s->config.relative.to)
               cs->rel_to = strdup(s->config.relative.to);
             cs->rel_align = s->config.relative.align;
             cs->mode_refresh = s->config.mode.refresh;
             cs->mode_w = s->config.mode.w;
             cs->mode_h = s->config.mode.h;
             cs->rotation = s->config.rotation;
             cs->priority = s->config.priority;
             cs->rel_mode = s->config.relative.mode;
             cs->enabled = s->config.enabled;
          }
     }
   printf("--------------------------------------------------\n");
}

static void
_config_apply(E_Randr2 *r, E_Config_Randr2 *cfg)
{
   Eina_List *l;
   E_Randr2_Screen *s;
   E_Config_Randr2_Screen *cs;

   if ((!r) || (!cfg)) return;
   EINA_LIST_FOREACH(r->screens, l, s)
     {
        printf("RRR: apply '%s'...\n", s->info.name);
        cs = NULL;
        if ((!_screen_closed(s)) && (s->info.connected))
          cs = e_randr2_config_screen_find(s, cfg);
        printf("RRR: connected =  %i\n", s->info.connected);
        if ((cs) && (cs->enabled))
          {
             printf("RRR: ... enabled\n");
             s->config.enabled = EINA_TRUE;
             s->config.mode.w = cs->mode_w;
             s->config.mode.h = cs->mode_h;
             s->config.mode.refresh = cs->mode_refresh;
             s->config.mode.preferred = EINA_FALSE;
             s->config.rotation = cs->rotation;
             s->config.priority = cs->priority;
             printf("RRR: ... priority = %i\n", cs->priority);
             free(s->config.relative.to);
             if (cs->rel_to) s->config.relative.to = strdup(cs->rel_to);
             else s->config.relative.to = NULL;
             s->config.relative.mode = cs->rel_mode;
             s->config.relative.align = cs->rel_align;
          }
        else
          {
             printf("RRR: ... disabled\n");
             s->config.enabled = EINA_FALSE;
             s->config.geom.x = 0;
             s->config.geom.y = 0;
             s->config.geom.w = 0;
             s->config.geom.h = 0;
             s->config.mode.w = 0;
             s->config.mode.h = 0;
             s->config.mode.refresh = 0.0;
             s->config.mode.preferred = EINA_FALSE;
             s->config.rotation = 0;
             s->config.priority = 0;
             free(s->config.relative.to);
             s->config.relative.to = NULL;
             s->config.relative.mode = E_RANDR2_RELATIVE_NONE;
             s->config.relative.align = 0.0;
          }
        s->config.configured = EINA_TRUE;
     }
}

static int
_config_screen_match_count(E_Randr2 *r, E_Config_Randr2 *cfg)
{
   Eina_List *l, *ll;
   E_Randr2_Screen *s;
   E_Config_Randr2_Screen *cs;
   int count = 0;

   EINA_LIST_FOREACH(cfg->screens, l, cs)
     {
        if (!cs->id) continue;
        EINA_LIST_FOREACH(r->screens, ll, s)
          {
             if ((!s->id) || (!s->info.connected) ||
                 (_screen_closed(s))) continue;
             if (!strcmp(cs->id, s->id)) count++;
          }
     }
   return count;
}

static char *
_screens_fingerprint(E_Randr2 *r)
{
   Eina_List *l;
   E_Randr2_Screen *s;
   Eina_Strbuf *buf;
   char *str;

   buf = eina_strbuf_new();
   if (!buf) return NULL;
   EINA_LIST_FOREACH(r->screens, l, s)
     {
        if (!s->id) eina_strbuf_append(buf, ":NULL:");
        else
          {
             eina_strbuf_append(buf, ":");
             eina_strbuf_append(buf, s->id);
             eina_strbuf_append(buf, ":");
             if (s->info.lid_closed) eina_strbuf_append(buf, ":LC:");
             else eina_strbuf_append(buf, ":LO:");
          }
     }
   str = eina_strbuf_string_steal(buf);
   eina_strbuf_free(buf);
   return str;
}

static Eina_Bool
_screens_differ(E_Randr2 *r1, E_Randr2 *r2)
{
   char *s1, *s2;
   Eina_Bool changed = EINA_FALSE;
   Eina_List *l, *ll;
   E_Randr2_Screen *s, *ss;

   // check monitor outputs and edids, plugged in things
   s1 = _screens_fingerprint(r1);
   s2 = _screens_fingerprint(r2);
   if ((!s1) && (!s2)) return EINA_FALSE;
   printf("RRR: check fingerprint...\n");
   if ((s1) && (s2) && (strcmp(s1, s2))) changed = EINA_TRUE;
   printf("RRR: ... fingerprint says %i\n", changed);
   free(s1);
   free(s2);
   // check screen config
   EINA_LIST_FOREACH(r2->screens, l, s)
     {
        if (!s->id) continue;
        EINA_LIST_FOREACH(r2->screens, ll, ss)
          {
             if ((ss->id) && (!strcmp(s->id, ss->id))) break;
             ss = NULL;
          }
        if (!ss) changed = EINA_TRUE;
        else if ((s->config.geom.x != ss->config.geom.x) ||
                 (s->config.geom.y != ss->config.geom.y) ||
                 (s->config.geom.w != ss->config.geom.w) ||
                 (s->config.geom.h != ss->config.geom.h) ||
                 (s->config.mode.w != ss->config.mode.w) ||
                 (s->config.mode.h != ss->config.mode.h) ||
                 (s->config.enabled != ss->config.enabled) ||
                 (s->config.rotation != ss->config.rotation) ||
                 (s->info.lid_closed != ss->info.lid_closed))
          changed = EINA_TRUE;
     }
   printf("RRR: changed = %i\n", changed);
   return changed;
}

static Eina_Bool
_cb_screen_change_delay(void *data EINA_UNUSED)
{
   _screen_delay_timer = NULL;
   printf("RRR: ... %i %i\n", event_screen, event_ignore);
   // if we had a screen plug/unplug etc. event and we shouldnt ignore it...
   if ((event_screen) && (!event_ignore))
     {
        E_Randr2 *rtemp;
        Eina_Bool change = EINA_FALSE;

        printf("RRR: reconfigure screens due to event...\n");
        rtemp = _info_get();
        if (rtemp)
          {
             if (_screens_differ(e_randr2, rtemp)) change = EINA_TRUE;
             _info_free(rtemp);
          }
        printf("RRR: change = %i\n", change);
        // we plugged or unplugged some monitor - re-apply config so
        // known screens can be coonfigured
        if (change) e_randr2_config_apply();
     }
   // update screen info after the above apply or due to external changes
   e_randr2_screeninfo_update();
   // tell the rest of e some screen reconfigure thing happened
   ecore_event_add(E_EVENT_RANDR_CHANGE, NULL, NULL, NULL);
   event_screen = EINA_FALSE;
   event_ignore = EINA_FALSE;
   return EINA_FALSE;
}

static E_Randr2_Screen *
_screen_output_find(const char *out)
{
   E_Randr2_Screen *s;
   Eina_List *l;
   EINA_LIST_FOREACH(e_randr2->screens, l, s)
     {
        if (!strcmp(s->info.name, out)) return s;
     }
   return NULL;
}

static E_Randr2_Screen *
_screen_id_find(const char *id)
{
   E_Randr2_Screen *s;
   Eina_List *l;
   EINA_LIST_FOREACH(e_randr2->screens, l, s)
     {
        if (!strcmp(s->id, id)) return s;
     }
   return NULL;
}

static void
_screen_config_takeover(void)
{
   Eina_List *l;
   E_Randr2_Screen *s;
   EINA_LIST_FOREACH(e_randr2->screens, l, s)
     {
        s->config.configured = EINA_TRUE;
     }
}

static E_Config_Randr2_Screen *_config_screen_string_find(E_Config_Randr2 *cfg, const char *id);
static E_Randr2_Screen *_screen_fuzzy_fallback_find(E_Config_Randr2 *cfg, const char *id);

static E_Config_Randr2_Screen *
_config_screen_string_find(E_Config_Randr2 *cfg, const char *id)
{
   Eina_List *l;
   E_Config_Randr2_Screen *cs;

   if ((!id) || (!cfg)) return NULL;
   EINA_LIST_FOREACH(cfg->screens, l, cs)
     {
        if (!cs->id) continue;
        if (!strcmp(cs->id, id)) return cs;
     }
   return NULL;
}

static E_Randr2_Screen *
_screen_fuzzy_fallback_find(E_Config_Randr2 *cfg, const char *id)
{
   E_Randr2_Screen *s = NULL;
   char *p, *name;

   // strip out everythng in the string from / on as that is edid
   // and fall back to finding just the output name in the rel
   // to identifier, rather than the specific screen id
   name = alloca(strlen(id) + 1);
   strcpy(name, id);
   if ((p = strchr(name, '/'))) *p = 0;

   s = _screen_id_find(id);
   if (!s) s = _screen_output_find(name);
   if (!s)
     {
        E_Config_Randr2_Screen *cs;

        cs = _config_screen_string_find(cfg, id);
        if ((cs) && (cs->id)) return _screen_fuzzy_fallback_find(cfg, cs->id);
     }
   return s;
}

static E_Config_Randr2_Screen *
_config_screen_clone_resolve(E_Config_Randr2 *cfg, const char *id, int *x, int *y)
{
   E_Config_Randr2_Screen *cs;
   E_Randr2_Screen *s;
   char *p, *name;

   cs = _config_screen_string_find(cfg, id);
   if (!cs) return NULL;

   name = alloca(strlen(cs->id) + 1);
   strcpy(name, cs->id);
   if ((p = strchr(name, '/'))) *p = 0;

   s = _screen_id_find(cs->id);
   if (!s) s = _screen_output_find(name);
   if (!s)
     {
        if ((cs->rel_mode == E_RANDR2_RELATIVE_CLONE) && (cs->rel_to))
          return _config_screen_clone_resolve(cfg, cs->rel_to, x, y);
        return NULL;
     }
   _screen_config_do(s);
   *x = s->config.geom.x;
   *y = s->config.geom.y;
   return cs;
}

static int _config_do_recurse = 0;

static void
_screen_config_do(E_Randr2_Screen *s)
{
   E_Randr2_Screen *s2 = NULL;

   printf("RRR: screen do '%s'\n", s->info.name);
   if (_config_do_recurse > 10)
     {
        ERR("screen config loop!");
        return;
     }
   _config_do_recurse++;
   // if screen has a dependency...
   if ((s->config.relative.mode != E_RANDR2_RELATIVE_UNKNOWN) &&
       (s->config.relative.mode != E_RANDR2_RELATIVE_NONE) &&
       (s->config.relative.to))
     {
        // if this screen is relative TO something (clone or left/right etc.
        // then calculate what it is relative to first
        s2 = _screen_fuzzy_fallback_find(e_randr2_cfg, s->config.relative.to);
        printf("RRR: '%s' is relative to '%s'\n", s->info.name, s2 ? s2->info.name : "NONE");
        if (s2)
          {
             _screen_config_do(s2);
             if (!s2->config.enabled) s2 = NULL;
          }
     }
   s->config.geom.x = 0;
   s->config.geom.y = 0;
   if ((s->config.rotation == 0) || (s->config.rotation == 180))
     {
        s->config.geom.w = s->config.mode.w;
        s->config.geom.h = s->config.mode.h;
     }
   else
     {
        s->config.geom.w = s->config.mode.h;
        s->config.geom.h = s->config.mode.w;
     }
   if (s2)
     {
        if (s->config.relative.mode == E_RANDR2_RELATIVE_CLONE)
          {
             printf("RRR: clone relative\n");
             s->config.geom.x = s2->config.geom.x;
             s->config.geom.y = s2->config.geom.y;
             s->config.geom.w = s2->config.geom.w;
             s->config.geom.h = s2->config.geom.h;
             s->config.mode.w = s2->config.mode.w;
             s->config.mode.h = s2->config.mode.h;
             s->config.rotation = s2->config.rotation;
             s->config.mode.refresh = s2->config.mode.refresh;
          }
        else if (s->config.relative.mode == E_RANDR2_RELATIVE_TO_LEFT)
          {
             printf("RRR: to left relative\n");
             s->config.geom.x = s2->config.geom.x - s->config.geom.w;
             s->config.geom.y = s2->config.geom.y +
             ((s2->config.geom.h - s->config.geom.h) *
              s->config.relative.align);
          }
        else if (s->config.relative.mode == E_RANDR2_RELATIVE_TO_RIGHT)
          {
             printf("RRR: to right relative\n");
             s->config.geom.x = s2->config.geom.x + s2->config.geom.w;
             s->config.geom.y = s2->config.geom.y +
             ((s2->config.geom.h - s->config.geom.h) *
              s->config.relative.align);
          }
        else if (s->config.relative.mode == E_RANDR2_RELATIVE_TO_ABOVE)
          {
             printf("RRR: to above relative\n");
             s->config.geom.x = s2->config.geom.x +
             ((s2->config.geom.w - s->config.geom.w) *
              s->config.relative.align);
             s->config.geom.y = s2->config.geom.y - s->config.geom.h;
          }
        else if (s->config.relative.mode == E_RANDR2_RELATIVE_TO_BELOW)
          {
             printf("RRR: to below relative\n");
             s->config.geom.x = s2->config.geom.x +
             ((s2->config.geom.w - s->config.geom.w) *
              s->config.relative.align);
             s->config.geom.y = s2->config.geom.y + s2->config.geom.h;
          }
     }
   else
     {
        if ((s->config.relative.mode == E_RANDR2_RELATIVE_CLONE) &&
            (s->config.relative.to))
          {
             E_Config_Randr2_Screen *cs;
             int x = 0, y = 0;

             cs = _config_screen_clone_resolve(e_randr2_cfg,
                                               s->config.relative.to, &x, &y);
             printf("RRR: clone relative - config %p\n", cs);
             if (cs)
               {
                  s->config.geom.x = x;
                  s->config.geom.y = y;
                  s->config.mode.w = cs->mode_w;
                  s->config.mode.h = cs->mode_h;
                  s->config.rotation = cs->rotation;
                  s->config.mode.refresh = cs->mode_refresh;
                  if ((cs->rotation == 0) || (cs->rotation == 180))
                    {
                       s->config.geom.w = s->config.mode.w;
                       s->config.geom.h = s->config.mode.h;
                    }
                  else
                    {
                       s->config.geom.w = s->config.mode.h;
                       s->config.geom.h = s->config.mode.w;
                    }
               }
          }
     }
   _config_do_recurse--;
}

static void
_screen_config_eval(void)
{
   Eina_List *l;
   E_Randr2_Screen *s;
   int minx, miny, maxx, maxy;

   EINA_LIST_FOREACH(e_randr2->screens, l, s)
     {
        if (s->config.configured) _screen_config_do(s);
     }
   minx = 65535;
   miny = 65535;
   maxx = -65536;
   maxy = -65536;
   printf("RRR:--------------------------------\n");
   EINA_LIST_FOREACH(e_randr2->screens, l, s)
     {
        if (!s->config.enabled) continue;
        if (s->config.geom.x < minx) minx = s->config.geom.x;
        if (s->config.geom.y < miny) miny = s->config.geom.y;
        if ((s->config.geom.x + s->config.geom.w) > maxx)
          maxx = s->config.geom.x + s->config.geom.w;
        if ((s->config.geom.y + s->config.geom.h) > maxy)
          maxy = s->config.geom.y + s->config.geom.h;
        printf("RRR: s: '%s' @ %i %i - %ix%i\n",
               s->info.name,
               s->config.geom.x, s->config.geom.y,
               s->config.geom.w, s->config.geom.h);
     }
   printf("RRR:--- %i %i -> %i %i\n", minx, miny, maxx, maxy);
   EINA_LIST_FOREACH(e_randr2->screens, l, s)
     {
        s->config.geom.x -= minx;
        s->config.geom.y -= miny;
     }
   e_randr2->w = maxx - minx;
   e_randr2->h = maxy - miny;
}

static void
_screen_config_maxsize(void)
{
   Eina_List *l;
   E_Randr2_Screen *s;
   int maxx, maxy;

   maxx = -65536;
   maxy = -65536;
   printf("RRR:-------------------------------- 2\n");
   EINA_LIST_FOREACH(e_randr2->screens, l, s)
     {
        if (!s->config.enabled) continue;
        if ((s->config.geom.x + s->config.geom.w) > maxx)
          maxx = s->config.geom.x + s->config.geom.w;
        if ((s->config.geom.y + s->config.geom.h) > maxy)
          maxy = s->config.geom.y + s->config.geom.h;
        printf("RRR: '%s': %i %i %ix%i\n",
               s->info.name,
               s->config.geom.x, s->config.geom.y,
               s->config.geom.w, s->config.geom.h);
     }
   printf("RRR: result max: %ix%i\n", maxx, maxy);
   e_randr2->w = maxx;
   e_randr2->h = maxy;
}

/////////////////////////////////////////////////////////////////////////
// X11 backend
static Eina_Bool
_output_init(void)
{
   // is randr extn there?
   return ecore_x_randr_query();
}

static void
_output_shutdown(void)
{
}

static void
_output_events_listen(void)
{
   // add handler for randr screen change events
   E_LIST_HANDLER_APPEND(_ev_handlers, ECORE_X_EVENT_SCREEN_CHANGE,
                         _cb_screen_change, NULL);
   E_LIST_HANDLER_APPEND(_ev_handlers, ECORE_X_EVENT_RANDR_CRTC_CHANGE,
                         _cb_crtc_change, NULL);
   E_LIST_HANDLER_APPEND(_ev_handlers, ECORE_X_EVENT_RANDR_OUTPUT_CHANGE,
                         _cb_output_change, NULL);
   // if it's 1.2 or better then we can select for these events
   if (ecore_x_randr_version_get() >= E_RANDR_VERSION_1_2)
     {
        Ecore_X_Window root = ecore_x_window_root_first_get();
        ecore_x_randr_events_select(root, EINA_TRUE);
     }
}

static void
_output_events_unlisten(void)
{
   // clear up event listening
   if (ecore_x_randr_version_get() >= E_RANDR_VERSION_1_2)
     {
        Ecore_X_Window root = ecore_x_window_root_first_get();
        ecore_x_randr_events_select(root, EINA_FALSE);
     }
}

static char *
_output_screen_get(Ecore_X_Window root, Ecore_X_Randr_Output o)
{
   // get the name of the screen - likely a model name or manufacturer name
   char *name;
   unsigned long len = 0;
   unsigned char *edid = ecore_x_randr_output_edid_get(root, o, &len);
   if (!edid) return NULL;
   name = ecore_x_randr_edid_display_name_get(edid, len);
   free(edid);
   return name;
}

static Ecore_X_Randr_Edid_Display_Interface_Type
_output_conn_type_get(Ecore_X_Window root, Ecore_X_Randr_Output o)
{
   // get what kind of connector (hdmi, dvi, displayport etc.) - vga is 
   Ecore_X_Randr_Edid_Display_Interface_Type type;
   unsigned long len = 0;
   unsigned char *edid = ecore_x_randr_output_edid_get(root, o, &len);
   if (!edid) return ECORE_X_RANDR_EDID_DISPLAY_INTERFACE_UNDEFINED;
   type = ecore_x_randr_edid_display_interface_type_get(edid, len);
   free(edid);
   return type;
}

static char *
_output_name_get(Ecore_X_Window root, Ecore_X_Randr_Output o)
{
   // get the output name - like connector (hdmi-0, dp1, dvi-0-1 etc.)
   char *name = ecore_x_randr_output_name_get(root, o, NULL);
   if (name) return name;
   return _output_screen_get(root, o);
}

static Eina_Bool
_is_lid_name(const char *name)
{
   // a fixed list of possible built in connector names - likely a laptop
   // or device internal display
   if (!name) return EINA_FALSE;
   if      (strstr(name, "LVDS")) return EINA_TRUE;
   else if (strstr(name, "lvds")) return EINA_TRUE;
   else if (strstr(name, "Lvds")) return EINA_TRUE;
   else if (strstr(name, "LCD"))  return EINA_TRUE;
   else if (strstr(name, "eDP"))  return EINA_TRUE;
   else if (strstr(name, "edp"))  return EINA_TRUE;
   else if (strstr(name, "EDP"))  return EINA_TRUE;
   return EINA_FALSE;
}

static char *
_edid_string_get(Ecore_X_Window root, Ecore_X_Randr_Output o)
{
   // convert the edid binary data into a hex string so we can use it as
   // part of a name
   unsigned char *edid = NULL;
   unsigned long len = 0;
   char *edid_str = NULL;

   edid = ecore_x_randr_output_edid_get(root, o, &len);
   if (edid)
     {
        unsigned int k, kk;

        edid_str = malloc((len * 2) + 1);
        if (edid_str)
          {
             const char *hexch = "0123456789abcdef";

             for (kk = 0, k = 0; k < len; k++)
               {
                  edid_str[kk    ] = hexch[(edid[k] >> 4) & 0xf];
                  edid_str[kk + 1] = hexch[ edid[k]       & 0xf];
                  kk += 2;
               }
             edid_str[kk] = 0;
          }
        free(edid);
     }
   return edid_str;
}

static E_Randr2_Screen *
_info_unconf_primary_find(E_Randr2 *r)
{
   Eina_List *l;
   E_Randr2_Screen *s, *s_primary = NULL;
   int priority = 0;

   EINA_LIST_FOREACH(r->screens, l, s)
     {
        if (!((s->config.enabled) &&
              (s->config.mode.w > 0) && (s->config.mode.h > 0) &&
              (s->config.geom.w > 0) && (s->config.geom.h > 0)))
          continue;
        if (s->config.priority > priority)
          {
             s_primary = s;
             priority = s->config.priority;
          }
     }
   return s_primary;
}

static E_Randr2_Screen *
_info_unconf_left_find(E_Randr2 *r)
{
   Eina_List *l;
   E_Randr2_Screen *s, *s_left = NULL;
   int left_x = 0x7fffffff;
   int left_size = 0;

   EINA_LIST_FOREACH(r->screens, l, s)
     {
        if (!((s->config.enabled) &&
              (s->config.mode.w > 0) && (s->config.mode.h > 0) &&
              (s->config.geom.w > 0) && (s->config.geom.h > 0)))
          continue;
        if ((s->config.geom.x <= left_x) &&
            ((s->config.geom.w * s->config.geom.h) > left_size))
          {
             left_size = s->config.geom.w * s->config.geom.h;
             left_x = s->config.geom.x;
             s_left = s;
          }
     }
   return s_left;
}

static E_Randr2_Screen *
_info_unconf_closest_find(E_Randr2 *r, E_Randr2_Screen *s2, Eina_Bool configured)
{
   Eina_List *l;
   E_Randr2_Screen *s, *s_sel = NULL;
   int dist = 0x7fffffff;
   int dx, dy;

   EINA_LIST_FOREACH(r->screens, l, s)
     {
        if (s == s2) continue;
        if (!((s->config.enabled) &&
              (s->config.mode.w > 0) && (s->config.mode.h > 0) &&
              (s->config.geom.w > 0) && (s->config.geom.h > 0)))
          continue;
        if ((!configured) &&
            (s->config.relative.mode != E_RANDR2_RELATIVE_UNKNOWN))
          continue;
        else if ((configured) &&
                 (s->config.relative.mode == E_RANDR2_RELATIVE_UNKNOWN))
          continue;
        dx = (s->config.geom.x + (s->config.geom.w / 2)) -
          (s2->config.geom.x + (s2->config.geom.w / 2));
        dy = (s->config.geom.y + (s->config.geom.h / 2)) -
          (s2->config.geom.y + (s2->config.geom.h / 2));
        dx = sqrt((dx * dx) + (dy * dy));
        if (dx < dist)
          {
             s_sel = s;
             dist = dx;
          }
     }
   return s_sel;
}

static void
_info_relative_fixup(E_Randr2 *r)
{
   E_Randr2_Screen *s, *s2;
   int d, dx, dy;

   s = _info_unconf_primary_find(r);
   if (s) s->config.relative.mode = E_RANDR2_RELATIVE_NONE;
   else
     {
        s = _info_unconf_left_find(r);
        if (!s) return;
        s->config.relative.mode = E_RANDR2_RELATIVE_NONE;
     }
   for (;;)
     {
        // find the next screen that is closest to the last one we configured
        /// that is still not configured yet
        s = _info_unconf_closest_find(r, s, EINA_FALSE);
        if (!s) break;
        s2 = _info_unconf_closest_find(r, s, EINA_TRUE);
        // fix up s->config.relative.mode, s->config.relative.to and
        // s->config.relative.align to match (as closely as possible)
        // the geometry given - config s relative to s2
        if (!s2) s->config.relative.mode = E_RANDR2_RELATIVE_NONE;
        else
          {
             s->config.relative.to = strdup(s2->id);
             s->config.relative.align = 0.0;
             s->config.relative.mode = E_RANDR2_RELATIVE_NONE;
             if ((s->config.geom.x + s->config.geom.w) <=
                 s2->config.geom.x)
               {
                  s->config.relative.mode = E_RANDR2_RELATIVE_TO_LEFT;
                  d = s->config.geom.h - s2->config.geom.h;
                  dy = s2->config.geom.y - s->config.geom.y;
                  if (d != 0)
                    s->config.relative.align = ((double)dy) / ((double)d);
               }
             else if (s->config.geom.x >=
                      (s2->config.geom.x + s2->config.geom.w))
               {
                  s->config.relative.mode = E_RANDR2_RELATIVE_TO_RIGHT;
                  d = s->config.geom.h - s2->config.geom.h;
                  dy = s2->config.geom.y - s->config.geom.y;
                  if (d != 0)
                    s->config.relative.align = ((double)dy) / ((double)d);
               }
             else if ((s->config.geom.y + s->config.geom.h) <=
                      s2->config.geom.y)
               {
                  s->config.relative.mode = E_RANDR2_RELATIVE_TO_ABOVE;
                  d = s->config.geom.w - s2->config.geom.w;
                  dx = s2->config.geom.x - s->config.geom.x;
                  if (d != 0)
                    s->config.relative.align = ((double)dx) / ((double)d);
               }
             else if (s->config.geom.y >=
                      (s2->config.geom.y + s2->config.geom.h))
               {
                  s->config.relative.mode = E_RANDR2_RELATIVE_TO_BELOW;
                  d = s->config.geom.w - s2->config.geom.w;
                  dx = s2->config.geom.x - s->config.geom.x;
                  if (d != 0)
                    s->config.relative.align = ((double)dx) / ((double)d);
               }
             else if ((s->config.geom.x == s2->config.geom.x) &&
                      (s->config.geom.y == s2->config.geom.y) &&
                      (s->config.geom.w == s2->config.geom.w) &&
                      (s->config.geom.h == s2->config.geom.h))
               {
                  s->config.relative.mode = E_RANDR2_RELATIVE_CLONE;
               }
             if (s->config.relative.align < 0.0)
               s->config.relative.align = 0.0;
             else if (s->config.relative.align > 1.0)
               s->config.relative.align = 1.0;
          }
     }
}

static E_Randr2 *
_info_get(void)
{
   Ecore_X_Randr_Crtc *crtcs = NULL;
   Ecore_X_Randr_Output *outputs = NULL;
   int crtcs_num = 0, outputs_num = 0, i, j, k;
   Ecore_X_Window root = ecore_x_window_root_first_get();
   E_Randr2 *r = calloc(1, sizeof(E_Randr2));
   if (!r) return NULL;

   printf("RRR: ................. info get!\n");
   // do this to force xrandr to update its content
   ecore_x_randr_config_timestamp_get(root);
   ecore_x_randr_screen_size_range_get(root, NULL, NULL, NULL, NULL);
   ecore_x_sync();

   crtcs = ecore_x_randr_crtcs_get(root, &crtcs_num);
   outputs = ecore_x_randr_outputs_get(root, &outputs_num);

   for (i = 0; i < outputs_num; i++)
     {
        Ecore_X_Randr_Mode *modes;
        Ecore_X_Randr_Edid_Display_Interface_Type conn;
        int modes_num = 0, modes_pref = 0, priority;
        E_Config_Randr2_Screen *cs;
        E_Randr2_Screen *s = calloc(1, sizeof(E_Randr2_Screen));
        if (!s) continue;

        s->info.name = _output_name_get(root, outputs[i]);
        printf("RRR: .... out %s\n", s->info.name);
        if (!s->info.name)
          {
             free(s);
             continue;
          }
        s->info.screen = _output_screen_get(root, outputs[i]);
        s->info.edid = _edid_string_get(root, outputs[i]);
        if (s->info.edid)
        s->id = malloc(strlen(s->info.name) + 1 + strlen(s->info.edid) + 1);
        else
        s->id = malloc(strlen(s->info.name) + 1 + 1);
        if (!s->id)
          {
             free(s->info.screen);
             free(s->info.edid);
             free(s);
             continue;
          }
        strcpy(s->id, s->info.name);
        strcat(s->id, "/");
        if (s->info.edid) strcat(s->id, s->info.edid);
        conn = _output_conn_type_get(root, outputs[i]);
        if (conn == ECORE_X_RANDR_EDID_DISPLAY_INTERFACE_UNDEFINED)
          s->info.connector = E_RANDR2_CONNECTOR_UNDEFINED;
        else if (conn == ECORE_X_RANDR_EDID_DISPLAY_INTERFACE_DVI)
          s->info.connector = E_RANDR2_CONNECTOR_DVI;
        else if (conn == ECORE_X_RANDR_EDID_DISPLAY_INTERFACE_HDMI_A)
          s->info.connector = E_RANDR2_CONNECTOR_HDMI_A;
        else if (conn == ECORE_X_RANDR_EDID_DISPLAY_INTERFACE_HDMI_B)
          s->info.connector = E_RANDR2_CONNECTOR_HDMI_B;
        else if (conn == ECORE_X_RANDR_EDID_DISPLAY_INTERFACE_MDDI)
          s->info.connector = E_RANDR2_CONNECTOR_MDDI;
        else if (conn == ECORE_X_RANDR_EDID_DISPLAY_INTERFACE_DISPLAY_PORT)
          s->info.connector = E_RANDR2_CONNECTOR_DISPLAY_PORT;
        s->info.is_lid = _is_lid_name(s->info.name);
        s->info.lid_closed = s->info.is_lid && e_acpi_lid_is_closed();
        printf("RRR: ...... lid_closed = %i (%i && %i)\n", s->info.lid_closed, s->info.is_lid, e_acpi_lid_is_closed());
        if (ecore_x_randr_output_connection_status_get(root, outputs[i]) ==
            ECORE_X_RANDR_CONNECTION_STATUS_CONNECTED)
          s->info.connected = EINA_TRUE;
        printf("RRR: ...... connected %i\n", s->info.connected);
        if (ecore_x_randr_output_backlight_level_get(root, outputs[i]) >= 0.0)
          s->info.backlight = EINA_TRUE;
        ecore_x_randr_output_size_mm_get(root, outputs[i],
                                         &(s->info.size.w), &(s->info.size.h));
        modes = ecore_x_randr_output_modes_get(root, outputs[i],
                                               &modes_num, &modes_pref);
        printf("RRR: ...... modes %p\n", modes);
        if (modes)
          {
             for (j = 0; j < modes_num; j++)
               {
                  Ecore_X_Randr_Mode_Info *minfo =
                    ecore_x_randr_mode_info_get(root, modes[j]);
                  if (minfo)
                    {
                       E_Randr2_Mode *m = calloc(1, sizeof(E_Randr2_Mode));
                       if (m)
                         {
                            m->w = minfo->width;
                            m->h = minfo->height;
                            m->refresh =
                            (double)minfo->dotClock /
                            (double)(minfo->hTotal * minfo->vTotal);
                            if (j == (modes_pref - 1))
                              m->preferred = EINA_TRUE;
                            s->info.modes = eina_list_append(s->info.modes, m);
                         }
                       ecore_x_randr_mode_info_free(minfo);
                    }
               }
             free(modes);
          }
        cs = NULL;
        priority = 0;
        if (e_randr2_cfg) cs = e_randr2_config_screen_find(s, e_randr2_cfg);
        if (cs)
          priority = cs->priority;
        else if (ecore_x_randr_primary_output_get(root) == outputs[i])
          priority = 100;
        s->config.priority = priority;
        for (j = 0; j < crtcs_num; j++)
          {
             Eina_Bool ok, possible;
             Ecore_X_Randr_Crtc_Info *info =
               ecore_x_randr_crtc_info_get(root, crtcs[j]);
             if (info)
               {
                  ok = EINA_FALSE;
                  possible = EINA_FALSE;
                  for (k = 0; k < info->noutput; k++)
                    {
                       if (info->outputs[k] == outputs[i])
                         {
                            ok = EINA_TRUE;
                            break;
                         }
                    }
                  if (!ok)
                    {
                       for (k = 0; k < info->npossible; k++)
                         {
                            if (info->possible[k] == outputs[i])
                              {
                                 ok = EINA_TRUE;
                                 possible = EINA_TRUE;
                                 break;
                              }
                         }
                    }
                  if (ok)
                    {
                       if (!possible)
                         {
                            Ecore_X_Randr_Mode_Info *minfo;

                            s->config.geom.x = info->x;
                            s->config.geom.y = info->y;
                            s->config.geom.w = info->width;
                            s->config.geom.h = info->height;
                            if (info->rotation & ECORE_X_RANDR_ORIENTATION_ROT_0)
                              s->config.rotation = 0;
                            else if (info->rotation & ECORE_X_RANDR_ORIENTATION_ROT_90)
                              s->config.rotation = 90;
                            else if (info->rotation & ECORE_X_RANDR_ORIENTATION_ROT_180)
                              s->config.rotation = 180;
                            else if (info->rotation & ECORE_X_RANDR_ORIENTATION_ROT_270)
                              s->config.rotation = 270;
                            minfo = ecore_x_randr_mode_info_get(root,
                                                                info->mode);
                            if (minfo)
                              {
                                 s->config.enabled = EINA_TRUE;
                                 s->config.mode.w = minfo->width;
                                 s->config.mode.h = minfo->height;
                                 s->config.mode.refresh =
                                   (double)minfo->dotClock /
                                   (double)(minfo->hTotal * minfo->vTotal);
                                 ecore_x_randr_mode_info_free(minfo);
                              }
                            printf("RRR: '%s' %i %i %ix%i\n",
                                   s->info.name,
                                   s->config.geom.x, s->config.geom.y,
                                   s->config.geom.w, s->config.geom.h);
                         }
                       if (info->rotations & ECORE_X_RANDR_ORIENTATION_ROT_0)
                         s->info.can_rot_0 = EINA_TRUE;
                       if (info->rotations & ECORE_X_RANDR_ORIENTATION_ROT_90)
                         s->info.can_rot_90 = EINA_TRUE;
                       if (info->rotations & ECORE_X_RANDR_ORIENTATION_ROT_180)
                         s->info.can_rot_180 = EINA_TRUE;
                       if (info->rotations & ECORE_X_RANDR_ORIENTATION_ROT_270)
                         s->info.can_rot_270 = EINA_TRUE;
                    }
                  ecore_x_randr_crtc_info_free(info);
               }
          }
        r->screens = eina_list_append(r->screens, s);
     }

   free(outputs);
   free(crtcs);

   _info_relative_fixup(r);
   return r;
}

static Eina_Bool
_cb_screen_change(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_X_Event_Screen_Change *ev = event;
   printf("RRR: CB screen change...\n");
   ecore_x_randr_config_timestamp_get(ev->root);
   ecore_x_randr_screen_current_size_get(ev->root, NULL, NULL, NULL, NULL);
   ecore_x_sync();
   if (!e_randr2_cfg->ignore_hotplug_events)
     e_randr2_screen_refresh_queue(EINA_TRUE);
   return EINA_TRUE;
}

static Eina_Bool
_cb_crtc_change(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_X_Event_Randr_Crtc_Change *ev = event;
   printf("RRR: CB crtc change...\n");
   ecore_x_randr_config_timestamp_get(ev->win);
   ecore_x_randr_screen_current_size_get(ev->win, NULL, NULL, NULL, NULL);
   ecore_x_sync();
   if (!e_randr2_cfg->ignore_hotplug_events)
     e_randr2_screen_refresh_queue(EINA_FALSE);
   return EINA_TRUE;
}

static Eina_Bool
_cb_output_change(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_X_Event_Randr_Output_Change *ev = event;
   printf("RRR: CB output change...\n");
   ecore_x_randr_config_timestamp_get(ev->win);
   ecore_x_randr_screen_current_size_get(ev->win, NULL, NULL, NULL, NULL);
   ecore_x_sync();
   if (!e_randr2_cfg->ignore_hotplug_events)
     e_randr2_screen_refresh_queue(EINA_TRUE);
   return EINA_TRUE;
}

static Eina_Bool
_output_name_find(Ecore_X_Window root, const char *name, Ecore_X_Randr_Output *outputs, int outputs_num, Ecore_X_Randr_Output *out_ret)
{
   int i;
   char *n;

   for (i = 0; i < outputs_num; i++)
     {
        n = _output_name_get(root, outputs[i]);
        if ((n) && (!strcmp(n, name)))
          {
             free(n);
             *out_ret = outputs[i];
             return EINA_TRUE;
          }
        free(n);
     }
   return EINA_FALSE;
}

static Eina_Bool
_output_exists(Ecore_X_Randr_Output out, Ecore_X_Randr_Crtc_Info *info)
{
   int i;

   for (i = 0; i < info->npossible; i++)
     {
        if (out == info->possible[i]) return EINA_TRUE;
     }
   return EINA_FALSE;
}

static Eina_Bool
_rotation_exists(int rot, Ecore_X_Randr_Crtc_Info *info)
{
   if ((rot == 0) && (info->rotations & ECORE_X_RANDR_ORIENTATION_ROT_0))
     return EINA_TRUE;
   if ((rot == 90) && (info->rotations & ECORE_X_RANDR_ORIENTATION_ROT_90))
     return EINA_TRUE;
   if ((rot == 180) && (info->rotations & ECORE_X_RANDR_ORIENTATION_ROT_180))
     return EINA_TRUE;
   if ((rot == 270) && (info->rotations & ECORE_X_RANDR_ORIENTATION_ROT_270))
     return EINA_TRUE;
   return EINA_FALSE;
}

static Ecore_X_Randr_Mode
_mode_screen_find(Ecore_X_Window root, E_Randr2_Screen *s, Ecore_X_Randr_Output out)
{
   Ecore_X_Randr_Mode_Info *minfo;
   Ecore_X_Randr_Mode mode = 0, *modes;
   int modes_num = 0, modes_pref = 0, distance = 0x7fffffff;
   int diff, i;
   double refresh;

   modes = ecore_x_randr_output_modes_get(root, out, &modes_num, &modes_pref);
   if (!modes) printf("RRR: modes for '%s' FETCH FAILED!!!\n", s->info.name);
   printf("RRR: modes for '%s' are %p [%i]\n", s->info.name, modes, modes_num);
   if (modes)
     {
        for (i = 0; i < modes_num; i++)
          {
             minfo = ecore_x_randr_mode_info_get(root, modes[i]);
             if (!minfo) continue;
             refresh = (double)minfo->dotClock /
               (double)(minfo->hTotal * minfo->vTotal);
             diff =
               (100 * abs(s->config.mode.w - minfo->width)) +
               (100 * abs(s->config.mode.h - minfo->height)) +
               abs((100 * s->config.mode.refresh) - (100 * refresh));
             if (diff < distance)
               {
                  mode = modes[i];
                  distance = diff;
               }
             ecore_x_randr_mode_info_free(minfo);
          }
        free(modes);
     }
   return mode;
}

static void
_screen_config_apply(void)
{
   Eina_List *l;
   E_Randr2_Screen *s;
   Ecore_X_Window root = ecore_x_window_root_first_get();
   int minw, minh, maxw, maxh, nw, nh, pw, ph, ww, hh;
   Ecore_X_Randr_Crtc *crtcs = NULL;
   Ecore_X_Randr_Output *outputs = NULL, out, *outconf;
   E_Randr2_Screen **screenconf;
   int crtcs_num = 0, outputs_num = 0, i, numout;
   Ecore_X_Randr_Crtc_Info *info;
   int top_priority = 0;

   ecore_x_grab();
   // set virtual resolution
   nw = e_randr2->w;
   nh = e_randr2->h;
   ecore_x_randr_screen_size_range_get(root, &minw, &minh, &maxw, &maxh);
   ecore_x_randr_screen_current_size_get(root, &pw, &ph, NULL, NULL);
     {
        int dww = 0, dhh = 0, dww2 = 0, dhh2 = 0;
        ecore_x_randr_screen_current_size_get(root, &dww, &dhh, &dww2, &dhh2);
        printf("RRR: cur size: %ix%i\n", dww, dhh);
     }
   printf("RRR: size range: %ix%i -> %ix%i\n", minw, minh, maxw, maxh);
   if (nw > maxw) nw = maxw;
   if (nh > maxh) nh = maxh;
   if (nw < minw) nw = minw;
   if (nh < minh) nh = minh;
   ww = nw; if (nw < pw) ww = pw;
   hh = nh; if (nh < ph) hh = ph;
   ecore_x_randr_screen_current_size_set(root, ww, hh, -1, -1);
     {
        int dww = 0, dhh = 0, dww2 = 0, dhh2 = 0;
        ecore_x_randr_screen_current_size_get(root, &dww, &dhh, &dww2, &dhh2);
        printf("RRR: cur size: %ix%i\n", dww, dhh);
     }
   printf("RRR: set vsize: %ix%i\n", nw, nh);

   crtcs = ecore_x_randr_crtcs_get(root, &crtcs_num);
   outputs = ecore_x_randr_outputs_get(root, &outputs_num);

   if ((crtcs) && (outputs))
     {
        outconf = alloca(crtcs_num * sizeof(Ecore_X_Randr_Output));
        screenconf = alloca(crtcs_num * sizeof(E_Randr2_Screen *));
        memset(outconf, 0, crtcs_num * sizeof(Ecore_X_Randr_Output));
        memset(screenconf, 0, crtcs_num * sizeof(E_Randr2_Screen *));

        // decide which outputs get which crtcs
        EINA_LIST_FOREACH(e_randr2->screens, l, s)
          {
             printf("RRR: find output for '%s'\n", s->info.name);
             // XXX: find clones and set them as outputs in an array
             if ((s->config.configured) &&
                 (_output_name_find(root, s->info.name, outputs,
                                    outputs_num, &out)))
               {
                  printf("RRR:   enabled: %i\n", s->config.enabled);
                  if (s->config.enabled)
                    {
                       if (s->config.priority > top_priority)
                         top_priority = s->config.priority;
                       for (i = 0; i < crtcs_num; i++)
                         {
                            if (!outconf[i])
                              {
                                 printf("RRR:     crtc slot empty: %i\n", i);
                                 info = ecore_x_randr_crtc_info_get(root,
                                                                    crtcs[i]);
                                 if (info)
                                   {
                                      if (_output_exists(out, info) &&
                                          _rotation_exists(s->config.rotation,
                                                           info))
                                        {
                                           printf("RRR:       assign slot out: %x\n", out);
                                           outconf[i] = out;
                                           screenconf[i] = s;
                                           ecore_x_randr_crtc_info_free(info);
                                           break;
                                        }
                                   }
                              }
                         }
                    }
               }
          }
        numout = 0;
        for (i = 0; i < crtcs_num; i++)
          {
             if (outconf[i]) numout++;
          }
        if (numout)
          {
        // set up a crtc to drive each output (or not)
        for (i = 0; i < crtcs_num; i++)
          {
             // XXX: find clones and set them as outputs in an array
             if (outconf[i])
               {
                  Ecore_X_Randr_Orientation orient =
                    ECORE_X_RANDR_ORIENTATION_ROT_0;
                  Ecore_X_Randr_Mode mode;

                  mode = _mode_screen_find(root, screenconf[i], outconf[i]);
                  if (screenconf[i]->config.rotation == 0)
                    orient = ECORE_X_RANDR_ORIENTATION_ROT_0;
                  else if (screenconf[i]->config.rotation == 90)
                    orient = ECORE_X_RANDR_ORIENTATION_ROT_90;
                  else if (screenconf[i]->config.rotation == 180)
                    orient = ECORE_X_RANDR_ORIENTATION_ROT_180;
                  else if (screenconf[i]->config.rotation == 270)
                    orient = ECORE_X_RANDR_ORIENTATION_ROT_270;
                  printf("RRR: crtc on: %i = '%s'     @ %i %i    - %ix%i orient %i mode %x out %x\n",
                         i, screenconf[i]->info.name,
                         screenconf[i]->config.geom.x,
                         screenconf[i]->config.geom.y,
                         screenconf[i]->config.geom.w,
                         screenconf[i]->config.geom.h,
                         orient, mode, outconf[i]);
                  if (!ecore_x_randr_crtc_settings_set
                      (root, crtcs[i], &(outconf[i]), 1,
                       screenconf[i]->config.geom.x,
                       screenconf[i]->config.geom.y,
                       mode, orient))
                    printf("RRR:   failed to set crtc!!!!!!\n");
                  ecore_x_randr_crtc_panning_area_set
                    (root, crtcs[i],
                     screenconf[i]->config.geom.x,
                     screenconf[i]->config.geom.y,
                     screenconf[i]->config.geom.w,
                     screenconf[i]->config.geom.h);
                  if (screenconf[i]->config.priority == top_priority)
                    {
                       ecore_x_randr_primary_output_set(root, outconf[i]);
                       top_priority = -1;
                    }
               }
             else
               {
                  printf("RRR: crtc off: %i\n", i);
                  ecore_x_randr_crtc_settings_set
                    (root, crtcs[i], NULL, 0, 0, 0, 0,
                     ECORE_X_RANDR_ORIENTATION_ROT_0);
               }
          }
          }
        else
          {
             printf("RRR: EERRRRRROOOORRRRRRR no outputs to configure!\n");
          }
     }
   free(outputs);
   free(crtcs);

   printf("RRR: set vsize: %ix%i\n", nw, nh);
   ecore_x_randr_screen_current_size_set(root, nw, nh, -1, -1);
     {
        int dww = 0, dhh = 0, dww2 = 0, dhh2 = 0;
        ecore_x_randr_screen_current_size_get(root, &dww, &dhh, &dww2, &dhh2);
        printf("RRR: cur size: %ix%i\n", dww, dhh);
//        ecore_x_randr_screen_reset(root);
//        ecore_x_randr_screen_current_size_set(root, nw, nh, -1, -1);
//        ecore_x_sync();
//        ecore_x_randr_screen_current_size_get(root, &dww, &dhh, &dww2, &dhh2);
//        printf("RRR: cur size: %ix%i\n", dww,d hh);
     }
   ecore_x_randr_screen_size_range_get(root, NULL, NULL, NULL, NULL);
   ecore_x_ungrab();
   ecore_x_sync();

   // ignore the next batch of randr events - we caused them ourselves
   // XXX: a problem. thew first time we configure the screen we may not
   // get any events back to clear the ignore flag below, so only apply
   // here if the randr config now doesnt match what we want to set up.
//   event_ignore = EINA_TRUE;
}

EAPI void
e_randr2_screen_refresh_queue(Eina_Bool lid_event)
{
   // delay handling of screen shances as they can come in in a series over
   // time and thus we can batch up responding to them by waiting 1.0 sec
   if (_screen_delay_timer)
     ecore_timer_reset(_screen_delay_timer);
   else
     _screen_delay_timer = ecore_timer_add(1.0, _cb_screen_change_delay, NULL);
   event_screen = !!lid_event;
}

EAPI E_Config_Randr2_Screen *
e_randr2_config_screen_find(E_Randr2_Screen *s, E_Config_Randr2 *cfg)
{
   Eina_List *l;
   E_Config_Randr2_Screen *cs;

   if ((!s) || (!cfg)) return NULL;
   if (!s->id) return NULL;
   EINA_LIST_FOREACH(cfg->screens, l, cs)
     {
        if (!cs->id) continue;
        if (!strcmp(cs->id, s->id)) return cs;
     }
   return NULL;
}

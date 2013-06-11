#include "e.h"

/* local function prototypes */
static void _e_bg_signal(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *source __UNUSED__);

/* local variables */
EAPI int E_EVENT_BG_UPDATE = 0;

EINTERN int 
e_bg_init(void)
{
   Eina_List *l = NULL;
   E_Config_Desktop_Background *cfbg = NULL;

   if (e_config->desktop_default_background)
     e_filereg_register(e_config->desktop_default_background);

   EINA_LIST_FOREACH(e_config->desktop_backgrounds, l, cfbg)
     {
        if (!cfbg) continue;
        e_filereg_register(cfbg->file);
     }

   E_EVENT_BG_UPDATE = ecore_event_type_new();

   return 1;
}

EINTERN int 
e_bg_shutdown(void)
{
   Eina_List *l = NULL;
   E_Config_Desktop_Background *cfbg = NULL;

   if (e_config->desktop_default_background)
     e_filereg_deregister(e_config->desktop_default_background);

   EINA_LIST_FOREACH(e_config->desktop_backgrounds, l, cfbg)
     {
        if (!cfbg) continue;
        e_filereg_deregister(cfbg->file);
     }

   return 1;
}

EAPI void 
e_bg_zone_update(E_Zone *zone, E_Bg_Transition transition)
{
   const char *bgfile = "", *trans = "";
   E_Desk *desk;

   if (transition == E_BG_TRANSITION_START) 
     trans = e_config->transition_start;
   else if (transition == E_BG_TRANSITION_DESK) 
     trans = e_config->transition_desk;
   else if (transition == E_BG_TRANSITION_CHANGE) 
     trans = e_config->transition_change;
   if ((!trans) || (!trans[0])) transition = E_BG_TRANSITION_NONE;

   desk = e_desk_current_get(zone);
   if (desk)
     bgfile = e_bg_file_get(zone->container->num, zone->num, desk->x, desk->y);
   else
     bgfile = e_bg_file_get(zone->container->num, zone->num, -1, -1);

   printf("Bg File: %s\n", bgfile);

   if (zone->o_bg) 
     {
        const char *pfile = "";

        edje_object_file_get(zone->o_bg, &pfile, NULL);
        if (!strcmp(pfile, bgfile)) return;
     }

   if (transition == E_BG_TRANSITION_NONE)
     {
        if (zone->o_bg) 
          {
             evas_object_del(zone->o_bg);
             zone->o_bg = NULL;
          }
     }
   else 
     {
        char buff[PATH_MAX];

        if (zone->o_bg)
          {
             if (zone->o_prev_bg) evas_object_del(zone->o_prev_bg);
             zone->o_prev_bg = zone->o_bg;
             if (zone->o_trans) evas_object_del(zone->o_trans);
             zone->o_trans = NULL;
             zone->o_bg = NULL;
          }
        zone->o_trans = edje_object_add(zone->container->bg_evas);
        evas_object_data_set(zone->o_trans, "e_zone", zone);
        snprintf(buff, sizeof(buff), "e/transitions/%s", trans);
        e_theme_edje_object_set(zone->o_trans, "base/theme/transitions", buff);
        edje_object_signal_callback_add(zone->o_trans, "e,state,done", "*", 
                                        _e_bg_signal, zone);
        evas_object_move(zone->o_trans, zone->x, zone->y);
        evas_object_resize(zone->o_trans, zone->w, zone->h);
        evas_object_layer_set(zone->o_trans, -1);
        evas_object_clip_set(zone->o_trans, zone->o_bg_clip);
        evas_object_show(zone->o_trans);
     }

   if (eina_str_has_extension(bgfile, ".edj"))
     {
        zone->o_bg = edje_object_add(zone->container->bg_evas);
        evas_object_move(zone->o_bg, zone->x, zone->y);
        evas_object_resize(zone->o_bg, zone->w, zone->h);
        evas_object_data_set(zone->o_bg, "e_zone", zone);
        edje_object_file_set(zone->o_bg, bgfile, "e/desktop/background");
     }
   else 
     {
        /* TODO */
     }

   if (transition == E_BG_TRANSITION_NONE)
     {
        evas_object_move(zone->o_bg, zone->x, zone->y);
        evas_object_resize(zone->o_bg, zone->w, zone->h);
        evas_object_layer_set(zone->o_bg, -1);
     }
   evas_object_clip_set(zone->o_bg, zone->o_bg_clip);
   evas_object_show(zone->o_bg);

   if (transition != E_BG_TRANSITION_NONE)
     {
	edje_extern_object_max_size_set(zone->o_prev_bg, 65536, 65536);
	edje_extern_object_min_size_set(zone->o_prev_bg, 0, 0);
	edje_object_part_swallow(zone->o_trans, "e.swallow.bg.old",
				 zone->o_prev_bg);
	edje_extern_object_max_size_set(zone->o_bg, 65536, 65536);
	edje_extern_object_min_size_set(zone->o_bg, 0, 0);
	edje_object_part_swallow(zone->o_trans, "e.swallow.bg.new",
				 zone->o_bg);
	edje_object_signal_emit(zone->o_trans, "e,action,start", "e");
     }
}

EAPI const char *
e_bg_file_get(int con_num, int zone_num, int desk_x, int desk_y)
{
   const E_Config_Desktop_Background *cfbg;
   Eina_List *l, *entries;
   const char *bgfile = "";
   char *entry;
   Eina_Bool ok = EINA_FALSE;

   cfbg = e_bg_config_get(con_num, zone_num, desk_x, desk_y);

   /* fall back to default */
   if (cfbg)
     {
	bgfile = cfbg->file;
	if (bgfile)
	  {
	     if (bgfile[0] != '/')
	       {
		  const char *bf;

		  bf = e_path_find(path_backgrounds, bgfile);
		  if (bf) bgfile = bf;
	       }
	  }
     }
   else
     {
	bgfile = e_config->desktop_default_background;
	if (bgfile)
	  {
	     if (bgfile[0] != '/')
	       {
		  const char *bf;

		  bf = e_path_find(path_backgrounds, bgfile);
		  if (bf) bgfile = bf;
	       }
	  }
        if ((bgfile) && (eina_str_has_extension(bgfile, ".edj")))
          {
             entries = edje_file_collection_list(bgfile);
             if (entries)
               {
                  EINA_LIST_FOREACH(entries, l, entry)
                    {
                       if (!strcmp(entry, "e/desktop/background"))
                         {
                            ok = EINA_TRUE;
                            break;
                         }
                    }
                  edje_file_collection_list_free(entries);
               }
          }
        else if ((bgfile) && (bgfile[0])) 
          ok = EINA_TRUE;

	if (!ok)
	  bgfile = e_theme_edje_file_get("base/theme/background",
					 "e/desktop/background");
     }

   return bgfile;
}

EAPI const E_Config_Desktop_Background *
e_bg_config_get(int con_num, int zone_num, int desk_x, int desk_y)
{
   Eina_List *l, *ll, *entries;
   E_Config_Desktop_Background *bg = NULL, *cfbg = NULL;
   const char *bgfile = "";
   char *entry;
   int current_spec = 0; /* how specific the setting is - we want the least general one that applies */

   /* look for desk specific background. */
   if ((con_num >= 0) || (zone_num >= 0) || (desk_x >= 0) || (desk_y >= 0))
     {
	EINA_LIST_FOREACH(e_config->desktop_backgrounds, l, cfbg)
	  {
	     int spec;
             
	     if (!cfbg) continue;
	     spec = 0;
	     if (cfbg->container == con_num) spec++;
	     else if (cfbg->container >= 0) continue;
	     if (cfbg->zone == zone_num) spec++;
	     else if (cfbg->zone >= 0) continue;
	     if (cfbg->desk_x == desk_x) spec++;
	     else if (cfbg->desk_x >= 0) continue;
	     if (cfbg->desk_y == desk_y) spec++;
	     else if (cfbg->desk_y >= 0) continue;

	     if (spec <= current_spec) continue;
	     bgfile = cfbg->file;
	     if (bgfile)
	       {
		  if (bgfile[0] != '/')
		    {
		       const char *bf;

		       bf = e_path_find(path_backgrounds, bgfile);
		       if (bf) bgfile = bf;
		    }
	       }
             if (eina_str_has_extension(bgfile, ".edj"))
               {
                  entries = edje_file_collection_list(bgfile);
                  if (entries)
                    {
                       EINA_LIST_FOREACH(entries, ll, entry)
                         {
                            if (!strcmp(entry, "e/desktop/background"))
                              {
                                 bg = cfbg;
                                 current_spec = spec;
                              }
                         }
                       edje_file_collection_list_free(entries);
                    }
               }
             else
               {
                  bg = cfbg;
                  current_spec = spec;
               }
	  }
     }

   return bg;
}

EAPI void 
e_bg_default_set(const char *file)
{
   /* E_Event_Bg_Update *ev; */
   Eina_Bool changed;

   file = eina_stringshare_add(file);
   changed = file != e_config->desktop_default_background;

   if (!changed)
     {
	eina_stringshare_del(file);
	return;
     }

   if (e_config->desktop_default_background)
     {
	e_filereg_deregister(e_config->desktop_default_background);
	eina_stringshare_del(e_config->desktop_default_background);
     }

   if (file)
     {
	e_filereg_register(file);
	e_config->desktop_default_background = file;
     }
   else
     e_config->desktop_default_background = NULL;

   /* ev = E_NEW(E_Event_Bg_Update, 1); */
   /* ev->container = -1; */
   /* ev->zone = -1; */
   /* ev->desk_x = -1; */
   /* ev->desk_y = -1; */
   /* ecore_event_add(E_EVENT_BG_UPDATE, ev, _e_bg_event_bg_update_free, NULL); */
}

EAPI void 
e_bg_add(int con, int zone, int dx, int dy, const char *file)
{
   const Eina_List *l;
   E_Config_Desktop_Background *cfbg;

   file = eina_stringshare_add(file);

   EINA_LIST_FOREACH(e_config->desktop_backgrounds, l, cfbg)
     {
	if ((cfbg) && (cfbg->container == con) && (cfbg->zone == zone) &&
	    (cfbg->desk_x == dx) && (cfbg->desk_y == dy) &&
	    (cfbg->file == file))
	  {
	     eina_stringshare_del(file);
	     return;
	  }
     }

   e_bg_del(con, zone, dx, dy);

   cfbg = E_NEW(E_Config_Desktop_Background, 1);
   cfbg->container = con;
   cfbg->zone = zone;
   cfbg->desk_x = dx;
   cfbg->desk_y = dy;
   cfbg->file = file;
   e_config->desktop_backgrounds = 
     eina_list_append(e_config->desktop_backgrounds, cfbg);

   e_filereg_register(cfbg->file);
}

EAPI void 
e_bg_del(int con, int zone, int dx, int dy)
{
   Eina_List *l;
   E_Config_Desktop_Background *cfbg;

   EINA_LIST_FOREACH(e_config->desktop_backgrounds, l, cfbg)
     {
	if (!cfbg) continue;
	if ((cfbg->container == con) && (cfbg->zone == zone) &&
	    (cfbg->desk_x == dx) && (cfbg->desk_y == dy))
	  {
	     e_config->desktop_backgrounds = 
               eina_list_remove_list(e_config->desktop_backgrounds, l);
	     e_filereg_deregister(cfbg->file);
	     if (cfbg->file) eina_stringshare_del(cfbg->file);
	     free(cfbg);
	     break;
	  }
     }
}

EAPI void 
e_bg_update(void)
{
   Eina_List *l, *ll, *lll;
   E_Manager *man;
   E_Container *con;
   E_Zone *zone;

   EINA_LIST_FOREACH(e_manager_list(), l, man)
     {
	EINA_LIST_FOREACH(man->containers, ll, con)
	  {
	     EINA_LIST_FOREACH(con->zones, lll, zone)
	       {
		  e_zone_bg_reconfigure(zone);
	       }
	  }
     }
}

/* local functions */
static void
_e_bg_signal(void *data, Evas_Object *obj __UNUSED__, const char *emission __UNUSED__, const char *source __UNUSED__)
{
   E_Zone *zone;

   zone = data;
   if (zone->o_prev_bg)
     {
	evas_object_del(zone->o_prev_bg);
	zone->o_prev_bg = NULL;
     }
   if (zone->o_trans)
     {
	evas_object_del(zone->o_trans);
	zone->o_trans = NULL;
     }
   evas_object_move(zone->o_bg, zone->x, zone->y);
   evas_object_resize(zone->o_bg, zone->w, zone->h);
   evas_object_layer_set(zone->o_bg, -1);
   evas_object_clip_set(zone->o_bg, zone->o_bg_clip);
   evas_object_show(zone->o_bg);
}

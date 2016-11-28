#include <Eet.h>
#include <e.h>

#define BRYCE_MIGRATION 1

#include "../time/config_descriptor.h"
#include "../clock/config_descriptor.h"
#include "../ibar/config_descriptor.h"
#include "../luncher/config_descriptor.h"

typedef struct _E_Gadget_Config
{
   int id;
   int zone;
   const char *type;
   struct {
        const char *name;
   } style;
   double x;
   double y;
   double w;
   double h;
} E_Gadget_Config;

typedef struct _E_Gadget_Site
{
   E_Gadget_Site_Gravity gravity;
   E_Gadget_Site_Orient orient;
   E_Gadget_Site_Anchor anchor;
   unsigned char autoadd;
   const char *name;
   Eina_List *gadgets;
} E_Gadget_Site;

typedef struct _E_Gadget_Sites
{
   Eina_List *sites;
} E_Gadget_Sites;

typedef struct _Bryce
{
   const char *name;
   const char *style;
   unsigned int zone;
   int size;
   unsigned int layer;
   unsigned char autosize;
   unsigned char autohide;
   E_Gadget_Site_Orient orient;
   E_Gadget_Site_Anchor anchor;
   unsigned int version;
} Bryce;

typedef struct _Bryces
{
   Eina_List *bryces;
} Bryces;



E_Config *e_config = NULL;
static E_Gadget_Sites *_sites = NULL;
static Bryces *_bryces = NULL;

E_Config_DD *e_remember_edd;
static Eet_Data_Descriptor *_edd_bryces;
static Eet_Data_Descriptor *_edd_bryce;
static Eet_Data_Descriptor *_edd_sites;
static Eet_Data_Descriptor *_edd_gadget_site;
static Eet_Data_Descriptor *_edd_gadget_config;

static void
_init(const char *path)
{
   Eet_File *f;
   char buf[PATH_MAX];

   eet_init();

   e_config_descriptor_init(EINA_FALSE);
   clock_config_descriptor_init();
   time_config_descriptor_init();
   ibar_config_descriptor_init();
   luncher_config_descriptor_init();

   snprintf(buf, sizeof(buf), "%s/e.cfg", path);
   f = eet_open(buf, EET_FILE_MODE_READ);
   if (!f)
     {
        printf("Can't open config file\n");
     }

   e_config = eet_data_read(f, e_config_descriptor_get(), "config");
   if (!e_config)
     {
        fprintf(stderr, "Error on loading config file\n");
        eet_close(f);
        return;
     }
   eet_close(f);


}

static void
_bryce_init(const char *path)
{
   Eet_Data_Descriptor_Class eddc_gadget_config, eddc_gadget_site, eddc_sites, eddc_bryce, eddc_bryces;
   char buf[PATH_MAX];
   Eet_File *f;

   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc_gadget_config, E_Gadget_Config);
   _edd_gadget_config = eet_data_descriptor_stream_new(&eddc_gadget_config);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_gadget_config, E_Gadget_Config, "id", id, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_gadget_config, E_Gadget_Config, "zone", zone, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_gadget_config, E_Gadget_Config, "type", type, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_gadget_config, E_Gadget_Config, "style.name", style.name, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_gadget_config, E_Gadget_Config, "x", x, EET_T_DOUBLE);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_gadget_config, E_Gadget_Config, "y", y, EET_T_DOUBLE);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_gadget_config, E_Gadget_Config, "w", w, EET_T_DOUBLE);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_gadget_config, E_Gadget_Config, "h", h, EET_T_DOUBLE);

   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc_gadget_site, E_Gadget_Site);
   _edd_gadget_site = eet_data_descriptor_stream_new(&eddc_gadget_site);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_gadget_site, E_Gadget_Site, "gravity", gravity, EET_T_UINT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_gadget_site, E_Gadget_Site, "orient", orient, EET_T_UINT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_gadget_site, E_Gadget_Site, "anchor", anchor, EET_T_UINT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_gadget_site, E_Gadget_Site, "autoadd", autoadd, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_gadget_site, E_Gadget_Site, "name", name, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_LIST(_edd_gadget_site, E_Gadget_Site, "gadgets", gadgets, _edd_gadget_config);

   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc_sites, E_Gadget_Sites);
   _edd_sites = eet_data_descriptor_stream_new(&eddc_sites);
   EET_DATA_DESCRIPTOR_ADD_LIST(_edd_sites, E_Gadget_Sites, "sites", sites, _edd_gadget_site);

   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc_bryce, Bryce);
   _edd_bryce = eet_data_descriptor_stream_new(&eddc_bryce);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_bryce, Bryce, "name", name, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_bryce, Bryce, "style", style, EET_T_STRING);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_bryce, Bryce, "zone", zone, EET_T_UINT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_bryce, Bryce, "size", size, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_bryce, Bryce, "layer", layer, EET_T_UINT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_bryce, Bryce, "autosize", autosize, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_bryce, Bryce, "autohide", autohide, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_bryce, Bryce, "orient", orient, EET_T_UINT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_bryce, Bryce, "anchor", anchor, EET_T_UINT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_edd_bryce, Bryce, "version", version, EET_T_UINT);

   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc_bryces, Bryces);
   _edd_bryces = eet_data_descriptor_stream_new(&eddc_bryces);
   EET_DATA_DESCRIPTOR_ADD_LIST(_edd_bryces, Bryces, "bryces", bryces, _edd_bryce);


   snprintf(buf, sizeof(buf), "%s/e_gadgets_sites.cfg", path);
   f = eet_open(buf, EET_FILE_MODE_READ);
   if (!f)
     {
        printf("Can't open config file\n");
     }

   _sites = eet_data_read(f, _edd_sites, "config");
   if (!_sites)
     {
        fprintf(stderr, "Error on loading config file\n");
        eet_close(f);
        return;
     }
   eet_close(f);

   printf("gadget site loaded %d\n", eina_list_count(_sites->sites));
   snprintf(buf, sizeof(buf), "%s/e_bryces.cfg", path);
   f = eet_open(buf, EET_FILE_MODE_READ);
   if (!f)
     {
        printf("Can't open config file\n");
     }

   _bryces = eet_data_read(f, _edd_bryces, "config");
   if (!_bryces)
     {
        fprintf(stderr, "Error on loading config bryce file\n");
        eet_close(f);
        return;
     }
   eet_close(f);
}

static void
_bryce_shutdown(const char *path)
{
   Eet_File *f;
   char buf[PATH_MAX];

   snprintf(buf, sizeof(buf), "%s/e_gadgets_sites.cfg", path);
   f = eet_open(buf, EET_FILE_MODE_READ_WRITE);
   if (!f)
     {
        printf("Can't open config file\n");
     }

   eet_data_write(f, _edd_sites, "config", _sites, 1);
   eet_close(f);

   snprintf(buf, sizeof(buf), "%s/e_bryces.cfg", path);
   f = eet_open(buf, EET_FILE_MODE_READ_WRITE);
   if (!f)
     {
        printf("Can't open config file\n");
     }

   eet_data_write(f, _edd_bryces, "config", _bryces, 1);
   eet_close(f);
}

typedef int (*Migrate_Cb)(const char *path, const char *id);

typedef struct _Bryce_Migration_Module_Compat{
     const char *shelf;
     const char *bryce;
     Migrate_Cb migrate;
} Bryce_Migration_Module_Compat;


static int
_start_migrate(const char *path, const char *id)
{
   return 0;
}

static int
_pager_migrate(const char *path, const char *id)
{
   return 0;
}

static int
_ibar_migrate(const char *path, const char *id)
{
   Eet_File *f;
   Ibar_Config *ibar;
   Eina_List *l;
   Ibar_Config_Item *it;
   Luncher_Config *luncher_config;
   Luncher_Config_Item *luncher_it;
   char buf[PATH_MAX];

   snprintf(buf, sizeof(buf), "%s/module.ibar.cfg", path);
   printf("%s open %s\n", path, buf);
   f = eet_open(buf, EET_FILE_MODE_READ);
   ibar = eet_data_read(f, ibar_config_descriptor_get(), "config");
   eet_close(f);

   EINA_LIST_FOREACH(ibar->items, l, it)
     {
        if (it->id == id) break;
     }

   if (l)
     {
        printf("Found Ibar config!\n");
        printf("Ibar dir %s\n", it->dir);
        snprintf(buf, sizeof(buf), "%s/module.luncher.cfg", path);
        f = eet_open(buf, EET_FILE_MODE_READ);
        luncher_config = eet_data_read(f, luncher_config_descriptor_get(), "config");
        eet_close(f);
        luncher_it = calloc(1, sizeof(Luncher_Config_Item));
        luncher_it->id = eina_list_count(luncher_config->items) + 1;
        luncher_it->dir = eina_stringshare_ref(it->dir);
        luncher_it->style = eina_stringshare_add("default");
        luncher_config->items = eina_list_append(luncher_config->items, luncher_it);
        f = eet_open(buf, EET_FILE_MODE_READ_WRITE);
        eet_data_write(f, luncher_config_descriptor_get(), "config",
                       luncher_config, 1);
        eet_close(f);
        return luncher_it->id;
     }

   return -1;
}

static int
_clock_migrate(const char *path, const char *id)
{
   Eet_File *f, *ff;
   Clock_Config *clock;
   Time_Config *time_config;
   Eina_List *l;
   Clock_Config_Item *it;
   Time_Config_Item *time_it;
   char buf[PATH_MAX];

   snprintf(buf, sizeof(buf), "%s/module.clock.cfg", path);
   f = eet_open(buf, EET_FILE_MODE_READ);
   clock = eet_data_read(f, clock_config_descriptor_get(), "config");
   eet_close(f);

   EINA_LIST_FOREACH(clock->items, l, it)
     {
        if (it->id == id) break;
     }
   if (l)
     {
        snprintf(buf, sizeof(buf), "%s/module.time.cfg", path);
        ff = eet_open(buf, EET_FILE_MODE_READ);
        time_config = eet_data_read(ff, time_config_descriptor_get(), "config");
        eet_close(ff);
        time_it = calloc(1, sizeof(Time_Config_Item));
        time_it->id = eina_list_count(time_config->items) + 1;
        time_it->weekend.start = it->weekend.start;
        time_it->weekend.len = it->weekend.len;
        time_it->week.start = it->week.start;
        time_it->digital_clock = it->digital_clock ? 1 : 0;
        time_it->digital_24h = it->digital_24h ? 1 : 0;
        time_it->show_seconds = it->show_seconds ? 1 : 0;
        time_it->show_date = it->show_date ? 1 : 0;
        time_it->time_str[0] = eina_stringshare_add("%I:%M");
        time_it->time_str[1] = eina_stringshare_add("%F");

        time_config->items = eina_list_append(time_config->items, time_it);

        ff = eet_open(buf, EET_FILE_MODE_READ_WRITE);
        eet_data_write(ff, time_config_descriptor_get(), "config",
                       time_config, 1);
        eet_close(ff);

        return time_it->id;

     }
   return -1;
}

static Bryce_Migration_Module_Compat _compat[] =
{
     {"start", "Start", _start_migrate},
     {"pager", "Pager Gadget", _pager_migrate},
     {"ibar", "Luncher Bar", _ibar_migrate},
     {"clock", "Digital Clock", _clock_migrate}
};



static void
_migrate_shelf(const char *path, E_Config_Shelf *shelf, const char *bryce, E_Gadget_Site_Anchor anchor, E_Gadget_Site_Gravity gravity, E_Gadget_Site_Orient orient)
{
   Eina_List *l, *ll;
   E_Config_Gadcon *gc;
   E_Config_Gadcon_Client *client;
   int i;
   int id;
   E_Gadget_Site *site;
   E_Gadget_Config *gcfg;
   char buf[PATH_MAX];
   Bryce *b;

   site = calloc(1, sizeof(E_Gadget_Site));

   printf("Starting migration %s %s\n", path, shelf->name);

   EINA_LIST_FOREACH(e_config->gadcons, l, gc)
     {
        printf("gadcon %s\n", gc->name);
        if (!strcmp(gc->name, shelf->name))
          {
             EINA_LIST_FOREACH(gc->clients, ll, client)
               {
                  id = -1;
                  for (i = 0; i < (sizeof(_compat) / sizeof(_compat[0])); ++i)
                    {
                       if (!strcmp(_compat[i].shelf, client->name))
                         {
                            id = _compat[i].migrate(path, client->id);
                            break;
                         }
                    }
                  if (id < 0)
                    {
                       printf("Fail to migrate %s\n", client->name);
                       continue;
                    }
                  gcfg = calloc(1, sizeof(E_Gadget_Config));
                  gcfg->id = id;
                  gcfg->type = eina_stringshare_add(_compat[i].bryce);
                  site->gadgets = eina_list_append(site->gadgets, gcfg);
               }
             e_config->gadcons = eina_list_remove_list(e_config->gadcons, l);
             break;
          }
     }
   if (l)
     {
        snprintf(buf, sizeof(buf), "__bryce%s", bryce);
        site->name = eina_stringshare_add(buf);
        site->gravity = gravity;
        site->orient = orient;
        site->anchor = anchor;

        printf("Append %d %d\n", eina_list_count(_sites->sites), eina_list_count(site->gadgets));
        _sites->sites = eina_list_append(_sites->sites, site);
        printf("Append %d %d\n", eina_list_count(_sites->sites), eina_list_count(site->gadgets));

        b = calloc(1, sizeof(Bryce));
        b->name = eina_stringshare_add(bryce);
        b->style = eina_stringshare_add("default");
        b->zone = shelf->zone; /* TODO */
        b->size = 48;
        b->layer = shelf->layer;
        b->autosize = !!shelf->fit_along;
        b->autohide = !!shelf->autohide;
        b->orient = orient;
        b->anchor = anchor;
        b->version = 2;
        _bryces->bryces = eina_list_append(_bryces->bryces, b);
     }
}

static void
_migrate(const char *path)
{
   E_Config_Shelf *shelf;
   E_Gadget_Site_Orient orient;
   E_Gadget_Site_Anchor anchor;
   E_Gadget_Site_Gravity gravity;
   const char *loc, *loc2;
   char buf[PATH_MAX];
   Eet_File *f;



   EINA_LIST_FREE(e_config->shelves, shelf)
     {
        switch(shelf->orient)
          {
//           case E_GADCON_ORIENT_FLOAT: ???? UNUSED ????
           case E_GADCON_ORIENT_HORIZ:
              orient = E_GADGET_SITE_ORIENT_HORIZONTAL;
              anchor = E_GADGET_SITE_ANCHOR_TOP;
              break;
           case E_GADCON_ORIENT_TOP:
              orient = E_GADGET_SITE_ORIENT_HORIZONTAL;
              anchor = E_GADGET_SITE_ANCHOR_TOP;
              break;
           case E_GADCON_ORIENT_BOTTOM:
              orient = E_GADGET_SITE_ORIENT_HORIZONTAL;
              anchor = E_GADGET_SITE_ANCHOR_BOTTOM;
              break;
           case E_GADCON_ORIENT_CORNER_TL:
              orient = E_GADGET_SITE_ORIENT_HORIZONTAL;
              anchor = E_GADGET_SITE_ANCHOR_TOP | E_GADGET_SITE_ANCHOR_LEFT;
              break;
           case E_GADCON_ORIENT_CORNER_TR:
              orient = E_GADGET_SITE_ORIENT_HORIZONTAL;
              anchor = E_GADGET_SITE_ANCHOR_TOP | E_GADGET_SITE_ANCHOR_RIGHT;
              break;
           case E_GADCON_ORIENT_CORNER_BL:
              orient = E_GADGET_SITE_ORIENT_HORIZONTAL;
              anchor = E_GADGET_SITE_ANCHOR_BOTTOM | E_GADGET_SITE_ANCHOR_LEFT;
              break;
           case E_GADCON_ORIENT_CORNER_BR:
              orient = E_GADGET_SITE_ORIENT_HORIZONTAL;
              anchor = E_GADGET_SITE_ANCHOR_BOTTOM | E_GADGET_SITE_ANCHOR_RIGHT;
              break;
           case E_GADCON_ORIENT_VERT:
              orient = E_GADGET_SITE_ORIENT_VERTICAL;
              anchor = E_GADGET_SITE_ANCHOR_LEFT;
              break;
           case E_GADCON_ORIENT_LEFT:
              orient = E_GADGET_SITE_ORIENT_VERTICAL;
              anchor = E_GADGET_SITE_ANCHOR_LEFT;
              break;
           case E_GADCON_ORIENT_RIGHT:
              orient = E_GADGET_SITE_ORIENT_VERTICAL;
              anchor = E_GADGET_SITE_ANCHOR_RIGHT;
              break;
           case E_GADCON_ORIENT_CORNER_LT:
              orient = E_GADGET_SITE_ORIENT_VERTICAL;
              anchor = E_GADGET_SITE_ANCHOR_LEFT | E_GADGET_SITE_ANCHOR_TOP;
              break;
           case E_GADCON_ORIENT_CORNER_RT:
              orient = E_GADGET_SITE_ORIENT_VERTICAL;
              anchor = E_GADGET_SITE_ANCHOR_RIGHT | E_GADGET_SITE_ANCHOR_TOP;
              break;
           case E_GADCON_ORIENT_CORNER_LB:
              orient = E_GADGET_SITE_ORIENT_VERTICAL;
              anchor = E_GADGET_SITE_ANCHOR_LEFT | E_GADGET_SITE_ANCHOR_BOTTOM;
              break;
           case E_GADCON_ORIENT_CORNER_RB:
              orient = E_GADGET_SITE_ORIENT_VERTICAL;
              anchor = E_GADGET_SITE_ANCHOR_RIGHT | E_GADGET_SITE_ANCHOR_BOTTOM;
              break;
          }
        if (orient == E_GADGET_SITE_ORIENT_HORIZONTAL)
          {
             if (anchor & E_GADGET_SITE_ANCHOR_LEFT)
               gravity = E_GADGET_SITE_GRAVITY_LEFT;
             else if (anchor & E_GADGET_SITE_ANCHOR_RIGHT)
               gravity = E_GADGET_SITE_GRAVITY_RIGHT;
             else
               gravity = E_GADGET_SITE_GRAVITY_CENTER;
          }
        else
          {
             if (anchor & E_GADGET_SITE_ANCHOR_TOP)
               gravity = E_GADGET_SITE_GRAVITY_TOP;
             else if (anchor & E_GADGET_SITE_ANCHOR_BOTTOM)
               gravity = E_GADGET_SITE_GRAVITY_BOTTOM;
             else
               gravity = E_GADGET_SITE_GRAVITY_CENTER;
          }

        if (anchor & E_GADGET_SITE_ANCHOR_TOP)
          loc = "top";
        else if (anchor & E_GADGET_SITE_ANCHOR_BOTTOM)
          loc = "bottom";
        else if (anchor & E_GADGET_SITE_ANCHOR_LEFT)
          loc = "left";
        else if (anchor & E_GADGET_SITE_ANCHOR_RIGHT)
          loc = "right";
        if (anchor & E_GADGET_SITE_ANCHOR_RIGHT)
          loc2 = "right";
        else if (anchor & E_GADGET_SITE_ANCHOR_LEFT)
          loc2 = "left";
        else if (anchor & E_GADGET_SITE_ANCHOR_TOP)
          loc2 = "top";
        else if (anchor & E_GADGET_SITE_ANCHOR_BOTTOM)
          loc2 = "bottom";

        snprintf(buf, sizeof(buf), "bryce_%s_%s_%d", loc, loc2, shelf->zone);

        _migrate_shelf(path, shelf, buf, anchor, gravity, orient);
     }
   snprintf(buf, sizeof(buf), "%s/e.cfg", path);
   f = eet_open(buf, EET_FILE_MODE_READ_WRITE);
   if (!f)
     {
        printf("Can't open config file\n");
     }

   eet_data_write(f, e_config_descriptor_get(), "config",
                  e_config, 1);
   eet_close(f);
}

static void
_shutdown(void)
{
   clock_config_descriptor_shutdown();
   time_config_descriptor_shutdown();
   ibar_config_descriptor_shutdown();
   luncher_config_descriptor_shutdown();
   e_config_descriptor_shutdown();
   eet_shutdown();
}

static const Ecore_Getopt options =
{
   "Bryce migration",
   "%prog [options]",
   "0.1",
   "(C) 2011 Enlightenment, see AUTHORS.",
   "GPL, see COPYING",
   "Bryce migration tool",
   EINA_TRUE,
   {
      ECORE_GETOPT_STORE_STR('p', "path", "path of e config"),
      ECORE_GETOPT_STORE_STR('s', "shelf", "shelf name"),
      ECORE_GETOPT_STORE_STR('b', "bryce", "bryce name"),
      ECORE_GETOPT_STORE_UINT('a', "anchor", "anchor position"),
      ECORE_GETOPT_STORE_UINT('g', "gravity", "gadget gravity"),
      ECORE_GETOPT_STORE_UINT('o', "orient", "gadget orientation"),
      ECORE_GETOPT_HELP('h', "help"),
      ECORE_GETOPT_SENTINEL
   }
};

int
main(int argc, char **argv)
{
   int args;
   char *path;
   char *shelf;
   char *bryce;
   Eina_Bool quit_option;
   E_Gadget_Site_Gravity gravity;
   E_Gadget_Site_Orient orient;
   E_Gadget_Site_Anchor anchor;

   Ecore_Getopt_Value values[] =
     {
        ECORE_GETOPT_VALUE_STR(path),
        ECORE_GETOPT_VALUE_STR(shelf),
        ECORE_GETOPT_VALUE_STR(bryce),
        ECORE_GETOPT_VALUE_UINT(anchor),
        ECORE_GETOPT_VALUE_UINT(gravity),
        ECORE_GETOPT_VALUE_UINT(orient),
        ECORE_GETOPT_VALUE_BOOL(quit_option)
     };

   args = ecore_getopt_parse(&options, values, argc, argv);
   if (args < 0)
     return EXIT_FAILURE;
   if (quit_option)
     return EXIT_SUCCESS;


   printf("Welcome to migration tool\n");
   _init(path);
   _bryce_init(path);

   _migrate(path);

   _bryce_shutdown(path);
   _shutdown();

   return 0;
}




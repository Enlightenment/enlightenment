#include "e_illume_private.h"
#include "e_mod_main.h"

/* local function prototypes */
static void _e_mod_kbd_device_ignore_load(void);
static void _e_mod_kbd_device_ignore_load_file(const char *file);
static void _e_mod_kbd_device_kbd_add(const char *udi);
static void _e_mod_kbd_device_kbd_del(const char *udi);
static void _e_mod_kbd_device_kbd_eval(void);
# include <Eeze.h>
static void _e_mod_kbd_device_udev_event(const char *device, Eeze_Udev_Event event, void *data EINA_UNUSED, Eeze_Udev_Watch *watch EINA_UNUSED);

/* local variables */
static int have_real_kbd = 0;
static Eeze_Udev_Watch *watch;
static Eina_List *_device_kbds = NULL, *_ignore_kbds = NULL;

void 
e_mod_kbd_device_init(void) 
{
   /* load the 'ignored' keyboard file */
   _e_mod_kbd_device_ignore_load();
   eeze_init();
   watch = eeze_udev_watch_add(EEZE_UDEV_TYPE_KEYBOARD, EEZE_UDEV_EVENT_NONE,
                               _e_mod_kbd_device_udev_event, NULL);
}

void 
e_mod_kbd_device_shutdown(void) 
{
   char *str;

   if (watch) eeze_udev_watch_del(watch);
   eeze_shutdown();
   /* free the list of ignored keyboards */
   EINA_LIST_FREE(_ignore_kbds, str)
     eina_stringshare_del(str);

   /* free the list of keyboards */
   EINA_LIST_FREE(_device_kbds, str)
     eina_stringshare_del(str);
}

/* local functions */
static void 
_e_mod_kbd_device_ignore_load(void) 
{
   char buff[PATH_MAX];

   /* load the 'ignore' file from the user's home dir */
   e_user_dir_concat_static(buff, "keyboards/ignore_built_in_keyboards");
   _e_mod_kbd_device_ignore_load_file(buff);

   /* load the 'ignore' file from the system/module dir */
   snprintf(buff, sizeof(buff), 
            "%s/ignore_built_in_keyboards", _e_illume_mod_dir);
   _e_mod_kbd_device_ignore_load_file(buff);
}

static void 
_e_mod_kbd_device_ignore_load_file(const char *file) 
{
   char buff[4096];
   FILE *f;

   /* can this file be opened */
   if (!(f = fopen(file, "r"))) return;

   /* parse out the info in the ignore file */
   while (fgets(buff, sizeof(buff), f))
     {
        char *p;
        int len;

        if (buff[0] == '#') continue;
        len = strlen(buff);
        if (len > 0)
          {
             if (buff[len - 1] == '\n') buff[len - 1] = 0;
          }
        p = buff;
        while (isspace(*p)) p++;

        /* append this kbd to the ignore list */
        if (*p) 
          {
             _ignore_kbds = 
               eina_list_append(_ignore_kbds, eina_stringshare_add(p));
          }
     }
   fclose(f);
}

static void 
_e_mod_kbd_device_udev_event(const char *device, Eeze_Udev_Event event, void *data EINA_UNUSED, Eeze_Udev_Watch *w EINA_UNUSED)
{
   if ((!device) || (!event)) return;

   if (((event & EEZE_UDEV_EVENT_ADD) == EEZE_UDEV_EVENT_ADD) ||
     ((event & EEZE_UDEV_EVENT_ONLINE) == EEZE_UDEV_EVENT_ONLINE))
     _e_mod_kbd_device_kbd_add(device);
   else if (((event & EEZE_UDEV_EVENT_REMOVE) == EEZE_UDEV_EVENT_REMOVE) ||
     ((event & EEZE_UDEV_EVENT_OFFLINE) == EEZE_UDEV_EVENT_OFFLINE))
     _e_mod_kbd_device_kbd_del(device);

   _e_mod_kbd_device_kbd_eval();
}

static void 
_e_mod_kbd_device_kbd_add(const char *udi) 
{
   const char *str;
   Eina_List *l;

   if (!udi) return;
   EINA_LIST_FOREACH(_device_kbds, l, str)
     if (!strcmp(str, udi)) return;
   _device_kbds = eina_list_append(_device_kbds, eina_stringshare_add(udi));
}

static void 
_e_mod_kbd_device_kbd_del(const char *udi) 
{
   const char *str;
   Eina_List *l;

   if (!udi) return;
   EINA_LIST_FOREACH(_device_kbds, l, str)
     if (!strcmp(str, udi)) 
       {
          eina_stringshare_del(str);
          _device_kbds = eina_list_remove_list(_device_kbds, l);
          break;
       }
}

static void 
_e_mod_kbd_device_kbd_eval(void) 
{
   Eina_List *l, *ll;
   const char *g, *gg;
   int have_real = 0;

   have_real = eina_list_count(_device_kbds);
   EINA_LIST_FOREACH(_device_kbds, l, g)
     EINA_LIST_FOREACH(_ignore_kbds, ll, gg)
       if (e_util_glob_match(g, gg)) 
         {
            have_real--;
            break;
         }

   if (have_real != have_real_kbd) 
     {
        have_real_kbd = have_real;
#if 0
//        if (have_real_kbd) e_mod_kbd_disable();
        else
#endif
//          e_mod_kbd_enable();
     }
}

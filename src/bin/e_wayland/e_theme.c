#include "e.h"

/* local structures */
typedef struct _E_Theme_Result E_Theme_Result;

struct _E_Theme_Result
{
   const char *file;
   const char *cache;
   Eina_Hash  *quickfind;
};

/* local function prototypes */
static Eina_Bool _e_theme_mappings_cb_free(const Eina_Hash *hash EINA_UNUSED, const void *key EINA_UNUSED, void *data, void *fdata EINA_UNUSED);
static Eina_Bool _e_theme_mappings_quickfind_cb_free(const Eina_Hash *hash, const void *key, void *data, void *fdata);
static void _e_theme_category_register(const char *category);
static const char *_e_theme_edje_file_get(const char *category, const char *group, Eina_Bool fallback_icon);

/* local variables */
static Eina_Hash *mappings = NULL;
static Eina_Hash *group_cache = NULL;
static Eina_List *categories = NULL;

EINTERN int 
e_theme_init(void)
{
   E_Config_Theme *et;
   Eina_List *l;

   e_theme_file_set("base", "default.edj");

   EINA_LIST_FOREACH(e_config->themes, l, et)
     {
        char buff[256];

        snprintf(buff, sizeof(buff), "base/%s", et->category);
        e_theme_file_set(buff, et->file);
     }

   if (!mappings) mappings = eina_hash_string_superfast_new(NULL);
   group_cache = eina_hash_string_superfast_new(NULL);

   return 1;
}

EINTERN int 
e_theme_shutdown(void)
{
   const char *str;

   if (mappings)
     {
        eina_hash_foreach(mappings, _e_theme_mappings_cb_free, NULL);
        eina_hash_free(mappings);
        mappings = NULL;
     }

   if (group_cache)
     {
        eina_hash_free(group_cache);
        group_cache = NULL;
     }

   EINA_LIST_FREE(categories, str)
     eina_stringshare_del(str);

   return 1;
}

EAPI Eina_Bool 
e_theme_edje_object_set(Evas_Object *obj, const char *category, const char *group)
{
   E_Theme_Result *res;
   char buf[256];
   char *p;

   /* find category -> edje mapping */
   _e_theme_category_register(category);
   res = eina_hash_find(mappings, category);
   if (res)
     {
        const char *str;

        /* if found check cached path */
        str = res->cache;
        if (!str)
          {
             /* no cached path */
             str = res->file;
             /* if its not an absolute path find it */
             if (str[0] != '/')
               str = e_path_find(path_themes, str);
             /* save cached value */
             if (str) res->cache = str;
          }
        if (str)
          {
             void *tres;
             int ok;

             snprintf(buf, sizeof(buf), "%s/::/%s", str, group);
             tres = eina_hash_find(group_cache, buf);
             if (!tres)
               {
                  ok = edje_object_file_set(obj, str, group);
                  /* save in the group cache hash */
                  if (ok)
                    eina_hash_add(group_cache, buf, res);
                  else
                    eina_hash_add(group_cache, buf, (void *)1);
               }
             else if (tres == (void *)1)
               ok = 0;
             else
               ok = 1;
             if (ok)
               {
                  if (tres)
                    edje_object_file_set(obj, str, group);
                  return 1;
               }
          }
     }
   /* no mapping or set failed - fall back */
   eina_strlcpy(buf, category, sizeof(buf));
   /* shorten string up to and not including last / char */
   p = strrchr(buf, '/');
   if (p) *p = 0;
   /* no / anymore - we are already as far back as we can go */
   else return 0;
   /* try this category */
   return e_theme_edje_object_set(obj, buf, group);
}

EAPI void 
e_theme_file_set(const char *category, const char *file)
{
   E_Theme_Result *res;

   if (group_cache)
     {
        eina_hash_free(group_cache);
        group_cache = NULL;
     }
   _e_theme_category_register(category);
   res = eina_hash_find(mappings, category);
   if (res)
     {
        eina_hash_del(mappings, category, res);
        if (res->file)
          {
             e_filereg_deregister(res->file);
             eina_stringshare_del(res->file);
          }
        if (res->cache) eina_stringshare_del(res->cache);
        E_FREE(res);
     }
   res = E_NEW(E_Theme_Result, 1);
   res->file = eina_stringshare_add(file);
   e_filereg_register(res->file);
   if (!mappings)
     mappings = eina_hash_string_superfast_new(NULL);
   eina_hash_add(mappings, category, res);
}

EAPI const char *
e_theme_edje_file_get(const char *category, const char *group)
{
   return _e_theme_edje_file_get(category, group, EINA_FALSE);
}

EAPI const char *
e_theme_edje_icon_fallback_file_get(const char *group)
{
   return _e_theme_edje_file_get("base", group, EINA_TRUE);
}

/* local functions */
static Eina_Bool 
_e_theme_mappings_cb_free(const Eina_Hash *hash EINA_UNUSED, const void *key EINA_UNUSED, void *data, void *fdata EINA_UNUSED)
{
   E_Theme_Result *res;

   res = data;
   if (res->file) eina_stringshare_del(res->file);
   if (res->cache) eina_stringshare_del(res->cache);
   if (res->quickfind)
     {
        eina_hash_foreach(res->quickfind, 
                          _e_theme_mappings_quickfind_cb_free, NULL);
        eina_hash_free(res->quickfind);
     }
   free(res);
   return EINA_TRUE;
}

static Eina_Bool
_e_theme_mappings_quickfind_cb_free(const Eina_Hash *hash EINA_UNUSED, const void *key, void *data EINA_UNUSED, void *fdata EINA_UNUSED)
{
   eina_stringshare_del(key);
   return EINA_TRUE;
}

static void
_e_theme_category_register(const char *category)
{
   Eina_List *l;
   int ret;

   if (!categories)
     categories = eina_list_append(categories, eina_stringshare_add(category));

   l = eina_list_search_sorted_near_list(categories, EINA_COMPARE_CB(strcmp),
                                         category, &ret);

   if (!ret) return;

   if (ret < 0)
     categories = eina_list_append_relative_list(categories, eina_stringshare_add(category), l);
   else
     categories = eina_list_prepend_relative_list(categories, eina_stringshare_add(category), l);
}

static const char *
_e_theme_edje_file_get(const char *category, const char *group, Eina_Bool fallback_icon)
{
   E_Theme_Result *res;
   char buf[PATH_MAX], *p;

   /* find category -> edje mapping */
   _e_theme_category_register(category);
   res = eina_hash_find(mappings, category);

   if ((e_config->icon_theme) && (!fallback_icon) &&
       (!strcmp(category, "base")) && (!strncmp(group, "e/icons", 7)))
     return "";

   if (res)
     {
        const char *str;

        /* if found check cached path */
        str = res->cache;
        if (!str)
          {
             /* no cached path */
             str = res->file;
             /* if its not an absolute path find it */
             if (str[0] != '/')
               str = e_path_find(path_themes, str);
             /* save cached value */
             if (str) res->cache = str;
          }
        if (str)
          {
             void *tres;
             Eina_List *coll;
             int ok;

             snprintf(buf, sizeof(buf), "%s/::/%s", str, group);
             tres = eina_hash_find(group_cache, buf);
             if (!tres)
               {
                  /* if the group exists - return */
                  if (!res->quickfind)
                    {
                       Eina_Stringshare *col;

                       res->quickfind = eina_hash_string_superfast_new(NULL);

                       /* create a quick find hash of all group entries */
                       coll = edje_file_collection_list(str);

                       EINA_LIST_FREE(coll, col)
                         eina_hash_direct_add(res->quickfind, col, col);
                    }
                  /* save in the group cache hash */
                  if (eina_hash_find(res->quickfind, group))
                    {
                       eina_hash_add(group_cache, buf, res);
                       ok = 1;
                    }
                  else
                    {
                       eina_hash_add(group_cache, buf, (void *)1);
                       ok = 0;
                    }
               }
             else if (tres == (void *)1) /* special pointer "1" == not there */
               ok = 0;
             else
               ok = 1;
             if (ok) return str;
          }
     }
   /* no mapping or set failed - fall back */
   eina_strlcpy(buf, category, sizeof(buf));
   /* shorten string up to and not including last / char */
   p = strrchr(buf, '/');
   if (p) *p = 0;
   /* no / anymore - we are already as far back as we can go */
   else return "";
   /* try this category */
   return e_theme_edje_file_get(buf, group);
}

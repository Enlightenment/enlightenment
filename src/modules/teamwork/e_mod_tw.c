#include "e_mod_main.h"

#define IMAGE_FETCH_TRIES 5

typedef struct
{
   const char *sha1;
   unsigned long long timestamp;
} Media_Cache;

typedef struct Media_Cache_List
{
   Eina_List *cache;
} Media_Cache_List;

typedef struct Media
{
   EINA_INLIST;
   Ecore_Con_Url *client;
   Eina_Binbuf *buf;
   const char *addr;
   unsigned long long timestamp;
   unsigned int tries;
   Eina_Bool dummy : 1;
   Eina_Bool valid : 1;
   Eina_Bool show : 1;
} Media;

static Eet_File *media = NULL;
static Eet_File *dummies = NULL;
static Eet_Data_Descriptor *cleaner_edd = NULL;
static Eet_Data_Descriptor *cache_edd = NULL;
static Ecore_Idler *media_cleaner = NULL;
static Eina_List *handlers = NULL;
static Media_Cache_List *tw_cache_list = NULL;

static Evas_Point last_coords = {0};

static Ecore_Timer *tw_hide_timer = NULL;

static Eldbus_Service_Interface *tw_dbus_iface = NULL;

typedef enum
{
    TEAMWORK_LINK_TYPE_NONE,
    TEAMWORK_LINK_TYPE_LOCAL_FILE,
    TEAMWORK_LINK_TYPE_LOCAL_DIRECTORY,
    TEAMWORK_LINK_TYPE_REMOTE
} Teamwork_Link_Type;

typedef enum
{
   TEAMWORK_SIGNAL_LINK_DOWNLOADING,
   TEAMWORK_SIGNAL_LINK_PROGRESS,
   TEAMWORK_SIGNAL_LINK_COMPLETE,
   TEAMWORK_SIGNAL_LINK_INVALID,
} Teamwork_Signal;

static const Eldbus_Signal tw_signals[] =
{
   [TEAMWORK_SIGNAL_LINK_DOWNLOADING] = {"LinkDownloading", ELDBUS_ARGS({"s", "URI"}, {"u", "Timestamp"}), 0},
   [TEAMWORK_SIGNAL_LINK_PROGRESS] = {"LinkProgress", ELDBUS_ARGS({"s", "URI"}, {"u", "Timestamp"}, {"d", "Percent Completion"}), 0},
   [TEAMWORK_SIGNAL_LINK_COMPLETE] = {"LinkComplete", ELDBUS_ARGS({"s", "URI"}, {"u", "Timestamp"}), 0},
   [TEAMWORK_SIGNAL_LINK_INVALID] = {"LinkInvalid", ELDBUS_ARGS({"s", "URI"}, {"u", "Timestamp"}), 0},
   {}
};

static void tw_show(Media *i);
static void tw_show_local_file(const char *uri);
static void tw_show_local_dir(const char *uri);
static int tw_media_add(const char *url, Eina_Binbuf *buf, unsigned long long timestamp);
static void download_media_cleanup(void);
static void tw_dummy_add(const char *url);
static Eina_Bool tw_dummy_check(const char *url);
static void tw_media_ping(const char *url, unsigned long long timestamp);
static Eina_Binbuf *tw_media_get(const char *url, unsigned long long timestamp);
static Eina_Bool tw_idler_start(void);

static void
dbus_signal_link_complete(Media *i)
{
   unsigned int u = ecore_time_unix_get();
   if (i->show) tw_show(i);
   i->show = 0;
   eldbus_service_signal_emit(tw_dbus_iface, TEAMWORK_SIGNAL_LINK_COMPLETE, i->addr, u);
}

static void
dbus_signal_link_invalid(Media *i)
{
   unsigned int u = ecore_time_unix_get();
   eldbus_service_signal_emit(tw_dbus_iface, TEAMWORK_SIGNAL_LINK_INVALID, i->addr, u);
}

static void
dbus_signal_link_progress(Media *i, double pr)
{
   unsigned int u = ecore_time_unix_get();
   eldbus_service_signal_emit(tw_dbus_iface, TEAMWORK_SIGNAL_LINK_PROGRESS, i->addr, u, pr);
}

static void
dbus_signal_link_downloading(Media *i)
{
   unsigned int u = ecore_time_unix_get();
   eldbus_service_signal_emit(tw_dbus_iface, TEAMWORK_SIGNAL_LINK_DOWNLOADING, i->addr, u);
}

static Eina_Bool
download_media_complete(void *data, int type EINA_UNUSED, Ecore_Con_Event_Url_Complete *ev)
{
   Media *i;

   if (data != tw_mod) return ECORE_CALLBACK_RENEW;
   i = ecore_con_url_data_get(ev->url_con);
   if (!i) return ECORE_CALLBACK_RENEW;
   if (!i->valid) return ECORE_CALLBACK_RENEW;
   i->timestamp = (unsigned long long)ecore_time_unix_get();
   if (tw_media_add(i->addr, i->buf, i->timestamp) == 1)
     tw_mod->media_size += eina_binbuf_length_get(i->buf);
   E_FREE_FUNC(i->client, ecore_con_url_free);
   dbus_signal_link_complete(i);
   download_media_cleanup();
   INF("MEDIA CACHE: %zu bytes", tw_mod->media_size);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
download_media_data(void *data, int type EINA_UNUSED, Ecore_Con_Event_Url_Data *ev)
{
   Media *i;

   if (data != tw_mod) return ECORE_CALLBACK_RENEW;
   i = ecore_con_url_data_get(ev->url_con);
   if (!i) return ECORE_CALLBACK_RENEW;
   if (i->dummy) return ECORE_CALLBACK_RENEW;
   if (!i->buf) i->buf = eina_binbuf_new();
   eina_binbuf_append_length(i->buf, ev->data, ev->size);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
download_media_status(void *data, int type EINA_UNUSED, Ecore_Con_Event_Url_Progress *ev)
{
   int status;
   const char *h;
   Media *i;
   const Eina_List *l;

   if (data != tw_mod) return ECORE_CALLBACK_RENEW;
   i = ecore_con_url_data_get(ev->url_con);
   if (!i) return ECORE_CALLBACK_RENEW;

   if (i->valid) return ECORE_CALLBACK_RENEW; //already checked
   status = ecore_con_url_status_code_get(ev->url_con);
   if (!status) return ECORE_CALLBACK_RENEW; //not ready yet
   DBG("%i code for media: %s", status, i->addr);
   if (status != 200)
     {
        E_FREE_FUNC(i->buf, eina_binbuf_free);
        E_FREE_FUNC(i->client, ecore_con_url_free);
        if ((status >= 400) || (status <= 300)) goto dummy;
        if (++i->tries < IMAGE_FETCH_TRIES)
          {
             i->client = ecore_con_url_new(i->addr);
             ecore_con_url_data_set(i->client, i);
             if (!ecore_con_url_get(i->client)) goto dummy;
          }
        return ECORE_CALLBACK_RENEW;
     }
   EINA_LIST_FOREACH(ecore_con_url_response_headers_get(ev->url_con), l, h)
     {
        if (strncasecmp(h, "Content-Type: ", sizeof("Content-Type: ") - 1)) continue;
        if (strncasecmp(h + sizeof("Content-Type: ") - 1, "image/", 6)) goto dummy;
     }
   i->valid = !i->dummy;
   if (i->valid) dbus_signal_link_progress(i, ev->down.now / ev->down.total);

   return ECORE_CALLBACK_RENEW;
dummy:
   dbus_signal_link_invalid(i);
   tw_dummy_add(i->addr);
   i->dummy = EINA_TRUE;
   E_FREE_FUNC(i->buf, eina_binbuf_free);
   E_FREE_FUNC(i->client, ecore_con_url_free);
   return ECORE_CALLBACK_RENEW;
}

static int
download_media_sort_cb(Media *a, Media *b)
{
   long long diff;
   diff = a->timestamp - b->timestamp;
   if (diff < 0) return -1;
   if (!diff) return 0;
   return 1;
}

static Media *
download_media_add(const char *url)
{
   Media *i;
   unsigned long long t;

   t = (unsigned long long)ecore_time_unix_get();

   i = eina_hash_find(tw_mod->media, url);
   if (i)
     {
        if (i->buf)
          {
             i->timestamp = t;
             tw_media_ping(url, i->timestamp);
          }
        else
          {
             /* randomly deleted during cache pruning */
             i->buf = tw_media_get(url, t);
             tw_mod->media_size += eina_binbuf_length_get(i->buf);
          }
        tw_mod->media_list = eina_inlist_promote(tw_mod->media_list, EINA_INLIST_GET(i));
        download_media_cleanup();
        return i;
     }
   if (tw_dummy_check(url)) return NULL;
   if (tw_config->disable_media_fetch) return NULL;
   i = calloc(1, sizeof(Media));
   i->addr = eina_stringshare_add(url);
   i->buf = tw_media_get(url, t);
   if (i->buf)
     tw_mod->media_size += eina_binbuf_length_get(i->buf);
   else
     {
        i->client = ecore_con_url_new(url);
        ecore_con_url_data_set(i->client, i);
        ecore_con_url_get(i->client);
        dbus_signal_link_downloading(i);
     }
   eina_hash_direct_add(tw_mod->media, url, i);
   tw_mod->media_list = eina_inlist_sorted_insert(tw_mod->media_list, EINA_INLIST_GET(i), (Eina_Compare_Cb)download_media_sort_cb);
   return i;
}

static void
download_media_free(Media *i)
{
   if (!i) return;
   tw_mod->media_list = eina_inlist_remove(tw_mod->media_list, EINA_INLIST_GET(i));
   if (i->client) ecore_con_url_free(i->client);
   if (i->buf) eina_binbuf_free(i->buf);
   eina_stringshare_del(i->addr);
   free(i);
}

static void
download_media_cleanup(void)
{
   Media *i;
   Eina_Inlist *l;

   if (tw_config->allowed_media_age)
     {
        if (tw_config->allowed_media_size < 0) return;
        if ((size_t)tw_config->allowed_media_size > (tw_mod->media_size / 1024 / 1024)) return;
     }
   if (!tw_mod->media_list) return;
   l = tw_mod->media_list->last;
   while (l)
     {
        i = EINA_INLIST_CONTAINER_GET(l, Media);
        l = l->prev;
        if (!i->buf) continue;
        /* only free the buffers here where possible to avoid having to deal with multiple list entries */
        if (tw_mod->media_size && (tw_mod->media_size >= eina_binbuf_length_get(i->buf)))
          tw_mod->media_size -= eina_binbuf_length_get(i->buf);
        E_FREE_FUNC(i->buf, eina_binbuf_free);
        if (!tw_config->allowed_media_age)
          /* if caching is disabled, just delete */
          eina_hash_del_by_key(tw_mod->media, i->addr);
        if ((size_t)tw_config->allowed_media_size > (tw_mod->media_size / 1024 / 1024))
          break;
     }
}

static Teamwork_Link_Type
dbus_link_uri_local_type_get(const char *uri)
{
   size_t len = strlen(uri);

   if (uri[len - 1] == '/') return TEAMWORK_LINK_TYPE_LOCAL_DIRECTORY;
   return TEAMWORK_LINK_TYPE_LOCAL_FILE;
}

static Teamwork_Link_Type
dbus_link_uri_type_get(const char *uri)
{
   if (!uri[0]) return TEAMWORK_LINK_TYPE_NONE; //invalid
   if (uri[0] == '/') return dbus_link_uri_local_type_get(uri + 1); //probably a file?
   if ((!strncasecmp(uri, "http://", 7)) || (!strncasecmp(uri, "https://", 8))) return TEAMWORK_LINK_TYPE_REMOTE;
   if (!strncmp(uri, "file://", 7)) return dbus_link_uri_local_type_get(uri + 7);
   WRN("Unknown link type for '%s'", uri);
   return TEAMWORK_LINK_TYPE_NONE;
}

static Eldbus_Message *
dbus_link_detect_cb(const Eldbus_Service_Interface *iface EINA_UNUSED, const Eldbus_Message *msg)
{
   const char *uri;
   unsigned int t;

   if (!tw_config->allowed_media_age) goto out;
   if (!eldbus_message_arguments_get(msg, "su", &uri, &t)) goto out;

   if (dbus_link_uri_type_get(uri) == TEAMWORK_LINK_TYPE_REMOTE)
     download_media_add(uri);
out:
   return eldbus_message_method_return_new(msg);
}

static void
dbus_link_show_helper(const char *uri, Eina_Bool signal_open)
{
   Teamwork_Link_Type type;

   if (tw_mod->pop && (!e_util_strcmp(e_object_data_get(E_OBJECT(tw_mod->pop)), uri))) return;
   type = dbus_link_uri_type_get(uri);
   switch (type)
     {
      case TEAMWORK_LINK_TYPE_NONE: break;
      case TEAMWORK_LINK_TYPE_LOCAL_DIRECTORY:
        if (signal_open) tw_show_local_dir(uri);
        break;
      case TEAMWORK_LINK_TYPE_LOCAL_FILE:
        tw_show_local_file(uri);
        break;
      case TEAMWORK_LINK_TYPE_REMOTE:
      {
         Media *i;

         i = eina_hash_find(tw_mod->media, uri);
         if (!i)
           {
              if (tw_dummy_check(uri)) break;
              i = download_media_add(uri);
              if (i)
                {
                   if (i->buf) tw_show(i);
                   else if (i->dummy) break;
                   else i->show = 1;
                }
           }
         else if (!i->dummy) tw_show(i);
         break;
      }
     }
}

static Eldbus_Message *
dbus_link_show_cb(const Eldbus_Service_Interface *iface EINA_UNUSED, const Eldbus_Message *msg)
{
   const char *uri;

   if (eldbus_message_arguments_get(msg, "s", &uri))
     {
        last_coords.x = last_coords.y = -1;
        dbus_link_show_helper(uri, 1);
     }
   return eldbus_message_method_return_new(msg);
}

static Eldbus_Message *
dbus_link_hide_cb(const Eldbus_Service_Interface *iface EINA_UNUSED, const Eldbus_Message *msg)
{
   const char *uri;

   if (eldbus_message_arguments_get(msg, "s", &uri))
     {
        if (tw_mod->pop && (!tw_mod->sticky) && (!e_util_strcmp(e_object_data_get(E_OBJECT(tw_mod->pop)), uri)))
          tw_hide(NULL);
     }
   return eldbus_message_method_return_new(msg);
}

static Eldbus_Message *
dbus_link_mouse_in_cb(const Eldbus_Service_Interface *iface EINA_UNUSED, const Eldbus_Message *msg)
{
   const char *uri;
   unsigned int t;

   if (eldbus_message_arguments_get(msg, "suii", &uri, &t, &last_coords.x, &last_coords.y))
     dbus_link_show_helper(uri, 0);
   return eldbus_message_method_return_new(msg);
}

static Eldbus_Message *
dbus_link_mouse_out_cb(const Eldbus_Service_Interface *iface EINA_UNUSED, const Eldbus_Message *msg)
{
   const char *uri;
   unsigned int t;

   if (eldbus_message_arguments_get(msg, "suii", &uri, &t, &last_coords.x, &last_coords.y))
     {
        if (tw_mod->pop && (!tw_mod->sticky) && (!e_util_strcmp(e_object_data_get(E_OBJECT(tw_mod->pop)), uri)))
          {
             if (tw_config->mouse_out_delay)
               {
                  if (tw_hide_timer) ecore_timer_reset(tw_hide_timer);
                  else tw_hide_timer = ecore_timer_add(tw_config->mouse_out_delay, tw_hide, NULL);
               }
             else
               tw_hide(NULL);
          }
     }
   return eldbus_message_method_return_new(msg);
}

static Eldbus_Message *
dbus_link_open_cb(const Eldbus_Service_Interface *iface EINA_UNUSED, const Eldbus_Message *msg)
{
   const char *uri;

   if (eldbus_message_arguments_get(msg, "s", &uri))
     {
        char *sb;
        size_t size = 4096, len = sizeof(E_BINDIR "/enlightenment_open ") - 1;

        sb = malloc(size);
        memcpy(sb, E_BINDIR "/enlightenment_open ", len);
        sb = e_util_string_append_quoted(sb, &size, &len, uri);
        ecore_exe_run(sb, NULL);
        free(sb);
     }
   return eldbus_message_method_return_new(msg);
}


static const Eldbus_Method tw_methods[] = {
   { "LinkDetect", ELDBUS_ARGS({"s", "URI"}, {"u", "Timestamp"}), NULL, dbus_link_detect_cb },
   { "LinkMouseIn", ELDBUS_ARGS({"s", "URI"}, {"u", "Timestamp"}, {"i", "X Coordinate"}, {"i", "Y Coordinate"}), NULL, dbus_link_mouse_in_cb },
   { "LinkMouseOut", ELDBUS_ARGS({"s", "URI"}, {"u", "Timestamp"}, {"i", "X Coordinate"}, {"i", "Y Coordinate"}), NULL, dbus_link_mouse_out_cb },
   { "LinkShow", ELDBUS_ARGS({"s", "URI"}), NULL, dbus_link_show_cb },
   { "LinkHide", ELDBUS_ARGS({"s", "URI"}), NULL, dbus_link_hide_cb },
   { "LinkOpen", ELDBUS_ARGS({"s", "URI"}), NULL, dbus_link_open_cb },
   { }
};

static const Eldbus_Service_Interface_Desc tw_desc =
{
   "org.enlightenment.wm.Teamwork", tw_methods, tw_signals
};

static Eet_Data_Descriptor *
media_cache_edd_new(void)
{
   Eet_Data_Descriptor *edd;
   Eet_Data_Descriptor_Class eddc;
   EET_EINA_FILE_DATA_DESCRIPTOR_CLASS_SET(&eddc, Media_Cache);
   edd = eet_data_descriptor_stream_new(&eddc);
#define ADD(name, type) \
  EET_DATA_DESCRIPTOR_ADD_BASIC(edd, Media_Cache, #name, name, EET_T_##type)

   ADD(sha1, INLINED_STRING);
   ADD(timestamp, ULONG_LONG);
#undef ADD
   return edd;
}

static int
media_cache_compare(Media_Cache *a, Media_Cache *b)
{
   long long diff;
   diff = a->timestamp - b->timestamp;
   if (diff < 0) return -1;
   if (!diff) return 0;
   return 1;
}

static void
media_cache_add(const char *sha1, unsigned long long timestamp)
{
   Media_Cache *ic;
   if (!tw_cache_list) return;
   ic = malloc(sizeof(Media_Cache));
   ic->sha1 = eina_stringshare_ref(sha1);
   ic->timestamp = timestamp;
   tw_cache_list->cache = eina_list_sorted_insert(tw_cache_list->cache, (Eina_Compare_Cb)media_cache_compare, ic);
}

static void
media_cache_del(const char *sha1)
{
   Eina_List *l, *l2;
   Media_Cache *ic;

   if (!tw_cache_list) return;
   EINA_LIST_FOREACH_SAFE(tw_cache_list->cache, l, l2, ic)
     {
        if (ic->sha1 == sha1) continue;
        tw_cache_list->cache = eina_list_remove_list(tw_cache_list->cache, l);
        return;
     }
}

static void
media_cache_update(const char *sha1, unsigned long long timestamp)
{
   Media_Cache *ic;
   Eina_List *l;

   if (!tw_cache_list) return;
   EINA_LIST_FOREACH(tw_cache_list->cache, l, ic)
     {
        if (ic->sha1 != sha1) continue;
        ic->timestamp = timestamp;
        break;
     }
   tw_cache_list->cache = eina_list_sort(tw_cache_list->cache, eina_list_count(tw_cache_list->cache), (Eina_Compare_Cb)media_cache_compare);
}

static Eina_Bool
media_cleaner_cb(void *data EINA_UNUSED)
{
   unsigned long long now;
   Media_Cache *ic;
   Eina_List *l, *l2;
   int cleaned = 0;
   if ((!cleaner_edd) || (!cache_edd) || (tw_config->allowed_media_age < 0) || (!tw_cache_list) || (!tw_cache_list->cache))
     {
        media_cleaner = NULL;
        return EINA_FALSE;
     }

   if (tw_config->allowed_media_age)
     {
        now = (unsigned long long)ecore_time_unix_get();
        now -= tw_config->allowed_media_age * 24 * 60 * 60;
     }
   else
     now = ULONG_LONG_MAX;
   EINA_LIST_FOREACH_SAFE(tw_cache_list->cache, l, l2, ic)
     {
        /* only clean up to 3 entries at a time to ensure responsiveness */
        if (cleaned >= 3) break;
        if (ic->timestamp >= now)
          {
             /* stop the idler for now to avoid pointless spinning */
             ecore_timer_add(24 * 60 * 60, (Ecore_Task_Cb)tw_idler_start, NULL);
             media_cleaner = NULL;
             return EINA_FALSE;
          }
        eet_delete(media, ic->sha1);
        tw_cache_list->cache = eina_list_remove_list(tw_cache_list->cache, l);
        eina_stringshare_del(ic->sha1);
        free(ic);
        cleaned++;
     }
   return EINA_TRUE;
}

static void
tw_dummy_add(const char *url)
{
   if (!dummies) return;
   eet_write(dummies, url, "0", 1, 0);
   INF("Added new dummy for url %s", url);
}

static Eina_Bool
tw_dummy_check(const char *url)
{
   char **list;
   int lsize;

   if (!dummies) return EINA_FALSE;
   list = eet_list(dummies, url, &lsize);
   if (lsize)
     {
        free(list);
        return EINA_TRUE;
     }
   return EINA_FALSE;
}

static int
tw_media_add(const char *url, Eina_Binbuf *buf, unsigned long long timestamp)
{
   const char *sha1;
   int lsize;
   char **list;

   if (!media) return -1;
   if (!tw_config->allowed_media_age) return 0; //disk caching disabled

   sha1 = sha1_encode(eina_binbuf_string_get(buf), eina_binbuf_length_get(buf));
   INF("Media: %s - %s", url, sha1);

   list = eet_list(media, url, &lsize);
   if (lsize)
     {
        eina_stringshare_del(sha1);
        free(list);
        return -1; /* should never happen */
     }
   list = eet_list(media, sha1, &lsize);
   if (lsize)
     {
        eet_alias(media, url, sha1, 0);
        eet_sync(media);
        INF("Added new alias for image %s", sha1);
        eina_stringshare_del(sha1);
        free(list);
        return 0;
     }

   eet_write(media, sha1, eina_binbuf_string_get(buf), eina_binbuf_length_get(buf), 1);
   eet_alias(media, url, sha1, 0);
   eet_sync(media);
   media_cache_add(sha1, timestamp);
   INF("Added new media with length %zu: %s", eina_binbuf_length_get(buf), sha1);
   eina_stringshare_del(sha1);
   return 1;
}

void
tw_media_del(const char *url)
{
   const char *alias;
   if (!media) return;
   alias = eet_alias_get(media, url);
   eet_delete(media, alias);
   media_cache_del(alias);
   eina_stringshare_del(alias);
}

static Eina_Binbuf *
tw_media_get(const char *url, unsigned long long timestamp)
{
   size_t size;
   unsigned char *img;
   Eina_Binbuf *buf = NULL;
   const char *alias;
   char **list;
   int lsize;

   if (!media) return NULL;

   list = eet_list(media, url, &lsize);
   if (!lsize) return NULL;
   free(list);

   img = eet_read(media, url, (int*)&size);
   alias = eet_alias_get(media, url);
   buf = eina_binbuf_manage_new_length(img, size);
   media_cache_update(alias, timestamp);

   eina_stringshare_del(alias);
   return buf;
}

static void
tw_media_ping(const char *url, unsigned long long timestamp)
{
   const char *alias;

   if (!media) return;

   alias = eet_alias_get(media, url);
   media_cache_update(alias, timestamp);

   eina_stringshare_del(alias);
}

static Eina_Bool
tw_idler_start(void)
{
   if (!media) return EINA_FALSE;
   media_cleaner = ecore_idler_add((Ecore_Task_Cb)media_cleaner_cb, NULL);
   return EINA_FALSE;
}

static void
tw_popup_del(void *obj)
{
   eina_stringshare_del(e_object_data_get(obj));
}

EINTERN void
tw_popup_opacity_set(void)
{
   if (tw_mod->pop && tw_mod->pop->cw)
     e_comp_win_opacity_set(tw_mod->pop->cw, lround((double)255 * (tw_config->popup_opacity / 100.)));
}

static void
tw_show_helper(Evas_Object *o, int w, int h)
{
   int px, py, pw, ph;
   double ratio = tw_config->popup_size / 100.;

   E_FREE_FUNC(tw_mod->pop, e_object_del);
   tw_mod->sticky = 0;
   tw_mod->pop = e_popup_new(e_zone_current_get(e_util_container_current_get()), 0, 0, 1, 1);
   e_popup_ignore_events_set(tw_mod->pop, 1);
   pw = MIN(w, (ratio * (double)tw_mod->pop->zone->w));
   pw = MIN(pw, tw_mod->pop->zone->w);
   if (pw == w) ph = h;
   else
     ph = lround((double)(pw * h) / ((double)w));
   if (ph > tw_mod->pop->zone->h)
     {
        ph = tw_mod->pop->zone->h;
        pw = lround((double)(ph * w) / ((double)h));
     }
   e_widget_preview_size_set(o, pw, ph);
   e_widget_preview_vsize_set(o, pw, ph);
   e_popup_layer_set(tw_mod->pop, E_COMP_CANVAS_LAYER_POPUP, 0);

   if ((last_coords.x == last_coords.y) && (last_coords.x == -1))
     {
        px = lround(ratio * (double)tw_mod->pop->zone->w) - (pw / 2);
        py = lround(ratio * (double)tw_mod->pop->zone->h) - (ph / 2);
        if (px + pw > tw_mod->pop->zone->w)
          px = tw_mod->pop->zone->w - pw;
        if (py + ph > tw_mod->pop->zone->h)
          py = tw_mod->pop->zone->h - ph;
     }
   else
     {
        /* prefer tooltip left of last_coords */
        px = last_coords.x - pw - 3;
        /* if it's offscreen, try right of last_coords */
        if (px < 0) px = last_coords.x + 3;
        /* fuck this, stick it right on the last_coords */
        if (px + pw + 3 > tw_mod->pop->zone->w)
          px = (last_coords.x / 2) - (pw / 2);
        /* give up */
        if (px < 0) px = 0;

        /* prefer tooltip above last_coords */
        py = last_coords.y - ph - 3;
        /* if it's offscreen, try below last_coords */
        if (py < 0) py = last_coords.y + 3;
        /* fuck this, stick it right on the last_coords */
        if (py + ph + 3 > tw_mod->pop->zone->h)
          py = (last_coords.y / 2) - (ph / 2);
        /* give up */
        if (py < 0) py = 0;
     }
   e_popup_move_resize(tw_mod->pop, px, py, pw, ph);
   e_popup_content_set(tw_mod->pop, o);
   e_popup_show(tw_mod->pop);
   tw_popup_opacity_set();
   E_OBJECT_DEL_SET(tw_mod->pop, tw_popup_del);
}

static void
tw_show(Media *i)
{
   Evas_Object *o, *ic, *prev;
   int w, h;

   if (!i->buf) i->buf = tw_media_get(i->addr, ecore_time_unix_get());
   if (!i->buf) return;
   prev = e_widget_preview_add(e_util_comp_current_get()->evas, 50, 50);
   o = evas_object_image_filled_add(e_widget_preview_evas_get(prev));
   evas_object_image_memfile_set(o, (void*)eina_binbuf_string_get(i->buf), eina_binbuf_length_get(i->buf), NULL, NULL);
   evas_object_image_size_get(o, &w, &h);
   ic = e_icon_add(e_widget_preview_evas_get(prev));
   e_icon_image_object_set(ic, o);
   e_widget_preview_extern_object_set(prev, ic);
   tw_show_helper(prev, w, h);
   e_object_data_set(E_OBJECT(tw_mod->pop), eina_stringshare_ref(i->addr));
   e_popup_object_add(tw_mod->pop, ic);
}

static void
tw_show_local_dir(const char *uri)
{
   E_Action *act;

   act = e_action_find("fileman");
   if (act) act->func.go(NULL, uri);
}

static void
tw_show_local_file(const char *uri)
{
   Evas_Object *o, *prev;
   int w, h;

   if (!evas_object_image_extension_can_load_get(uri)) return;
   prev = e_widget_preview_add(e_util_comp_current_get()->evas, 50, 50);
   o = e_icon_add(e_widget_preview_evas_get(prev));
   e_icon_file_set(o, uri);
   e_icon_preload_set(o, 1);
   e_icon_size_get(o, &w, &h);
   e_widget_preview_extern_object_set(prev, o);
   tw_show_helper(prev, w, h);
   e_popup_object_add(tw_mod->pop, o);
   e_object_data_set(E_OBJECT(tw_mod->pop), eina_stringshare_add(uri));
}

EINTERN Eina_Bool
tw_hide(void *d EINA_UNUSED)
{
   E_FREE_FUNC(tw_mod->pop, e_object_del);
   last_coords.x = last_coords.y = 0;
   E_FREE_FUNC(tw_hide_timer, ecore_timer_del);
   download_media_cleanup();
   return EINA_FALSE;
}

EINTERN int
e_tw_init(void)
{
   char buf[PATH_MAX];
   Eet_Data_Descriptor_Class eddc;

   tw_dbus_iface = e_msgbus_interface_attach(&tw_desc);

   e_user_dir_concat_static(buf, "images/cache.eet");
   media = eet_open(buf, EET_FILE_MODE_READ_WRITE);
   if (!media)
     {
        ERR("Could not open media cache file!");
        return 0;
     }
        
   cache_edd = media_cache_edd_new();
   EET_EINA_FILE_DATA_DESCRIPTOR_CLASS_SET(&eddc, Media_Cache_List);
   cleaner_edd = eet_data_descriptor_file_new(&eddc);
   EET_DATA_DESCRIPTOR_ADD_LIST(cleaner_edd, Media_Cache_List, "cache", cache, cache_edd);
   tw_cache_list = eet_data_read(media, cleaner_edd, "media_cache");

   e_user_dir_concat_static(buf, "images/dummies.eet");
   dummies = eet_open(buf, EET_FILE_MODE_READ_WRITE);

   E_LIST_HANDLER_APPEND(handlers, ECORE_CON_EVENT_URL_COMPLETE, download_media_complete, tw_mod);
   E_LIST_HANDLER_APPEND(handlers, ECORE_CON_EVENT_URL_PROGRESS, download_media_status, tw_mod);
   E_LIST_HANDLER_APPEND(handlers, ECORE_CON_EVENT_URL_DATA, download_media_data, tw_mod);

   tw_mod->media = eina_hash_string_superfast_new((Eina_Free_Cb)download_media_free);
   return 1;
}

EINTERN void
e_tw_shutdown(void)
{
   E_FREE_LIST(handlers, ecore_event_handler_del);
   if (media)
     {
        if (tw_cache_list)
          {
             Media_Cache *ic;
             eet_data_write(media, cleaner_edd, "media_cache", tw_cache_list, 1);
             EINA_LIST_FREE(tw_cache_list->cache, ic)
               {
                  eina_stringshare_del(ic->sha1);
                  free(ic);
               }
             free(tw_cache_list);
          }
        E_FREE_FUNC(media, eet_close);
     }
   E_FREE_FUNC(tw_dbus_iface, eldbus_service_interface_unregister);
   E_FREE_FUNC(dummies, eet_close);
   E_FREE_FUNC(cleaner_edd, eet_data_descriptor_free);
   E_FREE_FUNC(cache_edd, eet_data_descriptor_free);
   tw_hide(NULL);
   last_coords.x = last_coords.y = 0;
   eina_hash_free(tw_mod->media);
   E_FREE_FUNC(tw_mod->pop, e_object_del);
}

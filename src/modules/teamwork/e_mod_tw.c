#include "e_mod_main.h"

#define IMAGE_FETCH_TRIES 5

/**
 * https://phab.enlightenment.org/w/teamwork_api/
 */

typedef struct
{
   const char *sha1;
   unsigned long long timestamp;
   Eina_Bool video;
} Media_Cache;

typedef struct Media_Cache_List
{
   Eina_List *cache;
   Eina_Bool video;
} Media_Cache_List;

typedef struct Media
{
   Mod *mod;
   EINA_INLIST;
   Ecore_Con_Url *client;
   Eina_Binbuf *buf;
   const char *addr;
   unsigned long long timestamp;
   unsigned int tries;
   Ecore_Thread *video_thread;
   Eina_List *clients;
   Eina_Stringshare *tmpfile;
   Eina_Bool video;
   Eina_Bool dummy : 1;
   Eina_Bool valid : 1;
   Eina_Bool show : 1;
} Media;

typedef enum
{
   MEDIA_CACHE_TYPE_IMAGE,
   MEDIA_CACHE_TYPE_VIDEO,
} Media_Cache_Type;

static Eet_File *media[2] = {NULL};
static Eet_File *dummies = NULL;
static Eet_Data_Descriptor *cleaner_edd = NULL;
static Eet_Data_Descriptor *cache_edd = NULL;
static Ecore_Idler *media_cleaner[2] = {NULL};
static Eina_List *handlers = NULL;
static Media_Cache_List *tw_cache_list[2] = {NULL};

static Evas_Point last_coords = {0, 0};

static E_Client *tw_win;

static Ecore_Timer *tw_hide_timer = NULL;

static Eina_Stringshare *tw_tmpfile = NULL;
static int tw_tmpfd = -1;
static Ecore_Thread *tw_tmpthread = NULL;
static Media *tw_tmpthread_media = NULL;
static Eina_Bool tw_tooltip = EINA_FALSE;

typedef enum
{
    TEAMWORK_LINK_TYPE_NONE,
    TEAMWORK_LINK_TYPE_LOCAL_FILE,
    TEAMWORK_LINK_TYPE_LOCAL_DIRECTORY,
    TEAMWORK_LINK_TYPE_REMOTE
} Teamwork_Link_Type;

static void tw_show(Media *i);
static void tw_show_local_file(const char *uri);
static void tw_show_local_dir(const char *uri);
static int tw_media_add(const char *url, Eina_Binbuf *buf, unsigned long long timestamp, Eina_Bool video);
static void download_media_cleanup(void);
static void tw_dummy_add(const char *url);
static Eina_Bool tw_dummy_check(const char *url);
static void tw_media_ping(const char *url, unsigned long long timestamp, Eina_Bool video);
static Eina_Binbuf *tw_media_get(const char *url, unsigned long long timestamp, Eina_Bool *video);
static Eina_Bool tw_idler_start(void);

EINTERN Teamwork_Signal_Cb tw_signal_link_complete[E_PIXMAP_TYPE_NONE];
EINTERN Teamwork_Signal_Cb tw_signal_link_invalid[E_PIXMAP_TYPE_NONE];
EINTERN Teamwork_Signal_Progress_Cb tw_signal_link_progress[E_PIXMAP_TYPE_NONE];
EINTERN Teamwork_Signal_Cb tw_signal_link_downloading[E_PIXMAP_TYPE_NONE];

static void
media_client_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Media *i = data;
   E_Client *ec = e_comp_object_client_get(obj);

   i->clients = eina_list_remove(i->clients, ec);
}

static void
signal_link_complete(Media *i)
{
   E_Client *ec;

   if (i->show && (i->clients || (!tw_win))) tw_show(i);
   i->show = 0;

   EINA_LIST_FREE(i->clients, ec)
     {
        E_Pixmap_Type type = e_pixmap_type_get(ec->pixmap);
        if (e_client_has_xwindow(ec))
          type = E_PIXMAP_TYPE_X;
        if (tw_signal_link_complete[type])
          tw_signal_link_complete[type](ec, i->addr);
        evas_object_event_callback_del_full(ec->frame, EVAS_CALLBACK_DEL, media_client_del, i);
     }
}

static void
signal_link_invalid(Media *i)
{
   E_Client *ec;

   EINA_LIST_FREE(i->clients, ec)
     {
        E_Pixmap_Type type = e_pixmap_type_get(ec->pixmap);
        if (e_client_has_xwindow(ec))
          type = E_PIXMAP_TYPE_X;
        if (tw_signal_link_invalid[type])
          tw_signal_link_invalid[type](ec, i->addr);
        evas_object_event_callback_del_full(ec->frame, EVAS_CALLBACK_DEL, media_client_del, i);
     }
}

static void
signal_link_progress(Media *i, double pr)
{
   Eina_List *l;
   E_Client *ec;

   EINA_LIST_FOREACH(i->clients, l, ec)
     {
        E_Pixmap_Type type = e_pixmap_type_get(ec->pixmap);
        if (e_client_has_xwindow(ec))
          type = E_PIXMAP_TYPE_X;
        if (tw_signal_link_progress[type])
          tw_signal_link_progress[type](ec, i->addr, lround(pr));
     }
}

static void
signal_link_downloading(Media *i)
{
   Eina_List *l;
   E_Client *ec;

   EINA_LIST_FOREACH(i->clients, l, ec)
     {
        E_Pixmap_Type type = e_pixmap_type_get(ec->pixmap);
        if (e_client_has_xwindow(ec))
          type = E_PIXMAP_TYPE_X;
        if (tw_signal_link_downloading[type])
          tw_signal_link_downloading[type](ec, i->addr);
     }
}

static void
link_failure_show(void)
{
   tw_mod->pop = evas_object_rectangle_add(e_comp->evas);
   evas_object_color_set(tw_mod->pop, 0, 0, 0, 0);
   evas_object_pass_events_set(tw_mod->pop, 1);
   evas_object_geometry_set(tw_mod->pop, 0, 0, 1, 1);
   evas_object_show(tw_mod->pop);
   tw_tooltip = 1;
   elm_object_tooltip_text_set(e_comp->elm, _("Target URI could not be shown.<ps/>"
                                                            "Hold [Ctrl] to disable link fetching."));
   elm_object_tooltip_show(e_comp->elm);
}

static Eina_Bool
download_media_complete(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_Con_Event_Url_Complete *ev)
{
   Media *i;
   void **test = ecore_con_url_data_get(ev->url_con);

   if ((!test) || (*test != tw_mod)) return ECORE_CALLBACK_RENEW;
   i = (Media*)test;
   if (!i->valid) return ECORE_CALLBACK_DONE;
   i->timestamp = (unsigned long long)ecore_time_unix_get();
   if (tw_media_add(i->addr, i->buf, i->timestamp, i->video) == 1)
     tw_mod->media_size += eina_binbuf_length_get(i->buf);
   E_FREE_FUNC(i->client, ecore_con_url_free);
   signal_link_complete(i);
   download_media_cleanup();
   DBG("MEDIA CACHE: %zu bytes", tw_mod->media_size);
   return ECORE_CALLBACK_DONE;
}

static Eina_Bool
download_media_data(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_Con_Event_Url_Data *ev)
{
   Media *i;
   void **test = ecore_con_url_data_get(ev->url_con);

   if ((!test) || (*test != tw_mod)) return ECORE_CALLBACK_RENEW;
   i = (Media*)test;
   if (i->dummy) return ECORE_CALLBACK_DONE;
   if (!i->buf) i->buf = eina_binbuf_new();
   eina_binbuf_append_length(i->buf, ev->data, ev->size);
   return ECORE_CALLBACK_DONE;
}

static Eina_Bool
download_media_status(void *data EINA_UNUSED, int t EINA_UNUSED, Ecore_Con_Event_Url_Progress *ev)
{
   int status;
   const char *h;
   Media *i;
   const Eina_List *l;
   void **test = ecore_con_url_data_get(ev->url_con);

   if ((!test) || (*test != tw_mod)) return ECORE_CALLBACK_RENEW;
   i = (Media*)test;

   if (i->valid)
     {
        signal_link_progress(i, ev->down.now / ev->down.total);
        return ECORE_CALLBACK_DONE; //already checked
     }
   status = ecore_con_url_status_code_get(ev->url_con);
   if (!status) return ECORE_CALLBACK_DONE; //not ready yet
   if (ev->down.total / 1024 / 1024 > tw_config->allowed_media_fetch_size)
     {
        DBG("Media larger than allowed!");
        goto invalid;
     }
   DBG("%i code for media: %s", status, i->addr);
   if (status != 200)
     {
        E_FREE_FUNC(i->buf, eina_binbuf_free);
        E_FREE_FUNC(i->client, ecore_con_url_free);
        if ((status >= 400) || (status <= 301)) goto dummy;
        if (++i->tries < IMAGE_FETCH_TRIES)
          {
             i->client = ecore_con_url_new(i->addr);
             ecore_con_url_data_set(i->client, i);
             if (!ecore_con_url_get(i->client)) goto dummy;
          }
        return ECORE_CALLBACK_DONE;
     }
   EINA_LIST_FOREACH(ecore_con_url_response_headers_get(ev->url_con), l, h)
     {
        const char *type;

        if (strncasecmp(h, "Content-Type: ", sizeof("Content-Type: ") - 1)) continue;
        type = h + sizeof("Content-Type: ") - 1;
        i->video = ((!strncasecmp(type, "video/", 6)) || (!strncasecmp(type, "application/ogg", sizeof("application/ogg") -1)));
        if (i->video) break;
        if (strncasecmp(type, "image/", 6)) goto dummy;
        break;
     }
   i->valid = !i->dummy;
   if (i->valid) signal_link_progress(i, ev->down.now / ev->down.total);

   return ECORE_CALLBACK_DONE;
dummy:
   signal_link_invalid(i);
   tw_dummy_add(i->addr);
   i->dummy = EINA_TRUE;
invalid:
   E_FREE_FUNC(i->buf, eina_binbuf_free);
   E_FREE_FUNC(i->client, ecore_con_url_free);
   if (i->show)
     link_failure_show();
   i->show = 0;
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
   Eina_Bool add = EINA_FALSE;

   t = (unsigned long long)ecore_time_unix_get();

   i = eina_hash_find(tw_mod->media, url);
   if (i)
     {
        if (i->buf)
          {
             i->timestamp = t;
             tw_media_ping(url, i->timestamp, i->video);
          }
        else
          {
             /* randomly deleted during cache pruning */
             i->buf = tw_media_get(url, t, &i->video);
             if (i->buf) tw_mod->media_size += eina_binbuf_length_get(i->buf);
          }
        if (i->buf)
          {
             tw_mod->media_list = eina_inlist_promote(tw_mod->media_list, EINA_INLIST_GET(i));
             download_media_cleanup();
             return i;
          }
     }
   if (!i)
     {
        if (tw_dummy_check(url)) return NULL;
        if (tw_config->disable_media_fetch) return NULL;
        add = EINA_TRUE;
        i = calloc(1, sizeof(Media));
        i->mod = tw_mod;
        i->addr = eina_stringshare_add(url);
        i->buf = tw_media_get(url, t, &i->video);
     }
   if (i->buf)
     tw_mod->media_size += eina_binbuf_length_get(i->buf);
   else
     {
        i->client = ecore_con_url_new(url);
        ecore_con_url_data_set(i->client, i);
        ecore_con_url_get(i->client);
        signal_link_downloading(i);
     }
   if (!add) return i;
   eina_hash_add(tw_mod->media, url, i);
   tw_mod->media_list = eina_inlist_sorted_insert(tw_mod->media_list, EINA_INLIST_GET(i), (Eina_Compare_Cb)download_media_sort_cb);
   return i;
}

static void
download_media_free(Media *i)
{
   E_Client *ec;

   if (!i) return;
   tw_mod->media_list = eina_inlist_remove(tw_mod->media_list, EINA_INLIST_GET(i));
   if (i->client) ecore_con_url_free(i->client);
   if (i->buf) eina_binbuf_free(i->buf);
   if (i->tmpfile) ecore_file_unlink(i->tmpfile);
   EINA_LIST_FREE(i->clients, ec)
     evas_object_event_callback_del_full(ec->frame, EVAS_CALLBACK_DEL, media_client_del, i);
   eina_stringshare_del(i->tmpfile);
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
        if (i->video_thread) continue; //ignore currently-threaded media
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
link_uri_local_type_get(const char *uri)
{
   size_t len = strlen(uri);

   if (uri[len - 1] == '/') return TEAMWORK_LINK_TYPE_LOCAL_DIRECTORY;
   return TEAMWORK_LINK_TYPE_LOCAL_FILE;
}

static Teamwork_Link_Type
link_uri_type_get(const char *uri)
{
   if (!uri[0]) return TEAMWORK_LINK_TYPE_NONE; //invalid
   if (uri[0] == '/') return link_uri_local_type_get(uri + 1); //probably a file?
   if ((!strncasecmp(uri, "http://", 7)) || (!strncasecmp(uri, "https://", 8))) return TEAMWORK_LINK_TYPE_REMOTE;
   if (!strncmp(uri, "file://", 7)) return link_uri_local_type_get(uri + 7);
   WRN("Unknown link type for '%s'", uri);
   return TEAMWORK_LINK_TYPE_NONE;
}

EINTERN void
tw_link_detect(E_Client *ec, const char *uri)
{
   Media *i;

   if (!tw_config->allowed_media_age) return;

   if (link_uri_type_get(uri) != TEAMWORK_LINK_TYPE_REMOTE) return;
   i = download_media_add(uri);
   if ((!i->clients) || (!eina_list_data_find(i->clients, ec)))
     {
        i->clients = eina_list_append(i->clients, ec);
        evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_DEL, media_client_del, i);
     }
}

static void
link_show_helper(const char *uri, Eina_Bool signal_open)
{
   Teamwork_Link_Type type;
   Eina_Bool dummy = EINA_TRUE;

   if (tw_mod->pop && (!e_util_strcmp(evas_object_data_get(tw_mod->pop, "uri"), uri))) return;
   type = link_uri_type_get(uri);
   switch (type)
     {
      case TEAMWORK_LINK_TYPE_NONE:
        dummy = EINA_FALSE;
        break;
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
                   if (i->buf)
                     {
                        tw_show(i);
                        dummy = EINA_FALSE;
                     }
                   else if (i->dummy) break;
                   else
                     {
                        i->show = 1;
                        dummy = EINA_FALSE;
                     }
                }
           }
         else if (!i->dummy)
           {
              tw_show(i);
              dummy = EINA_FALSE;
           }
         break;
      }
     }
   if (tw_mod->pop) tw_mod->force = signal_open;
   else if (dummy)
     link_failure_show();
}

EINTERN void
tw_link_show(E_Client *ec, const char *uri, int x, int y)
{
   if (evas_key_modifier_is_set(evas_key_modifier_get(e_comp->evas), "Control")) return;
   tw_win = ec;
   last_coords.x = x;
   last_coords.y = y;
   link_show_helper(uri, 0);
   tw_mod->hidden = 0;
}

EINTERN void
tw_link_hide(E_Client *ec, const char *uri)
{
   if (tw_tooltip)
     elm_object_tooltip_hide(e_comp->elm);
   if (ec != tw_win) return;
   if (tw_mod->pop && (!tw_mod->sticky) &&
       ((tw_tmpfile && eina_streq(evas_object_data_get(tw_mod->pop, "uri"), tw_tmpfile)) ||
         eina_streq(evas_object_data_get(tw_mod->pop, "uri"), uri)))
     {
        if (tw_config->mouse_out_delay)
          {
             if (tw_hide_timer) ecore_timer_reset(tw_hide_timer);
             else tw_hide_timer = ecore_timer_add(tw_config->mouse_out_delay, tw_hide, NULL);
          }
        else
          tw_hide(NULL);
        tw_mod->force = 0;
     }
   else if (tw_tmpthread || tw_tmpfile)
     tw_hide(NULL);
   tw_mod->hidden = !tw_mod->pop;
}

EINTERN void
tw_link_open(E_Client *ec, const char *uri)
{
   if (ec->focused)
     e_util_open(uri, NULL);
}

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
   ADD(video, UCHAR);
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
media_cache_add(const char *sha1, unsigned long long timestamp, Eina_Bool video)
{
   Media_Cache *ic;
   if (!tw_cache_list[0]) return;
   ic = malloc(sizeof(Media_Cache));
   ic->sha1 = eina_stringshare_ref(sha1);
   ic->timestamp = timestamp;
   ic->video = video;
   tw_cache_list[video]->cache = eina_list_sorted_insert(tw_cache_list[video]->cache, (Eina_Compare_Cb)media_cache_compare, ic);
}

static void
media_cache_del(const char *sha1, Eina_Bool video)
{
   Eina_List *l, *l2;
   Media_Cache *ic;

   if (!tw_cache_list[0]) return;
   EINA_LIST_FOREACH_SAFE(tw_cache_list[video]->cache, l, l2, ic)
     {
        if (ic->sha1 == sha1) continue;
        tw_cache_list[video]->cache = eina_list_remove_list(tw_cache_list[video]->cache, l);
        return;
     }
}

static void
media_cache_update(const char *sha1, unsigned long long timestamp, Eina_Bool video)
{
   Media_Cache *ic;
   Eina_List *l;

   if (!tw_cache_list[video]) return;
   EINA_LIST_FOREACH(tw_cache_list[video]->cache, l, ic)
     {
        if (ic->sha1 != sha1) continue;
        ic->timestamp = timestamp;
        break;
     }
   tw_cache_list[video]->cache = eina_list_sort(tw_cache_list[video]->cache, 0, (Eina_Compare_Cb)media_cache_compare);
}

static Eina_Bool
media_cleaner_cb(void *data)
{
   unsigned long long now;
   Media_Cache_List *mcl = data;
   Media_Cache *ic;
   Eina_List *l, *l2;
   int cleaned = 0;
   if ((!cleaner_edd) || (!cache_edd) || (tw_config->allowed_media_age < 0) || (!mcl) || (!mcl->cache))
     {
        if (mcl)
          media_cleaner[mcl->video] = NULL;
        return EINA_FALSE;
     }

   if (tw_config->allowed_media_age)
     {
        now = (unsigned long long)ecore_time_unix_get();
        now -= tw_config->allowed_media_age * 24 * 60 * 60;
     }
   else
     now = ULLONG_MAX;
   EINA_LIST_FOREACH_SAFE(mcl->cache, l, l2, ic)
     {
        /* only clean up to 3 entries at a time to ensure responsiveness */
        if (cleaned >= 3) break;
        if (ic->timestamp >= now)
          {
             /* stop the idler for now to avoid pointless spinning */
             ecore_timer_add(24 * 60 * 60, (Ecore_Task_Cb)tw_idler_start, NULL);
             media_cleaner[mcl->video] = NULL;
             tw_cache_list[mcl->video] = mcl;
             return EINA_FALSE;
          }
        eet_delete(media[mcl->video], ic->sha1);
        mcl->cache = eina_list_remove_list(mcl->cache, l);
        eina_stringshare_del(ic->sha1);
        free(ic);
        cleaned++;
     }
   tw_cache_list[mcl->video] = mcl;
   return EINA_TRUE;
}

static void
tw_dummy_add(const char *url)
{
   if (!dummies) return;
   eet_write(dummies, url, "0", 1, 0);
   DBG("Added new dummy for url %s", url);
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
tw_media_add(const char *url, Eina_Binbuf *buf, unsigned long long timestamp, Eina_Bool video)
{
   const char *sha1;
   int lsize;
   char **list;

   if (!media[video]) return -1;
   if (!tw_config->allowed_media_age) return 0; //disk caching disabled

   sha1 = sha1_encode(eina_binbuf_string_get(buf), eina_binbuf_length_get(buf));
   DBG("Media: %s - %s", url, sha1);

   list = eet_list(media[video], url, &lsize);
   if (lsize)
     {
        /* should never happen; corruption likely */
        eet_delete(media[video], url);
        free(list);
     }
   list = eet_list(media[video], sha1, &lsize);
   if (lsize)
     {
        eet_alias(media[video], url, sha1, 0);
        eet_sync(media[video]);
        DBG("Added new alias for media %s", sha1);
        eina_stringshare_del(sha1);
        free(list);
        return 0;
     }

   eet_write(media[video], sha1, eina_binbuf_string_get(buf), eina_binbuf_length_get(buf), 0);
   eet_alias(media[video], url, sha1, 0);
   eet_sync(media[video]);
   media_cache_add(sha1, timestamp, video);
   DBG("Added new media with length %zu: %s", eina_binbuf_length_get(buf), sha1);
   eina_stringshare_del(sha1);
   return 1;
}

void
tw_media_del(const char *url, Eina_Bool video)
{
   const char *alias;
   if (!media[video]) return;
   alias = eet_alias_get(media[video], url);
   eet_delete(media[video], alias);
   media_cache_del(alias, video);
   eina_stringshare_del(alias);
}

static Eina_Binbuf *
tw_media_get(const char *url, unsigned long long timestamp, Eina_Bool *video)
{
   unsigned char *img;
   Eina_Binbuf *buf = NULL;
   const char *alias;
   char **list;
   int size, lsize;

   for (*video = 0; *video <= MEDIA_CACHE_TYPE_VIDEO; (*video)++)
     {
        if (!media[*video]) return NULL;

        list = eet_list(media[*video], url, &lsize);
        if (!lsize) continue;
        free(list);

        img = eet_read(media[*video], url, &size);
        alias = eet_alias_get(media[*video], url);
        buf = eina_binbuf_manage_new(img, size, EINA_FALSE);
        media_cache_update(alias, timestamp, *video);

        eina_stringshare_del(alias);
        return buf;
     }
   *video = 0;
   return NULL;
}

static void
tw_media_ping(const char *url, unsigned long long timestamp, Eina_Bool video)
{
   const char *alias;

   if (!media[video]) return;

   alias = eet_alias_get(media[video], url);
   media_cache_update(alias, timestamp, video);

   eina_stringshare_del(alias);
}

static Eina_Bool
tw_idler_start(void)
{
   if (!media[0]) return EINA_FALSE;
   media_cleaner[0] = ecore_idler_add((Ecore_Task_Cb)media_cleaner_cb, tw_cache_list[0]);
   media_cleaner[1] = ecore_idler_add((Ecore_Task_Cb)media_cleaner_cb, tw_cache_list[1]);
   return EINA_FALSE;
}

static void
tw_popup_del(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   eina_stringshare_del(evas_object_data_get(obj, "uri"));
}

EINTERN void
tw_popup_opacity_set(void)
{
   int c = lround((double)255 * (tw_config->popup_opacity / 100.));
   if (tw_mod->pop)
     evas_object_color_set(tw_mod->pop, c, c, c, c);
}

static void
tw_show_helper(Evas_Object *o, int w, int h)
{
   int px, py, pw, ph;
   double ratio = tw_config->popup_size / 100.;
   E_Zone *zone = e_zone_current_get();

   evas_object_hide(tw_mod->pop);
   evas_object_del(tw_mod->pop);
   tw_mod->sticky = 0;
   tw_mod->pop = e_comp_object_util_add(o, E_COMP_OBJECT_TYPE_POPUP);
   evas_object_pass_events_set(tw_mod->pop, 1);
   pw = MIN(w, (ratio * (double)zone->w));
   pw = MIN(pw, zone->w);
   if (pw == w) ph = h;
   else
     ph = lround((double)(pw * h) / ((double)w));
   if (ph > zone->h)
     {
        ph = zone->h;
        pw = lround((double)(ph * w) / ((double)h));
     }
   e_livethumb_vsize_set(o, pw, ph);
   evas_object_layer_set(tw_mod->pop, E_LAYER_POPUP);
   evas_object_resize(tw_mod->pop, pw, ph);

   if ((!tw_win) && (last_coords.x == last_coords.y) && (last_coords.x == -1))
     {
        px = lround(ratio * (double)zone->w) - (pw / 2);
        py = lround(ratio * (double)zone->h) - (ph / 2);
        if (px + pw > zone->w)
          px = zone->w - pw;
        if (py + ph > zone->h)
          py = zone->h - ph;
        evas_object_move(tw_mod->pop, px, py);
     }
   else if (tw_win)
     {
        int x, y;

        x = tw_win->client.x + last_coords.x;
        y = tw_win->client.y + last_coords.y;
        /* prefer tooltip left of last_coords */
        px = x - pw - 3;
        /* if it's offscreen, try right of last_coords */
        if (px < 0) px = x + 3;
        /* fuck this, stick it right on the last_coords */
        if (px + pw + 3 > zone->w)
          px = (x / 2) - (pw / 2);
        /* give up */
        if (px < 0) px = 0;

        /* prefer tooltip above last_coords */
        py = y - ph - 3;
        /* if it's offscreen, try below last_coords */
        if (py < 0) py = y + 3;
        /* fuck this, stick it right on the last_coords */
        if (py + ph + 3 > zone->h)
          py = (y / 2) - (ph / 2);
        /* give up */
        if (py < 0) py = 0;
        evas_object_move(tw_mod->pop, px, py);
     }
   else
     {
        e_comp_object_util_center_on(tw_mod->pop, zone->bg_clip_object);
     }
   evas_object_show(tw_mod->pop);
   tw_popup_opacity_set();
   evas_object_event_callback_add(tw_mod->pop, EVAS_CALLBACK_DEL, tw_popup_del, NULL);
}

static Eina_Bool
stupid_obj_del_workaround_hack(void *data)
{
   evas_object_del(data);
   return EINA_FALSE;
}

static void
tw_video_closed_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   evas_object_hide(obj);
   evas_object_hide(data);
   emotion_object_play_set(obj, EINA_FALSE);
   ecore_timer_add(3.0, stupid_obj_del_workaround_hack, data);
   if (!tw_tmpfile) return;
   eina_stringshare_replace(&tw_tmpfile, NULL);
}

static void
tw_video_opened_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   int iw, ih, w, h;
   double ratio = tw_config->popup_size / 100.;
   E_Zone *zone;

   evas_object_smart_callback_del_full(obj, "frame_decode", tw_video_opened_cb, data);
   if (tw_mod->hidden && (!tw_mod->sticky) && (!tw_mod->force))
     {
        tw_video_closed_cb(data, obj, NULL);
        return;
     }
   emotion_object_size_get(obj, &iw, &ih);
   if ((iw <= 0) || (ih <= 0))
     {
        tw_video_closed_cb(data, obj, NULL);
        return;
     }

   zone = e_zone_current_get();
   w = MIN(zone->w, (ratio * (double)zone->w));
   ratio = emotion_object_ratio_get(obj);
   if (ratio > 0.0) iw = (ih * ratio) + 0.5;
   if (iw < 1) iw = 1;

   h = (w * ih) / iw;
   e_livethumb_thumb_set(data, obj);
   tw_show_helper(data, w, h);
   evas_object_data_set(tw_mod->pop, "uri", eina_stringshare_add(emotion_object_file_get(obj)));
}

static void
tw_video_del_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   if (emotion_object_file_get(obj) != tw_tmpfile) return;
   if (!tw_tmpfile) return;
   eina_stringshare_replace(&tw_tmpfile, NULL);
}

static void
tw_show_video(Evas_Object *prev, const char *uri)
{
   Evas_Object *o;

   o = emotion_object_add(e_livethumb_evas_get(prev));
   emotion_object_init(o, "gstreamer1");
   emotion_object_file_set(o, uri);
   emotion_object_play_set(o, EINA_TRUE);
   evas_object_smart_callback_add(o, "frame_decode", tw_video_opened_cb, prev);
   evas_object_smart_callback_add(o, "decode_stop", tw_video_closed_cb, prev);
   evas_object_resize(o, 1, 1);
   if (uri == tw_tmpfile)
     evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, tw_video_del_cb, NULL);
}

static void
tw_video_thread_cancel_cb(void *data, Ecore_Thread *eth)
{
   Media *i;

   if (ecore_thread_local_data_find(eth, "dead")) return;
   i = data;
   tw_tmpthread = i->video_thread = NULL;
   tw_tmpthread_media = NULL;
   close(tw_tmpfd);
   tw_tmpfd = -1;
   download_media_cleanup();
}

static void
tw_video_thread_done_cb(void *data, Ecore_Thread *eth)
{
   Media *i;
   Evas_Object *prev;

   if (ecore_thread_local_data_find(eth, "dead")) return;
   i = data;
   tw_tmpthread = i->video_thread = NULL;
   tw_tmpthread_media = NULL;
   close(tw_tmpfd);
   tw_tmpfd = -1;
   i->tmpfile = eina_stringshare_ref(tw_tmpfile);
   prev = e_livethumb_add(e_comp->evas);
   tw_show_video(prev, tw_tmpfile);
   download_media_cleanup();
}

static void
tw_video_thread_cb(void *data, Ecore_Thread *eth)
{
   int fd;
   Media *i;
   const void *buf;
   size_t pos = 0, tot;

   if (ecore_thread_local_data_find(eth, "dead")) return;
   i = data;
   fd = (intptr_t)ecore_thread_global_data_find("teamwork_media_file");

   if (ftruncate(fd, 0))
     {
        ERR("TRUNCATE FAILED: %s", strerror(errno));
        ecore_thread_cancel(eth);
        return; //fail if file can't be zeroed
     }
   tot = eina_binbuf_length_get(i->buf);
   buf = eina_binbuf_string_get(i->buf);
   while (pos < tot)
     {
        unsigned int num = 4096;

        if (pos + num >= tot)
          num = tot - pos;
        if (write(fd, (char*)buf + pos, num) < 0)
          {
             ERR("WRITE FAILED: %s", strerror(errno));
             ecore_thread_cancel(eth);
             return;
          }
        pos += num;
        if (ecore_thread_local_data_find(eth, "dead")) break;
     }
}

static void
tw_show(Media *i)
{
   Evas_Object *o, *ic, *prev;
   int w, h;

   if (!i->buf) i->buf = tw_media_get(i->addr, ecore_time_unix_get(), &i->video);
   if (!i->buf)
     {
        download_media_add(i->addr);
        return;
     }
   if (i->video)
     {
        char buf[PATH_MAX];
        Eina_Tmpstr *tmpfile;

        if (tw_config->disable_video) return;
        while (i->tmpfile)
          {
             if (!ecore_file_exists(i->tmpfile))
               {
                  eina_stringshare_replace(&i->tmpfile, NULL);
                  break;
               }
             if (tw_tmpthread)
               {
                  ecore_thread_local_data_add(tw_tmpthread, "dead", (void*)1, NULL, 0);
                  E_FREE_FUNC(tw_tmpthread, ecore_thread_cancel);
                  tw_tmpthread_media->video_thread = NULL;
               }
             if (tw_tmpfd != -1)
               {
                  close(tw_tmpfd);
                  tw_tmpfd = -1;
               }
             eina_stringshare_del(tw_tmpfile);
             tw_tmpfile = eina_stringshare_ref(i->tmpfile);
             prev = e_livethumb_add(e_comp->evas);
             tw_show_video(prev, tw_tmpfile);
             return;
          }
        snprintf(buf, sizeof(buf), "teamwork-%s-XXXXXX", ecore_file_file_get(i->addr));
        if (tw_tmpfile)
          {
             if (tw_tmpthread)
               {
                  ecore_thread_local_data_add(tw_tmpthread, "dead", (void*)1, NULL, 0);
                  E_FREE_FUNC(tw_tmpthread, ecore_thread_cancel);
                  tw_tmpthread_media->video_thread = NULL;
               }
             close(tw_tmpfd);
          }
        tw_tmpfd = eina_file_mkstemp(buf, &tmpfile);
        eina_stringshare_replace(&tw_tmpfile, tmpfile);
        if (tw_tmpfd < 0)
          {
             ERR("ERROR: %s", strerror(errno));
             download_media_cleanup();
             eina_stringshare_replace(&tw_tmpfile, NULL);
             tw_tmpthread_media = NULL;
             eina_tmpstr_del(tmpfile);
             return;
          }
        tw_tmpthread_media = i;
        ecore_thread_global_data_add("teamwork_media_file", (intptr_t*)(long)tw_tmpfd, NULL, 0);
        i->video_thread = tw_tmpthread = ecore_thread_run(tw_video_thread_cb, tw_video_thread_done_cb, tw_video_thread_cancel_cb, i);
        return;
     }
   else
     {
        prev = e_livethumb_add(e_comp->evas);
        o = evas_object_image_filled_add(e_livethumb_evas_get(prev));
        evas_object_image_memfile_set(o, (void*)eina_binbuf_string_get(i->buf), eina_binbuf_length_get(i->buf), NULL, NULL);
        evas_object_image_size_get(o, &w, &h);
        ic = e_icon_add(e_livethumb_evas_get(prev));
        e_icon_image_object_set(ic, o);
     }
   e_livethumb_thumb_set(prev, ic);
   tw_show_helper(prev, w, h);
   evas_object_data_set(tw_mod->pop, "uri", eina_stringshare_ref(i->addr));
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
   Eina_Bool video = EINA_FALSE;

   video = emotion_object_extension_may_play_get(uri);
   if (video)
     {
        if (tw_config->disable_video) return;
     }
   else
     {
        if (!evas_object_image_extension_can_load_get(uri)) return;
     }
   prev = e_livethumb_add(e_comp->evas);
   if (video)
     {
        tw_show_video(prev, uri);
        return;
     }
   else
     {
        o = e_icon_add(e_livethumb_evas_get(prev));
        e_icon_file_set(o, uri);
        e_icon_preload_set(o, 1);
        e_icon_size_get(o, &w, &h);
     }
   e_livethumb_thumb_set(prev, o);
   tw_show_helper(prev, w, h);
   evas_object_data_set(tw_mod->pop, "uri", eina_stringshare_add(uri));
}

static void
tw_handler_hide(void)
{
   if (tw_mod->force || tw_mod->sticky) return;
   if (tw_config->mouse_out_delay)
     {
        if (tw_hide_timer) ecore_timer_reset(tw_hide_timer);
        else tw_hide_timer = ecore_timer_add(tw_config->mouse_out_delay, tw_hide, NULL);
     }
   else
     tw_hide(NULL);
   tw_mod->force = 0;
}

static Eina_Bool
button_press(void *data EINA_UNUSED, int type EINA_UNUSED, void *ev EINA_UNUSED)
{
   if (tw_mod->pop) tw_handler_hide();
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
desk_show(void *data EINA_UNUSED, int type EINA_UNUSED, E_Event_Desk_Show *ev EINA_UNUSED)
{
   if (tw_mod->pop) tw_handler_hide();
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
focus_out(void *data EINA_UNUSED, int type EINA_UNUSED, E_Event_Client *ev EINA_UNUSED)
{
   if (e_config->focus_policy == E_FOCUS_CLICK) return ECORE_CALLBACK_RENEW;
   if (tw_mod->pop) tw_handler_hide();
   return ECORE_CALLBACK_RENEW;
}

EINTERN Eina_Bool
tw_hide(void *d EINA_UNUSED)
{
   if (tw_tmpthread)
     {
        ecore_thread_local_data_add(tw_tmpthread, "dead", (void*)1, NULL, 0);
        E_FREE_FUNC(tw_tmpthread, ecore_thread_cancel);
        tw_tmpthread_media->video_thread = NULL;
     }
   if (tw_tmpfd != -1)
     {
        close(tw_tmpfd);
        tw_tmpfd = -1;
     }
   eina_stringshare_replace(&tw_tmpfile, NULL);
   tw_win = NULL;
   evas_object_hide(tw_mod->pop);
   E_FREE_FUNC(tw_mod->pop, evas_object_del);
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

#ifdef HAVE_WAYLAND
   if (e_comp->comp_type == E_PIXMAP_TYPE_WL)
     wl_tw_init();
#endif
#ifndef HAVE_WAYLAND_ONLY
   if (e_comp_util_has_x())
     x11_tw_init();
#endif

   e_user_dir_concat_static(buf, "images/tw_cache_images.eet");
   media[MEDIA_CACHE_TYPE_IMAGE] = eet_open(buf, EET_FILE_MODE_READ_WRITE);
   if (!media[MEDIA_CACHE_TYPE_IMAGE])
     {
        ERR("Could not open image cache file!");
        return 0;
     }

   e_user_dir_concat_static(buf, "images/tw_cache_video.eet");
   media[MEDIA_CACHE_TYPE_VIDEO] = eet_open(buf, EET_FILE_MODE_READ_WRITE);
   if (!media[MEDIA_CACHE_TYPE_VIDEO])
     {
        ERR("Could not open video cache file!");
        E_FREE_FUNC(media[MEDIA_CACHE_TYPE_IMAGE], eet_close);
        return 0;
     }
        
   cache_edd = media_cache_edd_new();
   EET_EINA_FILE_DATA_DESCRIPTOR_CLASS_SET(&eddc, Media_Cache_List);
   cleaner_edd = eet_data_descriptor_file_new(&eddc);
   EET_DATA_DESCRIPTOR_ADD_LIST(cleaner_edd, Media_Cache_List, "cache", cache, cache_edd);
   EET_DATA_DESCRIPTOR_ADD_BASIC(cleaner_edd, Media_Cache_List, "video", video, EET_T_UCHAR);
   tw_cache_list[MEDIA_CACHE_TYPE_IMAGE] = eet_data_read(media[MEDIA_CACHE_TYPE_IMAGE], cleaner_edd, "media_cache");
   if (!tw_cache_list[MEDIA_CACHE_TYPE_IMAGE])
     tw_cache_list[MEDIA_CACHE_TYPE_IMAGE] = E_NEW(Media_Cache_List, 1);
   tw_cache_list[MEDIA_CACHE_TYPE_VIDEO] = eet_data_read(media[MEDIA_CACHE_TYPE_VIDEO], cleaner_edd, "media_cache");
   if (!tw_cache_list[MEDIA_CACHE_TYPE_VIDEO])
     {
        tw_cache_list[MEDIA_CACHE_TYPE_VIDEO] = E_NEW(Media_Cache_List, 1);
        tw_cache_list[MEDIA_CACHE_TYPE_VIDEO]->video = 1;
     }

   e_user_dir_concat_static(buf, "images/dummies.eet");
   dummies = eet_open(buf, EET_FILE_MODE_READ_WRITE);

   E_LIST_HANDLER_APPEND(handlers, ECORE_CON_EVENT_URL_COMPLETE, download_media_complete, tw_mod);
   E_LIST_HANDLER_APPEND(handlers, ECORE_CON_EVENT_URL_PROGRESS, download_media_status, tw_mod);
   E_LIST_HANDLER_APPEND(handlers, ECORE_CON_EVENT_URL_DATA, download_media_data, tw_mod);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_FOCUS_OUT, focus_out, tw_mod);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_DESK_SHOW, desk_show, tw_mod);
   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_MOUSE_BUTTON_DOWN, button_press, tw_mod);
   E_LIST_HANDLER_APPEND(handlers, ECORE_EVENT_MOUSE_BUTTON_UP, button_press, tw_mod);

   tw_mod->media = eina_hash_string_superfast_new((Eina_Free_Cb)download_media_free);
   return 1;
}

EINTERN void
e_tw_shutdown(void)
{
   unsigned int x;

#ifdef HAVE_WAYLAND
   if (e_comp->comp_type == E_PIXMAP_TYPE_WL)
     wl_tw_shutdown();
#endif
#ifndef HAVE_WAYLAND_ONLY
   if (e_comp_util_has_x())
     x11_tw_shutdown();
#endif
   E_FREE_LIST(handlers, ecore_event_handler_del);
   for (x = 0; x <= MEDIA_CACHE_TYPE_VIDEO; x++)
     {
        if (media[x])
          {
             if (tw_cache_list[x])
               {
                  Media_Cache *ic;
                  eet_data_write(media[x], cleaner_edd, "media_cache", tw_cache_list[x], 1);
                  EINA_LIST_FREE(tw_cache_list[x]->cache, ic)
                    {
                       eina_stringshare_del(ic->sha1);
                       free(ic);
                    }
                  free(tw_cache_list[x]);
               }
             E_FREE_FUNC(media[x], eet_close);
          }
     }
   E_FREE_FUNC(dummies, eet_close);
   E_FREE_FUNC(cleaner_edd, eet_data_descriptor_free);
   E_FREE_FUNC(cache_edd, eet_data_descriptor_free);
   if (tw_tmpfd != -1)
     {
        close(tw_tmpfd);
        tw_tmpfd = -1;
     }
   eina_stringshare_replace(&tw_tmpfile, NULL);
   E_FREE_FUNC(tw_tmpthread, ecore_thread_cancel);
   tw_tmpthread_media = NULL;
   tw_hide(NULL);
   last_coords.x = last_coords.y = 0;
   eina_hash_free(tw_mod->media);
   evas_object_hide(tw_mod->pop);
   E_FREE_FUNC(tw_mod->pop, evas_object_del);
}

EINTERN void
tw_uri_show(const char *uri)
{
   link_show_helper(uri, 1);
}

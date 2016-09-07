#include "e.h"
#include "emix.h"

#include <pulse/pulseaudio.h>

#undef ERR
#undef DBG
#undef WRN
#define ERR(...)      EINA_LOG_ERR(__VA_ARGS__)
#define DBG(...)      EINA_LOG_DBG(__VA_ARGS__)
#define WRN(...)      EINA_LOG_WARN(__VA_ARGS__)

#define PA_VOLUME_TO_INT(_vol) \
   ((_vol * EMIX_VOLUME_BARRIER) / \
    PA_VOLUME_NORM)
#define INT_TO_PA_VOLUME(_vol) \
   ((PA_VOLUME_NORM * _vol) / \
    EMIX_VOLUME_BARRIER)

typedef struct _Context
{
   pa_mainloop_api api;
   pa_context *context;
   pa_context_state_t state;
   Emix_Event_Cb cb;
   const void *userdata;
   Ecore_Timer *connect;
   int default_sink;

   Eina_List *sinks, *sources, *inputs;
   Eina_Bool connected;
} Context;

typedef struct _Sink
{
   Emix_Sink base;
   int idx;
} Sink;

typedef struct _Sink_Input
{
   Emix_Sink_Input base;
   const char *icon;
   int idx;
} Sink_Input;

typedef struct _Source
{
   Emix_Source base;
   int idx;
} Source;

static Context *ctx = NULL;
extern pa_mainloop_api functable;

static pa_cvolume
_emix_volume_convert(const Emix_Volume volume)
{
   pa_cvolume vol;
   unsigned int i;

   vol.channels = volume.channel_count;
   for (i = 0; i < volume.channel_count; i++)
     vol.values[i] = INT_TO_PA_VOLUME(volume.volumes[i]);
   return vol;
}

static Emix_Volume
_pa_cvolume_convert(const pa_cvolume *volume)
{
   Emix_Volume vol;
   int i;

   vol.volumes = calloc(volume->channels, sizeof(int));
   if (!vol.volumes)
     {
        WRN("Could not allocate memory for volume");
        vol.channel_count = 0;
        return vol;
     }

   vol.channel_count = volume->channels;
   for (i = 0; i < volume->channels; i++)
     vol.volumes[i] = PA_VOLUME_TO_INT(volume->values[i]);
   return vol;
}

static void
_sink_del(Sink *sink)
{
   Emix_Port *port;

   EINA_SAFETY_ON_NULL_RETURN(sink);
   EINA_LIST_FREE(sink->base.ports, port)
     {
        eina_stringshare_del(port->name);
        eina_stringshare_del(port->description);
        free(port);
     }

   free(sink->base.volume.volumes);
   eina_stringshare_del(sink->base.name);
   free(sink);
}

static void
_sink_input_del(Sink_Input *input)
{
   EINA_SAFETY_ON_NULL_RETURN(input);

   free(input->base.volume.volumes);
   eina_stringshare_del(input->base.name);
   eina_stringshare_del(input->icon);
   free(input);
}

static void
_source_del(Source *source)
{
   EINA_SAFETY_ON_NULL_RETURN(source);

   free(source->base.volume.volumes);
   eina_stringshare_del(source->base.name);
   free(source);
}

static void
_sink_cb(pa_context *c EINA_UNUSED, const pa_sink_info *info, int eol,
         void *userdata EINA_UNUSED)
{
   Sink *sink;
   Emix_Port *port;
   uint32_t i;

   if (eol < 0)
     {
        if (pa_context_errno(c) == PA_ERR_NOENTITY)
           return;

        ERR("Sink callback failure");
        return;
     }

   if (eol > 0)
      return;

   DBG("sink index: %d\nsink name: %s", info->index,
       info->name);

   sink = calloc(1, sizeof(Sink));
   sink->idx = info->index;
   sink->base.name = eina_stringshare_add(info->description);
   sink->base.volume = _pa_cvolume_convert(&info->volume);
   sink->base.mute = !!info->mute;

   for (i = 0; i < info->n_ports; i++)
     {
        port = calloc(1, sizeof(Emix_Port));
        if (!port)
          {
             WRN("Could not allocate memory for Sink's port");
             continue;
          }

        port->available = !!info->ports[i]->available;
        port->name = eina_stringshare_add(info->ports[i]->name);
        port->description =  eina_stringshare_add(info->ports[i]->description);
        sink->base.ports = eina_list_append(sink->base.ports, port);
        if (info->ports[i]->name == info->active_port->name)
           port->active = EINA_TRUE;
     }

   ctx->sinks = eina_list_append(ctx->sinks, sink);
   if (ctx->cb)
      ctx->cb((void *)ctx->userdata, EMIX_SINK_ADDED_EVENT,
              (Emix_Sink *)sink);
}

static void
_sink_changed_cb(pa_context *c EINA_UNUSED, const pa_sink_info *info, int eol,
                 void *userdata EINA_UNUSED)
{
   Sink *sink = NULL, *s;
   Emix_Port *port;
   uint32_t i;
   Eina_List *l;

   if (eol < 0)
     {
        if (pa_context_errno(c) == PA_ERR_NOENTITY)
           return;

        ERR("Sink callback failure");
        return;
     }

   if (eol > 0)
      return;

   DBG("sink index: %d\nsink name: %s", info->index,
       info->name);

   EINA_LIST_FOREACH(ctx->sinks, l, s)
     {
        if (s->idx == (int)info->index)
          {
             sink = s;
             break;
          }
     }

   EINA_SAFETY_ON_NULL_RETURN(sink);

   sink->base.name = eina_stringshare_add(info->description);
   sink->base.volume = _pa_cvolume_convert(&info->volume);
   sink->base.mute = !!info->mute;

   if (sink->base.ports)
     {
        EINA_LIST_FREE(sink->base.ports, port)
          {
             eina_stringshare_del(port->name);
             eina_stringshare_del(port->description);
             free(port);
          }
     }
   for (i = 0; i < info->n_ports; i++)
     {
        port = calloc(1, sizeof(Emix_Port));
        if (!port)
          {
             WRN("Could not allocate memory for Sink's port");
             continue;
          }

        port->available = !!info->ports[i]->available;
        port->name = eina_stringshare_add(info->ports[i]->name);
        port->description =  eina_stringshare_add(info->ports[i]->description);
        sink->base.ports = eina_list_append(sink->base.ports, port);
        if (info->ports[i]->name == info->active_port->name)
           port->active = EINA_TRUE;
     }

   if (ctx->cb)
     ctx->cb((void *)ctx->userdata, EMIX_SINK_CHANGED_EVENT,
                  (Emix_Sink *)sink);
}

static void
_sink_remove_cb(int index, void *data EINA_UNUSED)
{
   Sink *sink;
   Eina_List *l;
   DBG("Removing sink: %d", index);

   EINA_LIST_FOREACH(ctx->sinks, l, sink)
     {
        if (sink->idx == index)
          {
             ctx->sinks = eina_list_remove_list(ctx->sinks, l);
             if (ctx->cb)
               ctx->cb((void *)ctx->userdata, EMIX_SINK_REMOVED_EVENT,
                            (Emix_Sink *)sink);
             _sink_del(sink);
             break;
          }
     }
}

static const char *
_icon_from_properties(pa_proplist *l)
{
   const char *t;

   if ((t = pa_proplist_gets(l, PA_PROP_MEDIA_ICON_NAME)))
      return t;

   if ((t = pa_proplist_gets(l, PA_PROP_WINDOW_ICON_NAME)))
      return t;

   if ((t = pa_proplist_gets(l, PA_PROP_APPLICATION_ICON_NAME)))
      return t;

   if ((t = pa_proplist_gets(l, PA_PROP_MEDIA_ROLE)))
     {

        if (strcmp(t, "video") == 0 ||
            strcmp(t, "phone") == 0)
           return t;

        if (strcmp(t, "music") == 0)
           return "audio";

        if (strcmp(t, "game") == 0)
           return "applications-games";

        if (strcmp(t, "event") == 0)
           return "dialog-information";
     }

   return "audio-card";
}

static void
_sink_input_cb(pa_context *c EINA_UNUSED, const pa_sink_input_info *info,
               int eol, void *userdata EINA_UNUSED)
{
   Sink_Input *input;
   Eina_List *l;
   Sink *s;
   EINA_SAFETY_ON_NULL_RETURN(ctx);

   if (eol < 0)
     {
        if (pa_context_errno(c) == PA_ERR_NOENTITY)
          return;

        ERR("Sink input callback failure");
        return;
     }

   if (eol > 0)
      return;

   input = calloc(1, sizeof(Sink_Input));
   EINA_SAFETY_ON_NULL_RETURN(input);

   DBG("sink input index: %d\nsink input name: %s", info->index,
       info->name);

   input->idx = info->index;
   input->base.name = eina_stringshare_add(info->name);
   input->base.volume = _pa_cvolume_convert(&info->volume);
   input->base.mute = !!info->mute;
   EINA_LIST_FOREACH(ctx->sinks, l, s)
     {
        if (s->idx == (int)info->sink)
          input->base.sink = (Emix_Sink *)s;
     }
   input->icon = eina_stringshare_add(_icon_from_properties(info->proplist));
   ctx->inputs = eina_list_append(ctx->inputs, input);

   if (ctx->cb)
     ctx->cb((void *)ctx->userdata, EMIX_SINK_INPUT_ADDED_EVENT,
             (Emix_Sink_Input *)input);
}

static void
_sink_input_changed_cb(pa_context *c EINA_UNUSED,
                       const pa_sink_input_info *info, int eol,
                       void *userdata EINA_UNUSED)
{
   Sink_Input *input = NULL, *i;
   Sink *s = NULL;
   Eina_List *l;

   EINA_SAFETY_ON_NULL_RETURN(ctx);
   if (eol < 0)
     {
        if (pa_context_errno(c) == PA_ERR_NOENTITY)
           return;

        ERR("Sink input changed callback failure");
        return;
     }

   if (eol > 0)
      return;

   EINA_LIST_FOREACH(ctx->inputs, l, i)
     {
        if (i->idx == (int)info->index)
          {
            input = i;
            break;
          }
     }

   DBG("sink input changed index: %d\n", info->index);

   if (!input)
     {
        input = calloc(1, sizeof(Sink_Input));
        EINA_SAFETY_ON_NULL_RETURN(input);
        ctx->inputs = eina_list_append(ctx->inputs, input);
     }
   input->idx = info->index;
   input->base.volume = _pa_cvolume_convert(&info->volume);
   input->base.mute = !!info->mute;

   EINA_LIST_FOREACH(ctx->sinks, l, s)
     {
        if (s->idx == (int)info->sink)
          input->base.sink = (Emix_Sink *)s;
     }

   if (ctx->cb)
     ctx->cb((void *)ctx->userdata, EMIX_SINK_INPUT_CHANGED_EVENT,
             (Emix_Sink_Input *)input);
}

static void
_sink_input_remove_cb(int index, void *data EINA_UNUSED)
{
   Sink_Input *input;
   Eina_List *l;
   EINA_SAFETY_ON_NULL_RETURN(ctx);

   DBG("Removing sink input: %d", index);

   EINA_LIST_FOREACH(ctx->inputs, l, input)
     {
        if (input->idx == index)
          {
             ctx->inputs = eina_list_remove_list(ctx->inputs, l);
             if (ctx->cb)
               ctx->cb((void *)ctx->userdata,
                       EMIX_SINK_INPUT_REMOVED_EVENT,
                       (Emix_Sink_Input *)input);
             _sink_input_del(input);

             break;
          }
     }
}

static void
_source_cb(pa_context *c EINA_UNUSED, const pa_source_info *info,
           int eol, void *userdata EINA_UNUSED)
{
   Source *source;
   EINA_SAFETY_ON_NULL_RETURN(ctx);

   if (eol < 0)
     {
        if (pa_context_errno(c) == PA_ERR_NOENTITY)
           return;

        ERR("Source callback failure");
        return;
     }

   if (eol > 0)
      return;

   source = calloc(1, sizeof(Source));
   EINA_SAFETY_ON_NULL_RETURN(source);

   source->idx = info->index;
   source->base.name = eina_stringshare_add(info->name);
   source->base.volume = _pa_cvolume_convert(&info->volume);
   source->base.mute = !!info->mute;

   ctx->sources = eina_list_append(ctx->sources, source);
   if (ctx->cb)
     ctx->cb((void *)ctx->userdata, EMIX_SOURCE_ADDED_EVENT,
             (Emix_Source *)source);
}

static void
_source_changed_cb(pa_context *c EINA_UNUSED,
                       const pa_source_info *info, int eol,
                       void *userdata EINA_UNUSED)
{
   Source *source = NULL, *s;
   Eina_List *l;
   EINA_SAFETY_ON_NULL_RETURN(ctx);

   if (eol < 0)
     {
        if (pa_context_errno(c) == PA_ERR_NOENTITY)
           return;

        ERR("Source changed callback failure");
        return;
     }

   if (eol > 0)
      return;

   EINA_LIST_FOREACH(ctx->sources, l, s)
     {
        if (s->idx == (int)info->index)
          {
             source = s;
             break;
          }
     }

   DBG("source changed index: %d\n", info->index);

   if (!source)
     {
        source = calloc(1, sizeof(Source));
        EINA_SAFETY_ON_NULL_RETURN(source);
        ctx->sources = eina_list_append(ctx->sources, source);
     }
   source->idx= info->index;
   source->base.volume = _pa_cvolume_convert(&info->volume);
   source->base.mute = !!info->mute;

   if (ctx->cb)
     ctx->cb((void *)ctx->userdata, EMIX_SOURCE_CHANGED_EVENT,
                  (Emix_Source *)source);
}

static void
_source_remove_cb(int index, void *data EINA_UNUSED)
{
   Source *source;
   Eina_List *l;
   EINA_SAFETY_ON_NULL_RETURN(ctx);

   DBG("Removing source: %d", index);

   EINA_LIST_FOREACH(ctx->sources, l, source)
     {
        if (source->idx == index)
          {
             ctx->sources = eina_list_remove_list(ctx->sources, l);
             if (ctx->cb)
               ctx->cb((void *)ctx->userdata, EMIX_SOURCE_REMOVED_EVENT,
                       (Emix_Source *)source);

             _source_del(source);
             break;
          }
     }
}

static void
_sink_default_cb(pa_context *c EINA_UNUSED, const pa_sink_info *info, int eol,
                 void *userdata EINA_UNUSED)
{
   if (eol < 0)
     {
        if (pa_context_errno(c) == PA_ERR_NOENTITY)
           return;

        ERR("Sink callback failure");
        return;
     }

   if (eol > 0)
      return;

   DBG("sink index: %d\nsink name: %s", info->index,
       info->name);

   ctx->default_sink = info->index;
   if (ctx->cb)
      ctx->cb((void *)ctx->userdata, EMIX_READY_EVENT, NULL);
}

static void
_server_info_cb(pa_context *c, const pa_server_info *info,
           void *userdata)
{
   pa_operation *o;

   if (pa_context_errno(c) != PA_OK)
     {
        WRN("Could not get pa server info, error: %d", pa_context_errno(c));
        return;
     }

   EINA_SAFETY_ON_NULL_RETURN(info);

   if (!(o = pa_context_get_sink_info_by_name(c, info->default_sink_name,
                                              _sink_default_cb, userdata)))
     {
        ERR("pa_context_get_sink_info_by_name() failed");
        return;
     }
   pa_operation_unref(o);
}

static void
_subscribe_cb(pa_context *c, pa_subscription_event_type_t t,
              uint32_t index, void *data)
{
   pa_operation *o;

   switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
    case PA_SUBSCRIPTION_EVENT_SINK:
       if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) ==
           PA_SUBSCRIPTION_EVENT_REMOVE)
          _sink_remove_cb(index, data);
       else if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) ==
                PA_SUBSCRIPTION_EVENT_NEW)
         {
            if (!(o = pa_context_get_sink_info_by_index(c, index,
                                                        _sink_cb, data)))
              {
                 ERR("pa_context_get_sink_info_by_index() failed");
                 return;
              }
            pa_operation_unref(o);
         }
       else
         {
            if (!(o = pa_context_get_sink_info_by_index(c, index,
                                                        _sink_changed_cb,
                                                        data)))
              {
                 ERR("pa_context_get_sink_info_by_index() failed");
                 return;
              }
            pa_operation_unref(o);
         }
       break;

    case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
       if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) ==
           PA_SUBSCRIPTION_EVENT_REMOVE)
          _sink_input_remove_cb(index, data);
       else if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) ==
                PA_SUBSCRIPTION_EVENT_NEW)
         {
            if (!(o = pa_context_get_sink_input_info(c, index,
                                                     _sink_input_cb, data)))
              {
                 ERR("pa_context_get_sink_input_info() failed");
                 return;
              }
            pa_operation_unref(o);
         }
       else
         {
            if (!(o = pa_context_get_sink_input_info(c, index,
                                                     _sink_input_changed_cb,
                                                     data)))
              {
                 ERR("pa_context_get_sink_input_info() failed");
                 return;
              }
            pa_operation_unref(o);
         }
       break;

    case PA_SUBSCRIPTION_EVENT_SOURCE:
       if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) ==
           PA_SUBSCRIPTION_EVENT_REMOVE)
          _source_remove_cb(index, data);
       else if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) ==
                PA_SUBSCRIPTION_EVENT_NEW)
         {
            if (!(o = pa_context_get_source_info_by_index(c, index,
                                                          _source_cb, data)))
              {
                 ERR("pa_context_get_source_info() failed");
                 return;
              }
            pa_operation_unref(o);
         }
       else
         {
            if (!(o = pa_context_get_source_info_by_index(c, index,
                                                          _source_changed_cb,
                                                          data)))
              {
                 ERR("pa_context_get_source_info() failed");
                 return;
              }
            pa_operation_unref(o);
         }
       break;
    case PA_SUBSCRIPTION_EVENT_SERVER:
       if (!(o = pa_context_get_server_info(c, _server_info_cb,
                                            data)))
         {
            ERR("pa_context_get_server_info() failed");
            return;
         }
       pa_operation_unref(o);
       break;
    default:
       WRN("Event not handled");
       break;
   }
}

static Eina_Bool _pulse_connect(void *data);
static void _disconnect_cb(void);

static void
_pulse_pa_state_cb(pa_context *context, void *data)
{
   pa_operation *o;

   switch (pa_context_get_state(context))
     {
      case PA_CONTEXT_UNCONNECTED:
      case PA_CONTEXT_CONNECTING:
      case PA_CONTEXT_AUTHORIZING:
      case PA_CONTEXT_SETTING_NAME:
         break;

      case PA_CONTEXT_READY:
         {
            ctx->connect = NULL;
            ctx->connected = EINA_TRUE;
            pa_context_set_subscribe_callback(context, _subscribe_cb, ctx);
            if (!(o = pa_context_subscribe(context, (pa_subscription_mask_t)
                                           (PA_SUBSCRIPTION_MASK_SINK|
                                            PA_SUBSCRIPTION_MASK_SOURCE|
                                            PA_SUBSCRIPTION_MASK_SINK_INPUT|
                                            PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT|
                                            PA_SUBSCRIPTION_MASK_CLIENT|
                                            PA_SUBSCRIPTION_MASK_SERVER|
                                            PA_SUBSCRIPTION_MASK_CARD),
                                           NULL, NULL)))
              {
                 ERR("pa_context_subscribe() failed");
                 return;
              }
            pa_operation_unref(o);

            if (!(o = pa_context_get_sink_info_list(context, _sink_cb, ctx)))
              {
                 ERR("pa_context_get_sink_info_list() failed");
                 return;
              }
            pa_operation_unref(o);

            if (!(o = pa_context_get_sink_input_info_list(context,
                                                          _sink_input_cb,
                                                          ctx)))
              {
                 ERR("pa_context_get_sink_input_info_list() failed");
                 return;
              }
            pa_operation_unref(o);

            if (!(o = pa_context_get_source_info_list(context, _source_cb,
                                                      ctx)))
              {
                 ERR("pa_context_get_source_info_list() failed");
                 return;
              }
            pa_operation_unref(o);

            if (!(o = pa_context_get_server_info(context, _server_info_cb,
                                                 ctx)))
              {
                 ERR("pa_context_get_server_info() failed");
                 return;
              }
            pa_operation_unref(o);
            break;
         }

      case PA_CONTEXT_FAILED:
         WRN("PA_CONTEXT_FAILED");
         if (!ctx->connect)
           ctx->connect = ecore_timer_add(1.0, _pulse_connect, data);
         goto err;
      case PA_CONTEXT_TERMINATED:
         ERR("PA_CONTEXT_TERMINATE:");
      default:
         if (ctx->connect)
           {
              ecore_timer_del(ctx->connect);
              ctx->connect = NULL;
           }
         goto err;
     }
   return;

err:
   if (ctx->connected)
     {
        _disconnect_cb();
        ctx->connected = EINA_FALSE;
     }
   pa_context_unref(ctx->context);
   ctx->context = NULL;
}

static Eina_Bool
_pulse_connect(void *data)
{
   pa_proplist *proplist;
   Context *c = data;

   proplist = pa_proplist_new();
   pa_proplist_sets(proplist, PA_PROP_APPLICATION_NAME, "Efl Volume Control");
   pa_proplist_sets(proplist, PA_PROP_APPLICATION_ID,
                    "org.enlightenment.volumecontrol");
   pa_proplist_sets(proplist, PA_PROP_APPLICATION_ICON_NAME, "audio-card");
#if !defined(EMIXER_BUILD) && defined(HAVE_WAYLAND) && !defined(HAVE_WAYLAND_ONLY)
   char *display;

   if (e_comp->comp_type != E_PIXMAP_TYPE_X)
     {
        display = getenv("DISPLAY");
        if (display) display = strdup(display);
        e_env_unset("DISPLAY");
     }
#endif
   c->context = pa_context_new_with_proplist(&(c->api), NULL, proplist);
   if (!c->context)
     {
        WRN("Could not create the pulseaudio context");
        goto err;
     }

   pa_context_set_state_callback(c->context, _pulse_pa_state_cb, c);
   if (pa_context_connect(c->context, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0)
     {
        WRN("Could not connect to pulse");
        goto err;
     }
#if !defined(EMIXER_BUILD) && defined(HAVE_WAYLAND) && !defined(HAVE_WAYLAND_ONLY)
   if (e_comp->comp_type != E_PIXMAP_TYPE_X)
     {
        e_env_set("DISPLAY", display);
        free(display);
     }
#endif

   pa_proplist_free(proplist);
   return ECORE_CALLBACK_DONE;

 err:
   pa_proplist_free(proplist);
   return ECORE_CALLBACK_RENEW;
}

static void
_shutdown(void)
{
   if (!ctx)
      return;

   if (ctx->connect)
     {
        ecore_timer_del(ctx->connect);
        ctx->connect = NULL;
     }
   if (ctx->context)
      pa_context_unref(ctx->context);
   if (ctx->connected)
      _disconnect_cb();
   free(ctx);
   ctx = NULL;
}

static Eina_Bool
_init(Emix_Event_Cb cb, const void *data)
{
   if (ctx)
     return EINA_TRUE;

   ctx = calloc(1, sizeof(Context));
   if (!ctx)
     {
        ERR("Could not create Epulse Context");
        return EINA_FALSE;
     }

   ctx->api = functable;
   ctx->api.userdata = ctx;

   /* The reason of compares with EINA_TRUE is because ECORE_CALLBACK_RENEW
      is EINA_TRUE. The function _pulse_connect returns ECORE_CALLBACK_RENEW
      when could not connect to pulse.
   */
   if (_pulse_connect(ctx) == EINA_TRUE)
     {
        _shutdown();
        return EINA_FALSE;
     }

   ctx->cb = cb;
   ctx->userdata = data;

   return EINA_TRUE;
 }

static void
_disconnect_cb()
{
   Source *source;
   Sink *sink;
   Sink_Input *input;

   if (ctx->cb)
      ctx->cb((void *)ctx->userdata, EMIX_DISCONNECTED_EVENT, NULL);

   EINA_LIST_FREE(ctx->sources, source)
      _source_del(source);
   EINA_LIST_FREE(ctx->sinks, sink)
      _sink_del(sink);
   EINA_LIST_FREE(ctx->inputs, input)
      _sink_input_del(input);
}

static void
_source_volume_set(Emix_Source *source, Emix_Volume volume)
{
   pa_operation* o;
   pa_cvolume vol = _emix_volume_convert(volume);
   Source *s = (Source *)source;
   EINA_SAFETY_ON_FALSE_RETURN(ctx && ctx->context && source != NULL);

   if (!(o = pa_context_set_source_volume_by_index(ctx->context,
                                                   s->idx, &vol,
                                                   NULL, NULL)))
      ERR("pa_context_set_source_volume_by_index() failed");
}

static void
_source_mute_set(Emix_Source *source, Eina_Bool mute)
{
   pa_operation* o;
   Source *s = (Source *)source;
   EINA_SAFETY_ON_FALSE_RETURN(ctx && ctx->context && source != NULL);

   if (!(o = pa_context_set_source_mute_by_index(ctx->context,
                                                 s->idx, mute, NULL, NULL)))
      ERR("pa_context_set_source_mute() failed");
}

static const Eina_List *
_sinks_get(void)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, NULL);
   return ctx->sinks;
}

static const Eina_List *
_sources_get(void)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, NULL);
   return ctx->sources;
}

static const Eina_List *
_sink_inputs_get(void)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, NULL);
   return ctx->inputs;
}

static void
_sink_volume_set(Emix_Sink *sink, Emix_Volume volume)
{
   pa_operation* o;
   Sink *s = (Sink *)sink;
   pa_cvolume vol = _emix_volume_convert(volume);
   EINA_SAFETY_ON_FALSE_RETURN((ctx && ctx->context && sink != NULL));

   if (!(o = pa_context_set_sink_volume_by_index(ctx->context,
                                                 s->idx, &vol, NULL, NULL)))
      ERR("pa_context_set_sink_volume_by_index() failed");
}

static void
_sink_mute_set(Emix_Sink *sink, Eina_Bool mute)
{
   pa_operation* o;
   Sink *s = (Sink *)sink;
   EINA_SAFETY_ON_FALSE_RETURN((ctx && ctx->context && sink != NULL));

   if (!(o = pa_context_set_sink_mute_by_index(ctx->context,
                                               s->idx, mute, NULL, NULL)))
      ERR("pa_context_set_sink_mute() failed");
}

static void
_sink_input_volume_set(Emix_Sink_Input *input, Emix_Volume volume)
{
   pa_operation* o;
   Sink_Input *sink_input = (Sink_Input *)input;
   pa_cvolume vol = _emix_volume_convert(volume);
   EINA_SAFETY_ON_FALSE_RETURN(ctx && ctx->context && input != NULL);


   if (!(o = pa_context_set_sink_input_volume(ctx->context,
                                              sink_input->idx, &vol,
                                              NULL, NULL)))
      ERR("pa_context_set_sink_input_volume_by_index() failed");
}

static void
_sink_input_mute_set(Emix_Sink_Input *input, Eina_Bool mute)
{
   pa_operation* o;
   Sink_Input *sink_input = (Sink_Input *)input;
   EINA_SAFETY_ON_FALSE_RETURN(ctx && ctx->context && input != NULL);

   if (!(o = pa_context_set_sink_input_mute(ctx->context,
                                            sink_input->idx, mute,
                                            NULL, NULL)))
      ERR("pa_context_set_sink_input_mute() failed");
}

static void
_sink_input_move(Emix_Sink_Input *input, Emix_Sink *sink)
{
   pa_operation* o;
   Sink *s = (Sink *)sink;
   Sink_Input *i = (Sink_Input *)input;
   EINA_SAFETY_ON_FALSE_RETURN(ctx && ctx->context && input != NULL
                               && sink != NULL);

   if (!(o = pa_context_move_sink_input_by_index(ctx->context,
                                                 i->idx, s->idx, NULL,
                                                 NULL)))
      ERR("pa_context_move_sink_input_by_index() failed");
}

static Eina_Bool
_sink_port_set(Emix_Sink *sink, const Emix_Port *port)
{
   pa_operation* o;
   Sink *s = (Sink *)sink;
   EINA_SAFETY_ON_FALSE_RETURN_VAL(ctx && ctx->context &&
                                   sink != NULL && port != NULL, EINA_FALSE);

   if (!(o = pa_context_set_sink_port_by_index(ctx->context,
                                               s->idx, port->name, NULL,
                                               NULL)))
     {
        ERR("pa_context_set_source_port_by_index() failed");
        return EINA_FALSE;
     }
   pa_operation_unref(o);

   return EINA_TRUE;
}

static Eina_Bool
_sink_default_support(void)
{
   return EINA_TRUE;
}

static const Emix_Sink *
_sink_default_get(void)
{
   Sink *s;
   Eina_List *l;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, NULL);
   EINA_LIST_FOREACH(ctx->sinks, l, s)
      if (s->idx == ctx->default_sink)
        return (Emix_Sink *)s;

   return NULL;
}

static Eina_Bool
_sink_change_support(void)
{
   return EINA_TRUE;
}

static int
_max_volume(void)
{
   return 150;
}

static Emix_Backend
_pulseaudio_backend =
{
   _init,
   _shutdown,
   _max_volume,
   _sinks_get,
   _sink_default_support,
   _sink_default_get,
   NULL,
   _sink_mute_set,
   _sink_volume_set,
   _sink_port_set,
   _sink_change_support,
   _sink_inputs_get,
   _sink_input_mute_set,
   _sink_input_volume_set,
   _sink_input_move,
   _sources_get,
   _source_mute_set,
   _source_volume_set,
   NULL,
};

E_API Emix_Backend *
emix_backend_pulse_get(void)
{
   return &_pulseaudio_backend;
}

E_API const char *emix_backend_pulse_name = "PULSEAUDIO";

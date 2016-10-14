#include "e.h"

E_API int E_EVENT_CLIENT_VOLUME = -1;
E_API int E_EVENT_CLIENT_MUTE = -1;
E_API int E_EVENT_CLIENT_UNMUTE = -1;

static void
_e_client_volume_event_simple_free(void *d EINA_UNUSED, E_Event_Client *ev)
{
   UNREFD(ev->ec, 3);
   e_object_unref(E_OBJECT(ev->ec));
   free(ev);
}

static void
_e_client_volume_event_simple(E_Client *ec, int type)
{
   E_Event_Client *ev;

   ev = E_NEW(E_Event_Client, 1);
   ev->ec = ec;
   REFD(ec, 3);
   e_object_ref(E_OBJECT(ec));
   ecore_event_add(type, ev,
                   (Ecore_End_Cb)_e_client_volume_event_simple_free, NULL);
}


EINTERN int
e_client_volume_init(void)
{
   E_EVENT_CLIENT_VOLUME = ecore_event_type_new();
   E_EVENT_CLIENT_MUTE = ecore_event_type_new();
   E_EVENT_CLIENT_UNMUTE = ecore_event_type_new();

   return EINA_TRUE;
}

EINTERN void
e_client_volume_shutdown(void)
{
}

E_API E_Client_Volume_Sink *
e_client_volume_sink_new(E_Client_Volume_Sink_Get func_get, E_Client_Volume_Sink_Set func_set, E_Client_Volume_Sink_Min_Get func_min_get, E_Client_Volume_Sink_Max_Get func_max_get, void *data)
{
   E_Client_Volume_Sink *sink;

   sink = E_NEW(E_Client_Volume_Sink, 1);
   sink->func_set = func_set;
   sink->func_get = func_get;
   sink->func_min_get = func_min_get;
   sink->func_max_get = func_max_get;
   sink->data = data;

   return sink;
}

E_API void
e_client_volume_sink_del(E_Client_Volume_Sink *sink)
{
   E_Client *ec;

   EINA_LIST_FREE(sink->clients, ec)
     {
        ec->sinks = eina_list_remove(ec->sinks, sink);
     }
   free(sink);
}

E_API void
e_client_volume_sink_set(E_Client_Volume_Sink *sink, int volume, Eina_Bool mute)
{
   EINA_SAFETY_ON_NULL_RETURN(sink);
   if (sink->func_set)
     sink->func_set(volume, mute, sink->data);
}

E_API void
e_client_volume_sink_get(const E_Client_Volume_Sink *sink, int *volume, Eina_Bool *mute)
{
   EINA_SAFETY_ON_NULL_RETURN(sink);
   if (sink->func_get)
     sink->func_get(volume, mute, sink->data);
}

E_API int
e_client_volume_sink_min_get(const E_Client_Volume_Sink *sink)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(sink, 0);
   if (sink->func_min_get)
     return sink->func_min_get(sink->data);

   return 0;
}

E_API int
e_client_volume_sink_max_get(const E_Client_Volume_Sink *sink)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(sink, 0);
   if (sink->func_max_get)
     return sink->func_max_get(sink->data);
   return 0;
}

E_API void
e_client_volume_sink_append(E_Client *ec, E_Client_Volume_Sink *sink)
{
   int volume_min;
   int volume_max;
   int volume;
   Eina_Bool mute;

   EINA_SAFETY_ON_NULL_RETURN(ec);

   ec->sinks = eina_list_append(ec->sinks, sink);
   sink->clients = eina_list_append(sink->clients, ec);
   if (ec->volume_control_enabled)
     {
        volume_min = e_client_volume_sink_min_get(sink);
        if (ec->volume_min < volume_min)
          ec->volume_min = volume_min;
        volume_max = e_client_volume_sink_max_get(sink);
        if (ec->volume_max > volume_max)
          ec->volume_max = volume_max;
        if ((ec->volume_min > ec->volume)
            || (ec->volume_max < ec->volume))
          e_client_volume_set(ec, ec->volume);
        e_client_volume_sink_get(sink, &volume, &mute);
        if ((ec->volume != volume) || (ec->mute != mute))
          {
             e_client_volume_sink_set(sink, ec->volume, ec->mute);
          }
     }
   else
     {
        ec->volume_min = e_client_volume_sink_min_get(sink);
        ec->volume_max = e_client_volume_sink_max_get(sink);
        e_client_volume_sink_get(sink, &volume, &mute);
        ec->volume = volume;
        ec->mute = !!mute;
        ec->volume_control_enabled = EINA_TRUE;
     }
}

E_API void
e_client_volume_sink_remove(E_Client *ec, E_Client_Volume_Sink *sink)
{
   EINA_SAFETY_ON_NULL_RETURN(ec);
   ec->sinks = eina_list_remove(ec->sinks, sink);
   sink->clients = eina_list_remove(sink->clients, ec);
}

E_API void
e_client_volume_sink_update(E_Client_Volume_Sink *sink)
{
   Eina_List *l;
   int volume;
   Eina_Bool mute;
   E_Client *ec;

   EINA_SAFETY_ON_NULL_RETURN(sink);

   e_client_volume_sink_get(sink, &volume, &mute);
   EINA_LIST_FOREACH(sink->clients, l, ec)
     {
        e_client_volume_set(ec, volume);
        e_client_volume_mute_set(ec, mute);
     }
}

E_API void
e_client_volume_set(E_Client *ec, int volume)
{
   Eina_List *l;
   E_Client_Volume_Sink *sink;

   EINA_SAFETY_ON_NULL_RETURN(ec);
   if (volume < ec->volume_min) volume = ec->volume_min;
   if (volume > ec->volume_max) volume = ec->volume_max;
   if (volume == ec->volume) return;
   ec->volume = volume;

   EINA_LIST_FOREACH(ec->sinks, l, sink)
     {
        e_client_volume_sink_set(sink, ec->volume, ec->mute);
     }

   _e_client_volume_event_simple(ec, E_EVENT_CLIENT_VOLUME);
}

E_API void
e_client_volume_mute_set(E_Client *ec, Eina_Bool mute)
{
   Eina_List *l;
   E_Client_Volume_Sink *sink;

   EINA_SAFETY_ON_NULL_RETURN(ec);
   if (!!mute == ec->mute) return;
   ec->mute = !!mute;

   EINA_LIST_FOREACH(ec->sinks, l, sink)
     {
        if (sink->func_set)
          sink->func_set(ec->volume, ec->mute, sink->data);
     }
   if (mute)
     _e_client_volume_event_simple(ec, E_EVENT_CLIENT_MUTE);
   else
     _e_client_volume_event_simple(ec, E_EVENT_CLIENT_UNMUTE);
}


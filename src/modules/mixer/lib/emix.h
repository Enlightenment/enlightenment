#ifndef EMIX_H
#define EMIX_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <Eina.h>
#include <Evas.h>

#ifdef E_API
#undef E_API
#endif

#ifdef __GNUC__
# if __GNUC__ >= 4
#  define E_API __attribute__ ((visibility("default")))
# else
#  define E_API
# endif
#else
# define E_API
#endif


#define EMIX_VOLUME_BARRIER 100

enum Emix_Event {
   EMIX_READY_EVENT = 0,
   EMIX_DISCONNECTED_EVENT,
   EMIX_SINK_ADDED_EVENT,
   EMIX_SINK_REMOVED_EVENT,
   EMIX_SINK_CHANGED_EVENT,
   EMIX_SINK_INPUT_ADDED_EVENT,
   EMIX_SINK_INPUT_REMOVED_EVENT,
   EMIX_SINK_INPUT_CHANGED_EVENT,
   EMIX_SOURCE_ADDED_EVENT,
   EMIX_SOURCE_REMOVED_EVENT,
   EMIX_SOURCE_CHANGED_EVENT
};

typedef struct _Emix_Volume {
   unsigned int channel_count;
   // the index of the field is the id of the channel, the value the volume
   int *volumes;
} Emix_Volume;

typedef struct _Emix_Port {
   Eina_Bool active;
   Eina_Bool available;
   const char *name;
   const char *description;
} Emix_Port;

typedef struct _Emix_Sink {
   const char *name;
   Emix_Volume volume;
   Eina_Bool mute;
   Eina_List *ports;
} Emix_Sink;

typedef struct _Emix_Sink_Input {
   const char *name;
   Emix_Volume volume;
   Eina_Bool mute;
   Emix_Sink *sink;
} Emix_Sink_Input;

typedef struct _Emix_Source {
   const char *name;
   Emix_Volume volume;
   Eina_Bool mute;
} Emix_Source;

typedef void (*Emix_Event_Cb)(void *data, enum Emix_Event event,
                              void *event_info);

typedef struct _Emix_Backend {
   Eina_Bool             (*ebackend_init)(Emix_Event_Cb cb, const void *data);
   void                  (*ebackend_shutdown)(void);

   int                   (*ebackend_max_volume_get)(void);

   const Eina_List*      (*ebackend_sinks_get)(void);
   Eina_Bool             (*ebackend_sink_default_support)(void);
   const Emix_Sink*      (*ebackend_sink_default_get)(void);
   void                  (*ebackend_sink_default_set)(Emix_Sink *sink);
   void                  (*ebackend_sink_mute_set)(Emix_Sink *sink,
                                                   Eina_Bool mute);
   void                  (*ebackend_sink_volume_set)(Emix_Sink *sink,
                                                     Emix_Volume volume);
   Eina_Bool             (*ebackend_sink_port_set)(Emix_Sink *sink,
                                                   const Emix_Port *port);
   Eina_Bool             (*ebackend_sink_change_support)(void);

   const Eina_List*      (*ebackend_sink_inputs_get)(void);
   void                  (*ebackend_sink_input_mute_set)(
                                        Emix_Sink_Input *input, Eina_Bool mute);
   void                  (*ebackend_sink_input_volume_set)(
                                    Emix_Sink_Input *input, Emix_Volume volume);
   void                  (*ebackend_sink_input_sink_change)(
                                       Emix_Sink_Input *input, Emix_Sink *sink);

   const Eina_List*      (*ebackend_sources_get)(void);
   void                  (*ebackend_source_mute_set)(Emix_Source *source,
                                                     Eina_Bool mute);
   void                  (*ebackend_source_volume_set)(Emix_Source *source,
                                                       Emix_Volume volume);

   Evas_Object*          (*ebackend_advanced_options_add)(Evas_Object *parent);
} Emix_Backend;


E_API Eina_Bool           emix_init(void);
E_API void                emix_shutdown(void);
E_API const Eina_List*    emix_backends_available(void);
E_API Eina_Bool           emix_backend_set(const char *backend);

E_API Eina_Bool           emix_event_callback_add(Emix_Event_Cb cb,
                                                 const void *data);
E_API Eina_Bool           emix_event_callback_del(Emix_Event_Cb cb);

E_API int                 emix_max_volume_get(void);

E_API const Eina_List*    emix_sinks_get(void);
E_API Eina_Bool           emix_sink_default_support(void);
E_API const Emix_Sink*    emix_sink_default_get(void);
E_API Eina_Bool           emix_sink_port_set(Emix_Sink *sink, Emix_Port *port);
E_API void                emix_sink_default_set(Emix_Sink *sink);
E_API void                emix_sink_mute_set(Emix_Sink *sink, Eina_Bool mute);
E_API void                emix_sink_volume_set(Emix_Sink *sink,
                                              Emix_Volume volume);
E_API Eina_Bool           emix_sink_change_support(void);

E_API const Eina_List*    emix_sink_inputs_get(void);
E_API void                emix_sink_input_mute_set(Emix_Sink_Input *input,
                                                  Eina_Bool mute);
E_API void                emix_sink_input_volume_set(Emix_Sink_Input *input,
                                                    Emix_Volume volume);
E_API void                emix_sink_input_sink_change(Emix_Sink_Input *input,
                                                     Emix_Sink *sink);

E_API const Eina_List*    emix_sources_get(void);
E_API void                emix_source_mute_set(Emix_Source *source,
                                                  Eina_Bool mute);
E_API void                emix_source_volume_set(Emix_Source *source,
                                                Emix_Volume volume);
E_API Evas_Object*        emix_advanced_options_add(Evas_Object *parent);

#endif  /* EMIX_H */

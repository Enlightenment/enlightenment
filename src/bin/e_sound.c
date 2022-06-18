#include <e.h>

#include <Ecore_Audio.h>

static int _can_sound_out = -1;
static Eo *_sound_out = NULL;
static int _outs = 0;
static Ecore_Timer *_outs_end_timer = NULL;

static void
_cb_out_fail(void *data EINA_UNUSED, const Efl_Event *event)
{
   efl_unref(event->object);
   _sound_out = NULL;
   _can_sound_out = 0;
}

static void
_cb_out_ready(void *data EINA_UNUSED, const Efl_Event *event EINA_UNUSED)
{
   _can_sound_out = 1;
}

static Eina_Bool
_cb_outs_end_timer(void *data EINA_UNUSED)
{
   efl_unref(_sound_out);
   _sound_out = NULL;
   return EINA_FALSE;
}

static void
_out_end(void)
{
   if (_outs > 0) _outs--;
   if (_outs == 0)
     {
        if (_outs_end_timer) ecore_timer_del(_outs_end_timer);
        _outs_end_timer = ecore_timer_add(2.0, _cb_outs_end_timer, NULL);
     }
}

static void
_cb_in_stopped(void *data EINA_UNUSED, const Efl_Event *event)
{
   efl_unref(event->object);
   _out_end();
}

E_API int
e_sound_init(void)
{
   ecore_audio_init();
   return 1;
}

E_API int
e_sound_shutdown(void)
{
   if (_outs_end_timer)
     {
        ecore_timer_del(_outs_end_timer);
        _outs_end_timer = NULL;
     }
   if (_sound_out)
     {
        efl_unref(_sound_out);
        _sound_out = NULL;
     }
   ecore_audio_shutdown();
   return 1;
}

E_API void
e_sound_file_play(const char *file, double vol)
{
   Eo *in;
   char buf[PATH_MAX];

   if (_can_sound_out == 0) return;
   if (!_sound_out)
     _sound_out = efl_add_ref
       (ECORE_AUDIO_OUT_PULSE_CLASS, NULL,
        efl_event_callback_add(efl_added,
                               ECORE_AUDIO_OUT_PULSE_EVENT_CONTEXT_FAIL,
                               _cb_out_fail, NULL),
        efl_event_callback_add(efl_added,
                               ECORE_AUDIO_OUT_PULSE_EVENT_CONTEXT_READY,
                               _cb_out_ready, NULL)
       );
   if (!_sound_out) return;

   if (_outs_end_timer)
     {
        ecore_timer_del(_outs_end_timer);
        _outs_end_timer = NULL;
     }
   _outs++;
   snprintf(buf, sizeof(buf), "sound-file[%s]", file);
   in = efl_add_ref(ECORE_AUDIO_IN_SNDFILE_CLASS, NULL,
                    efl_name_set(efl_added, buf),
                    efl_event_callback_add(efl_added,
                                           ECORE_AUDIO_IN_EVENT_IN_STOPPED,
                                           _cb_in_stopped, NULL));
   if (!in)
     {
        _out_end();
        return;
     }
   if (!ecore_audio_obj_source_set(in, file))
     {
        efl_unref(in);
        _out_end();
        return;
     }
   ecore_audio_obj_volume_set(in, vol);
   if (!ecore_audio_obj_out_input_attach(_sound_out, in))
     {
        efl_unref(in);
        _out_end();
        return;
     }
}

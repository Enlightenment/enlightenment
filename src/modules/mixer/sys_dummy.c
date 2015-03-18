#include "e_mod_mixer.h"

static const char *_name = NULL;

static void
_e_mixer_dummy_set(void)
{
   if (!_name) _name = eina_stringshare_add("No ALSA mixer found!");
}

E_Mixer_System *
e_mixer_system_new(const char *name)
{
   _e_mixer_dummy_set();

   if (!name) return NULL;

   if (name == _name || strcmp(name, _name) == 0)
     return (E_Mixer_System *)-1;
   else
     return NULL;
}

void
e_mixer_system_del(E_Mixer_System *self EINA_UNUSED)
{
}

int
e_mixer_system_callback_set(const E_Mixer_System *self EINA_UNUSED, int (*func)(void *data, E_Mixer_System *self) EINA_UNUSED, void *data EINA_UNUSED)
{
   return 0;
}

Eina_List *
e_mixer_system_get_cards(void)
{
   _e_mixer_dummy_set();

   return eina_list_append(NULL, eina_stringshare_ref(_name));
}

const char *
e_mixer_system_get_default_card(void)
{
   _e_mixer_dummy_set();

   return eina_stringshare_ref(_name);
}

const char *
e_mixer_system_get_card_name(const char *card)
{
   _e_mixer_dummy_set();

   if (card == _name || strcmp(card, _name) == 0)
     return eina_stringshare_ref(_name);
   else
     return NULL;
}

Eina_List *
e_mixer_system_get_channels(const E_Mixer_System *self EINA_UNUSED)
{
   E_Mixer_Channel_Info *ch_info;

   _e_mixer_dummy_set();

   ch_info = malloc(sizeof(*ch_info));
   ch_info->id = (void*)-2;
   ch_info->name = eina_stringshare_ref(_name);
   ch_info->capabilities = E_MIXER_CHANNEL_CAN_MUTE|E_MIXER_CHANNEL_HAS_PLAYBACK;

   return eina_list_append(NULL, ch_info);
}

Eina_List *
e_mixer_system_get_channel_names(const E_Mixer_System *self EINA_UNUSED)
{
   _e_mixer_dummy_set();

   return eina_list_append(NULL, eina_stringshare_ref(_name));
}

const char *
e_mixer_system_get_default_channel_name(const E_Mixer_System *self EINA_UNUSED)
{
   _e_mixer_dummy_set();

   return eina_stringshare_ref(_name);
}

E_Mixer_Channel_Info *
e_mixer_system_get_channel_by_name(const E_Mixer_System *self EINA_UNUSED, const char *name)
{
   E_Mixer_Channel_Info *ch_info;

   _e_mixer_dummy_set();

   if (name == _name || strcmp(name, _name) == 0)
     {
        ch_info = malloc(sizeof(*ch_info));
        ch_info->id = (void*)-2;
        ch_info->name = eina_stringshare_ref(_name);
        ch_info->capabilities = E_MIXER_CHANNEL_CAN_MUTE|E_MIXER_CHANNEL_HAS_PLAYBACK;

        return ch_info;
     }
   else
     return NULL;
}

int
e_mixer_system_get_volume(const E_Mixer_System *self EINA_UNUSED,
                          const E_Mixer_Channel_Info *channel EINA_UNUSED,
                          int *left, int *right)
{
   if (left)
     *left = 0;
   if (right)
     *right = 0;

   return 1;
}

int
e_mixer_system_set_volume(const E_Mixer_System *self EINA_UNUSED,
                          const E_Mixer_Channel_Info *channel EINA_UNUSED,
                          int left EINA_UNUSED, int right EINA_UNUSED)
{
   return 0;
}

int
e_mixer_system_get_mute(const E_Mixer_System *self EINA_UNUSED,
                        const E_Mixer_Channel_Info *channel EINA_UNUSED,
                        int *mute)
{
   if (mute)
     *mute = 1;

   return 1;
}

int
e_mixer_system_set_mute(const E_Mixer_System *self EINA_UNUSED,
                        const E_Mixer_Channel_Info *channel EINA_UNUSED,
                        int mute EINA_UNUSED)
{
   return 0;
}

int
e_mixer_system_get_state(const E_Mixer_System *self EINA_UNUSED,
                         const E_Mixer_Channel_Info *channel EINA_UNUSED,
                         E_Mixer_Channel_State *state)
{
   const E_Mixer_Channel_State def = {1, 0, 0};

   if (state)
     *state = def;

   return 1;
}

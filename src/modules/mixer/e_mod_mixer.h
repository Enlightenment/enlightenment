#ifndef E_MOD_MIXER_H
#define E_MOD_MIXER_H

#include "e.h"

typedef void E_Mixer_App;
typedef void E_Mixer_System;
typedef void E_Mixer_Channel;

typedef struct _E_Mixer_Channel_State
{
   int mute;
   int left;
   int right;
} E_Mixer_Channel_State;

#define E_MIXER_CHANNEL_CAN_MUTE       0x01
#define E_MIXER_CHANNEL_IS_MONO        0x02
#define E_MIXER_CHANNEL_HAS_CAPTURE    0x04
#define E_MIXER_CHANNEL_HAS_PLAYBACK   0x08
#define E_MIXER_CHANNEL_GROUP_MASK     0xFC
#define E_MIXER_CHANNEL_USABLE_MASK    0xFD

#define e_mod_mixer_channel_is_mutable(_ch) \
   ( ((_ch)->capabilities & E_MIXER_CHANNEL_CAN_MUTE )!=0 )
#define e_mod_mixer_channel_is_mono(_ch) \
   ( ((_ch)->capabilities & E_MIXER_CHANNEL_IS_MONO )!=0 )
#define e_mod_mixer_channel_has_capture(_ch) \
   ( ((_ch)->capabilities & E_MIXER_CHANNEL_HAS_CAPTURE )!=0 )
#define e_mod_mixer_channel_has_playback(_ch) \
   ( ((_ch)->capabilities & E_MIXER_CHANNEL_HAS_PLAYBACK )!=0 )
#define e_mod_mixer_channel_is_boost(_ch) \
   ( ((_ch)->capabilities & E_MIXER_CHANNEL_HAS_PLAYBACK )!=0 && \
     ((_ch)->capabilities & E_MIXER_CHANNEL_HAS_CAPTURE )!=0 )
#define e_mod_mixer_channel_has_no_volume(_ch) \
   ( ((_ch)->capabilities & E_MIXER_CHANNEL_HAS_PLAYBACK )==0 && \
     ((_ch)->capabilities & E_MIXER_CHANNEL_HAS_CAPTURE )==0 )
#define e_mod_mixer_channel_group_get(_ch) \
   ( (_ch)->capabilities & E_MIXER_CHANNEL_GROUP_MASK )
#define e_mod_mixer_capabilities_usable(_capa) \
   ( ((_capa) & E_MIXER_CHANNEL_USABLE_MASK)!=0 )

typedef struct _E_Mixer_Channel_Info
{
   int                      capabilities;
   const char              *name;
   E_Mixer_Channel         *id;
   E_Mixer_App             *app;
} E_Mixer_Channel_Info;

typedef int (*E_Mixer_Volume_Set_Cb)(const E_Mixer_System *, const E_Mixer_Channel_Info *, int, int);
typedef int (*E_Mixer_Volume_Get_Cb)(const E_Mixer_System *, const E_Mixer_Channel_Info *, int *, int *);
typedef int (*E_Mixer_Mute_Get_Cb)(const E_Mixer_System *, const E_Mixer_Channel_Info *, int *);
typedef int (*E_Mixer_Mute_Set_Cb)(const E_Mixer_System *, const E_Mixer_Channel_Info *, int);
typedef int (*E_Mixer_State_Get_Cb)(const E_Mixer_System *, const E_Mixer_Channel_Info *, E_Mixer_Channel_State *);
typedef int (*E_Mixer_Capture_Cb)(const E_Mixer_System *, const E_Mixer_Channel_Info *);
typedef void *(*E_Mixer_Cb)();

extern Eina_Bool _mixer_using_default;
extern E_Mixer_Volume_Get_Cb e_mod_mixer_volume_get;
extern E_Mixer_Volume_Set_Cb e_mod_mixer_volume_set;
extern E_Mixer_Mute_Get_Cb e_mod_mixer_mute_get;
extern E_Mixer_Mute_Set_Cb e_mod_mixer_mute_set;
extern E_Mixer_State_Get_Cb e_mod_mixer_state_get;
extern E_Mixer_Cb e_mod_mixer_new;
extern E_Mixer_Cb e_mod_mixer_del;
extern E_Mixer_Cb e_mod_mixer_channel_default_name_get;
extern E_Mixer_Cb e_mod_mixer_channel_info_get_by_name;
extern E_Mixer_Cb e_mod_mixer_channel_names_get;
extern E_Mixer_Cb e_mod_mixer_card_name_get;
extern E_Mixer_Cb e_mod_mixer_card_names_get;
extern E_Mixer_Cb e_mod_mixer_card_default_get;

void e_mod_mixer_channel_info_free(E_Mixer_Channel_Info*);
Eina_List *e_mod_mixer_channel_infos_get(const E_Mixer_System *sys);
void e_mod_mixer_channel_infos_free(Eina_List*);
void e_mod_mixer_channel_names_free(Eina_List*);
void e_mod_mixer_card_names_free(Eina_List*);

void e_mixer_default_setup(void);
void e_mixer_pulse_setup();

/* ALSA | DUMMY */
int e_mixer_system_callback_set(const E_Mixer_System *self, int (*func)(void *data, E_Mixer_System *self), void *data);

E_Mixer_System *e_mixer_system_new(const char *card);
void e_mixer_system_del(E_Mixer_System *self);

Eina_List *e_mixer_system_get_cards(void);
const char *e_mixer_system_get_default_card(void);
const char *e_mixer_system_get_card_name(const char *card);

Eina_List *e_mixer_system_get_channels(const E_Mixer_System *self);
Eina_List *e_mixer_system_get_channel_names(const E_Mixer_System *self);

const char *e_mixer_system_get_default_channel_name(const E_Mixer_System *self);
E_Mixer_Channel_Info *e_mixer_system_get_channel_by_name(const E_Mixer_System *self, const char *name);

int e_mixer_system_get_volume(const E_Mixer_System *self, const E_Mixer_Channel_Info *channel, int *left, int *right);
int e_mixer_system_set_volume(const E_Mixer_System *self, const E_Mixer_Channel_Info *channel, int left, int right);
int e_mixer_system_get_mute(const E_Mixer_System *self, const E_Mixer_Channel_Info *channel, int *mute);
int e_mixer_system_set_mute(const E_Mixer_System *self, const E_Mixer_Channel_Info *channel, int mute);
int e_mixer_system_get_state(const E_Mixer_System *self, const E_Mixer_Channel_Info *channel, E_Mixer_Channel_State *state);

/* PULSE */
int pulse_init(void);
Eina_Bool e_mixer_pulse_ready(void);
Eina_Bool e_mixer_pulse_init(void);
void e_mixer_pulse_shutdown(void);

E_Mixer_System *e_mixer_pulse_new(const char *name);
void e_mixer_pulse_del(E_Mixer_System *self);

Eina_List *e_mixer_pulse_get_cards(void);
const char *e_mixer_pulse_get_default_card(void);
const char *e_mixer_pulse_get_card_name(const char *card);

Eina_List *e_mixer_pulse_get_channels(const E_Mixer_System *self);
Eina_List *e_mixer_pulse_get_channel_names(const E_Mixer_System *self);

const char *e_mixer_pulse_get_default_channel_name(const E_Mixer_System *self);
E_Mixer_Channel_Info *e_mixer_pulse_get_channel_by_name(const E_Mixer_System *self, const char *name);

int e_mixer_pulse_get_volume(const E_Mixer_System *self, const E_Mixer_Channel_Info *channel, int *left, int *right);
int e_mixer_pulse_set_volume(const E_Mixer_System *self, const E_Mixer_Channel_Info *channel, int left, int right);
int e_mixer_pulse_get_mute(const E_Mixer_System *self, const E_Mixer_Channel_Info *channel, int *mute);
int e_mixer_pulse_set_mute(const E_Mixer_System *self, const E_Mixer_Channel_Info *channel, int mute);
int e_mixer_pulse_get_state(const E_Mixer_System *self, const E_Mixer_Channel_Info *channel, E_Mixer_Channel_State *state);

/**
 * @addtogroup Optional_Devices
 * @{
 *
 * @defgroup Module_Mixer Audio Mixer (Volume Control)
 *
 * Controls the audio volume and mute status for both playback
 * (output) and record (input) devices.
 *
 * Can work with ALSA (http://www.alsa-project.org/) or PulseAudio
 * (http://www.pulseaudio.org/).
 *
 * @}
 */
#endif /* E_MOD_SYSTEM_H */

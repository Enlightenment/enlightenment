#include "e_mod_mixer.h"
#include <alsa/asoundlib.h>
#include <poll.h>

struct e_mixer_callback_desc
{
   int             (*func)(void *data,
                           E_Mixer_System *self);
   void           *data;
   E_Mixer_System *self;
   Ecore_Idler    *idler;
   Eina_List      *handlers;
};

static int _mixer_callback_add(const E_Mixer_System *self,
                               int (*func)(void *data, E_Mixer_System *self),
                               void *data);
static int _mixer_callback_del(const E_Mixer_System *self,
                               struct e_mixer_callback_desc *desc);

static Eina_Bool
_cb_dispatch(void *data)
{
   struct e_mixer_callback_desc *desc;
   int r;

   desc = data;
   snd_mixer_handle_events(desc->self);
   r = desc->func(desc->data, desc->self);
   desc->idler = NULL;

   if (!r)
     _mixer_callback_del(desc->self, desc);  /* desc is invalid then. */

   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_cb_fd_handler(void *data,
               Ecore_Fd_Handler *fd_handler)
{
   struct e_mixer_callback_desc *desc;

   desc = data;

   if (ecore_main_fd_handler_active_get(fd_handler, ECORE_FD_ERROR))
     {
        desc->handlers = eina_list_remove(desc->handlers, fd_handler);
        if (!desc->handlers)
          {
             E_Mixer_System *s;
             int (*f)(void *,
                      E_Mixer_System *);
             void *d;

             s = desc->self;
             f = desc->func;
             d = desc->data;
             _mixer_callback_del(s, desc);
             _mixer_callback_add(s, f, d);
          }
        return ECORE_CALLBACK_CANCEL;
     }

   if (!desc->idler)
     desc->idler = ecore_idler_add(_cb_dispatch, desc);
   return ECORE_CALLBACK_RENEW;
}

static int
_mixer_callback_add(const E_Mixer_System *self,
                    int (*func)(void *data, E_Mixer_System *self),
                    void *data)
{
   struct e_mixer_callback_desc *desc;
   struct pollfd *pfds;
   int len;

   len = snd_mixer_poll_descriptors_count((snd_mixer_t *)self);
   if (len <= 0)
     return 0;

   desc = malloc(sizeof(struct e_mixer_callback_desc));
   if (!desc)
     return 0;

   desc->func = func;
   desc->data = data;
   desc->self = (E_Mixer_System *)self;
   desc->idler = NULL;
   desc->handlers = NULL;

   pfds = alloca(len * sizeof(struct pollfd));
   len = snd_mixer_poll_descriptors((snd_mixer_t *)self, pfds, len);
   if (len <= 0)
     {
        free(desc);
        return 0;
     }

   while (len > 0)
     {
        Ecore_Fd_Handler *fd_handler;

        len--;
        fd_handler = ecore_main_fd_handler_add(
            pfds[len].fd, ECORE_FD_READ, _cb_fd_handler, desc, NULL, NULL);
        desc->handlers = eina_list_prepend(desc->handlers, fd_handler);
     }

   snd_mixer_set_callback_private((snd_mixer_t *)self, desc);

   return 1;
}

static int
_mixer_callback_del(const E_Mixer_System *self,
                    struct e_mixer_callback_desc *desc)
{
   Ecore_Fd_Handler *handler;

   EINA_LIST_FREE(desc->handlers, handler)
     ecore_main_fd_handler_del(handler);

   snd_mixer_set_callback_private((snd_mixer_t *)self, NULL);

   memset(desc, 0, sizeof(*desc));
   free(desc);

   return 1;
}

static int
_mixer_callback_replace(const E_Mixer_System *self EINA_UNUSED,
                        struct e_mixer_callback_desc *desc,
                        int (*func)(void *data, E_Mixer_System *self),
                        void *data)
{
   desc->func = func;
   desc->data = data;

   return 1;
}

E_Mixer_System *
e_mixer_system_new(const char *name)
{
   snd_mixer_t *handle;
   int err;

   if (!name)
     return NULL;

   err = snd_mixer_open(&handle, 0);
   if (err < 0)
     goto error_open;

   err = snd_mixer_attach(handle, name);
   if (err < 0)
     goto error_load;

   err = snd_mixer_selem_register(handle, NULL, NULL);
   if (err < 0)
     goto error_load;

   err = snd_mixer_load(handle);
   if (err < 0)
     goto error_load;

   return handle;

error_load:
   snd_mixer_close(handle);
error_open:
   fprintf(stderr, "MIXER: Cannot get hardware info: %s\n", snd_strerror(err));
   return NULL;
}

void
e_mixer_system_del(E_Mixer_System *self)
{
   struct e_mixer_callback_desc *desc;

   if (!self)
     return;

   desc = snd_mixer_get_callback_private(self);
   if (desc)
     _mixer_callback_del(self, desc);

   snd_mixer_close(self);
}

int
e_mixer_system_callback_set(const E_Mixer_System *self,
                            int (*func)(void *data, E_Mixer_System *self),
                            void *data)
{
   struct e_mixer_callback_desc *desc;

   if (!self)
     return 0;

   desc = snd_mixer_get_callback_private(self);
   if (!desc)
     {
        if (func)
          return _mixer_callback_add(self, func, data);
        return 1;
     }
   else
     {
        if (func)
          return _mixer_callback_replace(self, desc, func, data);
        else
          return _mixer_callback_del(self, desc);
     }
}

Eina_List *
e_mixer_system_get_cards(void)
{
   int err, card_num;
   Eina_List *cards;

   cards = NULL;
   card_num = -1;
   while (((err = snd_card_next(&card_num)) == 0) && (card_num >= 0))
     {
        snd_ctl_t *control;
        char buf[256];

        snprintf(buf, sizeof(buf), "hw:%d", card_num);

        if (snd_ctl_open(&control, buf, 0) < 0)
          break;
        snd_ctl_close(control);
        cards = eina_list_append(cards, eina_stringshare_add(buf));
     }

   if (err < 0)
     fprintf(stderr, "MIXER: Cannot get available card number: %s\n",
             snd_strerror(err));

   return cards;
}

const char *
e_mixer_system_get_default_card(void)
{
   static const char buf[] = "hw:0";
   snd_ctl_t *control;

   if (snd_ctl_open(&control, buf, 0) < 0)
     return NULL;
   snd_ctl_close(control);
   return eina_stringshare_add(buf);
}

const char *
e_mixer_system_get_card_name(const char *card)
{
   snd_ctl_card_info_t *hw_info;
   const char *name;
   snd_ctl_t *control;
   int err;

   if (!card)
     return NULL;

   snd_ctl_card_info_alloca(&hw_info);

   err = snd_ctl_open(&control, card, 0);
   if (err < 0)
     return NULL;

   err = snd_ctl_card_info(control, hw_info);
   if (err < 0)
     {
        fprintf(stderr, "MIXER: Cannot get hardware info: %s: %s\n", card,
                snd_strerror(err));
        snd_ctl_close(control);
        return NULL;
     }

   snd_ctl_close(control);
   name = snd_ctl_card_info_get_name(hw_info);
   if (!name)
     {
        fprintf(stderr, "MIXER: Cannot get hardware name: %s\n", card);
        return NULL;
     }

   return eina_stringshare_add(name);
}

static int
_mixer_channel_has_capabilities(snd_mixer_elem_t *elem)
{
   if (!snd_mixer_selem_is_active(elem)) return 0;
   if (snd_mixer_selem_has_playback_volume(elem)) return 1;
   if (snd_mixer_selem_has_capture_volume(elem)) return 1;
   if (snd_mixer_selem_has_playback_switch(elem)) return 1;
   if (snd_mixer_selem_has_capture_switch(elem)) return 1;

   return 0;
}

static int
_mixer_channel_capabilities(snd_mixer_elem_t *elem)
{
   int capabilities = 0;

   /*
    * never vol_joined if not mono
    *    -> mono is enough
    * never switch_joined if not switch
    *    -> switch is enough
    * never common_vol if not (playback_vol && capture vol)
    *    -> palyback_vol & capture_vol is enough
    */
   if (!snd_mixer_selem_is_active(elem))
     return 0;

   if (snd_mixer_selem_has_capture_volume(elem))
     capabilities |= E_MIXER_CHANNEL_HAS_CAPTURE;
   if (snd_mixer_selem_has_playback_volume(elem))
     capabilities |= E_MIXER_CHANNEL_HAS_PLAYBACK;
   if (snd_mixer_selem_has_playback_switch(elem) ||
       snd_mixer_selem_has_capture_switch(elem))
     capabilities |= E_MIXER_CHANNEL_CAN_MUTE;
   if (snd_mixer_selem_is_playback_mono(elem)==1 ||
       snd_mixer_selem_is_capture_mono(elem)==1)
     capabilities |= E_MIXER_CHANNEL_IS_MONO;

   return capabilities;
}

Eina_List *
e_mixer_system_get_channels(const E_Mixer_System *self)
{
   int capabilities;
   Eina_List *channels;
   snd_mixer_elem_t *elem;
   E_Mixer_Channel_Info *ch_info;

   if (!self)
     return NULL;

   channels = NULL;

   elem = snd_mixer_first_elem((snd_mixer_t *)self);
   for (; elem; elem = snd_mixer_elem_next(elem))
     {
        capabilities = _mixer_channel_capabilities(elem);
        if (!e_mod_mixer_capabilities_usable(capabilities))
          continue;

        ch_info = malloc(sizeof(*ch_info));
        ch_info->id = elem;
        ch_info->name = eina_stringshare_add(snd_mixer_selem_get_name(elem));
        ch_info->capabilities = capabilities;

        channels = eina_list_append(channels, ch_info);
     }

   return channels;
}

Eina_List *
e_mixer_system_get_channel_names(const E_Mixer_System *self)
{
   Eina_List *channels;
   snd_mixer_elem_t *elem;
   snd_mixer_selem_id_t *sid;

   if (!self)
     return NULL;

   channels = NULL;
   snd_mixer_selem_id_alloca(&sid);

   elem = snd_mixer_first_elem((snd_mixer_t *)self);
   for (; elem; elem = snd_mixer_elem_next(elem))
     {
        const char *name;
        if (!_mixer_channel_has_capabilities(elem))
          continue;

        snd_mixer_selem_get_id(elem, sid);
        name = snd_mixer_selem_id_get_name(sid);
        if (name)
          channels = eina_list_append(channels, eina_stringshare_add(name));
     }

   return channels;
}

const char *
e_mixer_system_get_default_channel_name(const E_Mixer_System *self)
{
   snd_mixer_elem_t *elem;
   snd_mixer_selem_id_t *sid;

   if (!self)
     return NULL;

   snd_mixer_selem_id_alloca(&sid);

   elem = snd_mixer_first_elem((snd_mixer_t *)self);
   for (; elem; elem = snd_mixer_elem_next(elem))
     {
        const char *name;
        if (!_mixer_channel_has_capabilities(elem))
          continue;

        snd_mixer_selem_get_id(elem, sid);
        name = snd_mixer_selem_id_get_name(sid);
        if (name)
          return eina_stringshare_add(name);
     }

   return NULL;
}

E_Mixer_Channel_Info *
e_mixer_system_get_channel_by_name(const E_Mixer_System *self,
                                   const char *name)
{
   int capabilities;
   snd_mixer_elem_t *elem;
   snd_mixer_selem_id_t *sid;
   E_Mixer_Channel_Info *ch_info;

   if ((!self) || (!name))
     return NULL;

   snd_mixer_selem_id_alloca(&sid);

   elem = snd_mixer_first_elem((snd_mixer_t *)self);
   for (; elem; elem = snd_mixer_elem_next(elem))
     {
        const char *n;
        capabilities = _mixer_channel_capabilities(elem);
        if (!e_mod_mixer_capabilities_usable(capabilities))
          continue;

        snd_mixer_selem_get_id(elem, sid);
        n = snd_mixer_selem_id_get_name(sid);
        if (n && (strcmp(n, name) == 0))
          {
             ch_info = malloc(sizeof(*ch_info));
             ch_info->id = elem;
             ch_info->name = eina_stringshare_add(n);
             ch_info->capabilities = capabilities;

             return ch_info;
          }
     }

   return NULL;
}

int
e_mixer_system_get_volume(const E_Mixer_System *self,
                          const E_Mixer_Channel_Info *channel,
                          int *left,
                          int *right)
{
   long lvol, rvol, range, min, max;

   if ((!self) || (!channel) || (!channel->id) || (!left) || (!right))
     return 0;

   snd_mixer_handle_events((snd_mixer_t *)self);
   if (e_mod_mixer_channel_has_playback(channel))
     snd_mixer_selem_get_playback_volume_range(channel->id, &min, &max);
   else if (e_mod_mixer_channel_has_capture(channel))
     snd_mixer_selem_get_capture_volume_range(channel->id, &min, &max);
   else
     return 0;

   range = max - min;
   if (range < 1)
     return 0;

   if (e_mod_mixer_channel_has_playback(channel))
     {
        snd_mixer_selem_get_playback_volume(channel->id, 0, &lvol);
        if (!e_mod_mixer_channel_is_mono(channel))
          snd_mixer_selem_get_playback_volume(channel->id, 1, &rvol);
        else
          rvol = lvol;
     }
    else
     {
        snd_mixer_selem_get_capture_volume(channel->id, 0, &lvol);
        if (!e_mod_mixer_channel_is_mono(channel))
          snd_mixer_selem_get_capture_volume(channel->id, 1, &rvol);
        else
          rvol = lvol;
     }

   *left = rint((double)(lvol - min) * 100 / (double)range);
   *right = rint((double)(rvol - min) * 100 / (double)range);

   return 1;
}

int
e_mixer_system_set_volume(const E_Mixer_System *self,
                          const E_Mixer_Channel_Info *channel,
                          int left,
                          int right)
{
   long range, min, max, divide;
   int mode;

   if ((!self) || (!channel) || (!channel->id))
     return 0;

   snd_mixer_handle_events((snd_mixer_t *)self);
   snd_mixer_selem_get_playback_volume_range(channel->id, &min, &max);
   divide = 100 + min;
   if (divide == 0)
     {
        divide = 1; /* no zero-division */
        min++;
     }

   range = max - min;
   if (range < 1)
     return 0;

   mode = 0;
   if (left >= 0)
     {
        left = (((range * left) + (range / 2)) / divide) - min;
        mode |= 1;
     }

   if (!e_mod_mixer_channel_is_mono(channel) && (right >= 0))
     {
        right = (((range * right) + (range / 2)) / divide) - min;
        mode |= 2;
     }

   if (mode & 1)
     {
        if (e_mod_mixer_channel_has_playback(channel))
          snd_mixer_selem_set_playback_volume(channel->id, 0, left);
        else
          snd_mixer_selem_set_capture_volume(channel->id, 0, left);
     }
   if (mode & 2)
     {
        if (e_mod_mixer_channel_has_playback(channel))
          snd_mixer_selem_set_playback_volume(channel->id, 1, right);
        else
          snd_mixer_selem_set_capture_volume(channel->id, 1, right);
     }

   return 1;
}

int
e_mixer_system_get_mute(const E_Mixer_System *self,
                        const E_Mixer_Channel_Info *channel,
                        int *mute)
{
   if ((!self) || (!channel) || (!channel->id) || (!mute))
     return 0;

   snd_mixer_handle_events((snd_mixer_t *)self);

   if (e_mod_mixer_channel_is_mutable(channel))
     {
        int m;

        /* XXX: not checking for return, always returns 0 even if it worked.
         * alsamixer also don't check it. Bug?
         */
        if (e_mod_mixer_channel_has_capture(channel))
          snd_mixer_selem_get_capture_switch(channel->id, 0, &m);
        else
          snd_mixer_selem_get_playback_switch(channel->id, 0, &m);
        *mute = !m;
     }
   else
     *mute = 0;

   return 1;
}

int
e_mixer_system_set_mute(const E_Mixer_System *self,
                        const E_Mixer_Channel_Info *channel,
                        int mute)
{
   if ((!self) || (!channel) || (!channel->id))
     return 0;

   if (!e_mod_mixer_channel_is_mutable(channel))
     return 0;

   if (e_mod_mixer_channel_has_capture(channel))
      return snd_mixer_selem_set_capture_switch_all(channel->id, !mute);
   else
     return snd_mixer_selem_set_playback_switch_all(channel->id, !mute);
}

int
e_mixer_system_get_state(const E_Mixer_System *self,
                         const E_Mixer_Channel_Info *channel,
                         E_Mixer_Channel_State *state)
{
   int r;

   if (!state)
     return 0;

   r = e_mixer_system_get_mute(self, channel, &state->mute);
   r &= e_mixer_system_get_volume(self, channel, &state->left, &state->right);
   return r;
}

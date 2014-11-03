#include "e_mod_main.h"

extern const char _e_mixer_Name[];

typedef struct E_Mixer_App_Dialog_Data
{
   E_Mixer_System       *sys;
   const char           *sys_card_name;
   const char           *channel_name;
   int                   lock_sliders;
   Eina_List            *cards;
   Eina_List            *channel_infos;
   E_Mixer_Channel_Info *channel_info;
   E_Mixer_Channel_State state;

   struct e_mixer_app_ui
   {
      Evas_Object *hlayout;
      struct e_mixer_app_ui_cards
      {
         Evas_Object *frame;
         Evas_Object *list;
      } cards;
      struct e_mixer_app_ui_channels
      {
         Evas_Object *frame;
         Evas_Object *list;
      } channels;
      struct e_mixer_app_ui_channel_editor
      {
         Evas_Object *frame;
         Evas_Object *label_card;
         Evas_Object *card;
         Evas_Object *label_channel;
         Evas_Object *channel;
         Evas_Object *label_type;
         Evas_Object *type;
         Evas_Object *label_left;
         Evas_Object *left;
         Evas_Object *label_right;
         Evas_Object *right;
         Evas_Object *mute;
         Evas_Object *lock_sliders;
      } channel_editor;
   } ui;

   struct
   {
      void *data;
      void (*func)(E_Dialog *dialog, void *data);
   } del;
} E_Mixer_App_Dialog_Data;

static void
_cb_changed_left(void *data, Evas_Object *obj __UNUSED__)
{
   E_Mixer_App_Dialog_Data *app = data;
   E_Mixer_Channel_State *state;

   state = &app->state;
   if (app->lock_sliders && (state->left != state->right))
     {
        state->right = state->left;
        e_widget_slider_value_int_set(app->ui.channel_editor.right,
                                      state->right);
     }

   e_mod_mixer_volume_set(app->sys, app->channel_info,
                          state->left, state->right);
}

static void
_cb_changed_right(void *data, Evas_Object *obj __UNUSED__)
{
   E_Mixer_App_Dialog_Data *app = data;
   E_Mixer_Channel_State *state;

   state = &app->state;
   if (app->lock_sliders && (state->right != state->left))
     {
        state->left = state->right;
        e_widget_slider_value_int_set(app->ui.channel_editor.left,
                                      state->left);
     }

   e_mod_mixer_volume_set(app->sys, app->channel_info,
                          state->left, state->right);
}

static void
_cb_changed_mute(void *data, Evas_Object *obj __UNUSED__)
{
   E_Mixer_App_Dialog_Data *app = data;

   e_mod_mixer_mute_set(app->sys, app->channel_info, app->state.mute);
}

static void
_cb_changed_lock_sliders(void *data, Evas_Object *obj __UNUSED__)
{
   E_Mixer_App_Dialog_Data *app = data;
   E_Mixer_Channel_State *state;

   if (!app->lock_sliders)
     return;

   state = &app->state;
   if (state->left == state->right)
     return;

   state->left = state->right = (state->left + state->right) / 2;

   e_widget_slider_value_int_set(app->ui.channel_editor.left, state->left);
   e_widget_slider_value_int_set(app->ui.channel_editor.right, state->right);
   e_mod_mixer_volume_set(app->sys, app->channel_info,
                          state->left, state->right);
}

static void
_disable_channel_editor(E_Mixer_App_Dialog_Data *app)
{
   struct e_mixer_app_ui_channel_editor *ui = &app->ui.channel_editor;

   e_widget_entry_text_set(ui->card, "");
   e_widget_entry_text_set(ui->channel, "");
   e_widget_entry_text_set(ui->type, "");

   e_widget_slider_value_int_set(ui->left, 0);
   e_widget_slider_value_int_set(ui->right, 0);
   e_widget_check_checked_set(ui->mute, 0);
   e_widget_check_checked_set(ui->lock_sliders, 0);

   e_widget_disabled_set(ui->left, 1);
   e_widget_disabled_set(ui->right, 1);
   e_widget_disabled_set(ui->mute, 1);
   e_widget_disabled_set(ui->lock_sliders, 1);
}

static void
_update_channel_editor_state(E_Mixer_App_Dialog_Data *app, const E_Mixer_Channel_State state)
{
   struct e_mixer_app_ui_channel_editor *ui = &app->ui.channel_editor;

   if (!e_mod_mixer_channel_has_no_volume(app->channel_info))
     {
        if (e_mod_mixer_channel_is_mono(app->channel_info))
          e_widget_slider_value_int_set(ui->left, state.left);
        else
          {
             e_widget_slider_value_int_set(ui->left, state.left);
             e_widget_slider_value_int_set(ui->right, state.right);
          }
     }

   if (e_mod_mixer_channel_is_mutable(app->channel_info))
     e_widget_check_checked_set(ui->mute, state.mute);
}

static void
_populate_channel_editor(E_Mixer_App_Dialog_Data *app)
{
   struct e_mixer_app_ui_channel_editor *ui = &app->ui.channel_editor;
   E_Mixer_Channel_State state;

   if ((!app->sys_card_name) || (!app->channel_name))
     {
        _disable_channel_editor(app);
        return;
     }

   e_widget_entry_text_set(ui->card, app->sys_card_name);
   e_widget_entry_text_set(ui->channel, app->channel_name);

   if (e_mod_mixer_channel_is_boost(app->channel_info))
     e_widget_entry_text_set(ui->type, _("Boost"));
   else if (e_mod_mixer_channel_has_playback(app->channel_info))
     e_widget_entry_text_set(ui->type, _("Playback"));
   else if (e_mod_mixer_channel_has_capture(app->channel_info))
     e_widget_entry_text_set(ui->type, _("Capture"));
   else
      e_widget_entry_text_set(ui->type, _("Switch"));

   e_mod_mixer_state_get(app->sys, app->channel_info, &state);
   _update_channel_editor_state(app, state);

   if (e_mod_mixer_channel_is_mutable(app->channel_info))
     {
        e_widget_disabled_set(ui->mute, 0);
     }
   else
     {
        e_widget_disabled_set(ui->mute, 1);
        e_widget_check_checked_set(ui->mute, 0);
     }

   if (e_mod_mixer_channel_has_no_volume(app->channel_info))
     {
        app->lock_sliders = 1;
        e_widget_slider_value_int_set(ui->left, 0);
        e_widget_slider_value_int_set(ui->right, 0);
        e_widget_disabled_set(ui->lock_sliders, 1);
        e_widget_disabled_set(ui->left, 1);
        e_widget_disabled_set(ui->right, 1);
     }
   else if (e_mod_mixer_channel_is_mono(app->channel_info))
     {
        app->lock_sliders = 0;
        e_widget_slider_value_int_set(ui->right, 0);
        e_widget_disabled_set(ui->lock_sliders, 1);
        e_widget_disabled_set(ui->left, 0);
        e_widget_disabled_set(ui->right, 1);
     }
   else
     {
        app->lock_sliders = (state.left == state.right);
        e_widget_disabled_set(ui->lock_sliders, 0);
        e_widget_disabled_set(ui->left, 0);
        e_widget_disabled_set(ui->right, 0);
     }
   e_widget_check_checked_set(ui->lock_sliders, app->lock_sliders);
}

static void
_cb_channel_selected(void *data)
{
   E_Mixer_Channel_Info *info = data;
   E_Mixer_App_Dialog_Data *app;

   app = info->app;
   app->channel_info = info;
   _populate_channel_editor(app);
}

static int
_cb_system_update(void *data, E_Mixer_System *sys __UNUSED__)
{
   E_Mixer_App_Dialog_Data *app = data;
   E_Mixer_Channel_State state;

   if ((!app->sys) || (!app->channel_info))
     return 1;

   e_mod_mixer_state_get(app->sys, app->channel_info, &state);
   _update_channel_editor_state(app, state);

   return 1;
}

static void
_populate_channels(E_Mixer_App_Dialog_Data *app)
{
   Eina_List *l;
   Evas_Object *ilist;
   int header_input;
   int i, selected;

   ilist = app->ui.channels.list;
   edje_freeze();
   e_widget_ilist_freeze(ilist);
   e_widget_ilist_clear(ilist);

   if (app->sys)
     e_mod_mixer_del(app->sys);
   app->sys = e_mod_mixer_new(app->sys_card_name);
   if (_mixer_using_default)
     e_mixer_system_callback_set(app->sys, _cb_system_update, app);

   if (app->channel_infos)
     e_mod_mixer_channel_infos_free(app->channel_infos);
   app->channel_infos = e_mod_mixer_channel_infos_get(app->sys);

   i = 0;
   selected = 0;
   header_input = 0;
   for (l = app->channel_infos; l; l = l->next, i++)
     {
        E_Mixer_Channel_Info *info = l->data;

        if (header_input != e_mod_mixer_channel_group_get(info))
          {
             if (e_mod_mixer_channel_is_boost(info))
               e_widget_ilist_header_append(ilist, NULL, _("Boost"));
             else if (e_mod_mixer_channel_has_playback(info))
               e_widget_ilist_header_append(ilist, NULL, _("Playback"));
             else if (e_mod_mixer_channel_has_capture(info))
               e_widget_ilist_header_append(ilist, NULL, _("Capture"));
             else
               e_widget_ilist_header_append(ilist, NULL, _("Switch"));
             header_input = e_mod_mixer_channel_group_get(info);
             i++;
          }

        info->app = app;
        e_widget_ilist_append(ilist, NULL, info->name, _cb_channel_selected,
                              info, info->name);
        if ((selected == 0) && app->channel_name && info->name &&
            (strcmp(app->channel_name, info->name) == 0))
          {
             e_widget_ilist_selected_set(ilist, i);
             app->channel_info = info;
             selected = 1;
          }
     }

   if ((selected == 0) && (i > 0))
     e_widget_ilist_selected_set(ilist, 0);
   else
     app->channel_name = NULL;

   e_widget_ilist_go(ilist);
   e_widget_ilist_thaw(ilist);
   edje_thaw();
}

static void
_cb_card_selected(void *data)
{
   _populate_channels(data);
}

static void
_create_cards(E_Dialog *dialog __UNUSED__, Evas *evas, E_Mixer_App_Dialog_Data *app)
{
   struct e_mixer_app_ui_cards *ui = &app->ui.cards;
   const char *card;
   Eina_List *l;

   app->cards = e_mod_mixer_card_names_get();
   if (eina_list_count(app->cards) < 2)
     return;

   ui->list = e_widget_ilist_add(evas, 32, 32, &app->sys_card_name);
   e_widget_size_min_set(ui->list, 180, 100);
   e_widget_ilist_go(ui->list);
   EINA_LIST_FOREACH(app->cards, l, card)
     {
        const char *card_name;

        card_name = e_mod_mixer_card_name_get(card);

        if (!card_name)
          continue;

        e_widget_ilist_append(ui->list, NULL, card_name, _cb_card_selected,
                              app, card);

        eina_stringshare_del(card_name);
     }

   ui->frame = e_widget_framelist_add(evas, _("Cards"), 0);
   e_widget_framelist_object_append(ui->frame, ui->list);
   e_widget_list_object_append(app->ui.hlayout, ui->frame, 1, 0, 0.0);
}

static void
_create_channels(E_Dialog *dialog __UNUSED__, Evas *evas, E_Mixer_App_Dialog_Data *app)
{
   struct e_mixer_app_ui_channels *ui = &app->ui.channels;
   ui->list = e_widget_ilist_add(evas, 24, 24, &app->channel_name);
   e_widget_size_min_set(ui->list, 180, 100);
   e_widget_ilist_go(ui->list);

   ui->frame = e_widget_framelist_add(evas, _("Channels"), 0);
   e_widget_framelist_object_append(ui->frame, ui->list);
   e_widget_list_object_append(app->ui.hlayout, ui->frame, 1, 1, 0.0);
}

static void
_create_channel_editor(E_Dialog *dialog __UNUSED__, Evas *evas, E_Mixer_App_Dialog_Data *app)
{
   struct e_mixer_app_ui_channel_editor *ui = &app->ui.channel_editor;

   ui->label_card = e_widget_label_add(evas, _("Card:"));
   ui->card = e_widget_entry_add(evas, NULL, NULL, NULL, NULL);
   e_widget_entry_readonly_set(ui->card, 1);

   ui->label_channel = e_widget_label_add(evas, _("Channel:"));
   ui->channel = e_widget_entry_add(evas, NULL, NULL, NULL, NULL);
   e_widget_entry_readonly_set(ui->channel, 1);

   ui->label_type = e_widget_label_add(evas, _("Type:"));
   ui->type = e_widget_entry_add(evas, NULL, NULL, NULL, NULL);
   e_widget_entry_readonly_set(ui->type, 1);

   ui->label_left = e_widget_label_add(evas, _("Left:"));
   ui->left = e_widget_slider_add(evas, 1, 0, "%3.0f", 0.0, 100.0, 10.0, 100.0,
                                  NULL, &app->state.left, 150);
   e_widget_on_change_hook_set(ui->left, _cb_changed_left, app);

   ui->label_right = e_widget_label_add(evas, _("Right:"));
   ui->right = e_widget_slider_add(evas, 1, 0, "%3.0f", 0.0, 100.0, 10.0, 100.0,
                                   NULL, &app->state.right, 150);
   e_widget_on_change_hook_set(ui->right, _cb_changed_right, app);

   ui->mute = e_widget_check_add(evas, _("Mute"), &app->state.mute);
   e_widget_on_change_hook_set(ui->mute, _cb_changed_mute, app);

   ui->lock_sliders = e_widget_check_add(evas, _("Lock Sliders"),
                                         &app->lock_sliders);
   e_widget_on_change_hook_set(ui->lock_sliders, _cb_changed_lock_sliders, app);

   ui->frame = e_widget_framelist_add(evas, _("Edit"), 0);
   e_widget_framelist_object_append(ui->frame, ui->label_card);
   e_widget_framelist_object_append(ui->frame, ui->card);
   e_widget_framelist_object_append(ui->frame, ui->label_channel);
   e_widget_framelist_object_append(ui->frame, ui->channel);
   e_widget_framelist_object_append(ui->frame, ui->label_type);
   e_widget_framelist_object_append(ui->frame, ui->type);
   e_widget_framelist_object_append(ui->frame, ui->label_left);
   e_widget_framelist_object_append(ui->frame, ui->left);
   e_widget_framelist_object_append(ui->frame, ui->label_right);
   e_widget_framelist_object_append(ui->frame, ui->right);
   e_widget_framelist_object_append(ui->frame, ui->mute);
   e_widget_framelist_object_append(ui->frame, ui->lock_sliders);

   e_widget_list_object_append(app->ui.hlayout, ui->frame, 1, 1, 0.5);
}

static void
_create_ui(E_Dialog *dialog, E_Mixer_App_Dialog_Data *app)
{
   struct e_mixer_app_ui *ui = &app->ui;
   Evas *evas;
   int mw, mh;

   evas = evas_object_evas_get(dialog->win);

   ui->hlayout = e_widget_list_add(evas, 0, 1);
   _create_cards(dialog, evas, app);
   _create_channels(dialog, evas, app);
   _create_channel_editor(dialog, evas, app);

   e_widget_size_min_get(ui->hlayout, &mw, &mh);
   if (mw < 300)
     mw = 300;
   if (mh < 200)
     mh = 200;
   e_dialog_content_set(dialog, ui->hlayout, mw, mh);
}

static void
_mixer_app_dialog_del(E_Dialog *dialog, E_Mixer_App_Dialog_Data *app)
{
   if (!app)
     return;

   if (app->del.func)
     app->del.func(dialog, app->del.data);

   if ((!app->ui.cards.list) && (app->ui.channels.list))
     eina_stringshare_del(app->sys_card_name);

   if (app->cards)
     e_mod_mixer_card_names_free(app->cards);
   if (app->channel_infos)
     e_mod_mixer_channel_infos_free(app->channel_infos);
   e_mod_mixer_del(app->sys);

   e_util_defer_object_del(E_OBJECT(dialog));
   dialog->data = NULL;
   E_FREE(app);
}

static void
_cb_win_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Dialog *dialog = data;
   E_Mixer_App_Dialog_Data *app = dialog->data;
   _mixer_app_dialog_del(dialog, app);
}

static void
_cb_dialog_dismiss(void *data, E_Dialog *dialog)
{
   _mixer_app_dialog_del(dialog, data);
}

E_Dialog *
e_mixer_app_dialog_new(Evas_Object *parent EINA_UNUSED, void (*func)(E_Dialog *dialog, void *data), void *data)
{
   E_Mixer_App_Dialog_Data *app;
   E_Dialog *dialog;

   dialog = e_dialog_new(NULL, _e_mixer_Name, "e_mixer_app_dialog");
   if (!dialog)
     return NULL;

   app = E_NEW(E_Mixer_App_Dialog_Data, 1);
   if (!app)
     {
        e_object_del(E_OBJECT(dialog));
        return NULL;
     }

   dialog->data = app;
   app->del.data = data;
   app->del.func = func;

   e_dialog_title_set(dialog, _(_e_mixer_Name));

   evas_object_event_callback_add(dialog->win, EVAS_CALLBACK_DEL, _cb_win_del, dialog);

   _create_ui(dialog, app);

   e_dialog_button_add(dialog, _("Close"), NULL, _cb_dialog_dismiss, app);
   e_dialog_button_focus_num(dialog, 1);
   elm_win_center(dialog->win, 1, 1);
   e_dialog_show(dialog);
   e_dialog_border_icon_set(dialog, "preferences-desktop-mixer");

   // FIXME: what if module unloaded while mixer_app dialog up? bad!

   return dialog;
}

int
e_mixer_app_dialog_select(E_Dialog *dialog, const char *sys_card_name, const char *channel_name)
{
   int n, i;
   Eina_List *l;
   E_Mixer_App_Dialog_Data *app;

   if (!dialog)
     return 0;

   if ((!sys_card_name) || (!channel_name))
     return 0;

   app = dialog->data;
   if (!app)
     return 0;

   n = -1;
   for (i = 0, l = app->cards; l; i++, l = l->next)
     {
        if (strcmp(sys_card_name, l->data) == 0)
          {
             n = i;
             break;
          }
     }

   if (n < 0)
     {
        /* FIXME device disappeared, very bad !! */
        return 0;
     }

   /* app->channel_name will be overriden by selection in _populate_channels */
   app->channel_name = channel_name;

   if (app->ui.cards.list)
     e_widget_ilist_selected_set(app->ui.cards.list, n);
   else if (app->ui.channels.list)
     {
        app->sys_card_name = eina_stringshare_add(sys_card_name);
        _populate_channels(app);
     }

   return 1;
}


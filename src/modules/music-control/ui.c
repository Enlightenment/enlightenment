#include "private.h"

static void
_play_state_update(E_Music_Control_Instance *inst)
{
   if (!inst->popup) return;
   if (inst->ctxt->playning)
     edje_object_signal_emit(inst->content_popup, "btn,state,image,pause", "play");
   else
     edje_object_signal_emit(inst->content_popup, "btn,state,image,play", "play");
}

void
music_control_state_update_all(E_Music_Control_Module_Context *ctxt)
{
   E_Music_Control_Instance *inst;
   Eina_List *list;

   EINA_LIST_FOREACH(ctxt->instances, list, inst)
     _play_state_update(inst);
}

static void
_btn_clicked(void *data, Evas_Object *obj, const char *emission, const char *source)
{
   E_Music_Control_Instance *inst = data;

   if (!strcmp(source, "play"))
     {
        inst->ctxt->playning = !inst->ctxt->playning;
        music_control_state_update_all(inst->ctxt);
     }
}

static void
_popup_new(E_Music_Control_Instance *inst)
{
   Evas_Object *o;
   inst->popup = e_gadcon_popup_new(inst->gcc);

   o = edje_object_add(inst->popup->win->evas);
   edje_object_file_set(o, music_control_edj_path_get(),
                        "modules/music-control/popup");
   edje_object_signal_callback_add(o, "btn,clicked", "*", _btn_clicked, inst);

   e_gadcon_popup_content_set(inst->popup, o);
   e_gadcon_popup_show(inst->popup);
   inst->content_popup = o;
   _play_state_update(inst);
}

void
music_control_popup_del(E_Music_Control_Instance *inst)
{
   e_gadcon_popup_hide(inst->popup);
   e_object_del(E_OBJECT(inst->popup));
   inst->popup = NULL;
}

void
music_control_mouse_down_cb(void *data, Evas *evas, Evas_Object *obj, void *event)
{
   E_Music_Control_Instance *inst = data;
   Evas_Event_Mouse_Down *ev = event;

   if (ev->button == 1)
     {
        if (!inst->popup)
          _popup_new(inst);
        else
          music_control_popup_del(inst);
     }
}

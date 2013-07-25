#include "e.h"
//#include "e_comp_wl.h"
#include "e_surface.h"

/* local structures */
typedef struct _E_Smart_Data E_Smart_Data;
struct _E_Smart_Data
{
   /* canvas */
   Evas *evas;

   /* object geometry */
   Evas_Coord x, y, w, h;

   /* input geometry */
   struct 
     {
        Evas_Coord x, y, w, h;
     } input;

   /* main image object where we draw pixels to */
   Evas_Object *o_img;

   /* input rectangle */
   Evas_Object *o_input;

   double mouse_down_time;
};

/* smart function prototypes */
static void _e_smart_add(Evas_Object *obj);
static void _e_smart_del(Evas_Object *obj);
static void _e_smart_move(Evas_Object *obj, Evas_Coord x, Evas_Coord y);
static void _e_smart_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h);
static void _e_smart_show(Evas_Object *obj);
static void _e_smart_hide(Evas_Object *obj);
static void _e_smart_clip_set(Evas_Object *obj, Evas_Object *clip);
static void _e_smart_clip_unset(Evas_Object *obj);

/* local function prototypes */
static void _e_surface_cb_focus_in(void *data, Evas *evas EINA_UNUSED, void *event EINA_UNUSED);
static void _e_surface_cb_focus_out(void *data, Evas *evas EINA_UNUSED, void *event EINA_UNUSED);
static void _e_surface_cb_mouse_in(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event);
static void _e_surface_cb_mouse_out(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event);
static void _e_surface_cb_mouse_move(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event);
static void _e_surface_cb_mouse_down(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event);
static void _e_surface_cb_mouse_up(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj, void *event);
static void _e_surface_cb_mouse_wheel(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj, void *event);
static void _e_surface_cb_key_down(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event);
static void _e_surface_cb_key_up(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event);

EAPI Evas_Object *
e_surface_add(Evas *evas)
{
   /* Evas_Object *obj = NULL; */
   /* E_Smart_Data *sd = NULL; */

   static Evas_Smart *smart = NULL;
   static const Evas_Smart_Class sc = 
     {
        "smart_surface", EVAS_SMART_CLASS_VERSION, 
        _e_smart_add, _e_smart_del, _e_smart_move, _e_smart_resize, 
        _e_smart_show, _e_smart_hide, NULL, 
        _e_smart_clip_set, _e_smart_clip_unset, 
        NULL, NULL, NULL, NULL, NULL, NULL, NULL
     };

   /* create the smart class */
   if (!smart)
     if (!(smart = evas_smart_class_new(&sc)))
       return NULL;

   /* create new smart object */
   /* obj = evas_object_smart_add(evas, smart); */

   /* get the smart data and set reference to the surface */
   /* if ((sd = evas_object_smart_data_get(obj))) */
   /*   sd->ews = ews; */

   /* return newly created smart object */
   return evas_object_smart_add(evas, smart);
}

EAPI void 
e_surface_input_set(Evas_Object *obj, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
   E_Smart_Data *sd = NULL;

   /* try to get the objects smart data */
   if (!(sd = evas_object_smart_data_get(obj))) return;

   sd->input.x = x;
   sd->input.y = y;
   sd->input.w = w;
   sd->input.h = h;

   if ((w >= 0) && (h >= 0))
     {
        if (sd->o_img) evas_object_pass_events_set(sd->o_img, EINA_TRUE);
     }
   else
     if (sd->o_img) evas_object_pass_events_set(sd->o_img, EINA_FALSE);

   /* update input rectangle geometry */
   if (sd->o_input)
     {
        evas_object_move(sd->o_input, x, y);
        evas_object_resize(sd->o_input, w, h);
     }
}

EAPI void 
e_surface_damage_add(Evas_Object *obj, Evas_Coord x, Evas_Coord y, Evas_Coord w, Evas_Coord h)
{
   E_Smart_Data *sd = NULL;

   /* try to get the objects smart data */
   if (!(sd = evas_object_smart_data_get(obj))) return;

   /* update the image damaged area */
   if (sd->o_img)
     evas_object_image_data_update_add(sd->o_img, x, y, w, h);
}

EAPI void 
e_surface_image_set(Evas_Object *obj, Evas_Coord w, Evas_Coord h, void *pixels)
{
   E_Smart_Data *sd = NULL;

   /* try to get the objects smart data */
   if (!(sd = evas_object_smart_data_get(obj))) return;

   /* update the image damaged area */
   if (sd->o_img)
     {
        evas_object_image_size_set(sd->o_img, w, h);
        evas_object_image_data_copy_set(sd->o_img, pixels);
     }
}

EAPI void 
e_surface_border_input_set(Evas_Object *obj, E_Client *ec)
{
   E_Smart_Data *sd = NULL;

   /* try to get the objects smart data */
   if (!(sd = evas_object_smart_data_get(obj))) return;

   //e_client_input_object_set(bd, sd->o_input);
}

/* smart functions */
static void 
_e_smart_add(Evas_Object *obj)
{
   E_Smart_Data *sd = NULL;

   /* try to allocate space for the smart data structure */
   if (!(sd = E_NEW(E_Smart_Data, 1))) return;

   /* get a reference to the canvas */
   sd->evas = evas_object_evas_get(obj);
   evas_event_callback_add(sd->evas, EVAS_CALLBACK_CANVAS_FOCUS_IN, 
                           _e_surface_cb_focus_in, obj);
   evas_event_callback_add(sd->evas, EVAS_CALLBACK_CANVAS_FOCUS_OUT, 
                           _e_surface_cb_focus_out, obj);

   /* create the base input rectangle */
   sd->o_input = evas_object_rectangle_add(sd->evas);
   evas_object_color_set(sd->o_input, 0, 0, 0, 0);
   evas_object_propagate_events_set(sd->o_input, EINA_FALSE);
   evas_object_repeat_events_set(sd->o_input, EINA_FALSE);

   /* we have to set focus to the input object first, or else Evas will 
    * never report any key events (up/down) to us */
   evas_object_focus_set(sd->o_input, EINA_TRUE);

   /* add the event callbacks we need to listen for */
   evas_object_event_callback_add(sd->o_input, EVAS_CALLBACK_MOUSE_IN, 
                                  _e_surface_cb_mouse_in, obj);
   evas_object_event_callback_add(sd->o_input, EVAS_CALLBACK_MOUSE_OUT, 
                                  _e_surface_cb_mouse_out, obj);
   evas_object_event_callback_add(sd->o_input, EVAS_CALLBACK_MOUSE_MOVE, 
                                  _e_surface_cb_mouse_move, obj);
   evas_object_event_callback_add(sd->o_input, EVAS_CALLBACK_MOUSE_DOWN, 
                                  _e_surface_cb_mouse_down, obj);
   evas_object_event_callback_add(sd->o_input, EVAS_CALLBACK_MOUSE_UP, 
                                  _e_surface_cb_mouse_up, obj);
   evas_object_event_callback_add(sd->o_input, EVAS_CALLBACK_MOUSE_WHEEL, 
                                  _e_surface_cb_mouse_wheel, obj);
   evas_object_event_callback_add(sd->o_input, EVAS_CALLBACK_KEY_DOWN, 
                                  _e_surface_cb_key_down, obj);
   evas_object_event_callback_add(sd->o_input, EVAS_CALLBACK_KEY_UP, 
                                  _e_surface_cb_key_up, obj);
   evas_object_smart_member_add(sd->o_input, obj);

   /* create the image object */
   sd->o_img = evas_object_image_filled_add(sd->evas);
   evas_object_image_content_hint_set(sd->o_img, EVAS_IMAGE_CONTENT_HINT_DYNAMIC);
   evas_object_image_scale_hint_set(sd->o_img, EVAS_IMAGE_SCALE_HINT_DYNAMIC);
   evas_object_image_smooth_scale_set(sd->o_img, EINA_FALSE);
   evas_object_image_alpha_set(sd->o_img, EINA_TRUE);
   evas_object_pass_events_set(sd->o_img, EINA_FALSE);
   evas_object_propagate_events_set(sd->o_img, EINA_FALSE);
   evas_object_smart_member_add(sd->o_img, obj);

   /* set the objects smart data */
   evas_object_smart_data_set(obj, sd);
}

static void 
_e_smart_del(Evas_Object *obj)
{
   E_Smart_Data *sd = NULL;

   /* try to get the objects smart data */
   if (!(sd = evas_object_smart_data_get(obj))) return;

   /* delete the image object */
   if (sd->o_img) evas_object_del(sd->o_img);

   /* delete the input rectangle */
   if (sd->o_input) 
     {
        /* delete the callbacks */
        evas_object_event_callback_del(sd->o_input, EVAS_CALLBACK_MOUSE_IN, 
                                       _e_surface_cb_mouse_in);
        evas_object_event_callback_del(sd->o_input, EVAS_CALLBACK_MOUSE_OUT, 
                                       _e_surface_cb_mouse_out);
        evas_object_event_callback_del(sd->o_input, EVAS_CALLBACK_MOUSE_MOVE, 
                                       _e_surface_cb_mouse_move);
        evas_object_event_callback_del(sd->o_input, EVAS_CALLBACK_MOUSE_DOWN, 
                                       _e_surface_cb_mouse_down);
        evas_object_event_callback_del(sd->o_input, EVAS_CALLBACK_MOUSE_UP, 
                                       _e_surface_cb_mouse_up);
        evas_object_event_callback_del(sd->o_input, EVAS_CALLBACK_MOUSE_WHEEL, 
                                       _e_surface_cb_mouse_wheel);
        evas_object_event_callback_del(sd->o_input, EVAS_CALLBACK_KEY_DOWN, 
                                       _e_surface_cb_key_down);
        evas_object_event_callback_del(sd->o_input, EVAS_CALLBACK_KEY_UP, 
                                       _e_surface_cb_key_up);

        evas_object_del(sd->o_input);
     }

   /* delete the event callbacks */
   evas_event_callback_del(sd->evas, EVAS_CALLBACK_CANVAS_FOCUS_IN, 
                           _e_surface_cb_focus_in);
   evas_event_callback_del(sd->evas, EVAS_CALLBACK_CANVAS_FOCUS_OUT, 
                           _e_surface_cb_focus_out);

   /* free the allocated smart data structure */
   E_FREE(sd);

   /* set the objects smart data */
   evas_object_smart_data_set(obj, NULL);
}

static void 
_e_smart_move(Evas_Object *obj, Evas_Coord x, Evas_Coord y)
{
   E_Smart_Data *sd = NULL;

   /* try to get the objects smart data */
   if (!(sd = evas_object_smart_data_get(obj))) return;

   if ((sd->x == x) && (sd->y == y)) return;

   sd->x = x;
   sd->y = y;

   /* move the input rectangle */
   if (sd->o_input)
     evas_object_move(sd->o_input, sd->input.x, sd->input.y);

   /* move the image object */
   if (sd->o_img) evas_object_move(sd->o_img, x, y);
}

static void 
_e_smart_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
   E_Smart_Data *sd = NULL;

   /* try to get the objects smart data */
   if (!(sd = evas_object_smart_data_get(obj))) return;

   if ((sd->w == w) && (sd->h == h)) return;

   sd->w = w;
   sd->h = h;

   /* resize the input rectangle */
   if (sd->o_input)
     evas_object_resize(sd->o_input, sd->input.w, sd->input.h);

   /* resize the image object */
   if (sd->o_img) evas_object_resize(sd->o_img, w, h);
}

static void 
_e_smart_show(Evas_Object *obj)
{
   E_Smart_Data *sd = NULL;

   /* try to get the objects smart data */
   if (!(sd = evas_object_smart_data_get(obj))) return;

   /* show the input rectangle */
   if (sd->o_input) evas_object_show(sd->o_input);

   /* show the image object */
   if (sd->o_img) evas_object_show(sd->o_img);
}

static void 
_e_smart_hide(Evas_Object *obj)
{
   E_Smart_Data *sd = NULL;

   /* try to get the objects smart data */
   if (!(sd = evas_object_smart_data_get(obj))) return;

   /* hide the input rectangle */
   if (sd->o_input) evas_object_hide(sd->o_input);

   /* hide the image object */
   if (sd->o_img) evas_object_hide(sd->o_img);
}

static void 
_e_smart_clip_set(Evas_Object *obj, Evas_Object *clip)
{
   E_Smart_Data *sd = NULL;

   /* try to get the objects smart data */
   if (!(sd = evas_object_smart_data_get(obj))) return;

   /* TODO: Hmmm, set clip on the input rectangle ?? */

   /* set the clip on the image object */
   if (sd->o_img) evas_object_clip_set(sd->o_img, clip);
}

static void 
_e_smart_clip_unset(Evas_Object *obj)
{
   E_Smart_Data *sd = NULL;

   /* try to get the objects smart data */
   if (!(sd = evas_object_smart_data_get(obj))) return;

   /* TODO: Hmmm, unset clip on the input rectangle ?? */

   /* unset the image object clip */
   if (sd->o_img) evas_object_clip_unset(sd->o_img);
}

/* local functions */
static void 
_e_surface_cb_focus_in(void *data, Evas *evas EINA_UNUSED, void *event EINA_UNUSED)
{
   evas_object_smart_callback_call(data, "focus_in", NULL);
}

static void 
_e_surface_cb_focus_out(void *data, Evas *evas EINA_UNUSED, void *event EINA_UNUSED)
{
   evas_object_smart_callback_call(data, "focus_out", NULL);
}

static void 
_e_surface_cb_mouse_in(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   evas_object_smart_callback_call(data, "mouse_in", event);
}

static void 
_e_surface_cb_mouse_out(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   evas_object_smart_callback_call(data, "mouse_out", event);
}

static void 
_e_surface_cb_mouse_move(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   evas_object_smart_callback_call(data, "mouse_move", event);
}

static void 
_e_surface_cb_mouse_down(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   E_Smart_Data *sd = NULL;

   /* try to get the objects smart data */
   if (!(sd = evas_object_smart_data_get(data))) return;

   /* grab the loop time for this mouse down event
    * 
    * NB: we use this for comparison in the mouse_up callback due to 
    * some e_border grab shenanigans. Basically, this lets us ignore the 
    * spurious mouse_up that we get from e_border grabs */
   sd->mouse_down_time = ecore_loop_time_get();
   evas_object_smart_callback_call(data, "mouse_down", event);
}

static void 
_e_surface_cb_mouse_up(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   E_Smart_Data *sd = NULL;
   double timestamp;

   /* try to get the objects smart data */
   if (!(sd = evas_object_smart_data_get(data))) return;

   timestamp = ecore_loop_time_get();
   if (fabs(timestamp - sd->mouse_down_time) <= 0.010) return;

   evas_object_smart_callback_call(data, "mouse_up", event);
}

static void 
_e_surface_cb_mouse_wheel(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   evas_object_smart_callback_call(data, "mouse_wheel", event);
}

static void 
_e_surface_cb_key_down(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   evas_object_smart_callback_call(data, "key_down", event);
}

static void 
_e_surface_cb_key_up(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event)
{
   evas_object_smart_callback_call(data, "key_up", event);
}

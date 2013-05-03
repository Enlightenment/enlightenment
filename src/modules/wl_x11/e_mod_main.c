#include "e.h"
#include "e_mod_main.h"

/* local function prototypes */
static Eina_Bool _output_init(void);
static void _output_shutdown(E_Output_X11 *output);
static int _output_cb_frame(void *data);
static void _output_cb_destroy(E_Output *output);
static Eina_Bool _output_cb_window_destroy(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);

/* local variables */
static E_Compositor_X11 *_e_comp;
static Eina_List *_hdlrs = NULL;

EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Wl_X11" };

EAPI void *
e_modapi_init(E_Module *m)
{
   /* try to allocate space for comp structure */
   if (!(_e_comp = E_NEW(E_Compositor_X11, 1)))
     {
        ERR("Could not allocate space for compositor: %m");
        return NULL;
     }

   /* try to init ecore_x */
   if (!ecore_x_init(NULL))
     {
        ERR("Could not initialize ecore_x: %m");
        goto x_err;
     }

   /* get the X display */
   _e_comp->display = ecore_x_display_get();

   /* try to initialize generic compositor */
   if (!e_compositor_init(&_e_comp->base, _e_comp->display))
     {
        ERR("Could not initialize compositor: %m");
        goto comp_err;
     }

   /* try to initialize output */
   if (!_output_init())
     {
        ERR("Could not initialize output: %m");
        goto output_err;
     }

   E_LIST_HANDLER_APPEND(_hdlrs, ECORE_X_EVENT_WINDOW_DELETE_REQUEST, 
                         _output_cb_window_destroy, NULL);

   /* flush any pending events
    * 
    * NB: This advertises out any globals so it needs to be deferred 
    * until after the module has finished initialize */
   /* wl_event_loop_dispatch(_e_comp->base.wl.loop, 0); */

   return m;

output_err:
   /* shutdown the e_compositor */
   e_compositor_shutdown(&_e_comp->base);

comp_err:
   /* shutdown ecore_x */
   ecore_x_shutdown();

x_err:
   /* free the structure */
   E_FREE(_e_comp);

   return NULL;
}

EAPI int 
e_modapi_shutdown(E_Module *m)
{
   E_Output_X11 *output;

   /* destroy the list of handlers */
   E_FREE_LIST(_hdlrs, ecore_event_handler_del);

   /* destroy the outputs */
   EINA_LIST_FREE(_e_comp->base.outputs, output)
     _output_shutdown(output);

   /* shutdown generic compositor */
   if (&_e_comp->base) e_compositor_shutdown(&_e_comp->base);

   /* shutdown ecore_x */
   ecore_x_shutdown();

   /* free the structure */
   E_FREE(_e_comp);

   return 1;
}

/* local functions */
static Eina_Bool 
_output_init(void)
{
   E_Output_X11 *output;
   struct wl_event_loop *loop;

   /* try to allocate space for our output structure */
   if (!(output = E_NEW(E_Output_X11, 1))) 
     {
        ERR("Could not allocate space for output structure");
        return EINA_FALSE;
     }

   output->mode.flags = (WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED);
   output->mode.w = 1024;
   output->mode.h = 768;
   output->mode.refresh = 60;

   /* add this mode to the base outputs list of modes */
   output->base.modes = eina_list_append(output->base.modes, &output->mode);

   /* try to create the output window */
   if (!(output->win = 
         ecore_x_window_new(0, 0, 0, output->mode.w, output->mode.h)))
     {
        ERR("Failed to create ecore_x_window");
        return EINA_FALSE;
     }

   /* set window background color */
   ecore_x_window_background_color_set(output->win, 0, 0, 0);

   ecore_x_icccm_protocol_set(output->win, 
                              ECORE_X_WM_PROTOCOL_DELETE_REQUEST, EINA_TRUE);

   /* set window to not maximize */
   ecore_x_icccm_size_pos_hints_set(output->win, EINA_FALSE, 
                                    ECORE_X_GRAVITY_NW, 
                                    output->mode.w, output->mode.h, 
                                    output->mode.w, output->mode.h,
                                    0, 0, 1, 1, 0.0, 0.0);

   /* set window title */
   ecore_x_icccm_title_set(output->win, "E_Wayland X11 Compositor");
   ecore_x_icccm_name_class_set(output->win, "E_Wayland X11 Compositor", 
                                "e_wayland/X11 Compositor");

   /* show the window */
   ecore_x_window_show(output->win);

   output->base.current = &output->mode;
   output->base.original = output->base.current;
   output->base.make = "e_wayland";
   output->base.model = "none";
   output->base.cb_destroy = _output_cb_destroy;

   /* initialize base output */
   e_output_init(&output->base, &_e_comp->base, 0, 0, 
                 output->mode.w, output->mode.h, output->mode.flags);

   /* TODO: deal with render */

   /* get the wl event loop */
   loop = wl_display_get_event_loop(_e_comp->base.wl.display);

   /* add a timer to the event loop */
   output->frame_timer = 
     wl_event_loop_add_timer(loop, _output_cb_frame, output);

   /* add this output to the base compositors output list */
   _e_comp->base.outputs = eina_list_append(_e_comp->base.outputs, output);

   return EINA_TRUE;
}

static void 
_output_shutdown(E_Output_X11 *output)
{
   E_FREE(output);
}

static int 
_output_cb_frame(void *data)
{
   E_Output_X11 *output;

   if (!(output = data)) return 1;
   /* TODO: start repaint loop */
   return 1;
}

static void 
_output_cb_destroy(E_Output *output)
{
   E_Output_X11 *xout;

   xout = (E_Output_X11 *)output;

   /* remove the frame timer */
   wl_event_source_remove(xout->frame_timer);

   /* destroy the window */
   if (xout->win) ecore_x_window_free(xout->win);

   /* destroy the base output */
   e_output_shutdown(&xout->base);

   /* free the structure */
   E_FREE(xout);
}

static Eina_Bool 
_output_cb_window_destroy(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_X_Event_Window_Delete_Request *ev;
   E_Output_X11 *output;
   Eina_List *l;

   ev = event;

   /* loop the existing outputs */
   EINA_LIST_FOREACH(_e_comp->base.outputs, l, output)
     {
        /* try to match the output window */
        if (ev->win == output->win)
          {
             /* output window being closed, quit */
             /* NB: FIXME: This assumes we have only one output window */
             ecore_main_loop_quit();
             break;
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

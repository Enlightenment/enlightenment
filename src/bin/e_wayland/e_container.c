#include "e.h"

/* local function prototypes */
static void _e_container_cb_free(E_Container *con);
static Eina_Bool _e_container_cb_mouse_in(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);

/* local variables */
static Eina_List *_hdlrs = NULL;

EINTERN int 
e_container_init(void)
{
   E_LIST_HANDLER_APPEND(_hdlrs, ECORE_WL_EVENT_MOUSE_IN, 
                         _e_container_cb_mouse_in, NULL);

   return 1;
}

EINTERN int 
e_container_shutdown(void)
{
   E_FREE_LIST(_hdlrs, ecore_event_handler_del);

   return 1;
}

EAPI E_Container *
e_container_new(E_Manager *man)
{
   E_Container *con;
   static unsigned int num = 0;

   /* check for valid manager */
   E_OBJECT_CHECK_RETURN(man, NULL);
   E_OBJECT_TYPE_CHECK_RETURN(man, E_MANAGER_TYPE, NULL);

   /* try to allocate a new container object */
   if (!(con = E_OBJECT_ALLOC(E_Container, E_CONTAINER_TYPE, 
                              _e_container_cb_free)))
     return NULL;

   /* set container properties */
   con->man = man;
   con->x = man->x;
   con->y = man->y;
   con->w = man->w;
   con->h = man->h;
   con->num = num;
   num++;

   /* add this container to the managers list */
   man->containers = eina_list_append(man->containers, con);

   /* create the containers canvas */
   con->bg_ee = 
     e_canvas_new(0, con->x, con->y, con->w, con->h, 
                  EINA_FALSE, EINA_FALSE, &con->win);
   e_canvas_add(con->bg_ee);

   /* set some properties on the ecore_evas */
   ecore_evas_name_class_set(con->bg_ee, "E", "Background Window");
   ecore_evas_title_set(con->bg_ee, "Enlightenment Background");

   /* get the container window */
   /* con->win = ecore_evas_wayland_window_get(con->bg_ee)->id; */

   /* get the background canvas */
   con->bg_evas = ecore_evas_get(con->bg_ee);

   /* Evas_Object *o; */
   /* o = evas_object_rectangle_add(con->bg_evas); */
   /* evas_object_move(o, 0, 0); */
   /* evas_object_resize(o, con->w, con->h); */
   /* evas_object_show(o); */

   /* TODO: create zones */

   return con;
}

EAPI void 
e_container_show(E_Container *con)
{
   /* check for valid container */
   E_OBJECT_CHECK(con);
   E_OBJECT_TYPE_CHECK(con, E_CONTAINER_TYPE);

   /* check for already visible */
   if (con->visible) return;

   /* show the ecore_evas */
   ecore_evas_show(con->bg_ee);

   /* TODO: show zones */

   /* check for valid pointer */
   if (!con->ptr)
     {
        /* create new pointer */
        con->ptr = e_pointer_new(con->win, EINA_TRUE);
     }

   con->visible = EINA_TRUE;
}

EAPI void 
e_container_hide(E_Container *con)
{
   /* check for valid container */
   E_OBJECT_CHECK(con);
   E_OBJECT_TYPE_CHECK(con, E_CONTAINER_TYPE);

   /* check for already invisible */
   if (!con->visible) return;

   /* TODO: hide zones */

   /* hide the ecore_evas */
   ecore_evas_hide(con->bg_ee);

   con->visible = EINA_FALSE;
}

EAPI void 
e_container_all_freeze(void)
{
   Eina_List *l, *ll;
   E_Manager *man;
   E_Container *con;

   /* loop the managers */
   EINA_LIST_FOREACH(e_manager_list(), l, man)
     {
        /* loop the containers */
        EINA_LIST_FOREACH(man->containers, ll, con)
          {
             /* freeze events on the canvas */
             if (con->bg_evas) evas_event_freeze(con->bg_evas);
          }
     }
}

EAPI void 
e_container_all_thaw(void)
{
   Eina_List *l, *ll;
   E_Manager *man;
   E_Container *con;

   /* loop the managers */
   EINA_LIST_FOREACH(e_manager_list(), l, man)
     {
        /* loop the containers */
        EINA_LIST_FOREACH(man->containers, ll, con)
          {
             /* thaw events on the canvas */
             if (con->bg_evas) evas_event_thaw(con->bg_evas);
          }
     }
}

/* local functions */
static void 
_e_container_cb_free(E_Container *con)
{
   /* TODO: free zones */

   /* remove this container from the managers list */
   con->man->containers = eina_list_remove(con->man->containers, con);

   /* delete the pointer object */
   if (con->ptr) e_object_del(E_OBJECT(con->ptr));

   if (con->bg_ee)
     {
        /* delete the canvas */
        e_canvas_del(con->bg_ee);

        /* free the ecore_evas */
        ecore_evas_free(con->bg_ee);
     }

   /* free the object */
   E_FREE(con);
}

static Eina_Bool 
_e_container_cb_mouse_in(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_Wl_Event_Mouse_In *ev;

   ev = event;

   /* TODO: code me */

   return ECORE_CALLBACK_PASS_ON;
}

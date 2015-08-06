#define E_COMP_WL
#include "e.h"
#include <dlfcn.h>

#define WL_TEXT_STR "text/plain;charset=utf-8"

static void (*xconvertselection)(Ecore_X_Display *, Ecore_X_Atom, Ecore_X_Atom, Ecore_X_Atom, Ecore_X_Window, Ecore_X_Time);
static Ecore_X_Atom string_atom;
static Ecore_X_Atom xwl_dnd_atom;

static int32_t cur_fd = -1;

static void
_xdnd_finish(Eina_Bool success)
{
   ecore_x_client_message32_send(e_client_util_win_get(e_comp->wl_comp_data->drag_client), ECORE_X_ATOM_XDND_FINISHED, ECORE_X_EVENT_MASK_NONE,
     e_comp->cm_selection, !!success, (!!success) * ECORE_X_ATOM_XDND_ACTION_COPY, 0, 0);
}

static void
_xwayland_dnd_finish(void)
{
   ecore_x_window_hide(e_comp->cm_selection);
   ecore_x_window_prop_property_del(e_comp->cm_selection, ECORE_X_ATOM_XDND_TYPE_LIST);
   e_comp->wl_comp_data->drag_client = NULL;
   e_comp->wl_comp_data->drag_source = NULL;
   cur_fd = -1;
}

static void
_xwayland_drop(E_Drag *drag, int dropped)
{
   if (e_comp->comp_type != E_PIXMAP_TYPE_WL) return;
   e_comp->wl_comp_data->drag = NULL;
   if (e_object_is_del(E_OBJECT(drag)) || (!e_comp->wl_comp_data->selection.target))
     _xdnd_finish(0);
   else
     {
        struct wl_resource *res;

        res = e_comp_wl_data_find_for_client(wl_resource_get_client(e_comp->wl_comp_data->selection.target->comp_data->surface));
        if (res)
          {
             wl_data_device_send_drop(res);
             wl_data_device_send_leave(res);
             ecore_x_client_message32_send(e_client_util_win_get(e_comp->wl_comp_data->drag_client), ECORE_X_ATOM_XDND_DROP, ECORE_X_EVENT_MASK_NONE,
               e_comp->cm_selection, 0, ecore_x_current_time_get(), 0, 0);
          }
        return;
     }
   _xwayland_dnd_finish();
}

static void
_xwayland_target_send(E_Comp_Wl_Data_Source *source, uint32_t serial EINA_UNUSED, const char* mime_type)
{
   DBG("XWL Data Source Target Send");
   ecore_x_client_message32_send(e_client_util_win_get(e_comp->wl_comp_data->drag_client), ECORE_X_ATOM_XDND_STATUS, ECORE_X_EVENT_MASK_NONE,
     e_comp->cm_selection, 2 | !!mime_type, 0, 0, (!!mime_type) * ECORE_X_ATOM_XDND_ACTION_COPY);
}

static void
_xwayland_send_send(E_Comp_Wl_Data_Source *source, const char* mime_type, int32_t fd)
{
   Ecore_X_Atom type;

   DBG("XWL Data Source Source Send");

   _xdnd_finish(0);

   if (eina_streq(mime_type, WL_TEXT_STR))
     type = string_atom;
   else
     type = ecore_x_atom_get(mime_type);

   cur_fd = fd;
   xconvertselection(ecore_x_display_get(), ECORE_X_ATOM_SELECTION_XDND, type, xwl_dnd_atom, e_comp->cm_selection, 0);
}

static void
_xwayland_cancelled_send(E_Comp_Wl_Data_Source *source)
{
   DBG("XWL Data Source Cancelled Send");
   e_object_del(E_OBJECT(e_comp->wl_comp_data->drag));
}

static Eina_Bool
_xwl_fixes_selection_notify(void *d EINA_UNUSED, int t EINA_UNUSED, Ecore_X_Event_Fixes_Selection_Notify *ev)
{
   if (ev->atom == ECORE_X_ATOM_SELECTION_XDND)
     {
        if (ev->owner)
          {
             int x, y, num;
             unsigned char *data;
             const char **names = NULL;
             Eina_List *namelist = NULL;
             E_Comp_Wl_Data_Source *source;

             if (ecore_x_window_prop_property_get(ev->owner,
                                                  ECORE_X_ATOM_XDND_TYPE_LIST,
                                                  ECORE_X_ATOM_ATOM,
                                                  32,
                                                  &data,
                                                  &num))
               {
                  int i;
                  Ecore_X_Atom *types = (void*)data;

                  names = malloc(num * sizeof(void*));
                  for (i = 0; i < num; i++)
                    {
                       const char *name;

                       if (types[i] == string_atom)
                         {
                            name = names[i] = "UTF8_STRING";
                            namelist = eina_list_append(namelist, eina_stringshare_add(WL_TEXT_STR));
                         }
                       else
                         names[i] = name = ecore_x_atom_name_get(types[i]);
                       namelist = eina_list_append(namelist, eina_stringshare_add(name));
                    }
                  if (num > 3)
                    {
                       ecore_x_window_prop_property_set(e_comp->cm_selection,
                         ECORE_X_ATOM_XDND_TYPE_LIST, ECORE_X_ATOM_ATOM, 32, names, num);
                    }

                  free(data);
               }
             evas_pointer_canvas_xy_get(e_comp->evas, &x, &y);
             e_comp->wl_comp_data->drag_client = e_pixmap_find_client(E_PIXMAP_TYPE_X, ev->owner);
             e_comp->wl_comp_data->drag = e_drag_new(x, y, names, num, NULL, 0, NULL, _xwayland_drop);
             ecore_x_window_move_resize(e_comp->cm_selection, 0, 0, e_comp->w, e_comp->h);
             ecore_x_window_show(e_comp->cm_selection);
             e_drag_start(e_comp->wl_comp_data->drag, x, y);
             if (e_comp->wl_comp_data->ptr.ec)
               e_comp_wl_data_device_send_enter(e_comp->wl_comp_data->ptr.ec);
             e_comp_canvas_feed_mouse_up(0);
             source = e_comp_wl_data_manager_source_create(e_comp->wl_comp_data->xwl_client,
               e_comp->wl_comp_data->mgr.resource, 1);
             e_comp->wl_comp_data->drag_source = source;
             source->target = _xwayland_target_send;
             source->send = _xwayland_send_send;
             source->cancelled = _xwayland_cancelled_send;
             source->mime_types = namelist;
             free(names);
          }
        else
          {
             if (e_comp->wl_comp_data->drag)
               e_object_del(E_OBJECT(e_comp->wl_comp_data->drag));
             ecore_x_window_hide(e_comp->cm_selection);
             e_comp->wl_comp_data->drag = NULL;
             e_comp->wl_comp_data->drag_client = NULL;
          }
        e_screensaver_inhibit_toggle(!!ev->owner);
        return ECORE_CALLBACK_RENEW;
     }
   //if (ev->atom == ECORE_X_ATOM_SELECTION_CLIPBOARD)
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_xwl_selection_notify(void *d EINA_UNUSED, int t EINA_UNUSED, Ecore_X_Event_Selection_Notify *ev)
{
   Ecore_X_Selection_Data *sd;
   int wrote = 0;

   DBG("XWL SELECTION NOTIFY");
   if ((ev->selection != ECORE_X_SELECTION_XDND) && (ev->selection == ECORE_X_SELECTION_CLIPBOARD))
     {
        e_object_del(E_OBJECT(e_comp->wl_comp_data->drag));
        return ECORE_CALLBACK_RENEW;
     }
   /* FIXME: ecore-x events are fucked */
   //if (ecore_x_atom_get(ev->target) != xwl_dnd_atom) return ECORE_CALLBACK_RENEW;
   sd = ev->data;

   do
     {
        wrote += write(cur_fd, sd->data, sd->length);
     } while (wrote < sd->length);
   _xdnd_finish(1);
   close(cur_fd);
   _xwayland_dnd_finish();
   return ECORE_CALLBACK_RENEW;
}

EINTERN void
dnd_init(void)
{
   ecore_x_fixes_selection_notification_request(ecore_x_atom_get("CLIPBOARD"));
   ecore_x_fixes_selection_notification_request(ECORE_X_ATOM_SELECTION_XDND);
   ecore_event_handler_add(ECORE_X_EVENT_FIXES_SELECTION_NOTIFY, (Ecore_Event_Handler_Cb)_xwl_fixes_selection_notify, NULL);
   ecore_event_handler_add(ECORE_X_EVENT_SELECTION_NOTIFY, (Ecore_Event_Handler_Cb)_xwl_selection_notify, NULL);
   xconvertselection = dlsym(NULL, "XConvertSelection");
   string_atom = ecore_x_atom_get("UTF8_STRING");
   xwl_dnd_atom = ecore_x_atom_get("E_XWL_DND_ATOM_HAHA");
   e_comp_shape_queue();
}

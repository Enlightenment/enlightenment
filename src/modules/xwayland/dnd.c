#define E_COMP_WL
#include "e.h"
#include <dlfcn.h>

#define WL_TEXT_STR "text/plain;charset=utf-8"
#define INCR_CHUNK_SIZE 1 << 17

static void (*xconvertselection)(Ecore_X_Display *, Ecore_X_Atom, Ecore_X_Atom, Ecore_X_Atom, Ecore_X_Window, Ecore_X_Time);
static Ecore_X_Atom string_atom;
static Ecore_X_Atom xwl_dnd_atom;
static Ecore_X_Atom timestamp_atom;
static Ecore_X_Atom incr_atom;
static Ecore_X_Atom int_atom;

static int32_t cur_fd = -1;

static Eina_List *handlers;
static Eina_Hash *pipes;

typedef struct
{
   Ecore_Fd_Handler *fdh;
   E_Comp_Wl_Data_Source *source;
   Ecore_X_Window win;
   Ecore_X_Atom atom;
   Ecore_X_Atom selection;
   Ecore_X_Atom property;
   Eina_Binbuf *buf;
   Eina_Bool incr : 1;
} Pipe;

static void
_pipe_free(Pipe *p)
{
   close(ecore_main_fd_handler_fd_get(p->fdh));
   ecore_main_fd_handler_del(p->fdh);
   eina_binbuf_free(p->buf);
   free(p);
}

static void
_xdnd_finish(Eina_Bool success)
{
   ecore_x_client_message32_send(e_client_util_win_get(e_comp->wl_comp_data->drag_client), ECORE_X_ATOM_XDND_FINISHED, ECORE_X_EVENT_MASK_NONE,
     e_comp->cm_selection, !!success, (!!success) * ECORE_X_ATOM_XDND_ACTION_COPY, 0, 0);
}

static void
_incr_update(Pipe *p, Eina_Bool success)
{
   ecore_x_selection_notify_send(p->win, p->selection, p->atom, (!!success) * p->property, 0);
}

static void
_incr_upload(Pipe *p)
{
   size_t size;

   size = eina_binbuf_length_get(p->buf);
   size = MIN(size, INCR_CHUNK_SIZE);
   ecore_x_window_prop_property_set(p->win, p->property, p->atom, 8, (void*)eina_binbuf_string_get(p->buf), size);
   eina_binbuf_free(p->buf);
   p->buf = NULL;
}

static void
_xwayland_dnd_finish(void)
{
   ecore_x_window_hide(e_comp->cm_selection);
   ecore_x_window_prop_property_del(e_comp->cm_selection, ECORE_X_ATOM_XDND_TYPE_LIST);
   e_comp->wl_comp_data->drag_client = NULL;
   e_comp->wl_comp_data->drag_source = NULL;
   cur_fd = -1;
   e_screensaver_inhibit_toggle(0);
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
   if (ev->owner == e_comp->cm_selection)
     {
        e_comp->wl_comp_data->clipboard.xwl_owner = NULL;
        return ECORE_CALLBACK_RENEW;
     }
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
             e_screensaver_inhibit_toggle(1);
          }
        else
          {
             if (e_comp->wl_comp_data->drag)
               e_object_del(E_OBJECT(e_comp->wl_comp_data->drag));
          }
        e_screensaver_inhibit_toggle(!!ev->owner);
        return ECORE_CALLBACK_RENEW;
     }
   if (ev->atom == ECORE_X_ATOM_SELECTION_CLIPBOARD)
     {
        e_comp->wl_comp_data->clipboard.xwl_owner = ev->owner ? e_pixmap_find_client(E_PIXMAP_TYPE_X, ev->owner) : NULL;
     }
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

static Eina_Bool
_xwl_pipe_read(void *data, Ecore_Fd_Handler *fdh)
{
   Pipe *p = data;
   ssize_t len;
   unsigned char *buf;

   buf = malloc(INCR_CHUNK_SIZE);
   len = read(ecore_main_fd_handler_fd_get(fdh), (void*)buf, INCR_CHUNK_SIZE);
   if (len < 0)
     {
        _incr_update(p, 0);
        eina_hash_del_by_key(pipes, &p->win);
     }
   if (len == INCR_CHUNK_SIZE)
     {
        p->buf = eina_binbuf_manage_new(buf, len, 0);
        if (p->incr)
          _incr_upload(p);
        else
          {
             unsigned long size = INCR_CHUNK_SIZE;

             p->incr = 1;
             ecore_x_window_prop_property_set(p->win, p->atom, incr_atom, 32, &size, 1);
             _incr_update(p, 1);
          }
        ecore_main_fd_handler_active_set(p->fdh, 0);
        return ECORE_CALLBACK_RENEW;
     }
   if (len)
     p->buf = eina_binbuf_manage_new(buf, len, 0);
   _incr_upload(p);
   if (p->incr)
     ecore_main_fd_handler_active_set(p->fdh, 0);
   else
     {
        _incr_update(p, 1);
        eina_hash_del_by_key(pipes, &p->win);
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_xwl_selection_request(void *d EINA_UNUSED, int t EINA_UNUSED, Ecore_X_Event_Selection_Request *ev)
{
   E_Comp_Wl_Data_Source *source;
   const char *type;
   Eina_List *l;

   if (!e_comp->wl_comp_data->drag_source) return ECORE_CALLBACK_RENEW;

   source = e_comp->wl_comp_data->drag_source;
   if (ev->target == ECORE_X_ATOM_SELECTION_TARGETS)
     {
        Ecore_X_Atom *atoms;
        int i = 0;

        atoms = alloca((2 + eina_list_count(source->mime_types)) * sizeof(void*));
        EINA_LIST_FOREACH(source->mime_types, l, type)
          atoms[i++] = ecore_x_atom_get(type);
        atoms[i++] = timestamp_atom;
        atoms[i++] = ECORE_X_ATOM_SELECTION_TARGETS;
        ecore_x_window_prop_property_set(ev->requestor, ev->property, ECORE_X_ATOM_ATOM, 32, atoms, i);
        ecore_x_selection_notify_send(ev->requestor, ev->selection, ev->target, ev->property, 0);
     }
   else if (ev->target == timestamp_atom)
     {
        Ecore_X_Time timestamp;

        timestamp = ecore_x_current_time_get();
        ecore_x_window_prop_property_set(ev->requestor, ev->property, int_atom, 32, (void*)&timestamp, 1);
        ecore_x_selection_notify_send(ev->requestor, ev->selection, ev->target, ev->property, 0);
     }
   else
     {
        const char *name;
        Pipe *p;

        name = ecore_x_atom_name_get(ev->target);
        EINA_LIST_FOREACH(source->mime_types, l, type)
          if (eina_streq(name, type))
            {
               E_Client *ec;
               int fds[2];

               p = E_NEW(Pipe, 1);
               socketpair(AF_UNIX, (SOCK_STREAM | SOCK_CLOEXEC), 0, fds);
               fcntl(fds[0], F_SETFL, O_NONBLOCK);
               p->fdh = ecore_main_fd_handler_add(fds[0], ECORE_FD_READ, _xwl_pipe_read, p, NULL, NULL);
               p->win = ev->requestor;
               p->source = source;
               wl_data_source_send_send(source->resource, type, dup(fds[1]));
               close(fds[1]);
               p->atom = ev->target;
               p->selection = ev->selection;
               p->property = ev->property;
               ec = e_pixmap_find_client(E_PIXMAP_TYPE_X, ev->requestor);
               if (ec && ec->override)
                 ecore_x_window_sniff(ev->requestor);
               eina_hash_add(pipes, &p->win, p);
               break;
            }
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_xwl_property(void *d EINA_UNUSED, int t EINA_UNUSED, Ecore_X_Event_Window_Property *ev)
{
   Pipe *p;

#if (EFL_VERSION_MAJOR > 1) || (EFL_VERSION_MINOR >= 16)
   if (!ev->state) return ECORE_CALLBACK_RENEW;
#endif
   p = eina_hash_find(pipes, &ev->win);
   if (!p) return ECORE_CALLBACK_RENEW;
   /* FIXME: WHO FORGOT THE FUCKING STATE FLAG???? */
   ecore_main_fd_handler_active_set(p->fdh, ECORE_FD_READ);
   return ECORE_CALLBACK_RENEW;
}

EINTERN void
dnd_init(void)
{
   ecore_x_fixes_selection_notification_request(ecore_x_atom_get("CLIPBOARD"));
   ecore_x_fixes_selection_notification_request(ECORE_X_ATOM_SELECTION_XDND);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_FIXES_SELECTION_NOTIFY, _xwl_fixes_selection_notify, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_SELECTION_NOTIFY, _xwl_selection_notify, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_SELECTION_REQUEST, _xwl_selection_request, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_PROPERTY, _xwl_property, NULL);
   xconvertselection = dlsym(NULL, "XConvertSelection");
   string_atom = ecore_x_atom_get("UTF8_STRING");
   timestamp_atom = ecore_x_atom_get("TIMESTAMP");
   int_atom = ecore_x_atom_get("INTEGER");
   incr_atom = ecore_x_atom_get("TIMESTAMP");
   xwl_dnd_atom = ecore_x_atom_get("E_XWL_DND_ATOM_HAHA");
   pipes = eina_hash_int32_new((Eina_Free_Cb)_pipe_free);
   e_comp_shape_queue();
}

EINTERN void
dnd_shutdown(void)
{
   E_FREE_LIST(handlers, ecore_event_handler_del);
   E_FREE_FUNC(pipes, eina_hash_free);
}

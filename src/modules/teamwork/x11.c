#define E_COMP_X
#include "e_mod_main.h"

enum
{
   TEAMWORK_PRELOAD,
   TEAMWORK_ACTIVATE,
   TEAMWORK_DEACTIVATE,
   TEAMWORK_OPEN,
   TEAMWORK_NOPE,
};

enum
{
   TEAMWORK_COMPLETED,
   TEAMWORK_PROGRESS,
   TEAMWORK_STARTED,
};

static Ecore_Event_Handler *handler;
static Ecore_X_Atom atoms[4];
static Ecore_X_Atom server_atoms[3];
static Ecore_X_Atom prop;

static Eina_Bool
x11_message_handler(void *d EINA_UNUSED, int t EINA_UNUSED, Ecore_X_Event_Client_Message *ev)
{
   E_Client *ec;
   int i;
   char *uri;

   ec = e_pixmap_find_client(E_PIXMAP_TYPE_X, ev->win);
   if (!ec) return ECORE_CALLBACK_RENEW;
   for (i = 0; i < TEAMWORK_NOPE; i++)
     if (ev->message_type == atoms[i]) break;
   if (i == TEAMWORK_NOPE) return ECORE_CALLBACK_RENEW;

   uri = ecore_x_window_prop_string_get(ev->win, prop);
   if (!uri) return ECORE_CALLBACK_RENEW;
   if (ev->message_type == atoms[TEAMWORK_PRELOAD])
     /*
       format = 32
       data.l[0] = version
      */
     tw_link_detect(ec, uri);
   else if (ev->message_type == atoms[TEAMWORK_ACTIVATE])
     /*
       format = 32
       data.l[0] = version
       data.l[1] = window_x
       data.l[2] = window_y
      */
     tw_link_show(ec, uri, ev->data.l[1], ev->data.l[2]);
   else if (ev->message_type == atoms[TEAMWORK_DEACTIVATE])
     /*
       format = 32
       data.l[0] = version
      */
     tw_link_hide(ec, uri);
   else if (ev->message_type == atoms[TEAMWORK_OPEN])
     /*
       format = 32
       data.l[0] = version
      */
     tw_link_open(ec, uri);
   free(uri);
   return ECORE_CALLBACK_RENEW;
}

static void
x11_tw_link_complete(E_Client *ec, const char *uri EINA_UNUSED)
{
   if (e_object_is_del(E_OBJECT(ec))) return;
   ecore_x_client_message32_send(e_client_util_win_get(ec), server_atoms[TEAMWORK_COMPLETED],
     ECORE_X_EVENT_MASK_WINDOW_MANAGE | ECORE_X_EVENT_MASK_WINDOW_CHILD_CONFIGURE, E_TW_VERSION, 1, 0, 0, 0);
}

static void
x11_tw_link_invalid(E_Client *ec, const char *uri EINA_UNUSED)
{
   if (e_object_is_del(E_OBJECT(ec))) return;
   ecore_x_client_message32_send(e_client_util_win_get(ec), server_atoms[TEAMWORK_COMPLETED],
     ECORE_X_EVENT_MASK_WINDOW_MANAGE | ECORE_X_EVENT_MASK_WINDOW_CHILD_CONFIGURE, E_TW_VERSION, 0, 0, 0, 0);
}

static void
x11_tw_link_progress(E_Client *ec, const char *uri EINA_UNUSED, uint32_t pct)
{
   if (e_object_is_del(E_OBJECT(ec))) return;
   ecore_x_client_message32_send(e_client_util_win_get(ec), server_atoms[TEAMWORK_PROGRESS],
     ECORE_X_EVENT_MASK_WINDOW_MANAGE | ECORE_X_EVENT_MASK_WINDOW_CHILD_CONFIGURE, E_TW_VERSION, pct, 0, 0, 0);
}

static void
x11_tw_link_downloading(E_Client *ec, const char *uri EINA_UNUSED)
{
   if (e_object_is_del(E_OBJECT(ec))) return;
   ecore_x_client_message32_send(e_client_util_win_get(ec), server_atoms[TEAMWORK_STARTED],
     ECORE_X_EVENT_MASK_WINDOW_MANAGE | ECORE_X_EVENT_MASK_WINDOW_CHILD_CONFIGURE, E_TW_VERSION, 0, 0, 0, 0);
}

EINTERN Eina_Bool
x11_tw_init(void)
{
   const char *atom_names[] =
   {
      "_TEAMWORK_PRELOAD",
      "_TEAMWORK_ACTIVATE",
      "_TEAMWORK_DEACTIVATE",
      "_TEAMWORK_OPEN",
      "_TEAMWORK_COMPLETED",
      "_TEAMWORK_PROGRESS",
      "_TEAMWORK_STARTED",
      "_TEAMWORK_PROP",
   };
   Ecore_X_Atom at[EINA_C_ARRAY_LENGTH(atom_names)];
   int i;

   ecore_x_atoms_get(atom_names, EINA_C_ARRAY_LENGTH(atom_names), at);
   for (i = 0; i < 4; i++)
     atoms[i] = at[i];
   for (i = 4; i < 7; i++)
     server_atoms[i - 4] = at[i];
   prop = at[7];
   handler = ecore_event_handler_add(ECORE_X_EVENT_CLIENT_MESSAGE, (Ecore_Event_Handler_Cb)x11_message_handler, NULL);
   tw_signal_link_complete[E_PIXMAP_TYPE_X] = x11_tw_link_complete;
   tw_signal_link_invalid[E_PIXMAP_TYPE_X] = x11_tw_link_invalid;
   tw_signal_link_progress[E_PIXMAP_TYPE_X] = x11_tw_link_progress;
   tw_signal_link_downloading[E_PIXMAP_TYPE_X] = x11_tw_link_downloading;
   return EINA_TRUE;
}

EINTERN void
x11_tw_shutdown(void)
{
   E_FREE_FUNC(handler, ecore_event_handler_del);
   tw_signal_link_complete[E_PIXMAP_TYPE_X] = NULL;
   tw_signal_link_invalid[E_PIXMAP_TYPE_X] = NULL;
   tw_signal_link_progress[E_PIXMAP_TYPE_X] = NULL;
   tw_signal_link_downloading[E_PIXMAP_TYPE_X] = NULL;
}

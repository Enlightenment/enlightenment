#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <Eina.h>
#include <Ecore.h>

typedef struct
{
   char cmd[24];
   int  size;
} Message_Head;

static Ecore_Exe *exe;

static Eina_Bool
_cb_exe_del(void *data EINA_UNUSED, int ev_type EINA_UNUSED, void *event EINA_UNUSED)
{
   ecore_main_loop_quit();
   return EINA_TRUE;
}

static Eina_Binbuf *buf = NULL;

static Eina_Bool
_message_read(void)
{
   Message_Head *head;
   const unsigned char *data = eina_binbuf_string_get(buf);
   size_t len = eina_binbuf_length_get(buf);
   Eina_Binbuf *buf2;

   if (!data) return EINA_FALSE;
   if (len < sizeof(Message_Head)) return EINA_FALSE;
   head = (Message_Head *)data;
   if (len < (sizeof(Message_Head) + head->size)) return EINA_FALSE;
   printf("CMD: [%s]", head->cmd);
   if (head->size == 0) printf("\n\n");
   else printf(" [%s]\n\n", data + sizeof(Message_Head));
   buf2 = eina_binbuf_new();
   eina_binbuf_append_length(buf2,
                             data + sizeof(Message_Head) + head->size,
                             len - (sizeof(Message_Head) + head->size));
   eina_binbuf_free(buf);
   buf = buf2;
   return EINA_TRUE;
}

static Eina_Bool
_cb_exe_data(void *data EINA_UNUSED, int ev_type EINA_UNUSED, void *event)
{
   Ecore_Exe_Event_Data *ev = event;

   eina_binbuf_append_length(buf, ev->data, ev->size);
   while (_message_read());
   return EINA_TRUE;
}

static void
_cb_input(void *data EINA_UNUSED, Ecore_Thread *th)
{
   char b[4096], *msg = NULL;

   for (;;)
     {
        while (fgets(b, sizeof(b) - 1, stdin))
          {
             b[sizeof(b) - 1] = 0;
             size_t len = strlen(b);
             for (len = len - 1; len > 0; len--)
               {
                  if (b[len] == '\n') b[len] = '\0';
                  else if (b[len] == '\r') b[len] = '\0';
                  else break;
               }
             msg = strdup(b);
             if (msg) ecore_thread_feedback(th, msg);
          }
     }
}

static void
_cb_input_message(void *data EINA_UNUSED, Ecore_Thread *th EINA_UNUSED, void *msg_data)
{
   char *msg = msg_data;
   char *spc, *params = NULL;
   Message_Head head;

   head.size = 0;
   spc = strchr(msg, ' ');
   if (spc)
     {
        *spc = '\0';
        params = spc + 1;
        head.size = strlen(params) + 1;
     }
   strcpy(head.cmd, msg);
   ecore_exe_send(exe, &head, sizeof(Message_Head));
   if (head.size > 0) ecore_exe_send(exe, params, strlen(params) + 1);
}

static void
_cb_input_end(void *data EINA_UNUSED, Ecore_Thread *th EINA_UNUSED)
{
}

static void
_cb_input_cancel(void *data EINA_UNUSED, Ecore_Thread *th EINA_UNUSED)
{
}

int
main(int argc, const char **argv)
{
   const char *backend;

   if (argc < 2)
     {
        printf("use: %s /usr/local/lib/enlightenment/utils/enlightenment_system\n"
               " or the path to the enlightenment_system tool wherever it is\n",
               argv[0]);
        return -1;
     }
   backend = argv[1];

   eina_init();
   ecore_init();

   buf = eina_binbuf_new();
   ecore_event_handler_add(ECORE_EXE_EVENT_DEL, _cb_exe_del, NULL);
   ecore_event_handler_add(ECORE_EXE_EVENT_DATA, _cb_exe_data, NULL);
   exe = ecore_exe_pipe_run(backend,
                            ECORE_EXE_NOT_LEADER |
                            ECORE_EXE_TERM_WITH_PARENT |
                            ECORE_EXE_PIPE_READ |
                            ECORE_EXE_PIPE_WRITE, NULL);
   ecore_thread_feedback_run(_cb_input, _cb_input_message,
                             _cb_input_end, _cb_input_cancel, NULL, EINA_TRUE);

   ecore_main_loop_begin();

   ecore_shutdown();
   eina_shutdown();
   return 0;
}


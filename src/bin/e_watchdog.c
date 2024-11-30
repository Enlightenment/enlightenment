#include "e.h"

static Ecore_Thread *_watchdog_thread = NULL;
static Ecore_Pipe *_watchdog_pipe = NULL;
static unsigned long last_seq = 0;

static void
_cb_watchdog_thread_pingpong_pipe(void *data EINA_UNUSED, void *buf, unsigned int bytes)
{
   unsigned long long *seq = buf;
   unsigned long long seq_num = bytes / sizeof(unsigned long long);

   if (seq_num < 1)
     {
        return;
     }
   last_seq = seq[seq_num - 1];
}

static void
_cb_watchdog_thread_pingpong(void *data EINA_UNUSED, Ecore_Thread *thread)
{
   unsigned long long *seq_new;
   unsigned long long seq = 0;

   while (!ecore_thread_check(thread))
     {
        // send ping
        seq_new = malloc(sizeof(unsigned long long));
        if (seq_new)
          {
             seq++;
             *seq_new = seq;
             ecore_thread_feedback(thread, seq_new);
             // wait for ping from mainloop upto 10 sec
             if (ecore_pipe_wait(_watchdog_pipe, 1, 10.0) < 1)
               {
                  char buf[PATH_MAX];

                  printf("WD: Enlightenment main loop hung. No response to ping for 10sec\n");
                  // do hard-exit as cleanup isnt doable
                  e_user_dir_concat_static(buf, "/watchdog-crash");
                  // if ~/.e/e/watchdog-crash then crash (segv)
                  if (ecore_file_exists(buf))
                    {
                       printf("WD: Forcing a SEGV crash to get a backtracxe - see ~/.e-crashdump.txt\n");
                       kill(getpid(), SIGSEGV);
                    }
                  // otherwise just restart so user can march on
                  else
                    {
                       printf("WD: Exiting E allowing it to be restarted to un-block\n");
                       _exit(121);
                    }
               }
             // wait another 10 sec before pinging
             sleep(10);
          }
        else
          {
             printf("WD: Watchdog response alloc fail!!!!\n");
             // XXX: alloc fail
             break;
          }
     }
}

static void
_cb_watchdog_thread_pingpong_reply(void *data EINA_UNUSED, Ecore_Thread *thread EINA_UNUSED, void *msg)
{
   // reply back to mainloop with same ping number
   unsigned long long *seq = msg;

   ecore_pipe_write(_watchdog_pipe, seq, sizeof(unsigned long long));
   free(seq);
}

static void
_cb_watchdog_thread_pingpong_end(void *data EINA_UNUSED, Ecore_Thread *thread EINA_UNUSED)
{
   ecore_pipe_del(_watchdog_pipe);
   _watchdog_pipe = NULL;
   _watchdog_thread = NULL;
}

E_API void
e_watchdog_begin(void)
{
   // set up main-loop ping-pong to a thread
   _watchdog_pipe = ecore_pipe_add(_cb_watchdog_thread_pingpong_pipe, NULL);
   // stop mainloop watching with fd handler as wer wait manually
   ecore_pipe_freeze(_watchdog_pipe);
   _watchdog_thread = ecore_thread_feedback_run
     (_cb_watchdog_thread_pingpong,
      _cb_watchdog_thread_pingpong_reply,
      _cb_watchdog_thread_pingpong_end,
      NULL,
      NULL, EINA_TRUE);
}

E_API void
e_watchdog_end(void)
{
   if (_watchdog_thread) ecore_thread_cancel(_watchdog_thread);
   _watchdog_thread = NULL;
}

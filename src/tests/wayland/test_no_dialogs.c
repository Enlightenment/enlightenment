/* E_TEST_NO_DIALOGS: a test session never shows a dialog.
 *
 * window_focus with a direction that does not exist raises E's "Invalid
 * parameter" error dialog through e_util_dialog_show, so the dialog comes
 * from real compositor code. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "e_wl_testkit.h"

#define PROG "test-no-dialogs"

#define DIALOG_APP_ID "_error_dialog"

static Tk_Client *
_dialog_window(Tk *tk)
{
   static Tk_Client all[TK_MAX_CLIENTS];
   int n, i;

   n = tk_clients(tk, all, TK_MAX_CLIENTS);
   for (i = 0; i < n; i++)
     if (!strcmp(all[i].app_id, DIALOG_APP_ID)) return &all[i];
   return NULL;
}

int
main(void)
{
   Tk *tk;
   Tk_Client *c, *dia;
   unsigned int id;
   int i;

   tk = tk_connect(PROG);

   tk_toplevel_new(tk, "no-dialogs", "no dialogs", 200, 200);
   tk_settle(tk);
   c = tk_expect(tk, "no-dialogs");
   id = c->id;

   if (_dialog_window(tk))
     tk_fail(tk, "a dialog was already on screen before this test did "
                 "anything");

   tk_action(tk, id, "window_focus", "no-such-direction");
   tk_settle(tk);

   /* Dialogs are built from the main loop: give it several iterations. */
   for (i = 0; i < 10; i++)
     {
        dia = _dialog_window(tk);
        if (dia)
          tk_fail(tk, "an error dialog appeared (app_id '%s', title '%s', "
                      "%dx%d+%d+%d) with E_TEST_NO_DIALOGS set",
                  dia->app_id, dia->title, dia->w, dia->h, dia->x, dia->y);
        tk_sync(tk);
     }

   c = tk_expect(tk, "no-dialogs");
   if (!(c->states & WL_TEST_CLIENT_STATE_FOCUSED))
     tk_fail(tk, "the test's own window lost focus after an action that would "
                 "have raised a dialog");

   printf(PROG ": ok - no dialog, focus undisturbed\n");

   tk_disconnect(tk);
   return 0;
}

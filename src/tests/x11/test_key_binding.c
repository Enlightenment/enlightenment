/* A key binding acts on the focused window: the test profile binds
 * Super+i to window_maximized_toggle. The keys come from the X server,
 * through E's key grab, as a user's would. */
#include "x11_testkit.h"

#define WAIT_SECONDS 10.0

static void
_super_i(void)
{
   if ((!ecore_x_test_fake_key_down("Super_L")) ||
       (!ecore_x_test_fake_key_press("i")) ||
       (!ecore_x_test_fake_key_up("Super_L")))
     xtk_fail("cannot send keys");
}

static void
_wait_maximized(Evas_Object *win, Eina_Bool maximized, const char *what)
{
   double end = ecore_time_get() + WAIT_SECONDS;

   ecore_x_sync();
   while (elm_win_maximized_get(win) != maximized)
     {
        if (ecore_time_get() > end) xtk_fail("%s", what);
        ecore_main_loop_iterate();
        usleep(10 * 1000);
     }
}

int
main(int argc, char **argv)
{
   Evas_Object *win;
   double end;

   xtk_init(argc, argv, "test-x11-key-binding");
   win = elm_win_util_standard_add("key-binding", "key binding");
   elm_win_autodel_set(win, EINA_FALSE);
   evas_object_resize(win, 300, 200);
   evas_object_show(win);
   end = ecore_time_get() + WAIT_SECONDS;
   while (!elm_win_focus_get(win))
     {
        if (ecore_time_get() > end) xtk_fail("the window never got the focus");
        ecore_main_loop_iterate();
        usleep(10 * 1000);
     }

   _super_i();
   _wait_maximized(win, EINA_TRUE, "Super+i did not maximize the window");
   _super_i();
   _wait_maximized(win, EINA_FALSE, "Super+i again did not restore the window");

   printf("test-x11-key-binding: ok\n");
   xtk_shutdown(win);
   return 0;
}

#include "e.h"
#include "e_kbd_send.h"

#ifdef HAVE_WAYLAND
#include "input-method-unstable-v1-client-protocol.h"
#include "text-input-unstable-v1-client-protocol.h"


#endif

#ifndef HAVE_WAYLAND_ONLY
static const char *
_string_to_keysym(const char *str)
{
   if (e_comp_util_has_x())
     {
        int glyph;

        /* utf8 -> glyph id (unicode - ucs4) */
        glyph = 0;
        evas_string_char_next_get(str, 0, &glyph);
        if (glyph <= 0) return NULL;
        /* glyph id -> keysym */
        if (glyph > 0xff) glyph |= 0x1000000;
        return ecore_x_keysym_string_get(glyph);
     }
   else
     {
        return NULL;
     }
}
#endif

EAPI void
e_kbd_send_string_press(const char *str EINA_UNUSED, Kbd_Mod mod EINA_UNUSED)
{
#ifndef HAVE_WAYLAND_ONLY
   if (e_comp_util_has_x())
     {
        const char *key = NULL;

        key = _string_to_keysym(str);
        if (!key) return;
        e_kbd_send_keysym_press(key, mod);
     }
#endif
}

EAPI void
e_kbd_send_keysym_press(const char *key EINA_UNUSED, Kbd_Mod mod EINA_UNUSED)
{
#ifndef HAVE_WAYLAND_ONLY
   if (e_comp_util_has_x())
     {
        if (mod & KBD_MOD_CTRL) ecore_x_test_fake_key_down("Control_L");
        if (mod & KBD_MOD_ALT)  ecore_x_test_fake_key_down("Alt_L");
        if (mod & KBD_MOD_WIN)  ecore_x_test_fake_key_down("Super_L");
        ecore_x_test_fake_key_press(key);
        if (mod & KBD_MOD_WIN)  ecore_x_test_fake_key_up("Super_L");
        if (mod & KBD_MOD_ALT)  ecore_x_test_fake_key_up("Alt_L");
        if (mod & KBD_MOD_CTRL) ecore_x_test_fake_key_up("Control_L");
     }
   else
     {
/*
        struct wl_resource *res;
        uint32_t serial, *end, *k, keycode, timestamp;
        xkb_keysym_t keysym;
        Eina_List *l;

        timestamp = ecore_loop_time_get() * 1000.0;
        keysym = xkb_keysym_from_name(key, XKB_KEYSYM_NO_FLAGS);
        keycode = xkb_keymap_key_by_name(keymap, key);
        serial = wl_display_next_serial(e_comp_wl->wl.disp);
        EINA_LIST_FOREACH(e_comp_wl->kbd.focused, l, res)
          {
             //keycode = KEYCOADE_ctrl
             //e_comp_wl_input_keyboard_state_update(keycode, EINA_TRUE);
             //if (mod & KBD_MOD_CTRL) ecore_x_test_fake_key_down("Control_L");
             //if (mod & KBD_MOD_ALT)  ecore_x_test_fake_key_down("Alt_L");
             //if (mod & KBD_MOD_WIN)  ecore_x_test_fake_key_down("Super_L");
             wl_keyboard_send_key(res, serial, timestamp,
                                  keycode, WL_KEYBOARD_KEY_STATE_PRESSED);
          }
        serial = wl_display_next_serial(e_comp_wl->wl.disp);
        EINA_LIST_FOREACH(e_comp_wl->kbd.focused, l, res)
          {
             wl_keyboard_send_key(res, serial, timestamp,
                                  keycode, WL_KEYBOARD_KEY_STATE_RELEASED);
             //if (mod & KBD_MOD_WIN)  ecore_x_test_fake_key_up("Super_L");
             //if (mod & KBD_MOD_ALT)  ecore_x_test_fake_key_up("Alt_L");
             //if (mod & KBD_MOD_CTRL) ecore_x_test_fake_key_up("Control_L");
          }
 */
     }
#endif
}

EAPI void
e_kbd_send_init(void)
{
#ifndef HAVE_WAYLAND_ONLY
   if (e_comp_util_has_x())
     {
        return;
     }
#endif
#ifdef HAVE_WAYLAND
   if (!e_comp_util_has_x())
     {
     }
#endif
}


EAPI void
e_kbd_send_shutdown(void)
{
#ifndef HAVE_WAYLAND_ONLY
   if (e_comp_util_has_x())
     {
        return;
     }
#endif
#ifdef HAVE_WAYLAND
   if (!e_comp_util_has_x())
     {
     }
#endif
}

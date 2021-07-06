#include "e.h"

E_API int E_EVENT_AUTH_FPRINT_AVAILABLE = 0;
E_API int E_EVENT_AUTH_FPRINT_STATUS = 0;

static Eldbus_Connection *conn = NULL;

/////////////////////////////////////////////////////////////////////////////

EINTERN int
e_auth_init(void)
{
   E_EVENT_AUTH_FPRINT_AVAILABLE = ecore_event_type_new();
   E_EVENT_AUTH_FPRINT_STATUS = ecore_event_type_new();

   conn = eldbus_connection_get(ELDBUS_CONNECTION_TYPE_SYSTEM);
   return 1;
}

EINTERN int
e_auth_shutdown(void)
{
   if (conn) eldbus_connection_unref(conn);
   conn = NULL;
   return 1;
}

/////////////////////////////////////////////////////////////////////////////

E_API int
e_auth_begin(char *passwd)
{
   char buf[PATH_MAX];
   Ecore_Exe *exe = NULL;
   int ret = 0;
   size_t pwlen, buflen = 0;

   pwlen = strlen(passwd);

   snprintf(buf, sizeof(buf),
            "%s/enlightenment/utils/enlightenment_ckpasswd pw",
            e_prefix_lib_get());
   exe = ecore_exe_pipe_run(buf, ECORE_EXE_PIPE_WRITE, NULL);
   if (!exe) goto out;
   snprintf(buf, sizeof(buf), "pw %s", passwd);
   buflen = strlen(buf);
   if (ecore_exe_send(exe, buf, buflen) != EINA_TRUE) goto out;
   ecore_exe_close_stdin(exe);

   ret = ecore_exe_pid_get(exe);
   if (ret == -1)
     {
        ret = 0;
        goto out;
     }

   exe = NULL;

out:
   if (exe) ecore_exe_free(exe);

   e_util_memclear(passwd, pwlen);
   e_util_memclear(buf, buflen);
   return ret;
}

E_API int
e_auth_polkit_begin(char *passwd, const char *cookie, unsigned int uid)
{
   char buf[PATH_MAX];
   Ecore_Exe *exe = NULL;
   int ret = 0;
   size_t pwlen, buflen = 0;

   pwlen = strlen(passwd);

   snprintf(buf, sizeof(buf),
            "%s/enlightenment/utils/enlightenment_ckpasswd pk",
            e_prefix_lib_get());
   exe = ecore_exe_pipe_run(buf, ECORE_EXE_PIPE_WRITE, NULL);
   if (!exe) goto out;
   snprintf(buf, sizeof(buf), "%s %u %s", cookie, uid, passwd);
   buflen = strlen(buf);
   if (ecore_exe_send(exe, buf, buflen) != EINA_TRUE) goto out;
   ecore_exe_close_stdin(exe);

   ret = ecore_exe_pid_get(exe);
   if (ret == -1)
     {
        ret = 0;
        goto out;
     }
   exe = NULL;

out:
   if (exe) ecore_exe_free(exe);

   e_util_memclear(passwd, pwlen);
   e_util_memclear(buf, buflen);
   return ret;
}

/////////////////////////////////////////////////////////////////////////////

static Eldbus_Object *obj_fprint = NULL;
static Eldbus_Proxy *proxy_fprint = NULL;

static Eldbus_Object *obj_fprint_device = NULL;
static Eldbus_Proxy *proxy_fprint_device = NULL;
static E_Auth_Fprint_Type finger_type = E_AUTH_FPRINT_TYPE_UNKNOWN;
static const char *finger_name = NULL;
static const char *user_name = NULL;

static void
_cb_event_free(void *data EINA_UNUSED, void *event)
{
   free(event);
}

static void
_cb_verify_start(void *data EINA_UNUSED, const Eldbus_Message *m,
                 Eldbus_Pending *p EINA_UNUSED)
{
   const char *name = NULL, *text = NULL;

   printf("FP: verify start...\n");
   if (eldbus_message_error_get(m, &name, &text))
     {
        fprintf(stderr, "FP: Fprint err: %s %s\n", name, text);
        return;
     }
}

static void
_cb_verify_stop(void *data EINA_UNUSED, const Eldbus_Message *m EINA_UNUSED,
                Eldbus_Pending *p EINA_UNUSED)
{
   Eldbus_Message *m2;
   Eldbus_Message_Iter *iter;

   printf("FP: verify stop.... finger_name=%s\n", finger_name ? finger_name : "NULL");
   if (!finger_name) finger_name = eina_stringshare_add("right-index-finger");
   m2 = eldbus_proxy_method_call_new(proxy_fprint_device, "VerifyStart");
   if (m2)
     {
        iter = eldbus_message_iter_get(m2);
        eldbus_message_iter_basic_append(iter, 's', finger_name);
        eldbus_proxy_send(proxy_fprint_device, m2, _cb_verify_start, NULL, -1);
     }
}

static void
_verify_begin(void)
{
   Eldbus_Message *m2;

   printf("FP: verify begin...\n");
   m2 = eldbus_proxy_method_call_new(proxy_fprint_device, "VerifyStop");
   if (m2)
     {
        eldbus_proxy_send(proxy_fprint_device, m2, _cb_verify_stop, NULL, -1);
     }
}

static void
_cb_verify(void *data EINA_UNUSED, const Eldbus_Message *m)
{
   Eina_Bool val = EINA_FALSE;
   const char *txt = NULL;
   const char *name = NULL, *text = NULL;
   E_Event_Auth_Fprint_Status *ev;

   printf("FP: verify ...\n");
   if (eldbus_message_error_get(m, &name, &text))
     {
        fprintf(stderr, "FP: Fprint err: %s %s\n", name, text);
        return;
     }
   if (!eldbus_message_arguments_get(m, "sb", &txt, &val)) return;
   printf("FP:   verify = [%s] %i\n", txt, val);
   if (!txt) return;

   ev = calloc(1, sizeof(E_Event_Auth_Fprint_Status));
   if (!ev) return;

   if      (!strcmp(txt, "verify-match")) ev->status = E_AUTH_FPRINT_STATUS_AUTH;
   else if (!strcmp(txt, "verify-no-match")) ev->status = E_AUTH_FPRINT_STATUS_NO_AUTH;
   else if (!strcmp(txt, "verify-swipe-too-short")) ev->status = E_AUTH_FPRINT_STATUS_SHORT_SWIPE;
   else if (!strcmp(txt, "verify-finger-not-centered")) ev->status = E_AUTH_FPRINT_STATUS_NO_CENTER;
   else if (!strcmp(txt, "verify-remove-and-retry")) ev->status = E_AUTH_FPRINT_STATUS_REMOVE_RETRY;
   else if (!strcmp(txt, "verify-retry-scan")) ev->status = E_AUTH_FPRINT_STATUS_RETRY;
   else if (!strcmp(txt, "verify-disconnected")) ev->status = E_AUTH_FPRINT_STATUS_DISCONNECT;
   else if (!strcmp(txt, "verify-unknown-error")) ev->status = E_AUTH_FPRINT_STATUS_ERROR;
   else ev->status = E_AUTH_FPRINT_STATUS_ERROR;
   ecore_event_add(E_EVENT_AUTH_FPRINT_STATUS, ev, _cb_event_free, NULL);
   // reset the verify and try again if verify end and NOT an auth ok
   if ((val) && // end of verify
       (ev->status != E_AUTH_FPRINT_STATUS_AUTH))
     {
        _verify_begin();
     }
   if (ev->status == E_AUTH_FPRINT_STATUS_AUTH)
     {
        Eldbus_Message *m2;

        m2 = eldbus_proxy_method_call_new(proxy_fprint_device, "Release");
        eldbus_proxy_send(proxy_fprint_device, m2, NULL, NULL, -1);
     }
}

static void
_cb_list_enrolled_fingers(void *data EINA_UNUSED, const Eldbus_Message *m,
                          Eldbus_Pending *p EINA_UNUSED)
{
   Eldbus_Message_Iter *array = NULL;
   const char *name = NULL, *text = NULL;

   printf("FP: list fingers...\n");
   if (eldbus_message_error_get(m, &name, &text))
     {
        fprintf(stderr, "FP: Fprint err: %s %s\n", name, text);
        return;
     }
   printf("FP: list fingers...\n");
   if (eldbus_message_arguments_get(m, "as", &array))
     {
        char *txt = NULL;

        printf("FP:  ...\n");
        while (eldbus_message_iter_get_and_next(array, 's', &txt))
          {
             eina_stringshare_replace(&finger_name, txt);
             printf("FP:  first finger is [%s]\n", txt);
             if (finger_type != E_AUTH_FPRINT_TYPE_UNKNOWN)
               {
                  E_Event_Auth_Fprint_Available *ev = calloc(1, sizeof(E_Event_Auth_Fprint_Available));
                  if (ev)
                    {
                       ev->type = finger_type;
                       ecore_event_add(E_EVENT_AUTH_FPRINT_AVAILABLE, ev,
                                       _cb_event_free, NULL);
                    }
               }
             _verify_begin();
             break;
          }
        // call VeriftyStart 'first finger found'
     }
}

static void
_cb_claim(void *data EINA_UNUSED, const Eldbus_Message *m EINA_UNUSED,
          Eldbus_Pending *p EINA_UNUSED)
{
   Eldbus_Proxy *proxy = proxy_fprint_device;
   Eldbus_Message *m2;
   Eldbus_Message_Iter *iter;
   const char *name = NULL, *text = NULL;

   printf("FP: claim\n");
   if (eldbus_message_error_get(m, &name, &text))
     {
        fprintf(stderr, "FP: Fprint err: %s %s\n", name, text);
        return;
     }
   // ListEnrolledFingrs '$USER' -> "as"
   printf("FP: claim ok\n");
   m2 = eldbus_proxy_method_call_new(proxy, "ListEnrolledFingers");
   iter = eldbus_message_iter_get(m2);
   eldbus_message_iter_basic_append(iter, 's', user_name);
   eldbus_proxy_send(proxy, m2, _cb_list_enrolled_fingers, NULL, -1);
}

static void
_cb_prop_entry(void *data EINA_UNUSED, const void *key, Eldbus_Message_Iter *var)
{
   const char *skey = key;

   if (!strcmp(skey, "scan-type"))
     {
        const char *val = NULL;
        if (eldbus_message_iter_arguments_get(var, "s", &val))
          {
             if      (!strcmp(val, "press")) finger_type = E_AUTH_FPRINT_TYPE_PRESS;
             else if (!strcmp(val, "swipe")) finger_type = E_AUTH_FPRINT_TYPE_SWIPE;
             printf("FP: type = %s\n", val);
             if (finger_name)
               {
                  E_Event_Auth_Fprint_Available *ev =
                    calloc(1, sizeof(E_Event_Auth_Fprint_Available));
                  if (ev)
                    {
                       ev->type = finger_type;
                       ecore_event_add(E_EVENT_AUTH_FPRINT_AVAILABLE, ev,
                                       _cb_event_free, NULL);
                    }
               }
          }
     }
}

static void
_cb_props(void *data EINA_UNUSED, const Eldbus_Message *m,
          Eldbus_Pending *pending EINA_UNUSED)
{
   Eldbus_Message_Iter *array = NULL;

   if (eldbus_message_error_get(m, NULL, NULL)) return;

   if (eldbus_message_arguments_get(m, "a{sv}", &array))
     eldbus_message_iter_dict_iterate(array, "sv", _cb_prop_entry, NULL);
}

static void
_cb_get_default_device(void *data EINA_UNUSED, const Eldbus_Message *m,
                       Eldbus_Pending *p EINA_UNUSED)
{
   Eldbus_Object *obj;
   Eldbus_Proxy *proxy;
   const char *dev = NULL;
   const char *name = NULL, *text = NULL;

   printf("FP: get default device...\n");
   if (eldbus_message_error_get(m, &name, &text))
     {
        fprintf(stderr, "FP: Fprint err: %s %s\n", name, text);
        return;
     }
   if (!eldbus_message_arguments_get(m, "o", &dev)) return;
   printf("FP: dev = %s\n", dev);
   obj = eldbus_object_get(conn, "net.reactivated.Fprint", dev);
   if (obj)
     {
        obj_fprint_device = obj;

        printf("FP: 22 obj = %p\n", obj);
        proxy = eldbus_proxy_get(obj, "net.reactivated.Fprint.Device");
        if (proxy)
          {
             Eldbus_Message *m2;
             Eldbus_Message_Iter *iter;

             printf("FP: 22 proxy = %p\n", proxy);
             proxy_fprint_device = proxy;

             eldbus_proxy_property_get_all(proxy, _cb_props, NULL);

             eldbus_proxy_signal_handler_add(proxy, "VerifyStatus",
                                             _cb_verify, NULL);
             // Claim '$USER'
             m2 = eldbus_proxy_method_call_new(proxy, "Claim");
             iter = eldbus_message_iter_get(m2);
             eldbus_message_iter_basic_append(iter, 's', user_name);
             eldbus_proxy_send(proxy, m2, _cb_claim, NULL, -1);
          }
     }
}

E_API void
e_auth_fprint_begin(const char *user)
{
   Eldbus_Object *obj;
   Eldbus_Proxy *proxy;

   printf("FP: e_auth_fprint_begin\n");
   if (conn)
     {
        eina_stringshare_replace(&user_name, user);
        obj = eldbus_object_get(conn, "net.reactivated.Fprint",
                                "/net/reactivated/Fprint/Manager");
        printf("FP: obj=%p\n", obj);
        if (obj)
          {
             obj_fprint = obj;

             proxy = eldbus_proxy_get(obj, "net.reactivated.Fprint.Manager");
             printf("FP: proxy=%p\n", proxy);
             if (proxy)
               {
                  Eldbus_Message *m;

                  proxy_fprint = proxy;
                  m = eldbus_proxy_method_call_new(proxy, "GetDefaultDevice");
                  eldbus_proxy_send(proxy, m, _cb_get_default_device, NULL, -1);
               }
          }
     }
}

E_API void
e_auth_fprint_end(void)
{
   if (conn)
     {
        if (obj_fprint_device)
          {
             Eldbus_Message *m2;

             m2 = eldbus_proxy_method_call_new(proxy_fprint_device, "Release");
             eldbus_proxy_send(proxy_fprint_device, m2, NULL, NULL, -1);
             if (proxy_fprint_device)
               {
                  eldbus_proxy_unref(proxy_fprint_device);
                  proxy_fprint_device = NULL;
               }
             eldbus_object_unref(obj_fprint_device);
             obj_fprint_device = NULL;
          }
        if (obj_fprint)
          {
             if (proxy_fprint)
               {
                  eldbus_proxy_unref(proxy_fprint);
                  proxy_fprint = NULL;
               }
             eldbus_object_unref(obj_fprint);
             obj_fprint = NULL;
          }
     }
   eina_stringshare_replace(&finger_name, NULL);
   eina_stringshare_replace(&user_name, NULL);
   finger_type = E_AUTH_FPRINT_TYPE_UNKNOWN;
}

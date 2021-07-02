#ifndef E_AUTH_H
#define E_AUTH_H

typedef enum _E_Auth_Fprint_Type
{
   E_AUTH_FPRINT_TYPE_UNKNOWN,
   E_AUTH_FPRINT_TYPE_PRESS,
   E_AUTH_FPRINT_TYPE_SWIPE
} E_Auth_Fprint_Type;

typedef enum _E_Auth_Fprint_Status
{
   E_AUTH_FPRINT_STATUS_AUTH,
   E_AUTH_FPRINT_STATUS_NO_AUTH,
   E_AUTH_FPRINT_STATUS_SHORT_SWIPE,
   E_AUTH_FPRINT_STATUS_NO_CENTER,
   E_AUTH_FPRINT_STATUS_REMOVE_RETRY,
   E_AUTH_FPRINT_STATUS_RETRY,
   E_AUTH_FPRINT_STATUS_DISCONNECT,
   E_AUTH_FPRINT_STATUS_ERROR
} E_Auth_Fprint_Status;

typedef struct _E_Event_Auth_Fprint_Available
{
   E_Auth_Fprint_Type type;
} E_Event_Auth_Fprint_Available;

typedef struct _E_Event_Auth_Fprint_Status
{
   E_Auth_Fprint_Status status;
} E_Event_Auth_Fprint_Status;

extern E_API int E_EVENT_AUTH_FPRINT_AVAILABLE;
extern E_API int E_EVENT_AUTH_FPRINT_STATUS;

EINTERN int e_auth_init(void);
EINTERN int e_auth_shutdown(void);

E_API int e_auth_begin(char *passwd);
E_API int e_auth_polkit_begin(char *passwd, const char *cookie, unsigned int uid);

static inline int
e_auth_hash_djb2(const char *key, int len)
{
   unsigned int hash_num = 5381;
   const unsigned char *ptr;

   if (!key) return 0;
   for (ptr = (unsigned char *)key; len; ptr++, len--)
     hash_num = ((hash_num << 5) + hash_num) ^ *ptr; /* hash * 33 ^ c */

   return (int)hash_num;
}

E_API void e_auth_fprint_begin(const char *user);
E_API void e_auth_fprint_end(void);

#endif

#ifndef _E_NOTIFICATION_H
#define _E_NOTIFICATION_H

#include <EDBus.h>
#include <Eina.h>

typedef enum _E_Notification_Notify_Urgency
{
  E_NOTIFICATION_NOTIFY_URGENCY_LOW,
  E_NOTIFICATION_NOTIFY_URGENCY_NORMAL,
  E_NOTIFICATION_NOTIFY_URGENCY_CRITICAL
} E_Notification_Notify_Urgency;

typedef enum _E_Notification_Closed_Reason
{
  E_NOTIFICATION_CLOSED_REASON_EXPIRED, /** The notification expired. */
  E_NOTIFICATION_CLOSED_REASON_DISMISSED, /** The notification was dismissed by the user. */
  E_NOTIFICATION_CLOSED_REASON_REQUESTED, /** The notification was closed by a call to CloseNotification method. */
  E_NOTIFICATION_CLOSED_REASON_UNDEFINED /** Undefined/reserved reasons. */
} E_Notification_Closed_Reason;

typedef struct _E_Notification_Notify
{
   const char *app_name;
   unsigned replaces_id;
   const char *sumary;
   const char *body;
   int timeout;
   E_Notification_Notify_Urgency urgency;
   struct
   {
      const char *icon;
      const char *icon_path;
      struct
      {
         int width;
         int height;
         int rowstride;
         Eina_Bool has_alpha;
         int bits_per_sample;
         int channels;
         unsigned char *data;
         int data_size;
      } raw;
   } icon;
} E_Notification_Notify;

typedef unsigned int (*E_Notification_Notify_Cb)(void *data, E_Notification_Notify *n);
typedef void (*E_Notification_Close_Cb)(void *data, unsigned int id);

typedef struct _E_Notification_Server_Info
{
   const char *name;
   const char *vendor;
   const char *version;
   const char *spec_version;
   const char *capabilities[];
} E_Notification_Server_Info;

EAPI void e_notification_notify_free(E_Notification_Notify *notify);
EAPI void e_notification_stop(void);
EAPI Eina_Bool e_notification_start(E_Notification_Notify_Cb notification_cb, E_Notification_Close_Cb close_cb, const E_Notification_Server_Info *server_info, const void *data);
EAPI void e_notification_notification_closed(unsigned id, E_Notification_Closed_Reason reason);
EAPI Evas_Object *e_notification_raw_image_get(Evas *evas, E_Notification_Notify *notify);

#endif

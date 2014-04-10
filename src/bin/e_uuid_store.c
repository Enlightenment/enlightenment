/* vim:ts=8 sw=3 sts=3 expandtab cino=>5n-3f0^-2{2(0W1st0
 */

/* 1. Create mmaped memory blob, name the memory object
 * 2. Fill in table and keep it up-to-date
 * 3. (optional) Write the whole blob into a file on disk for later use)
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include <Eina.h>

#include <e.h>

/* Use anonymous mapping if we don't want a persistent file on the disk */
#define OBJECT_NAME "/e_uuid_store"
#define TABLE_SIZE 10*eina_cpu_page_size()

struct uuid_store *store;

void
e_uuid_dump(void)
 {
   struct uuid_table *table;
   struct table_entry *entry;
   Eina_List *l;

   if (store == NULL) return;

   table = store->table;
   if (table == NULL) return;

   INF("Dump UUID table:");
   EINA_LIST_FOREACH(table->entries, l, entry)
     INF("UUID %li, x=%i, y=%i, width=%i, heigth=%i", entry->uuid, entry->x, entry->y, entry->width, entry->heigth );
 }

EINTERN int
e_uuid_store_init(void)
 {
   /* FIXME think about refcounting here */
   eina_init();

   store = calloc(1, sizeof(struct uuid_store));
   if (store == NULL) return 0;

   store->shmfd = shm_open(OBJECT_NAME, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
   if (store->shmfd < 0)
    {
       ERR("shm_open failed");
       return 0;
    }

   /* Adjust in memory blob to our given table size */
   /* FIXME: How can we make sure we have the right size for our given table? */
   ftruncate(store->shmfd, TABLE_SIZE);

   store->table = (struct uuid_table *)mmap(NULL, TABLE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, store->shmfd, 0);
   if (store->table == NULL)
    {
       ERR("mmap failed");
       return 0;
    }

   INF("mmaped blob with size %i created", TABLE_SIZE);

   if (store->table->version)
     INF("UUID table with version %i", store->table->version);

   INF("UUID table with %i entries", store->table->entry_count);

   return 1;
 }

EINTERN int
e_uuid_store_shutdown(void)
 {
   /* Cleanup for shutdown */
   if (shm_unlink(OBJECT_NAME) != 0)
    {
       ERR("shm_unlink failed");
       return 0;
    }

   free(store);
   eina_shutdown();
   return 1;
 }

Eina_Bool
e_uuid_store_reload(void)
 {
   /* After crash reload the table with its POSIX object name from memory */
   return EINA_FALSE;
 }

Eina_Bool
e_uuid_store_entry_del(long uuid)
 {
   struct uuid_table *table;
   struct table_entry *entry;
   Eina_List *l;

   if (store == NULL) return EINA_FALSE;

   table = store->table;
   if (table == NULL) return EINA_FALSE;

   /* Search through uuid list and delete if found */
   EINA_LIST_FOREACH(table->entries, l, entry)
    {
      if (entry->uuid == uuid)
       {
         table->entries = eina_list_remove(table->entries, entry);
         free(entry);
         DBG("Removed entry with UUID %li", uuid);
         return EINA_TRUE;
       }
    }
   DBG("NOT removed entry with UUID %li. Entry not found.", uuid);
   return EINA_FALSE;
 }

/* FIXME: Think about having _add  and _update functions instead only update */

Eina_Bool
e_uuid_store_entry_update(long uuid, E_Client *ec)
 {
   struct uuid_table *table;
   struct table_entry *entry;
   Eina_List *l;

   if (store == NULL) return EINA_FALSE;

   table = store->table;
   if (table == NULL) return EINA_FALSE;

   /* Search through uuid list if it already exist if yes update */
   EINA_LIST_FOREACH(table->entries, l, entry)
    {
      if (entry->uuid == uuid)
       {
         entry->x = ec->x;
         entry->y = ec->y;
         entry->width = ec->client.w;
         entry->heigth = ec->client.h;
         DBG("Updated entry with UUID %li", uuid);
         return EINA_TRUE;
       }
    }

   /* We do not have this UUID in the table yet. Create it */
   entry = calloc(1, sizeof(struct table_entry));
   entry->uuid = uuid;
   entry->x = ec->x;
   entry->y = ec->y;
   entry->width = ec->client.w;
   entry->heigth = ec->client.h;
   table->entries = eina_list_append(table->entries, entry);
   table->entry_count = eina_list_count(table->entries);
   DBG("Created entry with UUID %li", uuid);

   return EINA_TRUE;
 }

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
   int i;
   char uuid_string[37];

   if (store == NULL) return;

   table = store->table;
   if (table == NULL) return;

   INF("Dump UUID table:");
   for (i = 0; i < UUID_STORE_TABLE_SIZE -1; i++)
    {
       if (uuid_is_null(table->entries[i].uuid)) continue;
       uuid_unparse(table->entries[i].uuid, uuid_string);
       INF("UUID %s, x=%i, y=%i, width=%i, heigth=%i", uuid_string, table->entries[i].x,
                                                        table->entries[i].y, table->entries[i].width,
                                                        table->entries[i].heigth);
    }
 }

/**
 * Initialize the UUID store
 *
 * @returns 1 if init was successful, 0 on failure
 */
EINTERN int
e_uuid_store_init(void)
 {
   /* FIXME think about refcounting here */
   eina_init();

   store = calloc(1, sizeof(struct uuid_store));
   if (store == NULL) return 0;

   /* Try to open existing SHM object */
   store->shmfd = shm_open(OBJECT_NAME, O_RDWR, S_IRWXU | S_IRWXG);
   if (store->shmfd < 0 && errno == ENOENT)
    {
       INF("shm_open failed to open an existing file %s", OBJECT_NAME);
       if (!e_uuid_store_reload()) return 0;
    }
   else if (store->shmfd < 0)
    {
       INF("shm_open failed");
       return 0;
    }

   /* Adjust in memory blob to our given table size */
   /* FIXME: How can we make sure we have the right size for our given table? */
   if (ftruncate(store->shmfd, TABLE_SIZE) < 0)
     {
        ERR("ftruncate failed: %s", strerror(errno));
        return 0;
     }

   store->table = (struct uuid_table *)mmap(NULL, TABLE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, store->shmfd, 0);
   if (store->table == NULL)
    {
       ERR("mmap failed");
       return 0;
    }

   INF("mmaped blob with size %i created", TABLE_SIZE);

   if (store->table->version)
     INF("UUID table with version %i", store->table->version);
   else
     store->table->version = 1;

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

   close(store->shmfd);
   free(store);
   eina_shutdown();
   return 1;
 }

Eina_Bool
e_uuid_store_reload(void)
 {
   /* After crash reload the table with its POSIX object name from memory */
   store->shmfd = shm_open(OBJECT_NAME, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG);
   if (store->shmfd < 0)
    {
       INF("shm_open failed");
       return EINA_FALSE;
    }
   return EINA_TRUE;
 }

Eina_Bool
e_uuid_store_entry_del(uuid_t uuid)
 {
   struct uuid_table *table;
   int i;
   char uuid_string[37];

   if (store == NULL) return EINA_FALSE;

   table = store->table;
   if (table == NULL) return EINA_FALSE;

   /* Search through uuid list and delete if found */
   for (i = 0; i < UUID_STORE_TABLE_SIZE -1; i++)
    {
      if (!uuid_compare(table->entries[i].uuid, uuid))
       {
         uuid_clear(table->entries[i].uuid);
         table->entries[i].x = 0;
         table->entries[i].x = 0;
         table->entries[i].width = 0;
         table->entries[i].heigth = 0;
         table->entry_count--;
         uuid_unparse(uuid, uuid_string);
         DBG("Removed entry with UUID %s", uuid_string);
         return EINA_TRUE;
       }
    }
   uuid_unparse(uuid, uuid_string);
   DBG("NOT removed entry with UUID %s. Entry not found.", uuid_string);
   return EINA_FALSE;
 }

/* FIXME: Think about having _add  and _update functions instead only update */

Eina_Bool
e_uuid_store_entry_update(uuid_t uuid, E_Client *ec)
 {
   struct uuid_table *table;
   int i, index = -1;
   char uuid_string[37];

   if (store == NULL) return EINA_FALSE;

   table = store->table;
   if (table == NULL) return EINA_FALSE;

   /* Search through uuid list if it already exist if yes update */
   for (i = 0; i < UUID_STORE_TABLE_SIZE -1; i++)
    {
      if (!uuid_compare(table->entries[i].uuid, uuid))
       {
         table->entries[i].x = ec->x;
         table->entries[i].y = ec->y;
         table->entries[i].width = ec->client.w;
         table->entries[i].heigth = ec->client.h;
         uuid_unparse(uuid, uuid_string);
         DBG("Updated entry with UUID %s", uuid_string);
         return EINA_TRUE;
       }
    }

   /* Find first empty entry */
   for (i = 0; i < UUID_STORE_TABLE_SIZE -1; i++)
    {
      if (uuid_is_null(table->entries[i].uuid))
        index = i;
    }

   if (index == -1)
     {
        ERR("UUID table full");
        return EINA_FALSE;
     }

   /* We do not have this UUID in the table yet. Create it */
   table->entries[index].x = ec->x;
   table->entries[index].y = ec->y;
   table->entries[index].width = ec->client.w;
   table->entries[index].heigth = ec->client.h;
   uuid_copy(table->entries[index].uuid, uuid);
   table->entry_count++;
   uuid_unparse(table->entries[index].uuid, uuid_string);
   DBG("Created entry with UUID %s", uuid_string);

   return EINA_TRUE;
 }

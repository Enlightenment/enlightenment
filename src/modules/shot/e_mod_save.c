#include "e_mod_main.h"

static int          quality = 90;
static int          screen = -1;
static Evas_Object *o_fsel = NULL;
static E_Dialog    *fsel_dia = NULL;

static void _file_select_ok_cb(void *data EINA_UNUSED, E_Dialog *dia);
static void _file_select_cancel_cb(void *data EINA_UNUSED, E_Dialog *dia);

typedef struct
{
   char *path, *outfile;
   void *data;
   int w, h, stride, quality;
   size_t size;
   int fd;
} Rgba_Writer_Data;

static void
_rgba_data_free(Rgba_Writer_Data *rdata)
{
   free(rdata->path);
   free(rdata->outfile);
   free(rdata->data);
   close(rdata->fd);
   free(rdata);
}

static void
_cb_rgba_writer_do(void *data, Ecore_Thread *th EINA_UNUSED)
{
   Rgba_Writer_Data *rdata = data;
   if (write(rdata->fd, rdata->data, rdata->size) < 0)
     ERR("Write of shot rgba data failed");
}

static void
_cb_rgba_writer_done(void *data, Ecore_Thread *th EINA_UNUSED)
{
   Rgba_Writer_Data *rdata = data;
   char buf[PATH_MAX];

   if (rdata->outfile)
     snprintf(buf, sizeof(buf), "%s/%s/upload '%s' %i %i %i %i '%s'",
              e_module_dir_get(shot_module), MODULE_ARCH,
              rdata->path, rdata->w, rdata->h, rdata->stride,
              rdata->quality, rdata->outfile);
   else
     snprintf(buf, sizeof(buf), "%s/%s/upload '%s' %i %i %i %i",
              e_module_dir_get(shot_module), MODULE_ARCH,
              rdata->path, rdata->w, rdata->h, rdata->stride,
              rdata->quality);
   share_save(buf);
   _rgba_data_free(rdata);
}

static void
_cb_rgba_writer_cancel(void *data, Ecore_Thread *th EINA_UNUSED)
{
   Rgba_Writer_Data *rdata = data;
   _rgba_data_free(rdata);
}

void
save_to(const char *file)
{
   int fd;
   char tmpf[256] = "e-shot-rgba-XXXXXX";
   Eina_Tmpstr *path = NULL;
   int imw = 0, imh = 0, imstride;

   fd = eina_file_mkstemp(tmpf, &path);
   if (fd >= 0)
     {
        unsigned char *data = NULL;
        Rgba_Writer_Data *thdat = NULL;
        size_t size = 0;
        Evas_Object *img = preview_image_get();

        if (screen == -1)
          {
             if (img)
               {
                  int w = 0, h = 0;
                  int stride = evas_object_image_stride_get(img);
                  unsigned char *src_data = evas_object_image_data_get(img, EINA_FALSE);

                  evas_object_image_size_get(img, &w, &h);
                  if ((stride > 0) && (src_data) && (h > 0))
                    {
                       imw = w;
                       imh = h;
                       imstride = stride;
                       size = stride * h;
                       data = malloc(size);
                       if (data) memcpy(data, src_data, size);
                    }
               }
          }
        else
          {
             if (img)
               {
                  int w = 0, h = 0;
                  int stride = evas_object_image_stride_get(img);
                  unsigned char *src_data = evas_object_image_data_get(img, EINA_FALSE);

                  evas_object_image_size_get(img, &w, &h);
                  if ((stride > 0) && (src_data) && (h > 0))
                    {
                       Eina_List *l;
                       E_Zone *z = NULL;

                       EINA_LIST_FOREACH(e_comp->zones, l, z)
                         {
                            if (screen == (int)z->num) break;
                            z = NULL;
                         }
                       if (z)
                         {
                            size = z->w * z->h * 4;
                            data = malloc(size);
                            if (data)
                              {
                                 int y;
                                 unsigned char *s, *d;

                                 imw = z->w;
                                 imh = z->h;
                                 imstride = imw * 4;
                                 d = data;
                                 for (y = z->y; y < (z->y + z->h); y++)
                                   {
                                      s = src_data + (stride * y) + (z->x * 4);
                                      memcpy(d, s, z->w * 4);
                                      d += z->w * 4;
                                   }
                              }
                         }
                    }
               }
          }
        if (data)
          {
             thdat = calloc(1, sizeof(Rgba_Writer_Data));
             if (thdat)
               {
                  thdat->path = strdup(path);
                  if (file) thdat->outfile = strdup(file);
                  if ((thdat->path) &&
                      (((file) && (thdat->outfile)) ||
                       (!file)))
                    {
                       thdat->data = data;
                       thdat->size = size;
                       thdat->fd = fd;
                       thdat->w = imw;
                       thdat->h = imh;
                       thdat->stride = imstride;
                       thdat->quality = quality;
                       ecore_thread_run(_cb_rgba_writer_do,
                                        _cb_rgba_writer_done,
                                        _cb_rgba_writer_cancel, thdat);
                    }
                  else
                    {
                       free(thdat->path);
                       free(thdat->outfile);
                       free(thdat);
                       thdat = NULL;
                    }
               }
             else
               {
                  close(fd);
                  free(data);
               }
          }
        if (!thdat) close(fd);
        eina_tmpstr_del(path);
     }
   return;
}

static void
_file_select_ok_cb(void *data EINA_UNUSED, E_Dialog *dia)
{
   const char *file;

   dia = fsel_dia;
   file = e_widget_fsel_selection_path_get(o_fsel);
   if ((!file) || (!file[0]) ||
       ((!eina_str_has_extension(file, ".jpg")) &&
        (!eina_str_has_extension(file, ".png"))))
     {
        e_util_dialog_show
        (_("Error - Unknown format"),
            _("File has an unspecified extension.<ps/>"
              "Please use '.jpg' or '.png' extensions<ps/>"
              "only as other formats are not<ps/>"
              "supported currently."));
        return;
     }
   save_to(file);
   if (dia) e_util_defer_object_del(E_OBJECT(dia));
   preview_abort();
   fsel_dia = NULL;
}

static void
_file_select_cancel_cb(void *data EINA_UNUSED, E_Dialog *dia)
{
   if (dia) e_util_defer_object_del(E_OBJECT(dia));
   preview_abort();
   fsel_dia = NULL;
}

static void
_file_select_del_cb(void *d EINA_UNUSED)
{
   preview_abort();
   fsel_dia = NULL;
}

void
save_dialog_show(void)
{
   E_Dialog *dia;
   Evas_Object *o;
   Evas_Coord mw, mh;
   time_t tt;
   struct tm *tm;
   char buf[PATH_MAX];

   time(&tt);
   tm = localtime(&tt);
   if (quality == 100)
     strftime(buf, sizeof(buf), "shot-%Y-%m-%d_%H-%M-%S.png", tm);
   else
     strftime(buf, sizeof(buf), "shot-%Y-%m-%d_%H-%M-%S.jpg", tm);
   fsel_dia = dia = e_dialog_new(NULL, "E", "_e_shot_fsel");
   e_dialog_resizable_set(dia, EINA_TRUE);
   e_dialog_title_set(dia, _("Select screenshot save location"));
   o = e_widget_fsel_add(evas_object_evas_get(dia->win), "desktop", "/", 
                         buf, NULL, NULL, NULL, NULL, NULL, 1);
   e_object_del_attach_func_set(E_OBJECT(dia), _file_select_del_cb);
   e_widget_fsel_window_set(o, dia->win);
   o_fsel = o;
   evas_object_show(o);
   e_widget_size_min_get(o, &mw, &mh);
   e_dialog_content_set(dia, o, mw, mh);
   e_dialog_button_add(dia, _("Save"), NULL,
                       _file_select_ok_cb, NULL);
   e_dialog_button_add(dia, _("Cancel"), NULL,
                       _file_select_cancel_cb, NULL);
   elm_win_center(dia->win, 1, 1);
   o = evas_object_rectangle_add(evas_object_evas_get(dia->win));
   e_dialog_show(dia);
}

Eina_Bool
save_have(void)
{
   if (fsel_dia) return EINA_TRUE;
   return EINA_FALSE;
}

void
save_abort(void)
{
   E_FREE_FUNC(fsel_dia, e_object_del);
}

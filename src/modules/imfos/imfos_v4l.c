#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <libv4l2.h>

#include <Eeze.h>
#include <e.h>
#include "e_mod_main.h"

#include <Ecore.h>
#include <Evas.h>
#include "imfos_v4l.h"
//#include "imfos_face.h"
#include <e.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

static int _imfos_xioctl(int fh, int request, void *arg);
static Eina_Bool _imfos_v4l_image_transform(Evas_Object *cam, void *data, size_t size);
static void _imfos_v4l_takeshot(void *data, Ecore_Thread *thread);
static void _imfos_v4l_end(void *data, Ecore_Thread *thread);
static void _imfos_v4l_cancel(void *data, Ecore_Thread *thread);

static Ecore_Animator *_imfos_v4l_anim = NULL;

static int _width;
static int _height;
static int _size;
static void *_img = NULL;
static void *_img_y = NULL;
static int _2126[256];
static int _7152[256];
static int _0722[256];

struct buffer {
     void   *start;
     size_t length;
};


static int
_imfos_xioctl(int fh, int request, void *arg)
{
   int r;

   do
     {
        r = v4l2_ioctl(fh, request, arg);
     } while (r == -1 && ((errno == EINTR) || (errno == EAGAIN)));

   if (r == -1)
     {
        fprintf(stderr, "error %d, %s\n", errno, strerror(errno));
        return 1;
     }
   return 0;
}

static Eina_Bool
_imfos_v4l_frame_anim(void *data)
{
   _imfos_v4l_anim = NULL;
   evas_object_image_pixels_dirty_set(data, 1);
   return EINA_FALSE;
}

static Eina_Bool
_imfos_v4l_image_transform(Evas_Object *cam, void *data, size_t size)
{
   unsigned char *b, *cb, *cyb;
   size_t i;
   unsigned int percent;

   percent = 0;
   if (cam)
     {
        if (!_img)
          {
             evas_object_image_pixels_get_callback_set(cam, NULL, NULL);
             evas_object_image_alpha_set(cam, 0);

             evas_object_image_colorspace_set(cam, EVAS_COLORSPACE_ARGB8888);
             evas_object_image_size_set(cam, _width, _height);
             _img = evas_object_image_data_get(cam, 1);
          }
     }

  if (!_img_y)
     _img_y = malloc(_width * _height);
   b = data;
   cb = _img;
   cyb = _img_y;

   for (i = 0; i < size; i += 3)
     {
        *cyb = (char)((_2126[b[2]] + _7152[b[1]] + _0722[b[0]]) >> 8);
        if (*cyb > 16)
          ++percent;
        if (cam)
          {
             cb[3] = 255;
             cb[2] = b[0];
             cb[1] = b[1];
             cb[0] = b[2];
             cb += 4;
          }
        b += 3;
        cyb += 1;
     }

   if (cam)
     {
        if (!_imfos_v4l_anim)
          _imfos_v4l_anim = ecore_animator_add(_imfos_v4l_frame_anim, cam);
        evas_object_image_data_set(cam, _img);
        evas_object_image_data_update_add(cam, 0, 0, _width, _height);
        evas_object_image_pixels_dirty_set(cam, 0);
     }

//   printf("percent %d%%\n", (percent * 100)/ (_width * _height));
   return (percent > (size / 10));
}

void
imfos_v4l_run(Imfos_Device *conf)
{
   struct v4l2_format              fmt;
   struct v4l2_buffer              buf;
   struct v4l2_requestbuffers      req;
   enum v4l2_buf_type              type;
   fd_set                          fds;
   struct timeval                  tv;
   int                             r, fd = -1;
   unsigned int                    i, n_buffers;
   char                            out_name[256];
   FILE                            *fout;
   struct buffer                   *buffers;
   Eina_Bool stop;
   double time_start;
   double loop_time_start;
   double loop_time_cur;
   double loop_time_avg = 0.0;

   stop = EINA_FALSE;
   fprintf(stderr, "imfos thread %s\n", conf->dev_name);

   fd = v4l2_open(conf->dev_name, O_RDWR | O_NONBLOCK, 0);
   if (fd < 0) {
        perror("Cannot open device");
        return;
   }

   CLEAR(fmt);
   fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   fmt.fmt.pix.width       = 640;
   fmt.fmt.pix.height      = 480;
   fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
   fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
   //fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;

   _imfos_xioctl(fd, VIDIOC_S_FMT, &fmt);

   if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB24) {
        fprintf(stderr, "Libv4l didn't accept RGB24 format. Can't proceed.\n");
        switch (fmt.fmt.pix.pixelformat) {
      case V4L2_PIX_FMT_ARGB32:
           printf("argb32\n");
           break;
      case V4L2_PIX_FMT_RGB24:
      case V4L2_PIX_FMT_BGR24:
           printf("rgb24\n");
           break;
      case V4L2_PIX_FMT_Y4:
           printf("y4\n");
           break;
      case V4L2_PIX_FMT_Y6:
           printf("y6\n");
           break;
      case V4L2_PIX_FMT_Y10:
           printf("y10\n");
           break;
      case V4L2_PIX_FMT_Y12:
           printf("y12\n");
           break;
      case V4L2_PIX_FMT_Y16:
           printf("y16\n");
           break;
      case V4L2_PIX_FMT_GREY:
           printf("grey\n");
           break;


      default:
           printf("%d\n", fmt.fmt.pix.pixelformat);
   }
   }
   if ((fmt.fmt.pix.width != 640) || (fmt.fmt.pix.height != 480))
     printf("Warning: driver is sending image at %dx%d\n",
            fmt.fmt.pix.width, fmt.fmt.pix.height);

   if (conf->param.v4l.cam)
     {
        printf("Sizing object %d %d\n", _width, _height);
        evas_object_image_colorspace_set(conf->param.v4l.cam, EVAS_COLORSPACE_ARGB8888);
        evas_object_image_size_set(conf->param.v4l.cam, _width, _height);
     }

   CLEAR(req);
   req.count = 2;
   req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   req.memory = V4L2_MEMORY_MMAP;
   _imfos_xioctl(fd, VIDIOC_REQBUFS, &req);

   buffers = calloc(req.count, sizeof(*buffers));
   for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        CLEAR(buf);

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = n_buffers;

        _imfos_xioctl(fd, VIDIOC_QUERYBUF, &buf);

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start = v4l2_mmap(NULL, buf.length,
                                             PROT_READ | PROT_WRITE, MAP_SHARED,
                                             fd, buf.m.offset);

        if (MAP_FAILED == buffers[n_buffers].start) {
             perror("mmap");
             return;
        }
   }

   for (i = 0; i < n_buffers; ++i) {
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        _imfos_xioctl(fd, VIDIOC_QBUF, &buf);
   }
   type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

   _imfos_xioctl(fd, VIDIOC_STREAMON, &type);
   for (i = 0; ; ++i) {
        loop_time_start = ecore_time_get();
        //fprintf(stderr, "try %d\n", i);
        do {
             FD_ZERO(&fds);
             FD_SET(fd, &fds);

             /* Timeout. */
             tv.tv_sec = 2;
             tv.tv_usec = 0;

             r = select(fd + 1, &fds, NULL, NULL, &tv);
        } while ((r == -1 && (errno = EINTR)));
        if (r == -1) {
             perror("select");
             goto v4l_close;
        }

        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        _imfos_xioctl(fd, VIDIOC_DQBUF, &buf);

        _width = fmt.fmt.pix.width;
        _height = fmt.fmt.pix.height;
        _size = buf.bytesused;
//        printf("copy %d %d %d - %d\n", buf.bytesused,
//               _width * _height * 2, _width, _height);

        if (_imfos_v4l_image_transform(conf->param.v4l.cam, buffers[buf.index].start,
                                       buf.bytesused))
          {
          /*
             if (imfos_face_search(_img_y, _width, _height, 0))
               {
                  stop = EINA_TRUE;
                  e_screensaver_notidle();
                  fprintf(stderr, "I saw you !\n");
               }
               */
          }
        _imfos_xioctl(fd, VIDIOC_QBUF, &buf);
        /*
        fprintf(stderr, "imfos %.8f %.8f\n",
                ecore_time_get() - time_start,
                loop_time_avg);
                */
        if (stop) break;
        if (imfos_devices_timeout(conf))
           break;
   }
   //   if (stop)
   //     e_screensaver_notidle();
v4l_close:
   type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   _imfos_xioctl(fd, VIDIOC_STREAMOFF, &type);
   for (i = 0; i < n_buffers; ++i)
     v4l2_munmap(buffers[i].start, buffers[i].length);
   v4l2_close(fd);
   fprintf(stderr, "v4l close %d\n", stop);
}

void
imfos_v4l_clean(Imfos_Device *conf)
{
   fprintf(stderr, "v4l end\n");

   if (_imfos_v4l_anim)
     ecore_animator_del(_imfos_v4l_anim);
   _imfos_v4l_anim = NULL;
   if (conf->param.v4l.cam)
     {
        evas_object_image_size_set(conf->param.v4l.cam, 1, 1);
        evas_object_image_data_set(conf->param.v4l.cam, NULL);
     }
   free(_img_y);
   _img_y = NULL;
   _img = NULL;
}

static void
_imfos_v4l_cancel(void *data, Ecore_Thread *thread)
{
   printf("v4l cancel\n");

}

void
imfos_v4l_init(void)
{
   unsigned int i;

   for (i = 0; i < 256; i++)
     {
        _2126[i] = 0.2116 * 256 * i;
        _7152[i] = 0.7152 * 256 * i;
        _0722[i] = 0.0722 * 256 * i;
     }
}

void
imfos_v4l_shutdown(void)
{
}

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

#include <Ecore.h>
#include <Evas.h>
#include "imfos_v4l.h"
#include "imfos_face.h"
#include <e.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

static int _imfos_xioctl(int fh, int request, void *arg);
static void _imfos_v4l_takeshot(void *data, Ecore_Thread *thread);
static void _imfos_v4l_end(void *data, Ecore_Thread *thread);
static void _imfos_v4l_cancel(void *data, Ecore_Thread *thread);

static const char *_imfos_v4l_path = NULL;
static Ecore_Thread *_imfos_v4l_thread = NULL;

static int _width;
static int _height;
static int _size;
//static void *_img = NULL;
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
_imfos_v4l_image_transform(void *data, size_t size)
{
   unsigned char *b, *cb, *cyb;
   size_t i;
   unsigned int percent;

   percent = 0;
   /*
   if (!_img)
     _img = malloc(_width * _height * 4);
     */
   if (!_img_y)
     _img_y = malloc(_width * _height);
   b = data;
//   cb = _img;
   cyb = _img_y;

   for (i = 0; i < size; i += 3)
     {
        /*
        cb[3] = 255;
        cb[2] = b[0];
        cb[1] = b[1];
        cb[0] = b[2];
        */
        *cyb = (char)((_2126[b[2]] + _7152[b[1]] + _0722[b[0]]) >> 8);
        if (*cyb > 16)
          ++percent;
        //cb += 4;
        b += 3;
        cyb += 1;
     }
   printf("percent %d%%\n", (percent * 100)/ (_width * _height));
   return (percent > (size / 10));
}

static void
_imfos_v4l_takeshot(void *data, Ecore_Thread *thread)
{
   struct v4l2_format              fmt;
   struct v4l2_buffer              buf;
   struct v4l2_requestbuffers      req;
   enum v4l2_buf_type              type;
   fd_set                          fds;
   struct timeval                  tv;
   int                             r, fd = -1;
   unsigned int                    i, n_buffers;
   char                            *dev_name = "/dev/video0";
   char                            out_name[256];
   FILE                            *fout;
   struct buffer                   *buffers;
   Eina_Bool stop;
   double time_start;
   double loop_time_start;
   double loop_time_cur;
   double loop_time_avg = 0.0;
   Imfos_V4l_Conf *conf;

   conf = data;
   time_start = ecore_time_get();

   stop = EINA_FALSE;
   fprintf(stderr, "imfos thread\n");

   fd = v4l2_open(dev_name, O_RDWR | O_NONBLOCK, 0);
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
        fprintf(stderr, "try %d\n", i);
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


        /* check image is right */
        /*
        _imfos_v4l_image_check(buffers[buf.index].start,
                               buf.bytesused);
                               */
        _width = fmt.fmt.pix.width;
        _height = fmt.fmt.pix.height;
        _size = buf.bytesused;
        //_img = buffers[buf.index].start;
        printf("copy %d %d %d - %d\n", buf.bytesused,
               _width * _height * 2, _width, _height);

        if (_imfos_v4l_image_transform(buffers[buf.index].start, buf.bytesused))
          {
             if (imfos_face_search(_img_y, _width, _height, 0))
               {
                  stop = EINA_TRUE;
                  fprintf(stderr, "Found your fucking head guy\n");
               }
          }
        _imfos_xioctl(fd, VIDIOC_QBUF, &buf);
        fprintf(stderr, "imfos %.8f %.8f\n",
                ecore_time_get() - time_start,
                loop_time_avg);
        if (stop) break;
        loop_time_cur = ecore_time_get() - loop_time_start;
        if (loop_time_cur > loop_time_avg)
          loop_time_avg = loop_time_cur;
        if ((ecore_time_get() - time_start + loop_time_avg) > conf->timeout)
          break;
   }
   if (stop)
     e_screensaver_notidle();
v4l_close:
   type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   _imfos_xioctl(fd, VIDIOC_STREAMOFF, &type);
   for (i = 0; i < n_buffers; ++i)
     v4l2_munmap(buffers[i].start, buffers[i].length);
   v4l2_close(fd);
   fprintf(stderr, "v4l close %d\n", stop);
}

static void
_imfos_v4l_end(void *data, Ecore_Thread *thread)
{
   fprintf(stderr, "v4l end\n");
   free(data);

   /*
   if (data) {
        evas_object_image_colorspace_set(data, EVAS_COLORSPACE_ARGB8888);
        //evas_object_image_colorspace_set(data, EVAS_COLORSPACE_GRY8);
        evas_object_image_size_set(data, _width, _height);
        evas_object_image_data_set(data, _img);
        //evas_object_image_data_set(data, _img_y);
   }
   */


   /*
   free(_img);
   _img = NULL;
   */
   free(_img_y);
   _img_y = NULL;
   _imfos_v4l_thread = NULL;
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
   /* list webcams */
   _imfos_v4l_path = eina_stringshare_add("/dev/video0");
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
   eina_stringshare_del(_imfos_v4l_path);
}

void
imfos_v4l_run(Imfos_V4l_Conf *conf)
{
   fprintf(stderr, "v4l run\n");
   _imfos_v4l_thread = ecore_thread_run(_imfos_v4l_takeshot,
                                        _imfos_v4l_end,
                                        _imfos_v4l_cancel,
                                        conf);
}

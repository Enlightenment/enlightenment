#define HAVE_OPENCV
#ifdef HAVE_OPENCV
#include "opencv2/objdetect/objdetect.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#endif

#include <Eina.h>
#include "imfos_face.h"

using namespace cv;

#define FACE_CASCADE_FILE_PATH "/usr/share/opencv/haarcascades/haarcascade_frontalface_alt.xml"

int
imfos_face_search(char *data, int width, int height, int stride)
{
#ifdef HAVE_OPENCV
   std::vector<Rect> faces;
   CascadeClassifier face_cascade;
   Mat frame_gray;
   Mat mat(height, width, CV_8UC1, data, width + stride);

   if (!face_cascade.load(FACE_CASCADE_FILE_PATH)) return 0;

   face_cascade.detectMultiScale(mat, faces,  1.1, 2, 0|CV_HAAR_SCALE_IMAGE, Size(30, 30));

   printf("Detect %d?\n",faces.size());

   return (faces.size() > 0) ? 1 : 0;
#else
   return 0;
#endif
}


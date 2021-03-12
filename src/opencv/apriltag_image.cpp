#include "apriltag_opencv.h"
#include "apriltag_family.h"
#include "getopt.h"
#include "homography.h"
#include "pose.h"
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

int main(int argc, char** argv) {

  getopt_t *getopt = getopt_create();

  getopt_add_bool(getopt, 'h', "help", 0, "Show this help");
  getopt_add_bool(getopt, 'q', "quiet", 0, "Reduce output");
  getopt_add_string(getopt, 'f', "family", "tag36h11", "Tag family to use");
  getopt_add_int(getopt, '\0', "border", "1", "Set tag family border size");
  getopt_add_int(getopt, 't', "threads", "4", "Use this many CPU threads");
  getopt_add_double(getopt, 'x', "decimate", "1.0", "Decimate input image by this factor");
  getopt_add_double(getopt, 'b', "blur", "0.0", "Apply low-pass blur to input");
  getopt_add_bool(getopt, '0', "refine-edges", 1, "Spend more time aligning edges of tags");
  getopt_add_bool(getopt, '1', "refine-decode", 1, "Spend more time decoding tags");
  getopt_add_bool(getopt, '2', "refine-pose", 1, "Spend more time computing pose of tags");
  getopt_add_bool(getopt, 'c', "contours", 1, "Use new contour-based quad detection");

  if (!getopt_parse(getopt, argc, argv, 1) || getopt_get_bool(getopt, "help")) {
    printf("Usage: %s [options] <Path of image file>\n", argv[0]);
    getopt_do_usage(getopt);
    exit(0);
  }

  const char *famname = getopt_get_string(getopt, "family");
  apriltag_family_t *tf = apriltag_family_create(famname);

  if (!tf) {
    printf("Unrecognized tag family name. Use e.g. \"tag36h11\".\n");
    exit(-1);
  }

  tf->black_border = getopt_get_int(getopt, "border");

  apriltag_detector_t *td = apriltag_detector_create();
  apriltag_detector_add_family(td, tf);

  if (getopt_get_bool(getopt, "contours")) {
    apriltag_detector_enable_quad_contours(td, 1);
  }

  td->quad_decimate = getopt_get_double(getopt, "decimate");
  td->quad_sigma = getopt_get_double(getopt, "blur");
  td->nthreads = getopt_get_int(getopt, "threads");
  td->debug = 0;
  td->refine_edges = getopt_get_bool(getopt, "refine-edges");
  td->refine_decode = getopt_get_bool(getopt, "refine-decode");
  td->refine_pose = getopt_get_bool(getopt, "refine-pose");

  const zarray_t *inputs = getopt_get_extra_args(getopt);

  const char* image_file = NULL;

  if (zarray_size(inputs) > 1) {
    printf("Usage: %s [options] <Path of image file>\n", argv[0]);
    exit(-1);
  }
  else if (zarray_size(inputs)) {
    char* input;
    zarray_get(inputs, 0, &input);
    image_file = input;
  }

  const double fx=3156.71852, fy=3129.52243, cx=359.097908, cy=239.736909, // Camera parameters
               tagsize=0.0762, z_sign=1.0;

  const char* MAT_FMT = "\t%12f, ";

  const char* window = "AprilTag";

  cv::Mat frame = cv::imread(image_file);

  cv::namedWindow(window);

  Mat8uc1 gray;

  if (frame.channels() == 3) {
    cv::cvtColor(frame, gray, cv::COLOR_RGB2GRAY);
  }
  else {
    frame.copyTo(gray);
  }

  image_u8_t* im8 = cv2im8_copy(gray);

  zarray_t *detections = apriltag_detector_detect(td, im8);

  printf("Detected %d tags.\n", zarray_size(detections));

  cv::Mat display = detectionsImage(detections, frame.size(), frame.type());

  for (int i = 0; i < zarray_size(detections); i++) {
    apriltag_detection_t *det;
    zarray_get(detections, i, &det);

    matd_t* M = pose_from_homography(det->H, fx, fy, cx, cy,
                                     tagsize, z_sign, det->p, NULL, NULL);

    printf("Detection %d of %d:\n \tFamily: tag%2dh%2d\n \tID: %d\n \tHamming: %d\n"
           "\tGoodness: %.3f\n \tMargin: %.3f\n \tCenter: (%.3f,%.3f)\n"
           "\tCorners: (%.3f,%.3f)\n\t\t (%.3f,%.3f)\n\t\t (%.3f,%.3f)\n\t\t (%.3f,%.3f)\n",
           i+1, zarray_size(detections), det->family->d*det->family->d, det->family->h,
           det->id, det->hamming, det->goodness, det->decision_margin, det->c[0], det->c[1],
           det->p[0][0], det->p[0][1], det->p[1][0], det->p[1][1], det->p[2][0], det->p[2][1],
           det->p[3][0], det->p[3][1]);
    printf("\tHomography:\n");
    matd_print(det->H, MAT_FMT);
    printf("\tPose:\n");
    matd_print(M, MAT_FMT);
    printf("\n");
  }

  printf("\n");

  apriltag_detections_destroy(detections);

  display = 0.5*display + 0.5*frame;
  cv::imshow(window, display);
  image_u8_destroy(im8);

  cv::waitKey(0);

  return 0;

}

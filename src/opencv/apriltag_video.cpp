#include "apriltag_opencv.h"
#include "apriltag_family.h"
#include "getopt.h"
#include "homography.h"
#include "pose.h"
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <string>
#include <iostream>

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
  getopt_add_bool(getopt, '1', "refine-decode", 0, "Spend more time decoding tags");
  getopt_add_bool(getopt, '2', "refine-pose", 0, "Spend more time computing pose of tags");
  getopt_add_bool(getopt, 'c', "contours", 0, "Use new contour-based quad detection");

  if (!getopt_parse(getopt, argc, argv, 1) || getopt_get_bool(getopt, "help")) {
    printf("Usage: %s [options] <camera index or path to movie file>\n", argv[0]);
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

  int camera_index = 0;
  const char* movie_file = NULL;

  if (zarray_size(inputs) > 1) {
    printf("Usage: %s [options] <camera index or path to movie file>\n", argv[0]);
    exit(-1);
  }
  else if (zarray_size(inputs)) {
    char* input;
    zarray_get(inputs, 0, &input);
    char* endptr;
    camera_index = strtol(input, &endptr, 10);
    if (!endptr || *endptr) {
      movie_file = input;
    }
  }

  cv::VideoCapture* cap;

  if (movie_file) {
    cap = new cv::VideoCapture(movie_file);
  }
  else {
    cap = new cv::VideoCapture(camera_index);
  }

  const double fx=3156.71852, fy=3129.52243, cx=359.097908, cy=239.736909, // Camera parameters
               tagsize=0.0762, z_sign=1.0;

  const char* MAT_FMT = "\t%12f, ";

  const char* window = "AprilTag";

  cv::Mat frame;
  cv::namedWindow(window);
  int num_savedimg = 0;

  while (1) {
    bool ok = cap->read(frame);
    if (!ok) { break; }
    cv::imshow(window, frame);

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
    cv::Mat display_copy = display.clone();

    for (int i = 0; i < zarray_size(detections); i++) {
      apriltag_detection_t *det;
      zarray_get(detections, i, &det);

      matd_t* M = pose_from_homography(det->H, fx, fy, cx, cy,
                                       tagsize, z_sign, det->p, NULL, NULL);

      // printf("Detection %d of %d:\n \tFamily: tag%2dh%2d\n \tID: %d\n \tHamming: %d\n"
      //        "\tGoodness: %.3f\n \tMargin: %.3f\n \tCenter: (%.3f,%.3f)\n"
      //        "\tCorners: (%.3f,%.3f)\n\t\t (%.3f,%.3f)\n\t\t (%.3f,%.3f)\n\t\t (%.3f,%.3f)\n",
      //        i+1, zarray_size(detections), det->family->d*det->family->d, det->family->h,
      //        det->id, det->hamming, det->goodness, det->decision_margin, det->c[0], det->c[1],
      //        det->p[0][0], det->p[0][1], det->p[1][0], det->p[1][1], det->p[2][0], det->p[2][1],
      //        det->p[3][0], det->p[3][1]);
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

      std::vector< cv::Point> vec_contour;
      for (int i = 0; i < 4; i++)
      {
        vec_contour.push_back(cv::Point(det->p[i][0], det->p[i][1]));
      }
      const cv::Point *pts = (const cv::Point*) cv::Mat(vec_contour).data;
      int npts = cv::Mat(vec_contour).rows;

      cv::polylines(display_copy, &pts, &npts, 1, true, cv::Scalar(0, 255, 0), 8);
      cv::putText(display_copy, std::to_string(det->id), cv::Point(det->c[0], det->c[1]), CV_FONT_NORMAL, 1, cv::Scalar(0, 0, 255), 2, 1);
    }

    printf("\n");

    apriltag_detections_destroy(detections);

    display = 0.5*frame + 0.5*display_copy;
    cv::imshow(window, display);

    int k_shapshot = cv::waitKey(1);
    if (k_shapshot == 97)  // 'a'
    {
      if(frame.empty())
      {
        std::cerr << "Something is wrong with the webcam, could not get frame." << std::endl;
      }
      // Save the frame into a file
      num_savedimg++;
      imwrite("../../saved_img/saved_img_" + std::to_string(num_savedimg) + ".bmp", frame);
      std::cout << "########### image " + std::to_string(num_savedimg) + " saved successfully! ###########" << std::endl;
    }

    image_u8_destroy(im8);

    int k = cv::waitKey(1);
    if (k == 27) { break; }

  }

  return 0;

}

# AprilTag

![Github Stars](https://badgen.net/github/stars/Tinker-Twins/AprilTag?icon=github&label=stars)
![Github Forks](https://badgen.net/github/forks/Tinker-Twins/AprilTag?icon=github&label=forks)

Check the [original README](README_original.md) of @Tinker-Twins

# For ENSNARE
## 0) build this AprilTag detection package
1. Clone this `AprilTag` repository to your local machine.

2. Install the library (build the source code) using the `install.sh` shell script (requires [CMake](https://cmake.org/)).
    ```bash
    cd ~/AprilTag
    ./install.sh
    ```
  
    _**Note:** To uninstall (clean) and rebuild the entire source code, use the the `uninstall.sh` and `install.sh` shell scripts._
    ```bash
    cd ~/AprilTag
    ./uninstall.sh
    ./install.sh
    ```
3. Calibrate your camera and note its intrinsic parameters `fx, fy, cx, cy`. You might find [this](https://github.com/Tinker-Twins/Camera-Calibration) repository helpful.
## 1) launch external camera connection
  > **Pre-requisition:** already setup the camera connection package `v4l2loopback`.  
  > Check the [SONY a7r4 setup guide](SONYa7r4_setup.md) if you haven't done this yet
  ### Step 1
  ```bash
  cd v4l2loopback/
  sudo modprobe v4l2loopback
  ```
  ### Step 2
  ```bash
  ls -1 /sys/devices/virtual/video4linux
  ```
  should retern `video2`
  ### Step 3
  ```bash
  sudo modprobe v4l2loopback exclusive_caps=1 max_buffers=2
  ```
  ```bash
  gphoto2 --stdout --capture-movie | ffmpeg -i - -vcodec rawvideo -pix_fmt yuv420p -threads 8 -f v4l2 /dev/video2
  ```

## 3) launch apriltag_video
  ### 1. Rebuild if necessary
  in the path `/Apriltag`  
  > rebuild the package if you've changed something in `/src`:  
  > ```bash
  > sudo ./uninstall.sh
  > sudo ./install.sh
  > ```
  ```bash
  cd build/bin
  ```
  ### 2. Launch the video detect programm
  ```bash
  sudo ./apriltag_video 2
  ```
  here `2` is the ID of the external camera
  > It's recommended to launch this with `sudo`, otherwise some required libraries might not be achieved.
  ### 3. Capture current frame as `.bmp` image:
  Click the mouse outside the terminal where `apriltag_video` is running to deactivate the input there.  
  Press `a` to capture, the image would be saved to `/AprilTag/saved_img/`

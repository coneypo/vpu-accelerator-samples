INTRO
----------
This project is used to create one gapiosd plugin which is used to draw 2d object on video frame.
2d objects include text, rectangle, circle and line.
The plugin depends on opencv G-API and supports BGR and NV12 video formats.

INSTALL STEPS
-------------
    $ ./autogen.sh
    $ make
    $ make intall

**Install Result Test:**

    $ gst-inspect-1.0 gapiosd

**Run sample**

    $ cd <patch of>/gstreamer-osd/sample
    a.jsonfile test
      $ ./test.sh (RGB)
      $ ./test_nv12.sh (NV12)
    b.GstStructure test
      $ gcc -Wall sample_struct.c -o sample $(pkg-config --cflags --libs gstreamer-1.0)
      $ ./sample

**Code Clean:**

    $ git clean -x -f -d

OPENCV LIB INSTALL STEPS
------------------------
**1.Install Dependencies**

    $ sudo apt-get install build-essential -y
    $ sudo apt-get install cmake git libgtk2.0-dev pkg-config libavcodec-dev libavformat-dev libswscale-dev libvorbis-dev libmp3lame-dev -y
    $ sudo apt-get install python-dev python-numpy libtbb2 libtbb-dev libjpeg-dev libpng-dev libtiff-dev libjasper-dev libdc1394-22-dev -y

**2.Install ffmpeg**

    $ git clone https://github.com/FFmpeg/FFmpeg.git -b release/4.1
    $ cd YOUR_FFMPEG_PATH/FFmpeg
    $ ./configure --disable-static --enable-shared --enable-libmp3lame --enable-libvorbis --enable-gpl --enable-version3 \
    --enable-nonfree --enable-pthreads --enable-libx264 --enable-libxvid --enable-postproc --enable-ffplay --enable-ffprobe
    $ make -j7
    $ sudo make install

**3.Install Opencv**

    $ git clone https://github.com/opencv/opencv.git
    $ cd YOUR_OPENCV_PATH/opencv
    $ mkdir build && cd build
    $ cmake -DCMAKE_BUILD_TYPE=Release -DOPENCV_GENERATE_PKGCONFIG=ON -DCMAKE_INSTALL_PREFIX=/usr/local ..
    $ make -j7
    $ sudo make install

**Reference Document:**

[Opencv Install reference document](https://docs.opencv.org/trunk/d7/d9f/tutorial_linux_install.html)

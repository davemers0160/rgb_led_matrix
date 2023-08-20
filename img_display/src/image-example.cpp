// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
//
// Example how to display an image, including animated images using
// ImageMagick. For a full utility that does a few more things, have a look
// at the led-image-viewer in ../utils
//
// Showing an image is not so complicated, essentially just copy all the
// pixels to the canvas. How to get the pixels ? In this example we're using
// the graphicsmagick library as universal image loader library that
// can also deal with animated images.
// You can of course do your own image loading or use some other library.
//
// This requires an external dependency, so install these first before you
// can call `make image-example`
//   sudo apt-get update
//   sudo apt-get install libgraphicsmagick++-dev libwebp-dev -y
//   make image-example

#include "led-matrix.h"


#include <signal.h>
//#include <stdio.h>
#include <unistd.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <exception>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

// #include <Magick++.h>
// #include <magick/image.h>

using rgb_matrix::Canvas;
using rgb_matrix::RGBMatrix;
using rgb_matrix::FrameCanvas;

// Make sure we can exit gracefully when Ctrl-C is pressed.
volatile bool interrupt_received = false;
static void InterruptHandler(int signo) 
{
  interrupt_received = true;
}


//-----------------------------------------------------------------------------
// Given the filename, load the image and scale to the size of the matrix.
// If this is an animated image, the resutlting vector will contain multiple.
static void load_images(std::string filename, int32_t target_width, int32_t target_height, std::vector<cv::Mat> &images) 
{
    //ImageVector result;

    //ImageVector frames;
    
    images.clear();
    images.resize(1);
    try 
    {
        images[0] = cv::imread(filename, cv::ImreadModes::IMREAD_ANYDEPTH | cv::ImreadModes::IMREAD_COLOR);        
    } 
    catch (std::exception &e) 
    {
        
        std::cout << "error reading images: " << e.what() << std::endl;
        images.clear();
        return;
    }

    if (images[0].empty()) 
    {
        std::cout << "No image found." << std::endl;
        images.clear();
        return;
    }

    for (auto &img : images) 
    {
        // interpolation options: INTER_NEAREST, INTER_LINEAR, INTER_CUBIC, INTER_AREA, INTER_LANCZOS4 
        cv::resize(img, img, cv::Size(target_width, target_height), 0, 0, cv::INTER_LINEAR);
    }

}   // end of load_images


//-----------------------------------------------------------------------------
// Copy an image to a Canvas. Note, the RGBMatrix is implementing the Canvas
// interface as well as the FrameCanvas we use in the double-buffering of the
// animted image.
void copy_image_to_canvas(const cv::Mat &image, Canvas *canvas) 
{
    uint32_t x, y;
    const int offset_x = 0, offset_y = 0;  // If you want to move the image.

    cv::Vec3b pix;

    // Copy all the pixels to the canvas.
    for (y = 0; y < image.rows; ++y) 
    {
        for (x = 0; x < image.cols; ++x) 
        {
            pix = image.at<cv::Vec3b>(y, x);
            canvas->SetPixel(x + offset_x, y + offset_y, pix[2], pix[1], pix[0]);
        }
    }
}   // end of copy_image_to_canvas

//-----------------------------------------------------------------------------
// An animated image has to constantly swap to the next frame.
// We're using double-buffering and fill an offscreen buffer first, then show.
void show_animated_image(const std::vector<cv::Mat> &images, RGBMatrix *matrix) 
{
    FrameCanvas *offscreen_canvas = matrix->CreateFrameCanvas();
    
    while (!interrupt_received) 
    {
        for (const auto &image : images) 
        {
            if (interrupt_received) 
                break;
            
            copy_image_to_canvas(image, offscreen_canvas);
            offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);
            usleep(10000);  // 1/100s converted to usec
        }
    }
}

//-----------------------------------------------------------------------------
void generate_random_images(RGBMatrix *matrix)
{
    cv::RNG rng;
	cv::Mat image(matrix->height(), matrix->width(), CV_8UC3);
   
    FrameCanvas *offscreen_canvas = matrix->CreateFrameCanvas();
    
    while (!interrupt_received) 
    {
        rng.fill(image, cv::RNG::UNIFORM, 0, 180);

        if (interrupt_received) 
            break;
        
        copy_image_to_canvas(image, offscreen_canvas);
        offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas);
        usleep(10000);  // 1/100s converted to usec
        
    }
    
}   // end of generate_random_images

//-----------------------------------------------------------------------------
int usage(const char *program_name) 
{
    std::cout << "Usage: " << std::string(program_name) << "[led-matrix-options] <image-filename>" << std::endl;
    rgb_matrix::PrintMatrixFlags(stderr);
    return 1;
}

int main(int argc, char *argv[]) 
{
    
    //Magick::InitializeMagick(*argv);

    // Initialize the RGB matrix with
    RGBMatrix::Options matrix_options;
    rgb_matrix::RuntimeOptions runtime_opt;

    std::vector<cv::Mat> images;
    
    if (!rgb_matrix::ParseOptionsFromFlags(&argc, &argv,&matrix_options, &runtime_opt)) 
    {
        return usage(argv[0]);
    }

    if (argc != 2)
        return usage(argv[0]);

    std::string filename = std::string(argv[1]);

    signal(SIGTERM, InterruptHandler);
    signal(SIGINT, InterruptHandler);

    RGBMatrix *matrix = RGBMatrix::CreateFromOptions(matrix_options, runtime_opt);
    if (matrix == NULL)
        return 1;

    load_images(filename, matrix->width(), matrix->height(), images);
    
    switch (images.size()) 
    {
    case 0:   // failed to load image.
        generate_random_images(matrix);
        break;
    
    case 1:   // Simple example: one image to show
        copy_image_to_canvas(images[0], matrix);
        
        while (!interrupt_received) 
            sleep(1000);  // Until Ctrl-C is pressed
        break;
    
    default:  // More than one image: this is an animation.
        // show_animated_image(images, matrix);
        break;
    }

    matrix->Clear();
    delete matrix;

    return 0;
}

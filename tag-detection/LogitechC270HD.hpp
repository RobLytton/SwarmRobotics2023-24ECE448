#ifndef LOGITECH_C270HD_HPP
#define LOGITECH_C270HD_HPP

#include "opencv2/opencv.hpp"

struct LogitechC270HD: public cv::VideoCapture {

    float parameters[9] {
        6.7361123292017487e+02, 0.,                     3.1950000000000000e+02,
        0.,                     6.7361123292017487e+02, 2.3950000000000000e+02,
        0.,                     0.,                     1.
    };

    LogitechC270HD(int index, int apiPref=0) {
        // Open camera
        open(index, apiPref);
        
        // Wait 2 seconds for camera to open, fail if key pressed or timeout
        int key = cv::waitKey(2000);
        if (key != -1 || !isOpened()) {
            std::cout << "Could not open camera" << std::endl;
            return;
        }

        // Read test frame
        cv::Mat pic;
        operator>>(pic);
    }

    void setBrightness(int brightness) {
        set(cv::CAP_PROP_BRIGHTNESS, brightness);
    }

    void setContrast(int contrast) {
        set(cv::CAP_PROP_CONTRAST, contrast);
    }

    void setSaturation(int saturation) {
        set(cv::CAP_PROP_SATURATION, saturation);
    }

    void setSharpness(int sharpness) {
        set(cv::CAP_PROP_SHARPNESS, sharpness);
    }

    void setExposure(int exposure) {
        set(cv::CAP_PROP_EXPOSURE, exposure);
    }

    int getExposure() {
        return get(cv::CAP_PROP_EXPOSURE);
    }

    void setAutoExposure(int enabled) {
        set(cv::CAP_PROP_AUTO_EXPOSURE, 1 + (enabled != 0 ? 2 : 0));
    }

    int getAutoExposure() {
        return get(cv::CAP_PROP_AUTO_EXPOSURE);
    }

    void setAutoWhiteBalance(int enabled) {
        set(cv::CAP_PROP_AUTO_WB, enabled);
    }

    int getAutoWhiteBalance() {
        return get(cv::CAP_PROP_AUTO_WB);
    }

};

#endif

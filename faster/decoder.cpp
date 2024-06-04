#pragma once

#include <decoder.h>
#include "JPEG_Reader.h"
#include <glog/logging.h>

Image Decode(std::istream& input) {
    Image image;
    JpegReader reader(input);

    if (reader.GetMarker() != SOI) {
        throw std::invalid_argument("Image has to start with SOI marker");
    }

    DLOG(INFO) << "Found SOI";

    while (true) {
        switch (reader.GetMarker()) {
            case SOI:
                DLOG(ERROR) << "SOI in the middle";
                throw std::runtime_error("Invalid Marker");
            case EOI:
                DLOG(ERROR) << "EOI in the middle";
                throw std::runtime_error("Invalid Marker");
            case COM:
                DLOG(INFO) << "Reading commentary";
                reader.ReadComment(image);
                break;
            case APP:
                DLOG(INFO) << "Reading APP";
                reader.ReadApp();
                break;
            case DQT:
                DLOG(INFO) << "Reading DQT";
                reader.ReadDQT();
                break;
            case SOF0:
                DLOG(INFO) << "Reading SOF0";
                reader.ReadSOF0(image);
                break;
            case DHT:
                DLOG(INFO) << "Reading HT";
                reader.ReadHT();
                break;
            case SOS:
                DLOG(INFO) << "Reading SOS";
                reader.ReadSOS(image);
                return image;
            default:
                DLOG(ERROR) << "Unknown marker";
                throw std::runtime_error("Invalid marker");
        }
    }
}

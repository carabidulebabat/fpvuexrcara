#include "VideoPlayer.h"
#include "AndroidThreadPrioValues.hpp"
#include "helper/NDKThreadHelper.hpp"
#include "helper/NDKHelper.hpp"
#include <android/log.h>
#define MAX_NAL_SIZE 3 * 1024 * 1024  // Taille maximale du tampon NAL (1 Mo)

VideoPlayer::VideoPlayer(NEW_FRAME_CALLBACK onNewFrame):
        mParser{std::bind(&VideoPlayer::onNewNALU, this, std::placeholders::_1)},
        videoDecoder(onNewFrame) {
    __android_log_print(ANDROID_LOG_ERROR, "com.geehe.fpvuexr", "VideoPlayer creating");
//    videoDecoder.registerOnDecoderRatioChangedCallback([this](const VideoRatio ratio) {
//        const bool changed=ratio!=this->latestVideoRatio;
//        this->latestVideoRatio=ratio;
//        latestVideoRatioChanged=changed;
//    });
//    videoDecoder.registerOnDecodingInfoChangedCallback([this](const DecodingInfo info) {
//        const bool changed=info!=this->latestDecodingInfo;
//        this->latestDecodingInfo=info;
//        latestDecodingInfoChanged=changed;
//    });
    videoDecoder.initDecoder();
}

//Not yet parsed bit stream (e.g. raw h264 or rtp data)
void VideoPlayer::onNewVideoData(const uint8_t* data, const std::size_t data_length,const VIDEO_DATA_TYPE videoDataType){
    //MLOGD << "onNewVideoData " << data_length;
    switch(videoDataType){
        case VIDEO_DATA_TYPE::RTP_H264:
            // MLOGD << "onNewVideoData RTP_H264 " << data_length;
            // mParser.parse_rtp_h264_stream(data,data_length);
            break;
        case VIDEO_DATA_TYPE::RAW_H264:
            // mParser.parse_raw_h264_stream(data,data_length);
            // mParser.parseJetsonRawSlicedH264(data,data_length);
            break;
        case VIDEO_DATA_TYPE::RTP_H265:
            //MLOGD << "onNewVideoData RTP_H265 " << data_length;
            //rtpToNalu(data,data_length);
            mParser.parse_rtp_h265_stream(data,data_length);
            break;
        case VIDEO_DATA_TYPE::RAW_H265:
            MLOGD << "onNewVideoData RTP_H265 " << data_length;
            //mParser.parse_raw_h265_stream(data,data_length);
            break;
    }
}

void VideoPlayer::rtpToNalu(const uint8_t* data, const std::size_t data_length) {
    uint32_t rtp_header = 0;
    if (data[8] & 0x80 && data[9] & 0x60) {
        rtp_header = 12;
    }
    uint32_t nal_size = 0;
    uint8_t* nal_buffer = static_cast<uint8_t *>(malloc(1024 * 1024));
    uint8_t* nal = decodeRTPFrame(data, data_length, rtp_header, nal_buffer, &nal_size);
    if (!nal) {
        MLOGD << "no frame";
        return;
    }
    if (nal_size < 5) {
        MLOGD << "rtpToNalu broken frame";
        return;
    }
    uint8_t nal_type_hevc = (nal[4] >> 1) & 0x3F;
    __android_log_print(ANDROID_LOG_DEBUG, "com.geehe.fpvue", "nal_type_hevc=%d",nal_type_hevc);

}


uint8_t* VideoPlayer::decodeRTPFrame(const uint8_t* rx_buffer, uint32_t rx_size, uint32_t header_size, uint8_t* nal_buffer, uint32_t* out_nal_size) {
    rx_buffer += header_size;
    rx_size -= header_size;
    //const size_t MAX_NAL_SIZE = 5 * 1024 * 1024;
    // Get NAL type
    uint8_t fragment_type_avc = rx_buffer[0] & 0x1F;
    uint8_t fragment_type_hevc = (rx_buffer[0] >> 1) & 0x3F;

    uint8_t start_bit = 0;
    uint8_t end_bit = 0;
    uint8_t copy_size = 4;  // Start with the NAL header size (4 bytes)
    //static uint32_t nal_buffer_secondary_size = 0;  // Size of secondary buffer
    static uint8_t nal_buffer_secondary[MAX_NAL_SIZE];  // Tampon secondaire pour les fragments
    static uint32_t nal_buffer_secondary_size = 0;

    if (fragment_type_avc == 28 || fragment_type_hevc == 49) {
        if (fragment_type_avc == 28) {
            start_bit = rx_buffer[1] & 0x80;
            end_bit = rx_buffer[1] & 0x40;
            nal_buffer[4] = (rx_buffer[0] & 0xE0) | (rx_buffer[1] & 0x1F);
        } else {
            start_bit = rx_buffer[2] & 0x80;
            end_bit = rx_buffer[2] & 0x40;
            nal_buffer[4] = (rx_buffer[0] & 0x81) | ((rx_buffer[2] & 0x3F) << 1);
            nal_buffer[5] = 1;
            copy_size++;
            rx_buffer++;  // Skip the header bytes
            rx_size--;
        }

        rx_buffer++;  // Skip the NAL header byte
        rx_size--;

        // Handle the case of a fragment start
        if (start_bit) {
            // Write NAL header
            nal_buffer[0] = 0;
            nal_buffer[1] = 0;
            nal_buffer[2] = 0;
            nal_buffer[3] = 1;

            // Copy the fragment data into the secondary buffer
            memcpy(nal_buffer_secondary + nal_buffer_secondary_size + copy_size, rx_buffer, rx_size);

            // Accumulate the size of the current fragment in the secondary buffer
            nal_buffer_secondary_size = rx_size;

            // Set the in_nal_size variable to the current size of the NAL
            in_nal_size = nal_buffer_secondary_size + copy_size;
        } else {
            // Copy the fragment data into the main nal_buffer
            memcpy(nal_buffer_secondary + nal_buffer_secondary_size, rx_buffer, rx_size);

            // Update the size of the NAL
            in_nal_size += rx_size;

            // Check if this is the last fragment of the NAL
            if (end_bit) {
                *out_nal_size = in_nal_size;  // Return the size of the NAL

                // Reset the secondary buffer size
                nal_buffer_secondary_size = 0;

                // Return the fully assembled NAL
                return nal_buffer;
            }
        }

        // If the fragment is not complete, return NULL to indicate more fragments are expected
        return NULL;
    } else {
        // If it's not a fragmented NAL, copy the data directly into the nal_buffer
        nal_buffer[0] = 0;
        nal_buffer[1] = 0;
        nal_buffer[2] = 0;
        nal_buffer[3] = 1;
        memcpy(nal_buffer + copy_size, rx_buffer, rx_size);
        *out_nal_size = rx_size + copy_size;  // Return the size of the complete NAL

        // Reset in_nal_size for future frames
        in_nal_size = 0;

        return nal_buffer;  // Return the full NAL
    }
}


void VideoPlayer::onNewNALU(const NALU& nalu){
    videoDecoder.interpretNALU(nalu);
}

void VideoPlayer::setVideoSurface() {
    //reset the parser so the statistics start again from 0
    // mParser.reset();
    //set the jni object for settings
    //videoDecoder.setOutputSurface(env, surface);
}


void VideoPlayer::start() {
    //AAssetManager *assetManager=NDKHelper::getAssetManagerFromContext2(env,androidContext);
    //mParser.setLimitFPS(-1); //Default: Real time !
    const int VS_PORT=5600;
    const int VS_PROTOCOL=RTP_H265;
    const auto videoDataType=static_cast<VIDEO_DATA_TYPE>(VS_PROTOCOL);
    mUDPReceiver=std::make_unique<UDPReceiver>(javaVm,VS_PORT, "V_UDP_R", FPV_VR_PRIORITY::CPU_PRIORITY_UDPRECEIVER_VIDEO, [this,videoDataType](const uint8_t* data, size_t data_length) {
        onNewVideoData(data,data_length,videoDataType);
    }, WANTED_UDP_RCVBUF_SIZE);
    mUDPReceiver->startReceiving();
}

void VideoPlayer::stop() {
    if(mUDPReceiver){
        mUDPReceiver->stopReceiving();
        mUDPReceiver.reset();
    }
}

std::string VideoPlayer::getInfoString()const{
    std::stringstream ss;
    if(mUDPReceiver){
        ss << "Listening for video on port " << mUDPReceiver->getPort();
        ss << "\nReceived: " << mUDPReceiver->getNReceivedBytes() << "B"
           << " | parsed frames: ";
          // << mParser.nParsedNALUs << " | key frames: " << mParser.nParsedKonfigurationFrames;
    } else{
        ss << "Not receiving udp raw / rtp / rtsp";
    }
    return ss.str();
}
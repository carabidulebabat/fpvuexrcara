#include <jni.h>
#include "WfbngLink.hpp"
#include <string>
#include <android/log.h>

#include <iostream>
#include <span>
#include <cstdint>
#include <initializer_list>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include "libusb.h"
#include "devourer/src/logger.h"
#include "devourer/src/WiFiDriver.h"
#include "wfb-ng/src/wifibroadcast.hpp"
#include "RxFrame.h"

#include <sstream>
#include <iostream>
#include <iomanip>
#include <thread>

std::string uint8_to_hex_string(const uint8_t *v, const size_t s) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < s; i++) {
        ss << std::hex << std::setw(2) << static_cast<int>(v[i]);
    }
    return ss.str();
}

WfbngLink::WfbngLink(JNIEnv* env, int fd, const char* key): fd(fd), aggregator(nullptr), keyPath(key) {}

int WfbngLink::run(JNIEnv* env, int wifiChannel) {
    int r;
    libusb_context *ctx = NULL;

    r = libusb_set_option(NULL, LIBUSB_OPTION_NO_DEVICE_DISCOVERY);
    __android_log_print(ANDROID_LOG_DEBUG, TAG,
                        "libusb_set_option LIBUSB_OPTION_NO_DEVICE_DISCOVERY: %d", r);

    r = libusb_init(&ctx);
    if (r < 0) {
        return r;
    }

    struct libusb_device_handle *dev_handle;
    r = libusb_wrap_sys_device(ctx, (intptr_t) fd, &dev_handle);
    __android_log_print(ANDROID_LOG_DEBUG, TAG, "libusb_wrap_sys_device: %d", r);
    if (r < 0) {
        libusb_exit(ctx);
        return r;
    }

    /*Check if kenel driver attached*/
    if (libusb_kernel_driver_active(dev_handle, 0)) {
        r = libusb_detach_kernel_driver(dev_handle, 0); // detach driver
        __android_log_print(ANDROID_LOG_DEBUG, TAG, "libusb_detach_kernel_driver: %d", r);

    }
    r = libusb_claim_interface(dev_handle, 0);
    __android_log_print(ANDROID_LOG_DEBUG, TAG, "libusb_claim_interface: %d", r);

    Logger_t log;
    WiFiDriver wifi_driver(log);
    rtlDevice = wifi_driver.CreateRtlDevice(dev_handle);
    if (!rtlDevice) {
        __android_log_print(ANDROID_LOG_DEBUG, TAG,
                            "CreateRtlDevice error");
    } else {
        __android_log_print(ANDROID_LOG_DEBUG, TAG,
                            "CreateRtlDevice success");
    }

    // Config
    // TODO(geehe) Get that form the android UI.
    int video_client_port = 5600;
    int mavlink_client_port = 14550;
    std::string client_addr = "127.0.0.1";
    uint32_t link_id = 7669206 ; // sha1 hash of link_domain="default"
    uint8_t video_radio_port = 0;
    uint8_t mavlink_radio_port = 0x10;
    uint64_t epoch = 0;

    uint32_t video_channel_id_f = (link_id << 8) + video_radio_port;
    uint32_t video_channel_id_be = htobe32(video_channel_id_f);
    uint8_t* video_channel_id_be8 = reinterpret_cast<uint8_t *>(&video_channel_id_be);

    uint32_t mavlink_channel_id_f = (link_id << 8) + mavlink_radio_port;
    uint32_t mavlink_channel_id_be = htobe32(mavlink_channel_id_f);
    uint8_t* mavlink_channel_id_be8 = reinterpret_cast<uint8_t *>(&mavlink_channel_id_be);

    try {
        Aggregator video_agg(client_addr, video_client_port, keyPath, epoch, video_channel_id_f);
        aggregator = &video_agg;
        Aggregator mavlink_agg(client_addr, mavlink_client_port, keyPath, epoch, mavlink_channel_id_f);

        auto packetProcessor = [&video_agg, video_channel_id_be8, &mavlink_agg, mavlink_channel_id_be8](const Packet &packet) {
            RxFrame frame(packet.Data);
            if (!frame.IsValidWfbFrame()) {
                return;
            }
            // TODO(geehe) Get data from libusb?
            int8_t rssi[4] = {1,1,1,1};
            uint32_t freq = 0;
            int8_t noise[4] = {1,1,1,1};
            uint8_t antenna[4] = {1,1,1,1};
            if (frame.MatchesChannelID(video_channel_id_be8)) {
                video_agg.process_packet(packet.Data.data() + sizeof(ieee80211_header), packet.Data.size() - sizeof(ieee80211_header) - 4, 0, antenna, rssi, noise, freq, NULL);
            } else if (frame.MatchesChannelID(mavlink_channel_id_be8)) {
                mavlink_agg.process_packet(packet.Data.data() + sizeof(ieee80211_header), packet.Data.size() - sizeof(ieee80211_header) - 4, 0, antenna, rssi, noise, freq, NULL);
            }
        };

        rtlDevice->Init(packetProcessor, SelectedChannel{
                .Channel = static_cast<uint8_t>(wifiChannel),
                .ChannelOffset = 0,
                .ChannelWidth = CHANNEL_WIDTH_20,
        });

    } catch (const std::runtime_error& error) {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "runtime_error: %s", error.what());
        return -1;
    }

    __android_log_print(ANDROID_LOG_DEBUG, TAG, "Init done, releasing...");

    r = libusb_release_interface(dev_handle, 0);
    __android_log_print(ANDROID_LOG_DEBUG, TAG, "libusb_release_interface: %d", r);

    libusb_exit(ctx);

    return 0;
}

void WfbngLink::stop(JNIEnv* env) {
    if (rtlDevice) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Stopping rtlDevice");
        rtlDevice->should_stop = true;
    }
}
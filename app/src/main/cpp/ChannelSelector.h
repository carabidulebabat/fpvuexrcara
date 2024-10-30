
#include <fstream>
#include <jni.h>
#include <stereokit.h>
#include <stereokit_ui.h>
#include <string>
#include <vector>

#include "Helpers.h"
#include "wfbngrtl8812/WfbngLink.hpp"

using namespace sk;

class ChannelSelector {
public:
  ChannelSelector(JavaVM *&javaVM_, WfbngLink &wfb_)
      : javaVM(javaVM_), wfb(wfb_), prevButtonState(false), displayCounter(0) {}
  JNIEnv *env = nullptr;
  bool lThreadAttached = false;

  void update(bool buttonState) {
    // Detect when button state goes from true to false
    if (prevButtonState && !buttonState) {
      switchChannel();
      displayCounter = 100; // Start displaying text for the next 100 updates
    }
    prevButtonState = buttonState;

    // Display text if the counter is active
    if (displayCounter > 0) {
      displayText();
      --displayCounter;
    }
  }

  void displayText() {
    std::string txt = "X1 button pressed, channel ";
    txt += std::to_string(channels[channelIndex]);

    text_add_at(
        txt.c_str(),
        matrix_trs({-0.1f, 0.3f, -1.4f}, quat_identity, {-1.0f, 1.0f, 1.0f}), 0,
        text_align_bottom_left);
  }

  int currentChannel() { return channels[channelIndex]; }

  void switchChannel() {
    attachThread(env, javaVM, lThreadAttached);

    channelIndex = (channelIndex + 1) % channels.size();
    wfb.stop(env);
  }

private:
  JavaVM *javaVM;
  WfbngLink &wfb;
  bool prevButtonState;
  int displayCounter;
  std::vector<int> channels = {36,  40,  44,  48,  52,  56,  60,  64,  100, 104,
                               108, 112, 116, 120, 124, 128, 132, 136, 140, 144,
                               149, 153, 157, 161, 165, 169, 171, 173};
  int channelIndex = 0;
};
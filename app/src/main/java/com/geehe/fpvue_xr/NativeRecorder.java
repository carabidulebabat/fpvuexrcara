package com.geehe.fpvue_xr;

public class NativeRecorder {
    static {
        System.loadLibrary("native-lib");
    }

    public native void startRecordingNative();
    public native void stopRecordingNative();
}

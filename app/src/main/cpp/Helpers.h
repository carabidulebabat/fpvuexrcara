#pragma once
#include <stereokit.h>
#include <stereokit_ui.h>
#include <random>
#include <fstream>
#include <jni.h>

inline int attachThread(JNIEnv*& env, JavaVM* lJavaVM, bool &attached){
    attached = false;
    // Get JNIEnv from lJavaVM using GetEnv to test whether
    // thread is attached or not to the VM. If not, attach it
    // (and note that it will need to be detached at the end
    //  of the function).
    switch (lJavaVM->GetEnv((void**)&env, JNI_VERSION_1_6)) {
        case JNI_OK: break;
        case JNI_EVERSION: return -1;
        case JNI_EDETACHED: {
            jint lResult = lJavaVM->AttachCurrentThread(&env, nullptr);
            if(lResult == JNI_ERR) {
                return -1;
            }
            attached = true;
        } break;
    }
    return 0;
}

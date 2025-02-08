package com.example.myapplication;

import android.content.Intent;
import android.media.MediaRecorder;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.Surface;
import android.view.WindowManager;
import android.hardware.display.VirtualDisplay;
import android.hardware.display.DisplayManager;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import java.io.File;
import java.io.IOException;

public class MainActivity extends AppCompatActivity {

    private static final int REQUEST_CODE_SCREEN_CAPTURE = 1000;

    // Managers et composants pour la capture d'écran
    private MediaProjectionManager mProjectionManager;
    private MediaProjection mMediaProjection;
    private MediaRecorder mMediaRecorder;
    private VirtualDisplay mVirtualDisplay;

    private int mScreenDensity;

    public native void nativeInit();

    private boolean isRecording = false;

    // Chargement de la bibliothèque native (à appeler depuis le code C++)
    static {
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Pas de setContentView() car aucune interface utilisateur n'est nécessaire

        // Initialisation du MediaProjectionManager
        mProjectionManager = (MediaProjectionManager) getSystemService(MEDIA_PROJECTION_SERVICE);
        nativeInit();

    }

    /**
     * Méthode publique pour démarrer l'enregistrement.
     * Si l'autorisation de capture d'écran n'est pas encore obtenue,
     * une demande (dialogue système) est lancée.
     * Cette méthode pourra être appelée via JNI depuis du code C++.
     */
    public void startRec() {
        startRecordingInternal();
    }

    /**
     * Méthode publique pour arrêter l'enregistrement.
     * Cette méthode pourra être appelée via JNI depuis du code C++.
     */
    public void stopRec() {
        if (isRecording) {
            stopRecordingInternal();
        }
    }

    /**
     * Gestion du résultat de la demande de capture d'écran.
     */
    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        if (requestCode == REQUEST_CODE_SCREEN_CAPTURE) {
            if (resultCode == RESULT_OK && data != null) {
                // Obtention du MediaProjection à partir de la réponse
                mMediaProjection = mProjectionManager.getMediaProjection(resultCode, data);
                startRecordingInternal();
            }
        }
        super.onActivityResult(requestCode, resultCode, data);
    }

    /**
     * Initialise et démarre le MediaRecorder ainsi que le VirtualDisplay.
     */
    private void startRecordingInternal() {
        try {
            mMediaRecorder = new MediaRecorder();
            // Si vous souhaitez enregistrer également l'audio, décommentez la ligne suivante
            // mMediaRecorder.setAudioSource(MediaRecorder.AudioSource.MIC);
            mMediaRecorder.setVideoSource(MediaRecorder.VideoSource.SURFACE);
            mMediaRecorder.setOutputFormat(MediaRecorder.OutputFormat.MPEG_4);


            mMediaRecorder.setOutputFile(getExternalCacheDir());

            mMediaRecorder.setVideoEncoder(MediaRecorder.VideoEncoder.H264);
            // Vous pouvez ajuster la résolution ; ici nous utilisons celle de l'écran
            mMediaRecorder.setVideoSize(1080, 1920);
            mMediaRecorder.setVideoFrameRate(30); // 30 FPS (modifiable)
            mMediaRecorder.setVideoEncodingBitRate(5 * 1000 * 1000); // 5 Mbps

            mMediaRecorder.prepare();

            // Création du VirtualDisplay pour la capture d'écran
            Surface recorderSurface = mMediaRecorder.getSurface();
            mVirtualDisplay = mMediaProjection.createVirtualDisplay("ScreenRecording",
                    1080, 1920, mScreenDensity,
                    DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR,
                    recorderSurface, null, null);

            mMediaRecorder.start();
            isRecording = true;
        } catch (IOException e) {
            e.printStackTrace();
            // En cas d'erreur, on arrête l'enregistrement proprement
            stopRecordingInternal();
        }
    }

    /**
     * Arrête l'enregistrement et libère les ressources.
     */
    private void stopRecordingInternal() {
        try {
            if (mMediaRecorder != null) {
                mMediaRecorder.stop();
                mMediaRecorder.reset();
                mMediaRecorder.release();
                mMediaRecorder = null;
            }
        } catch (RuntimeException e) {
            e.printStackTrace();
        }
        if (mVirtualDisplay != null) {
            mVirtualDisplay.release();
            mVirtualDisplay = null;
        }
        if (mMediaProjection != null) {
            mMediaProjection.stop();
            mMediaProjection = null;
        }
        isRecording = false;
    }
}

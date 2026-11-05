package com.google.android.gms.maps;

import android.content.Context;

public class MapsInitializer {
    public static final int SDK_VERSION = 21000000;

    public static void initialize(Context context) {
        // No-op
    }

    public static void initialize(Context context, OnMapsSdkInitializedCallback callback) {
        if (callback != null) {
            callback.onMapsSdkInitialized(Renderer.LATEST);
        }
    }

    public static int getMapsSDKVersion() {
        return SDK_VERSION;
    }

    public enum Renderer {
        LATEST,
        LEGACY
    }

    public interface OnMapsSdkInitializedCallback {
        void onMapsSdkInitialized(Renderer renderer);
    }
}
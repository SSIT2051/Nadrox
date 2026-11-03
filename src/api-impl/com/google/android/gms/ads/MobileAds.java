package com.google.android.gms.ads;

import android.content.Context;

public class MobileAds {
    public static void initialize(Context context) {
    }

    public static void initialize(Context context, OnInitializationCompleteListener listener) {
        if (listener != null) {
            listener.onInitializationComplete(new InitializationStatus() {
                public java.util.Map<String, AdapterStatus> getAdapterStatusMap() {
                    return new java.util.HashMap<String, AdapterStatus>();
                }
            });
        }
    }

    public interface OnInitializationCompleteListener {
        void onInitializationComplete(InitializationStatus initializationStatus);
    }

    public interface InitializationStatus {
        java.util.Map<String, AdapterStatus> getAdapterStatusMap();
    }

    public interface AdapterStatus {
        int getInitializationState();
        String getDescription();
    }
}

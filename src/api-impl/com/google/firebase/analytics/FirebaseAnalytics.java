package com.google.firebase.analytics;

import android.content.Context;
import android.os.Bundle;

public class FirebaseAnalytics {
    private static FirebaseAnalytics INSTANCE;

    public static FirebaseAnalytics getInstance(Context context) {
        if (INSTANCE == null) {
            INSTANCE = new FirebaseAnalytics();
        }
        return INSTANCE;
    }

    public void logEvent(String name, Bundle params) {
    }

    public void setUserProperty(String name, String value) {
    }

    public void setCurrentScreen(android.app.Activity activity, String screenName, String screenClassOverride) {
    }

    public void setAnalyticsCollectionEnabled(boolean enabled) {
    }

    public void setUserId(String id) {
    }
}

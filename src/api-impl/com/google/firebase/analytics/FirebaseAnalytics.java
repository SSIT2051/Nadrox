package com.google.firebase.analytics;

import android.content.Context;
import android.os.Bundle;

import com.google.android.gms.tasks.Task;

public class FirebaseAnalytics {
    private static FirebaseAnalytics instance;
    private final Context context;

    private FirebaseAnalytics(Context context) {
        this.context = context;
    }

    public static FirebaseAnalytics getInstance(Context context) {
        if (instance == null) {
            instance = new FirebaseAnalytics(context);
        }
        return instance;
    }

    public void logEvent(String name, Bundle params) {
        // No-op
    }

    public void logEvent(String name, String paramName, String paramValue) {
        // No-op
    }

    public void logEvent(String name, String paramName, long paramValue) {
        // No-op
    }

    public void logEvent(String name, String paramName, double paramValue) {
        // No-op
    }

    public void setUserId(String userId) {
        // No-op
    }

    public void setUserProperty(String name, String value) {
        // No-op
    }

    public void setAnalyticsCollectionEnabled(boolean enabled) {
        // No-op
    }

    public void setSessionTimeoutDuration(long timeout) {
        // No-op
    }

    public void resetAnalyticsData() {
        // No-op
    }

    public Task<String> getAppInstanceId() {
        return com.google.android.gms.tasks.Tasks.forResult("test-instance-id");
    }

    public void setDefaultEventParameters(Bundle parameters) {
        // No-op
    }

    public static void setLogLevel(int logLevel) {
        // No-op
    }
}
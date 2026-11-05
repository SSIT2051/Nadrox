package com.google.android.gms.common;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.content.DialogInterface;
import android.os.Bundle;

public class GoogleApiAvailability {
    public static final int SUCCESS = 0;
    public static final int SERVICE_MISSING = 1;
    public static final int SERVICE_VERSION_UPDATE_REQUIRED = 2;
    public static final int SERVICE_DISABLED = 3;
    public static final int SERVICE_INVALID = 9;
    public static final int DEVELOPER_ERROR = 10;
    public static final int SERVICE_UPDATING = 18;

    private static final GoogleApiAvailability INSTANCE = new GoogleApiAvailability();

    public static GoogleApiAvailability getInstance() {
        return INSTANCE;
    }

    public int isGooglePlayServicesAvailable(Context context) {
        return SUCCESS;
    }

    public int isGooglePlayServicesAvailable(Context context, int minApkVersion) {
        return SUCCESS;
    }

    public boolean isUserResolvableError(int errorCode) {
        return errorCode == SERVICE_MISSING || errorCode == SERVICE_VERSION_UPDATE_REQUIRED || errorCode == SERVICE_DISABLED;
    }

    public Dialog getErrorDialog(Activity activity, int errorCode, int requestCode) {
        return null;
    }

    public Dialog getErrorDialog(Activity activity, int errorCode, int requestCode, DialogInterface.OnCancelListener cancelListener) {
        return null;
    }

    public void showErrorDialogFragment(Activity activity, int errorCode, int requestCode) {}

    public void showErrorDialogFragment(Activity activity, int errorCode, int requestCode, DialogInterface.OnCancelListener cancelListener) {}

    public Intent getErrorResolutionIntent(int errorCode) {
        return null;
    }

    public int getApkVersion(Context context) {
        return 21000000;
    }

    public void cancelAvailabilityErrorNotifications(Context context) {}

    public Task<Void> makeGooglePlayServicesAvailable(Activity activity) {
        return com.google.android.gms.tasks.Tasks.forResult(null);
    }

    public Task<Void> makeGooglePlayServicesAvailable(Activity activity, int minApkVersion, int errorResolutionRequestCode, DialogInterface.OnCancelListener cancelListener) {
        return com.google.android.gms.tasks.Tasks.forResult(null);
    }

    public boolean isPlayServicesPossiblyUpdating(Context context, int errorCode) {
        return false;
    }

    public boolean isPlayStorePossiblyUpdating(Context context, int errorCode) {
        return false;
    }

    public String getErrorString(int errorCode) {
        switch (errorCode) {
            case SUCCESS: return "SUCCESS";
            case SERVICE_MISSING: return "SERVICE_MISSING";
            case SERVICE_VERSION_UPDATE_REQUIRED: return "SERVICE_VERSION_UPDATE_REQUIRED";
            case SERVICE_DISABLED: return "SERVICE_DISABLED";
            case SERVICE_INVALID: return "SERVICE_INVALID";
            case DEVELOPER_ERROR: return "DEVELOPER_ERROR";
            case SERVICE_UPDATING: return "SERVICE_UPDATING";
            default: return "UNKNOWN_ERROR_" + errorCode;
        }
    }

    public static final String GOOGLE_PLAY_SERVICES_PACKAGE = "com.google.android.gms";
}
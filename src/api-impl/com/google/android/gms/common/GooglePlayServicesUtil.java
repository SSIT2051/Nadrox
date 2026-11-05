package com.google.android.gms.common;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;

public class GooglePlayServicesUtil {
    public static final int SUCCESS = 0;
    public static final int SERVICE_MISSING = 1;
    public static final int SERVICE_VERSION_UPDATE_REQUIRED = 2;
    public static final int SERVICE_DISABLED = 3;
    public static final int SERVICE_INVALID = 9;
    public static final int DEVELOPER_ERROR = 10;

    public static final String GMS_ERROR_DIALOG = "GooglePlayServicesUtil.GmsErrorDialog";

    public static int isGooglePlayServicesAvailable(Context context) {
        return SUCCESS;
    }

    public static int isGooglePlayServicesAvailable(Context context, int minApkVersion) {
        return SUCCESS;
    }

    public static boolean isUserRecoverableError(int errorCode) {
        return errorCode == SERVICE_MISSING || errorCode == SERVICE_VERSION_UPDATE_REQUIRED || errorCode == SERVICE_DISABLED;
    }

    public static void showErrorDialogFragment(int errorCode, android.app.Activity activity, int requestCode) {
        // No-op - we always return SUCCESS
    }

    public static android.content.DialogFragment getErrorDialog(int errorCode, android.app.Activity activity, int requestCode) {
        return null;
    }

    public static android.content.DialogFragment getErrorDialog(int errorCode, android.app.Activity activity, int requestCode, android.content.DialogInterface.OnCancelListener cancelListener) {
        return null;
    }

    public static Intent getErrorResolutionIntent(int errorCode) {
        return null;
    }

    public static boolean showErrorNotification(int errorCode, Context context) {
        return false;
    }

    @Deprecated
    public static void showErrorDialogFragment(int errorCode, android.app.Activity activity, int requestCode, android.content.DialogInterface.OnCancelListener cancelListener) {}

    public static boolean checkGooglePlayServicesApk(Context context) {
        return true;
    }

    public static int getApkVersion(Context context) {
        return 21000000; // 21.0.0
    }

    public static boolean isGooglePlayServicesUid(Context context, int uid) {
        return false;
    }

    public static String getErrorString(int errorCode) {
        switch (errorCode) {
            case SUCCESS: return "SUCCESS";
            case SERVICE_MISSING: return "SERVICE_MISSING";
            case SERVICE_VERSION_UPDATE_REQUIRED: return "SERVICE_VERSION_UPDATE_REQUIRED";
            case SERVICE_DISABLED: return "SERVICE_DISABLED";
            case SERVICE_INVALID: return "SERVICE_INVALID";
            case DEVELOPER_ERROR: return "DEVELOPER_ERROR";
            default: return "UNKNOWN_ERROR_" + errorCode;
        }
    }
}
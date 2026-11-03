package com.google.android.gms.common;

public class GooglePlayServicesUtil {
    public static final int SUCCESS = 0;

    public static int isGooglePlayServicesAvailable(android.content.Context context) {
        return SUCCESS;
    }

    public static boolean isUserRecoverableError(int errorCode) {
        return false;
    }

    public static android.content.DialogFragment getErrorDialog(int errorCode, android.app.Activity activity, int requestCode) {
        return null;
    }
}

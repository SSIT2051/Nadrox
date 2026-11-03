package com.google.android.gms.common;

public class GoogleApiAvailability {
  public static final int SUCCESS = 0;
  public static final int SERVICE_VERSION_UPDATE_REQUIRED = 2;
  public static final int SERVICE_DISABLED = 3;
  public static final int SERVICE_INVALID = 9;

  private static final GoogleApiAvailability INSTANCE = new GoogleApiAvailability();

  public static GoogleApiAvailability getInstance() {
    return INSTANCE;
  }

  public int isGooglePlayServicesAvailable(android.content.Context context) {
    return SUCCESS;
  }

  public int isGooglePlayServicesAvailable(android.content.Context context, int minApkVersion) {
    return SUCCESS;
  }

  public boolean isUserResolvableError(int errorCode) {
    return false;
  }

  public android.content.DialogFragment getErrorDialog(
      android.app.Activity activity, int errorCode, int requestCode) {
    return null;
  }

  public void showErrorDialogFragment(
      android.app.Activity activity, int errorCode, int requestCode) {}
}

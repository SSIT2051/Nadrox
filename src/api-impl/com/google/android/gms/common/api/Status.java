package com.google.android.gms.common.api;

public class Status {
    public static final Status SUCCESS = new Status(0);
    public static final Status CANCELED = new Status(16);
    public static final Status INTERNAL_ERROR = new Status(8);

    private final int statusCode;

    public Status(int statusCode) {
        this.statusCode = statusCode;
    }

    public boolean isSuccess() {
        return statusCode == 0;
    }

    public int getStatusCode() {
        return statusCode;
    }
}

package com.android.billingclient.api;

public class BillingResult {
    private final int responseCode;
    private final String debugMessage;

    public BillingResult(int responseCode, String debugMessage) {
        this.responseCode = responseCode;
        this.debugMessage = debugMessage;
    }

    public int getResponseCode() {
        return responseCode;
    }

    public String getDebugMessage() {
        return debugMessage;
    }
}
package com.android.billingclient.api;

public class BillingResult {
    private int responseCode;

    private BillingResult(int responseCode) {
        this.responseCode = responseCode;
    }

    public static Builder newBuilder() {
        return new Builder();
    }

    public static class Builder {
        private int responseCode = BillingResponseCode.OK;

        public Builder setResponseCode(int responseCode) {
            this.responseCode = responseCode;
            return this;
        }

        public BillingResult build() {
            return new BillingResult(responseCode);
        }
    }

    public int getResponseCode() {
        return responseCode;
    }

    public String getDebugMessage() {
        return "";
    }
}

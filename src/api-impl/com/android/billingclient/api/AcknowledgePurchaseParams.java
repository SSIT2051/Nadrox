package com.android.billingclient.api;

public class AcknowledgePurchaseParams {
    private String purchaseToken;
    private String developerPayload;

    private AcknowledgePurchaseParams(Builder builder) {
        this.purchaseToken = builder.purchaseToken;
        this.developerPayload = builder.developerPayload;
    }

    public static class Builder {
        private String purchaseToken;
        private String developerPayload;

        public Builder setPurchaseToken(String purchaseToken) {
            this.purchaseToken = purchaseToken;
            return this;
        }

        public Builder setDeveloperPayload(String developerPayload) {
            this.developerPayload = developerPayload;
            return this;
        }

        public AcknowledgePurchaseParams build() {
            return new AcknowledgePurchaseParams(this);
        }
    }

    public String getPurchaseToken() { return purchaseToken; }
    public String getDeveloperPayload() { return developerPayload; }
}
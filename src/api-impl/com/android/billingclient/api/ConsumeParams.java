package com.android.billingclient.api;

public class ConsumeParams {
    private String purchaseToken;

    private ConsumeParams(Builder builder) {
        this.purchaseToken = builder.purchaseToken;
    }

    public static class Builder {
        private String purchaseToken;

        public Builder setPurchaseToken(String purchaseToken) {
            this.purchaseToken = purchaseToken;
            return this;
        }

        public ConsumeParams build() {
            return new ConsumeParams(this);
        }
    }

    public String getPurchaseToken() { return purchaseToken; }
}
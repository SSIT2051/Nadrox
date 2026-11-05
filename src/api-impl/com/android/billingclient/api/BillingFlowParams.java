package com.android.billingclient.api;

public class BillingFlowParams {
    private SkuDetails skuDetails;
    private String purchaseToken;
    private String obfuscatedAccountId;
    private String obfuscatedProfileId;

    private BillingFlowParams(Builder builder) {
        this.skuDetails = builder.skuDetails;
        this.purchaseToken = builder.purchaseToken;
        this.obfuscatedAccountId = builder.obfuscatedAccountId;
        this.obfuscatedProfileId = builder.obfuscatedProfileId;
    }

    public static class Builder {
        private SkuDetails skuDetails;
        private String purchaseToken;
        private String obfuscatedAccountId;
        private String obfuscatedProfileId;

        public Builder setSkuDetails(SkuDetails skuDetails) {
            this.skuDetails = skuDetails;
            return this;
        }

        public Builder setPurchaseToken(String purchaseToken) {
            this.purchaseToken = purchaseToken;
            return this;
        }

        public Builder setObfuscatedAccountId(String obfuscatedAccountId) {
            this.obfuscatedAccountId = obfuscatedAccountId;
            return this;
        }

        public Builder setObfuscatedProfileId(String obfuscatedProfileId) {
            this.obfuscatedProfileId = obfuscatedProfileId;
            return this;
        }

        public BillingFlowParams build() {
            return new BillingFlowParams(this);
        }
    }

    public SkuDetails getSkuDetails() { return skuDetails; }
    public String getPurchaseToken() { return purchaseToken; }
    public String getObfuscatedAccountId() { return obfuscatedAccountId; }
    public String getObfuscatedProfileId() { return obfuscatedProfileId; }
}
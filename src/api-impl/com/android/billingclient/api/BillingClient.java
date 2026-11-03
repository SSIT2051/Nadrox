package com.android.billingclient.api;

import android.content.Context;

public class BillingClient {
    public static Builder newBuilder(Context context) {
        return new Builder();
    }

    public static class Builder {
        public Builder setListener(PurchasesUpdatedListener listener) {
            return this;
        }

        public BillingClient build() {
            return new BillingClient();
        }
    }

    public void startConnection(BillingClientStateListener listener) {
        if (listener != null) {
            listener.onBillingSetupFinished(BillingResult.newBuilder().setResponseCode(BillingResponseCode.OK).build());
        }
    }

    public void endConnection() {
    }

    public com.google.android.gms.tasks.Task<PurchasesResult> queryPurchasesAsync(QueryPurchasesParams params) {
        return com.google.android.gms.tasks.Tasks.forResult(new PurchasesResult());
    }

    public com.google.android.gms.tasks.Task<ProductDetailsResult> queryProductDetailsAsync(QueryProductDetailsParams params) {
        return com.google.android.gms.tasks.Tasks.forResult(new ProductDetailsResult());
    }

    public com.google.android.gms.tasks.Task<BillingResult> launchBillingFlow(android.app.Activity activity, BillingFlowParams params) {
        return com.google.android.gms.tasks.Tasks.forResult(BillingResult.newBuilder().setResponseCode(BillingResponseCode.OK).build());
    }

    public boolean isReady() {
        return true;
    }

    public String getConnectionState() {
        return "CONNECTED";
    }
}

package com.android.billingclient.api;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;

import com.google.android.gms.tasks.Task;

public class BillingClient {
    public static final int BILLING_RESPONSE_RESULT_OK = 0;
    public static final int BILLING_RESPONSE_RESULT_USER_CANCELED = 1;
    public static final int BILLING_RESPONSE_RESULT_SERVICE_UNAVAILABLE = 2;
    public static final int BILLING_RESPONSE_RESULT_BILLING_UNAVAILABLE = 3;
    public static final int BILLING_RESPONSE_RESULT_ITEM_UNAVAILABLE = 4;
    public static final int BILLING_RESPONSE_RESULT_DEVELOPER_ERROR = 5;
    public static final int BILLING_RESPONSE_RESULT_ERROR = 6;
    public static final int BILLING_RESPONSE_RESULT_ITEM_ALREADY_OWNED = 7;
    public static final int BILLING_RESPONSE_RESULT_ITEM_NOT_OWNED = 8;

    private final Context context;
    private BillingClientStateListener stateListener;

    private BillingClient(Context context) {
        this.context = context;
    }

    public static Builder newBuilder(Context context) {
        return new Builder(context);
    }

    public static class Builder {
        private final Context context;
        private BillingClientStateListener stateListener;

        Builder(Context context) {
            this.context = context;
        }

        public Builder setListener(BillingClientStateListener listener) {
            this.stateListener = listener;
            return this;
        }

        public BillingClient build() {
            BillingClient client = new BillingClient(context);
            client.stateListener = stateListener;
            return client;
        }
    }

    public interface BillingClientStateListener {
        void onBillingSetupFinished(BillingResult billingResult);
        void onBillingServiceDisconnected();
    }

    public void startConnection(BillingClientStateListener listener) {
        this.stateListener = listener;
        // Immediately call onBillingSetupFinished with OK
        if (listener != null) {
            listener.onBillingSetupFinished(new BillingResult(BILLING_RESPONSE_RESULT_OK, "OK"));
        }
    }

    public boolean isReady() {
        return true;
    }

    public void endConnection() {}

    public Task<BillingResult> querySkuDetailsAsync(SkuDetailsParams params, SkuDetailsResponseListener listener) {
        return com.google.android.gms.tasks.Tasks.forResult(new BillingResult(BILLING_RESPONSE_RESULT_OK, "OK"));
    }

    public interface SkuDetailsResponseListener {
        void onSkuDetailsResponse(BillingResult billingResult, java.util.List<SkuDetails> skuDetailsList);
    }

    public Task<BillingResult> launchBillingFlow(Activity activity, BillingFlowParams params) {
        return com.google.android.gms.tasks.Tasks.forResult(new BillingResult(BILLING_RESPONSE_RESULT_OK, "OK"));
    }

    public Task<Purchase.PurchasesResult> queryPurchasesAsync(QueryPurchasesParams params) {
        return com.google.android.gms.tasks.Tasks.forResult(new Purchase.PurchasesResult(new BillingResult(BILLING_RESPONSE_RESULT_OK, "OK"), new java.util.ArrayList<>()));
    }

    public Task<BillingResult> consumeAsync(ConsumeParams params, ConsumeResponseListener listener) {
        return com.google.android.gms.tasks.Tasks.forResult(new BillingResult(BILLING_RESPONSE_RESULT_OK, "OK"));
    }

    public interface ConsumeResponseListener {
        void onConsumeResponse(BillingResult billingResult, String purchaseToken);
    }

    public Task<BillingResult> acknowledgePurchase(AcknowledgePurchaseParams params, AcknowledgePurchaseResponseListener listener) {
        return com.google.android.gms.tasks.Tasks.forResult(new BillingResult(BILLING_RESPONSE_RESULT_OK, "OK"));
    }

    public interface AcknowledgePurchaseResponseListener {
        void onAcknowledgePurchaseResponse(BillingResult billingResult);
    }
}
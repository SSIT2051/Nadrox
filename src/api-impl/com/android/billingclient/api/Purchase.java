package com.android.billingclient.api;

import java.util.List;

public class Purchase {
    private String purchaseToken;
    private String orderId;
    private String packageName;
    private String sku;
    private long purchaseTime;
    private int purchaseState;
    private String developerPayload;
    private String signature;
    private String originalJson;
    private boolean isAutoRenewing;
    private boolean isAcknowledged;

    public static class PurchasesResult {
        private final BillingResult billingResult;
        private final List<Purchase> purchasesList;

        public PurchasesResult(BillingResult billingResult, List<Purchase> purchasesList) {
            this.billingResult = billingResult;
            this.purchasesList = purchasesList;
        }

        public BillingResult getBillingResult() { return billingResult; }
        public List<Purchase> getPurchasesList() { return purchasesList; }
    }

    public String getPurchaseToken() { return purchaseToken; }
    public String getOrderId() { return orderId; }
    public String getPackageName() { return packageName; }
    public String getSku() { return sku; }
    public long getPurchaseTime() { return purchaseTime; }
    public int getPurchaseState() { return purchaseState; }
    public String getDeveloperPayload() { return developerPayload; }
    public String getSignature() { return signature; }
    public String getOriginalJson() { return originalJson; }
    public boolean isAutoRenewing() { return isAutoRenewing; }
    public boolean isAcknowledged() { return isAcknowledged; }
}
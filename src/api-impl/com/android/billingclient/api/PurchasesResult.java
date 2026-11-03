package com.android.billingclient.api;

import java.util.ArrayList;
import java.util.List;

public class PurchasesResult {
    public BillingResult getBillingResult() {
        return BillingResult.newBuilder().setResponseCode(BillingResponseCode.OK).build();
    }

    public List<Purchase> getPurchasesList() {
        return new ArrayList<Purchase>();
    }
}

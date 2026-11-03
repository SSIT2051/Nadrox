package com.android.billingclient.api;

import java.util.ArrayList;
import java.util.List;

public class ProductDetailsResult {
    public BillingResult getBillingResult() {
        return BillingResult.newBuilder().setResponseCode(BillingResponseCode.OK).build();
    }

    public List<ProductDetails> getProductDetailsList() {
        return new ArrayList<ProductDetails>();
    }
}

package com.android.billingclient.api;

import java.util.List;

public class Purchase {
    public String getPurchaseToken() {
        return "";
    }

    public String getOrderId() {
        return "";
    }

    public List<String> getSkus() {
        return java.util.Collections.emptyList();
    }

    public int getPurchaseState() {
        return 1;
    }

    public boolean isAcknowledged() {
        return true;
    }
}

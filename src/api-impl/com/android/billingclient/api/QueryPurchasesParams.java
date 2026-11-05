package com.android.billingclient.api;

public class QueryPurchasesParams {
    private String type;

    private QueryPurchasesParams(Builder builder) {
        this.type = builder.type;
    }

    public static class Builder {
        private String type;

        public Builder setProductType(String type) {
            this.type = type;
            return this;
        }

        public QueryPurchasesParams build() {
            return new QueryPurchasesParams(this);
        }
    }

    public String getProductType() { return type; }
}
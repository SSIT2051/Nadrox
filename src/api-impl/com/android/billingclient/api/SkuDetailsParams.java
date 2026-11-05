package com.android.billingclient.api;

import java.util.List;

public class SkuDetailsParams {
    private List<String> skusList;
    private String type;

    private SkuDetailsParams(Builder builder) {
        this.skusList = builder.skusList;
        this.type = builder.type;
    }

    public static class Builder {
        private List<String> skusList;
        private String type;

        public Builder setSkusList(List<String> skusList) {
            this.skusList = skusList;
            return this;
        }

        public Builder setType(String type) {
            this.type = type;
            return this;
        }

        public SkuDetailsParams build() {
            return new SkuDetailsParams(this);
        }
    }

    public List<String> getSkusList() { return skusList; }
    public String getType() { return type; }
}
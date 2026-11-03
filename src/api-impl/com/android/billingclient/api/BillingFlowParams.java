package com.android.billingclient.api;

import java.util.List;

public class BillingFlowParams {
    public static Builder newBuilder() {
        return new Builder();
    }

    public static class Builder {
        public Builder setProductDetailsParamsList(List<ProductDetailsParams> productDetailsParamsList) {
            return this;
        }

        public BillingFlowParams build() {
            return new BillingFlowParams();
        }
    }

    public static class ProductDetailsParams {
        public static Builder newBuilder() {
            return new Builder();
        }

        public static class Builder {
            public Builder setProductDetails(ProductDetails productDetails) {
                return this;
            }

            public ProductDetailsParams build() {
                return new ProductDetailsParams();
            }
        }
    }
}

package com.android.billingclient.api;

import java.util.List;

public class QueryProductDetailsParams {
    public static Builder newBuilder() {
        return new Builder();
    }

    public static class Builder {
        public Builder setProductList(List<Product> productList) {
            return this;
        }

        public QueryProductDetailsParams build() {
            return new QueryProductDetailsParams();
        }
    }

    public static class Product {
        public static Builder newBuilder() {
            return new Builder();
        }

        public static class Builder {
            public Builder setProductId(String productId) {
                return this;
            }

            public Builder setProductType(String productType) {
                return this;
            }

            public Product build() {
                return new Product();
            }
        }
    }
}

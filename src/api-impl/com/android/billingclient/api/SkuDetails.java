package com.android.billingclient.api;

public class SkuDetails {
    private String sku;
    private String type;
    private String price;
    private String priceCurrencyCode;
    private String title;
    private String description;
    private String priceAmountMicros;
    private long priceAmountMicrosLong;
    private String originalPrice;
    private String originalPriceCurrencyCode;
    private String subscriptionPeriod;
    private String freeTrialPeriod;
    private String introductoryPrice;
    private String introductoryPriceCurrencyCode;
    private String introductoryPricePeriod;
    private String introductoryPriceCycles;

    public String getSku() { return sku; }
    public String getType() { return type; }
    public String getPrice() { return price; }
    public String getPriceCurrencyCode() { return priceCurrencyCode; }
    public String getTitle() { return title; }
    public String getDescription() { return description; }
    public String getPriceAmountMicros() { return priceAmountMicros; }
    public long getPriceAmountMicrosLong() { return priceAmountMicrosLong; }
    public String getOriginalPrice() { return originalPrice; }
    public String getOriginalPriceCurrencyCode() { return originalPriceCurrencyCode; }
    public String getSubscriptionPeriod() { return subscriptionPeriod; }
    public String getFreeTrialPeriod() { return freeTrialPeriod; }
    public String getIntroductoryPrice() { return introductoryPrice; }
    public String getIntroductoryPriceCurrencyCode() { return introductoryPriceCurrencyCode; }
    public String getIntroductoryPricePeriod() { return introductoryPricePeriod; }
    public String getIntroductoryPriceCycles() { return introductoryPriceCycles; }
}
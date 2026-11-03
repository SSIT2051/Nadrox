package com.google.android.gms.auth.api.signin;

public class GoogleSignInOptions {
    public static class Builder {
        public Builder() {}

        public Builder requestEmail() {
            return this;
        }

        public Builder requestIdToken(String serverClientId) {
            return this;
        }

        public Builder requestId() {
            return this;
        }

        public Builder requestProfile() {
            return this;
        }

        public GoogleSignInOptions build() {
            return new GoogleSignInOptions();
        }
    }

    public static final GoogleSignInOptions DEFAULT_SIGN_IN = new Builder().requestEmail().build();
}

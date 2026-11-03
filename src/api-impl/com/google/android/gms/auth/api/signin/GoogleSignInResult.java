package com.google.android.gms.auth.api.signin;

import com.google.android.gms.common.api.Status;

public class GoogleSignInResult {
    public GoogleSignInAccount getSignInAccount() {
        return new GoogleSignInAccount();
    }

    public Status getStatus() {
        return Status.SUCCESS;
    }

    public boolean isSuccess() {
        return true;
    }
}

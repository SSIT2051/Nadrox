package com.google.android.gms.auth.api.signin;

import android.content.Context;

public class GoogleSignIn {
    public static GoogleSignInClient getClient(Context context, GoogleSignInOptions options) {
        return new GoogleSignInClient();
    }
}

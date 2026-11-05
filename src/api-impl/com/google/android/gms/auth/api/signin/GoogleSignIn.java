package com.google.android.gms.auth.api.signin;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import com.google.android.gms.tasks.Task;

public class GoogleSignIn {
    private GoogleSignIn() {}

    public static GoogleSignInClient getClient(Context context, GoogleSignInOptions options) {
        return new GoogleSignInClient(context, options);
    }

    public static GoogleSignInClient getClient(Activity activity, GoogleSignInOptions options) {
        return new GoogleSignInClient(activity, options);
    }

    public static Task<GoogleSignInAccount> getSignedInAccountFromIntent(Intent data) {
        GoogleSignInAccount account = new GoogleSignInAccount.Builder()
            .setId("0")
            .setEmail("test@example.com")
            .setDisplayName("Test User")
            .setGivenName("Test")
            .setFamilyName("User")
            .build();
        return com.google.android.gms.tasks.Tasks.forResult(account);
    }

    public static GoogleSignInAccount getLastSignedInAccount(Context context) {
        return new GoogleSignInAccount.Builder()
            .setId("0")
            .setEmail("test@example.com")
            .setDisplayName("Test User")
            .setGivenName("Test")
            .setFamilyName("User")
            .build();
    }

    public static boolean hasPermissions(GoogleSignInAccount account, com.google.android.gms.common.api.Scope... scopes) {
        return true;
    }

    public static Task<Void> requestPermissions(Activity activity, int requestCode, GoogleSignInAccount account, com.google.android.gms.common.api.Scope... scopes) {
        return com.google.android.gms.tasks.Tasks.forResult(null);
    }
}
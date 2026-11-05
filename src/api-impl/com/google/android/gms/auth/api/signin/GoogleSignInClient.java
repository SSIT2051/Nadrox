package com.google.android.gms.auth.api.signin;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.tasks.Task;

public class GoogleSignInClient {
    private final Context context;
    private final GoogleSignInOptions options;

    GoogleSignInClient(Context context, GoogleSignInOptions options) {
        this.context = context;
        this.options = options;
    }

    public Task<GoogleSignInAccount> silentSignIn() {
        GoogleSignInAccount account = new GoogleSignInAccount.Builder()
            .setId("0")
            .setEmail("test@example.com")
            .setDisplayName("Test User")
            .setGivenName("Test")
            .setFamilyName("User")
            .build();
        return com.google.android.gms.tasks.Tasks.forResult(account);
    }

    public Intent getSignInIntent() {
        Intent intent = new Intent();
        intent.putExtra("is_sign_in_intent", true);
        return intent;
    }

    public Task<Void> signOut() {
        return com.google.android.gms.tasks.Tasks.forResult(null);
    }

    public Task<Void> revokeAccess() {
        return com.google.android.gms.tasks.Tasks.forResult(null);
    }

    public Task<GoogleSignInAccount> getSignInIntentFromIntent(Intent data) {
        GoogleSignInAccount account = new GoogleSignInAccount.Builder()
            .setId("0")
            .setEmail("test@example.com")
            .setDisplayName("Test User")
            .setGivenName("Test")
            .setFamilyName("User")
            .build();
        return com.google.android.gms.tasks.Tasks.forResult(account);
    }

    public static GoogleSignInClient getClient(Context context, GoogleSignInOptions options) {
        return new GoogleSignInClient(context, options);
    }
}
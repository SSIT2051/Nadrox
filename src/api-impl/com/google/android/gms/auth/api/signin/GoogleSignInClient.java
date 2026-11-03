package com.google.android.gms.auth.api.signin;

import android.content.Intent;
import com.google.android.gms.tasks.Task;
import com.google.android.gms.common.api.GoogleApiClient;

public class GoogleSignInClient {
    public Task<GoogleSignInAccount> silentSignIn() {
        return com.google.android.gms.tasks.Tasks.forResult(new GoogleSignInAccount());
    }

    public Intent getSignInIntent() {
        return new Intent();
    }

    public Task<Void> signOut() {
        return com.google.android.gms.tasks.Tasks.forResult(null);
    }

    public Task<Void> revokeAccess() {
        return com.google.android.gms.tasks.Tasks.forResult(null);
    }
}

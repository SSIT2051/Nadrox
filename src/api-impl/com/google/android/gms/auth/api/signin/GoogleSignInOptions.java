package com.google.android.gms.auth.api.signin;

import android.accounts.Account;
import android.content.Context;
import android.os.Parcel;
import android.os.Parcelable;

import com.google.android.gms.common.api.Scope;

import java.util.ArrayList;
import java.util.List;

public class GoogleSignInOptions implements Parcelable {
    public static final int DEFAULT_GAMES_SIGN_IN = 1;
    public static final int DEFAULT_SIGN_IN = 2;

    private final List<Scope> scopes;
    private final String serverClientId;
    private final String language;
    private final boolean requestIdToken;
    private final boolean requestServerAuthCode;
    private final boolean requestEmail;
    private final boolean requestProfile;
    private final String hostedDomain;
    private final Account account;
    private final String extensions;

    private GoogleSignInOptions(Builder builder) {
        this.scopes = builder.scopes;
        this.serverClientId = builder.serverClientId;
        this.language = builder.language;
        this.requestIdToken = builder.requestIdToken;
        this.requestServerAuthCode = builder.requestServerAuthCode;
        this.requestEmail = builder.requestEmail;
        this.requestProfile = builder.requestProfile;
        this.hostedDomain = builder.hostedDomain;
        this.account = builder.account;
        this.extensions = builder.extensions;
    }

    public static final Creator<GoogleSignInOptions> CREATOR = new Creator<GoogleSignInOptions>() {
        @Override
        public GoogleSignInOptions createFromParcel(Parcel in) {
            return new GoogleSignInOptions(in);
        }

        @Override
        public GoogleSignInOptions[] newArray(int size) {
            return new GoogleSignInOptions[size];
        }
    };

    private GoogleSignInOptions(Parcel in) {
        int scopeCount = in.readInt();
        scopes = new ArrayList<>(scopeCount);
        for (int i = 0; i < scopeCount; i++) {
            scopes.add(Scope.CREATOR.createFromParcel(in));
        }
        serverClientId = in.readString();
        language = in.readString();
        requestIdToken = in.readByte() != 0;
        requestServerAuthCode = in.readByte() != 0;
        requestEmail = in.readByte() != 0;
        requestProfile = in.readByte() != 0;
        hostedDomain = in.readString();
        account = in.readParcelable(Account.class.getClassLoader());
        extensions = in.readString();
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeInt(scopes.size());
        for (Scope scope : scopes) {
            scope.writeToParcel(dest, flags);
        }
        dest.writeString(serverClientId);
        dest.writeString(language);
        dest.writeByte((byte) (requestIdToken ? 1 : 0));
        dest.writeByte((byte) (requestServerAuthCode ? 1 : 0));
        dest.writeByte((byte) (requestEmail ? 1 : 0));
        dest.writeByte((byte) (requestProfile ? 1 : 0));
        dest.writeString(hostedDomain);
        dest.writeParcelable(account, flags);
        dest.writeString(extensions);
    }

    @Override
    public int describeContents() {
        return 0;
    }

    public List<Scope> getScopes() {
        return scopes;
    }

    public String getServerClientId() {
        return serverClientId;
    }

    public String getLanguage() {
        return language;
    }

    public boolean isRequestIdToken() {
        return requestIdToken;
    }

    public boolean isRequestServerAuthCode() {
        return requestServerAuthCode;
    }

    public boolean isRequestEmail() {
        return requestEmail;
    }

    public boolean isRequestProfile() {
        return requestProfile;
    }

    public String getHostedDomain() {
        return hostedDomain;
    }

    public Account getAccount() {
        return account;
    }

    public String getExtensions() {
        return extensions;
    }

    public static class Builder {
        private final List<Scope> scopes = new ArrayList<>();
        private String serverClientId;
        private String language;
        private boolean requestIdToken;
        private boolean requestServerAuthCode;
        private boolean requestEmail;
        private boolean requestProfile;
        private String hostedDomain;
        private Account account;
        private String extensions;

        public Builder() {}

        public Builder(GoogleSignInOptions options) {
            scopes.addAll(options.scopes);
            serverClientId = options.serverClientId;
            language = options.language;
            requestIdToken = options.requestIdToken;
            requestServerAuthCode = options.requestServerAuthCode;
            requestEmail = options.requestEmail;
            requestProfile = options.requestProfile;
            hostedDomain = options.hostedDomain;
            account = options.account;
            extensions = options.extensions;
        }

        public Builder addScope(Scope scope) {
            scopes.add(scope);
            return this;
        }

        public Builder requestScopes(Scope scope, Scope... scopes) {
            this.scopes.add(scope);
            for (Scope s : scopes) {
                this.scopes.add(s);
            }
            return this;
        }

        public Builder requestIdToken(String serverClientId) {
            this.serverClientId = serverClientId;
            this.requestIdToken = true;
            return this;
        }

        public Builder requestServerAuthCode(String serverClientId) {
            this.serverClientId = serverClientId;
            this.requestServerAuthCode = true;
            return this;
        }

        public Builder requestEmail() {
            this.requestEmail = true;
            return this;
        }

        public Builder requestProfile() {
            this.requestProfile = true;
            return this;
        }

        public Builder setHostedDomain(String hostedDomain) {
            this.hostedDomain = hostedDomain;
            return this;
        }

        public Builder setAccountName(String accountName) {
            this.account = new Account(accountName, "com.google");
            return this;
        }

        public Builder setLanguage(String language) {
            this.language = language;
            return this;
        }

        public Builder setExtensions(String extensions) {
            this.extensions = extensions;
            return this;
        }

        public GoogleSignInOptions build() {
            return new GoogleSignInOptions(this);
        }
    }
}
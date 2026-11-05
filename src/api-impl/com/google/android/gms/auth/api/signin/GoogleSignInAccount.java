package com.google.android.gms.auth.api.signin;

import android.accounts.Account;
import android.os.Parcel;
import android.os.Parcelable;

public class GoogleSignInAccount implements Parcelable {
    private final String id;
    private final String idToken;
    private final String email;
    private final String displayName;
    private final String givenName;
    private final String familyName;
    private final String photoUrl;
    private final String serverAuthCode;
    private final Account account;

    private GoogleSignInAccount(Builder builder) {
        this.id = builder.id;
        this.idToken = builder.idToken;
        this.email = builder.email;
        this.displayName = builder.displayName;
        this.givenName = builder.givenName;
        this.familyName = builder.familyName;
        this.photoUrl = builder.photoUrl;
        this.serverAuthCode = builder.serverAuthCode;
        this.account = builder.account;
    }

    public static final Creator<GoogleSignInAccount> CREATOR = new Creator<GoogleSignInAccount>() {
        @Override
        public GoogleSignInAccount createFromParcel(Parcel in) {
            return new GoogleSignInAccount(in);
        }

        @Override
        public GoogleSignInAccount[] newArray(int size) {
            return new GoogleSignInAccount[size];
        }
    };

    private GoogleSignInAccount(Parcel in) {
        id = in.readString();
        idToken = in.readString();
        email = in.readString();
        displayName = in.readString();
        givenName = in.readString();
        familyName = in.readString();
        photoUrl = in.readString();
        serverAuthCode = in.readString();
        account = in.readParcelable(Account.class.getClassLoader());
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeString(id);
        dest.writeString(idToken);
        dest.writeString(email);
        dest.writeString(displayName);
        dest.writeString(givenName);
        dest.writeString(familyName);
        dest.writeString(photoUrl);
        dest.writeString(serverAuthCode);
        dest.writeParcelable(account, flags);
    }

    @Override
    public int describeContents() {
        return 0;
    }

    public String getId() {
        return id;
    }

    public String getIdToken() {
        return idToken;
    }

    public String getEmail() {
        return email;
    }

    public String getDisplayName() {
        return displayName;
    }

    public String getGivenName() {
        return givenName;
    }

    public String getFamilyName() {
        return familyName;
    }

    public String getPhotoUrl() {
        return photoUrl;
    }

    public String getServerAuthCode() {
        return serverAuthCode;
    }

    public Account getAccount() {
        return account;
    }

    public static class Builder {
        private String id = "0";
        private String idToken = "";
        private String email = "test@example.com";
        private String displayName = "Test User";
        private String givenName = "Test";
        private String familyName = "User";
        private String photoUrl = "";
        private String serverAuthCode = "";
        private Account account = new Account("test@gmail.com", "com.google");

        public Builder setId(String id) {
            this.id = id;
            return this;
        }

        public Builder setIdToken(String idToken) {
            this.idToken = idToken;
            return this;
        }

        public Builder setEmail(String email) {
            this.email = email;
            return this;
        }

        public Builder setDisplayName(String displayName) {
            this.displayName = displayName;
            return this;
        }

        public Builder setGivenName(String givenName) {
            this.givenName = givenName;
            return this;
        }

        public Builder setFamilyName(String familyName) {
            this.familyName = familyName;
            return this;
        }

        public Builder setPhotoUrl(String photoUrl) {
            this.photoUrl = photoUrl;
            return this;
        }

        public Builder setServerAuthCode(String serverAuthCode) {
            this.serverAuthCode = serverAuthCode;
            return this;
        }

        public Builder setAccount(Account account) {
            this.account = account;
            return this;
        }

        public GoogleSignInAccount build() {
            return new GoogleSignInAccount(this);
        }
    }
}
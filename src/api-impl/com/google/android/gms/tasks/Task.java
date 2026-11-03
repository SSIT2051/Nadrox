package com.google.android.gms.tasks;

import android.app.Activity;
import java.util.concurrent.Executor;

public class Task<TResult> {
    public boolean isComplete() {
        return true;
    }

    public boolean isSuccessful() {
        return true;
    }

    public TResult getResult() {
        return null;
    }

    public Exception getException() {
        return null;
    }

    public Task<TResult> addOnSuccessListener(OnSuccessListener<? super TResult> listener) {
        if (listener != null) {
            listener.onSuccess(null);
        }
        return this;
    }

    public Task<TResult> addOnSuccessListener(Executor executor, OnSuccessListener<? super TResult> listener) {
        if (listener != null) {
            listener.onSuccess(null);
        }
        return this;
    }

    public Task<TResult> addOnSuccessListener(Activity activity, OnSuccessListener<? super TResult> listener) {
        if (listener != null) {
            listener.onSuccess(null);
        }
        return this;
    }

    public Task<TResult> addOnFailureListener(OnFailureListener listener) {
        return this;
    }

    public Task<TResult> addOnFailureListener(Executor executor, OnFailureListener listener) {
        return this;
    }

    public Task<TResult> addOnFailureListener(Activity activity, OnFailureListener listener) {
        return this;
    }

    public Task<TResult> addOnCompleteListener(OnCompleteListener<TResult> listener) {
        if (listener != null) {
            listener.onComplete(this);
        }
        return this;
    }

    public Task<TResult> addOnCompleteListener(Executor executor, OnCompleteListener<TResult> listener) {
        if (listener != null) {
            listener.onComplete(this);
        }
        return this;
    }

    public Task<TResult> addOnCompleteListener(Activity activity, OnCompleteListener<TResult> listener) {
        if (listener != null) {
            listener.onComplete(this);
        }
        return this;
    }

    public interface OnSuccessListener<TResult> {
        void onSuccess(TResult result);
    }

    public interface OnFailureListener {
        void onFailure(Exception e);
    }

    public interface OnCompleteListener<TResult> {
        void onComplete(Task<TResult> task);
    }
}

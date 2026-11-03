package com.google.android.gms.tasks;

import java.util.concurrent.Executor;

public class Tasks {
    public static <TResult> Task<TResult> forResult(TResult result) {
        return new Task<TResult>() {
            @Override
            public TResult getResult() {
                return result;
            }
        };
    }

    public static <TResult> Task<TResult> forException(Exception exception) {
        Task<TResult> task = new Task<TResult>();
        return task;
    }

    public static void await(Task<?> task) throws Exception {
    }

    public static <TResult> TResult await(Task<TResult> task, long timeout, java.util.concurrent.TimeUnit unit) throws Exception {
        return task.getResult();
    }
}

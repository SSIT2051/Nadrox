package com.google.android.gms.games;

import android.content.Context;
import android.content.Intent;

import com.google.android.gms.tasks.Task;

public class AchievementsClient {
    private final Context context;

    AchievementsClient(Context context) {
        this.context = context;
    }

    public Task<Void> unlock(String achievementId) {
        return com.google.android.gms.tasks.Tasks.forResult(null);
    }

    public Task<Void> reveal(String achievementId) {
        return com.google.android.gms.tasks.Tasks.forResult(null);
    }

    public Task<Void> increment(String achievementId, int numSteps) {
        return com.google.android.gms.tasks.Tasks.forResult(null);
    }

    public Task<Void> setSteps(String achievementId, int numSteps) {
        return com.google.android.gms.tasks.Tasks.forResult(null);
    }

    public Task<AchievementBuffer> load(boolean forceReload) {
        return com.google.android.gms.tasks.Tasks.forResult(new AchievementBuffer());
    }

    public Task<AchievementBuffer> getAchievementsIntent() {
        return com.google.android.gms.tasks.Tasks.forResult(new AchievementBuffer());
    }

    public Intent getAchievementsIntent() {
        return new Intent();
    }
}
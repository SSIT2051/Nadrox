package com.google.android.gms.games;

import com.google.android.gms.tasks.Task;
import com.google.android.gms.tasks.Tasks;
import android.content.Intent;

public class AchievementsClient {
    public Task<Void> unlock(String achievementId) {
        return Tasks.forResult(null);
    }

    public Task<Void> increment(String achievementId, int numSteps) {
        return Tasks.forResult(null);
    }

    public Task<Void> setSteps(String achievementId, int numSteps) {
        return Tasks.forResult(null);
    }

    public Task<Void> reveal(String achievementId) {
        return Tasks.forResult(null);
    }

    public Task<Intent> getAchievementsIntent() {
        return Tasks.forResult(new Intent());
    }
}

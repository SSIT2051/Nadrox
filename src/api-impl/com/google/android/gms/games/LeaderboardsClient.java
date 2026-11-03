package com.google.android.gms.games;

import com.google.android.gms.tasks.Task;
import com.google.android.gms.tasks.Tasks;
import java.util.ArrayList;

public class LeaderboardsClient {
    public Task<Void> submitScore(String leaderboardId, long score) {
        return Tasks.forResult(null);
    }

    public Task<Void> submitScore(String leaderboardId, long score, String tag) {
        return Tasks.forResult(null);
    }

    public Task<Intent> getLeaderboardIntent(String leaderboardId) {
        return Tasks.forResult(new Intent());
    }

    public Task<Intent> getLeaderboardIntent(String leaderboardId, int timeSpan) {
        return Tasks.forResult(new Intent());
    }

    public Task<Intent> getAllLeaderboardsIntent() {
        return Tasks.forResult(new Intent());
    }
}

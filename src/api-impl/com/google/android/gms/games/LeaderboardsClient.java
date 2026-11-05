package com.google.android.gms.games;

import android.content.Context;

import com.google.android.gms.common.api.GoogleApiClient;
import com.google.android.gms.tasks.Task;

public class LeaderboardsClient {
    private final Context context;

    LeaderboardsClient(Context context) {
        this.context = context;
    }

    public Task<Void> submitScore(String leaderboardId, long score) {
        return com.google.android.gms.tasks.Tasks.forResult(null);
    }

    public Task<Void> submitScoreImmediate(String leaderboardId, long score) {
        return com.google.android.gms.tasks.Tasks.forResult(null);
    }

    public Task<LeaderboardScoreBuffer> loadCurrentPlayerLeaderboardScore(String leaderboardId, int span, int collection) {
        return com.google.android.gms.tasks.Tasks.forResult(new LeaderboardScoreBuffer());
    }

    public Task<LeaderboardScoreBuffer> loadTopScores(String leaderboardId, int span, int collection, int maxResults) {
        return com.google.android.gms.tasks.Tasks.forResult(new LeaderboardScoreBuffer());
    }

    public Task<LeaderboardScoreBuffer> loadMoreScores(LeaderboardScoreBuffer buffer, int maxResults) {
        return com.google.android.gms.tasks.Tasks.forResult(new LeaderboardScoreBuffer());
    }

    public Task<LeaderboardsClient> loadLeaderboardMetadata(String leaderboardId, boolean forceReload) {
        return com.google.android.gms.tasks.Tasks.forResult(this);
    }

    public Task<LeaderboardsClient> loadLeaderboardMetadata(int maxResults, boolean forceReload) {
        return com.google.android.gms.tasks.Tasks.forResult(this);
    }

    public Task<LeaderboardScoreBuffer> loadPlayerCenteredScores(String leaderboardId, int span, int collection, int maxResults) {
        return com.google.android.gms.tasks.Tasks.forResult(new LeaderboardScoreBuffer());
    }

    public Intent getLeaderboardIntent(String leaderboardId) {
        return new Intent();
    }

    public Intent getAllLeaderboardsIntent() {
        return new Intent();
    }
}
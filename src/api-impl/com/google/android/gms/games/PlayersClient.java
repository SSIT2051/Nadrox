package com.google.android.gms.games;

import android.content.Context;
import android.content.Intent;

import com.google.android.gms.tasks.Task;

public class PlayersClient {
    private final Context context;

    PlayersClient(Context context) {
        this.context = context;
    }

    public Task<PlayerBuffer> loadCurrentPlayer() {
        return com.google.android.gms.tasks.Tasks.forResult(new PlayerBuffer());
    }

    public Task<PlayerBuffer> loadPlayer(String playerId) {
        return com.google.android.gms.tasks.Tasks.forResult(new PlayerBuffer());
    }

    public Task<PlayerBuffer> loadPlayers(int pageSize, boolean forceReload) {
        return com.google.android.gms.tasks.Tasks.forResult(new PlayerBuffer());
    }

    public Task<PlayerBuffer> loadMorePlayers(PlayerBuffer buffer, int maxResults) {
        return com.google.android.gms.tasks.Tasks.forResult(new PlayerBuffer());
    }

    public Intent getPlayerSearchIntent() {
        return new Intent();
    }

    public Intent getCompareProfileIntent(String playerId) {
        return new Intent();
    }

    public Intent getProfileIntent(String playerId) {
        return new Intent();
    }
}
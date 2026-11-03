package com.google.android.gms.games;

import com.google.android.gms.tasks.Task;
import com.google.android.gms.tasks.Tasks;
import android.content.Intent;

public class PlayersClient {
    public Task<Player> getCurrentPlayer() {
        return Tasks.forResult(null);
    }

    public Task<String> getCurrentPlayerId() {
        return Tasks.forResult(null);
    }

    public Task<Intent> getCompareProfileIntent(String playerId) {
        return Tasks.forResult(new Intent());
    }
}

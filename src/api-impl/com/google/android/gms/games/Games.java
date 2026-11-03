package com.google.android.gms.games;

import android.content.Context;
import com.google.android.gms.auth.api.signin.GoogleSignInAccount;

public class Games {
    public static LeaderboardsClient getLeaderboardsClient(Context context, GoogleSignInAccount account) {
        return new LeaderboardsClient();
    }

    public static AchievementsClient getAchievementsClient(Context context, GoogleSignInAccount account) {
        return new AchievementsClient();
    }

    public static PlayersClient getPlayersClient(Context context, GoogleSignInAccount account) {
        return new PlayersClient();
    }
}

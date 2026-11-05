package com.google.android.gms.games;

import com.google.android.gms.common.data.DataBuffer;
import com.google.android.gms.common.data.DataHolder;

public class LeaderboardScoreBuffer extends DataBuffer<LeaderboardScore> {
    LeaderboardScoreBuffer() {
        super(new DataHolder());
    }

    LeaderboardScoreBuffer(DataHolder dataHolder) {
        super(dataHolder);
    }
}
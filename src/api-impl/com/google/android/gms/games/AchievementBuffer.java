package com.google.android.gms.games;

import com.google.android.gms.common.data.DataBuffer;
import com.google.android.gms.common.data.DataHolder;

public class AchievementBuffer extends DataBuffer<Achievement> {
    AchievementBuffer() {
        super(new DataHolder());
    }

    AchievementBuffer(DataHolder dataHolder) {
        super(dataHolder);
    }
}
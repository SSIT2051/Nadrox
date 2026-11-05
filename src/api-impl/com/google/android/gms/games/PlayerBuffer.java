package com.google.android.gms.games;

import com.google.android.gms.common.data.DataBuffer;
import com.google.android.gms.common.data.DataHolder;

public class PlayerBuffer extends DataBuffer<Player> {
    PlayerBuffer() {
        super(new DataHolder());
    }

    PlayerBuffer(DataHolder dataHolder) {
        super(dataHolder);
    }
}
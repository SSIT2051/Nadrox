package com.google.android.gms.games;

public class Achievement {
    public String achievementId;
    public String name;
    public String description;
    public int type; // TYPE_STANDARD = 1, TYPE_INCREMENTAL = 2
    public int state; // STATE_HIDDEN = 1, STATE_REVEALED = 2, STATE_UNLOCKED = 3
    public int currentSteps;
    public int totalSteps;
    public String formattedCurrentSteps;
    public String formattedTotalSteps;
    public long lastUpdatedTimestamp;
    public String imageUrl;
    public String unlockedImageUrl;
}
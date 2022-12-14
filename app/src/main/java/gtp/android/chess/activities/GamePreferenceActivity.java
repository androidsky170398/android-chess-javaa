package gtp.android.chess.activities;

import android.os.Bundle;

import gtp.android.chess.R;

public class GamePreferenceActivity extends BasePreferenceActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        addPreferencesFromResource(R.xml.game_prefs);

    }
}

package gtp.android.chess.activities;

import android.os.Bundle;

import gtp.android.chess.R;

public class GlobalPreferencesActivity extends BasePreferenceActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        addPreferencesFromResource(R.xml.global_prefs);

    }
}


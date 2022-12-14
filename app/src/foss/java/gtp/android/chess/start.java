package gtp.android.chess;

import android.os.Bundle;

import gtp.android.chess.activities.StartBaseActivity;

public class start extends StartBaseActivity {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        layoutResource = R.layout.start_foss;

        super.onCreate(savedInstanceState);
    }
}

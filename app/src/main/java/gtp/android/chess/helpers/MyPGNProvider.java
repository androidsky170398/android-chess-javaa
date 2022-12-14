package gtp.android.chess.helpers;

import android.content.UriMatcher;
import android.net.Uri;

import gtp.chess.PGNProvider;

public class MyPGNProvider extends PGNProvider {

    static {
        AUTHORITY = "gtp.android.chess.helpers.MyPGNProvider";
        CONTENT_URI = Uri.parse("content://" + AUTHORITY + "/games");

        sUriMatcher = new UriMatcher(UriMatcher.NO_MATCH);
        sUriMatcher.addURI(AUTHORITY, "games", GAMES);
        sUriMatcher.addURI(AUTHORITY, "games/#", GAMES_ID);
    }
}

package gtp.android.chess.tools;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.os.Handler;

import gtp.android.chess.puzzle.MyPuzzleProvider;
import gtp.android.chess.services.GameApi;
import gtp.chess.PGNColumns;

public class PuzzleImportProcessor extends PGNProcessor {
    private static final String TAG = "PuzzleImportProcessor";

    private GameApi gameApi;
    private ContentResolver contentResolver;

    public PuzzleImportProcessor(int mode, Handler updateHandler, GameApi gameApi, ContentResolver contentResolver) {
        super(mode, updateHandler);
        this.gameApi = gameApi;
        this.contentResolver = contentResolver;
    }

    @Override
    public synchronized boolean processPGN(final String sPGN) {

        if (gameApi.loadPGN(sPGN)) {
            ContentValues values = new ContentValues();
            values.put(PGNColumns.PGN, gameApi.exportFullPGN());

            contentResolver.insert(MyPuzzleProvider.CONTENT_URI_PUZZLES, values);
            return true;
        }
        return false;
    }

    @Override
    public String getString() {
        // TODO Auto-generated method stub
        return null;
    }

}

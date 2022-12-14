#include "Game.h"

int Game::DB_SIZE = 0;
FILE *Game::DB_FP = NULL;
int Game::DB_DEPTH = 5;

Game::Game(void) {
    m_board = new ChessBoard();
    m_promotionPiece = ChessBoard::QUEEN;
    for (int i = 0; i < MAX_DEPTH; i++) {
        m_boardFactory[i] = new ChessBoard();
    }
    m_boardRefurbish = new ChessBoard();

    DB_SIZE = 0;

    m_bSearching = false;
    reset();
}

// TODO clean up all chessboards related to m_board
Game::~Game(void) {
    if (DB_FP) {
        fclose(DB_FP);
        DB_FP = NULL;
    }
    DB_SIZE = 0;

    // clean up history
    ChessBoard *cb, *tmp;
    while ((cb = m_board->undoMove()) != NULL) {
        tmp = m_board;
        m_board = cb;
        delete tmp;
    }

    delete m_board;

    delete m_boardRefurbish;

    for (int i = 0; i < MAX_DEPTH; i++) {
        delete m_boardFactory[i];
    }
}

void Game::reset() {
    // clean up history
    ChessBoard *cb, *tmp;
    while ((cb = m_board->undoMove()) != NULL) {
        tmp = m_board;
        m_board = cb;
        delete tmp;
    }
    //

    m_board->reset();
    m_board->calcState(m_boardRefurbish);
}

void Game::commitBoard() {
    m_board->commitBoard();
    m_board->calcState(m_boardRefurbish);
}

ChessBoard *Game::getBoard() {
    return m_board;
}

void Game::setPromo(int p) {
    m_promotionPiece = p;
}

int Game::getBestMove() {
    return m_bestMove;
}
int Game::getBestMoveAt(int ply) {
    if (ply >= 0 && ply < MAX_DEPTH) {
        return m_arrPVMoves[ply];
    }
    return 0;
}

boolean Game::requestMove(int from, int to) {
    ChessBoard *nb = new ChessBoard();
    if (m_board->requestMove(from, to, nb, m_boardRefurbish, m_promotionPiece)) {
        m_board = nb;
        return true;
    } else {
        delete nb;
        DEBUG_PRINT("%d-%d not moved...(request)\n", from, to);
        return false;
    }
}

boolean Game::move(int move) {
    ChessBoard *nb = new ChessBoard();

    if (m_board->requestMove(move, nb, m_boardRefurbish)) {
        m_board = nb;
        return true;
    } else {
        delete nb;
        return false;
    }
}

void Game::undo() {
    ChessBoard *tmp = m_board;
    ChessBoard *cb = m_board->undoMove();
    if (cb != NULL) {
        m_board = cb;
        delete tmp;
    }
}

#pragma region Search methods

// returns the move found
void Game::setSearchTime(int secs) {
    m_milliesGiven = 1000 * (long) secs;
    m_bSearching = true;
}

void Game::search() {
    m_bSearching = true;
    m_bestMove = 0;
    m_bestValue = 0;
    m_bInterrupted = false;
    m_evalCount = 0;

    // 50 move rule and repetition check
    // no need to search if the game has allready ended
    if (m_board->isEnded()) {
        m_bSearching = false;
        return;
    }

    int move = searchDB();
    if (move != 0) {
        m_bestMove = move;
        m_bSearching = false;

        return;
    }

    startTime();

    // DEBUG_PRINT("Start alphabeta iterative deepening\n", 0);

    char buf[20];
    // reset principal variation for this search
    int i;
    for (i = 0; i < MAX_DEPTH; i++) {
        m_arrPVMoves[i] = 0;
    }

    int reachedDepth = 0;
    boolean bContinue = true;
    for (m_searchDepth = 1; m_searchDepth < (MAX_DEPTH - QUIESCE_DEPTH); m_searchDepth++) {
        // DEBUG_PRINT("Search at depth %d\n", m_searchDepth);

        bContinue =
            alphaBetaRoot(m_searchDepth, -ChessBoard::VALUATION_MATE, ChessBoard::VALUATION_MATE);

        // todo -----------------------------------------------------------
        // break; //debug

        if (bContinue) {
            reachedDepth++;
            if (m_bestValue == ChessBoard::VALUATION_MATE) {
                DEBUG_PRINT("Found checkmate, stopping search\n", 0);
                break;
            }
            // bail out if we're over 50% of time, next depth will take more than sum of previous
            if (usedTime()) {
                // DEBUG_PRINT("Bailing out\n", 0);
                break;
            }
        } else {
            if (m_bInterrupted) {
                DEBUG_PRINT("Interrupted search\n", 0);
            } else {
                DEBUG_PRINT("No continuation, only one move\n", 0);
            }
            break;
        }
    }

    Move::toDbgString(m_bestMove, buf);
    DEBUG_PRINT(
        "\n=====\nSearch\nvalue\t%d\nevalCnt\t%d\nMove\t%s\ndepth\t%d\nTime\t%ld ms\nNps\t%.2f\n",
        m_bestValue,
        m_evalCount,
        buf,
        reachedDepth,
        timePassed(),
        (double) m_evalCount / timePassed());

    m_bSearching = false;
}

boolean Game::alphaBetaRoot(const int depth, int alpha, const int beta) {
    if (m_bInterrupted || depth >= MAX_DEPTH) {
        return false;  //
    }
    int value = 0;

    m_board->scoreMovesPV(m_arrPVMoves[m_searchDepth - depth]);

    int best = (-ChessBoard::VALUATION_MATE) - 1;
    int move = 0, bestMove = 0;
    ChessBoard *nextBoard = m_boardFactory[depth];

    while (m_board->hasMoreMoves()) {
        move = m_board->getNextScoredMove();
        m_board->makeMove(move, nextBoard);
        // self check is illegal!
        if (nextBoard->checkInSelfCheck()) {
            // not valid, remove this move and continue
            m_board->removeMoveElementAt();
            continue;
        }

        // generate the moves for this next board in order to validate the board
        nextBoard->genMoves();

        if (nextBoard->checkInCheck()) {
            nextBoard->setMyMoveCheck();
            move = Move_setCheck(move);
        }
        // ok valid move
        // at depth one is at the leaves, so call quiescent search
        if (depth == 1) {
            value = -quiesce(nextBoard, QUIESCE_DEPTH, -beta, -alpha);
        } else {
            // if not PV then narrow search window

            value = -alphaBeta(nextBoard, depth - 1, -beta, -alpha);
        }

        if (value > best) {
            best = value;
            bestMove = move;
            m_arrPVMoves[m_searchDepth - depth] = move;
        }

        if (best > alpha) {
            alpha = best;
        }

        if (best >= beta) {
            break;
        }
    }

    // we're interrupted, no move and no value!
    if (m_bInterrupted) {
        return false;
    }

    m_bestMove = bestMove;
    m_bestValue = best;

    if (m_board->getNumMoves() <= 1) {
        if (m_board->getNumMoves() == 0) {
            DEBUG_PRINT("NO moves in aphaBetaRoot!", 0);
        }
        // do not contine, so return false
        return false;
    }

    return true;
}

// alphaBeta
int Game::alphaBeta(ChessBoard *board, const int depth, int alpha, const int beta) {
    if (m_evalCount % 1000 == 0) {
        if (timeUp()) {
            m_bInterrupted = true;
        }
    }
    if (m_bInterrupted || depth >= MAX_DEPTH) {
        return alpha;  //
    }
    int value = 0;

    // 50 move rule and repetition check
    if (board->checkEnded()) {
        return ChessBoard::VALUATION_DRAW;
    }
    board->scoreMovesPV(m_arrPVMoves[m_searchDepth - depth]);

    int best = (-ChessBoard::VALUATION_MATE) - 1;
    int move = 0;
    ChessBoard *nextBoard = m_boardFactory[depth];

    while (board->hasMoreMoves()) {
        move = board->getNextScoredMove();
        board->makeMove(move, nextBoard);

        // self check is illegal!
        if (nextBoard->checkInSelfCheck()) {
            // not valid, remove this move and continue
            board->removeMoveElementAt();
            continue;
        }

        // generate the moves for this next board in order to validate the board
        nextBoard->genMoves();

        if (nextBoard->checkInCheck()) {
            nextBoard->setMyMoveCheck();
            move = Move_setCheck(move);
        }
        // ok valid move
        // at depth one is at the leaves, so call quiescent search
        if (depth == 1) {
            value = -quiesce(nextBoard, QUIESCE_DEPTH, -beta, -alpha);

        } else {
            value = -alphaBeta(nextBoard, depth - 1, -beta, -alpha);
        }

        if (value > best) {
            best = value;
            m_arrPVMoves[m_searchDepth - depth] = move;
        }

        if (best > alpha) {
            alpha = best;
        }

        if (best >= beta) {
            break;
        }
    }
    // no valid moves, so mate or stalemate
    if (board->getNumMoves() == 0) {
        if (Move_isCheck(board->getMyMove())) {
            return (-ChessBoard::VALUATION_MATE);
        }
        return ChessBoard::VALUATION_DRAW;
    }

    return best;
}

//
int Game::quiesce(ChessBoard *board, const int depth, int alpha, const int beta) {
    // before any evaluation, first check if time is up
    if (m_evalCount % 1000 == 0) {
        if (timeUp()) {
            m_bInterrupted = true;
        }
    }

    if (m_bInterrupted || depth >= MAX_DEPTH) {
        return alpha;  //
    }

    // administer evaluation count and get the board value
    m_evalCount++;
    const int boardValue = board->boardValueExtension();
    int value;

    // at this point there can be a beta cutt-off unless we're in check

    if (!Move_isCheck(board->getMyMove())) {
        if (boardValue >= beta) {
            return beta;
        }
    }

    if (depth == 0)  // ok, max quiesce depth is reached; return this value
    {
        // get the value of this node
        return boardValue;
    }

    if (boardValue >= beta) {
        return beta;
    }

    // futility pruning here? i.e. can alpha be improved

    // update lower bound
    if (boardValue > alpha) {
        alpha = boardValue;
    }
    int move;
    // int best = (-ChessBoard::VALUATION_MATE)-1;

    ChessBoard *nextBoard = m_boardFactory[MAX_DEPTH - depth];
    board->scoreMoves();
    while (board->hasMoreMoves()) {
        move = board->getNextScoredMove();

        board->makeMove(move, nextBoard);

        // self check is illegal!
        if (nextBoard->checkInSelfCheck()) {
            // not valid, remove this move and continue
            board->removeMoveElementAt();
            continue;
        }

        // generate the moves for this next board in order to validate the board
        // todo, reset this if attackedMoveSquares are working
        // nextBoard->genMoves();

        if (nextBoard->checkInCheck()) {
            nextBoard->setMyMoveCheck();
            move = Move_setCheck(move);
        }

        // quiescent search
        if (Move_isHIT(move) || Move_isCheck(move) || Move_isPromotionMove(move)) {
            // a valid and active move, so continue quiescent search

            // optimization; only generate when needed
            // todo back to before checkInCheck?
            nextBoard->genMoves();

            value = -quiesce(nextBoard, depth - 1, -beta, -alpha);

            if (value > alpha) {
                alpha = value;

                if (value >= beta) {
                    return beta;
                }
            }
        }
    }

    // no valid moves, so mate or stalemate
    if (board->getNumMoves() == 0) {
        if (Move_isCheck(board->getMyMove())) {
            return (-ChessBoard::VALUATION_MATE);
        }
        return ChessBoard::VALUATION_DRAW;
    }

    return alpha;
}

// limited search, no quiesce
void Game::searchLimited(const int depth) {
    m_bSearching = true;
    m_bestMove = 0;
    m_bestValue = 0;
    m_bInterrupted = false;
    m_evalCount = 0;

    // 50 move rule and repetition check
    // no need to search if the game has allready ended
    if (m_board->isEnded()) {
        m_bSearching = false;
        return;
    }

    int move = searchDB();
    if (move != 0) {
        m_bestMove = move;
        m_bSearching = false;

        return;
    }

    m_searchDepth = depth;
    alphaBetaLimited(m_board, depth, -ChessBoard::VALUATION_MATE, ChessBoard::VALUATION_MATE);

    char buf[32];
    Move::toDbgString(m_bestMove, buf);
    DEBUG_PRINT("\n=====\nSearch\nvalue\t%d\nevalCnt\t%d\nMove\t%s\ndepth\t%d\n\n",
                m_bestValue,
                m_evalCount,
                buf,
                depth);

    m_bSearching = false;
}
int Game::alphaBetaLimited(ChessBoard *board, const int depth, int alpha, const int beta) {
    int value = 0;

    // 50 move rule and repetition check
    if (board->checkEnded()) {
        return ChessBoard::VALUATION_DRAW;
    }
    board->scoreMovesPV(m_arrPVMoves[m_searchDepth - depth]);

    int best = (-ChessBoard::VALUATION_MATE) - 1, bestMove = 0;
    int move = 0;
    ChessBoard *nextBoard = m_boardFactory[depth];

    while (board->hasMoreMoves()) {
        move = board->getNextScoredMove();
        board->makeMove(move, nextBoard);

        // self check is illegal!
        if (nextBoard->checkInSelfCheck()) {
            // not valid, remove this move and continue
            board->removeMoveElementAt();
            continue;
        }

        // generate the moves for this next board in order to validate the board
        nextBoard->genMoves();

        if (nextBoard->checkInCheck()) {
            nextBoard->setMyMoveCheck();
            move = Move_setCheck(move);
        }
        // ok valid move

        // at depth one is at the leaves, so call board value
        if (depth == 1) {
            m_evalCount++;
            value = -nextBoard->boardValueExtension();

        } else {
            value = -alphaBetaLimited(nextBoard, depth - 1, -beta, -alpha);
        }

        if (value > best) {
            best = value;
            m_arrPVMoves[m_searchDepth - depth] = move;
            bestMove = move;
        }

        if (best > alpha) {
            alpha = best;
        }

        if (best >= beta) {
            break;
        }
    }
    // no valid moves, so mate or stalemate
    if (board->getNumMoves() == 0) {
        if (Move_isCheck(board->getMyMove())) {
            return (-ChessBoard::VALUATION_MATE);
        }
        return ChessBoard::VALUATION_DRAW;
    }

    if (depth == m_searchDepth) {
        m_bestMove = bestMove;
        m_bestValue = best;
    }
    return best;
}

// search the hashkey database, randomly choose a move
int Game::searchDB() {
    if (DB_SIZE == 0 || DB_FP == NULL) {
        return 0;
    }
    if (m_board->getNumBoard() > Game::DB_DEPTH) {
        DEBUG_PRINT("Too many plies for database search\n", 0);
        return 0;
    }
    if (m_board->getFirstBoard()->getHashKey() != DEFAULT_START_HASH) {
        DEBUG_PRINT("Game not from default starting position (database search)\n", 0);
        return 0;
    }
    ChessBoard *tmpBoard = new ChessBoard();

    int moveArr[100], iCnt = 0, move;
    //
    m_board->getMoves();
    while (m_board->hasMoreMoves() && iCnt < 100) {
        move = m_board->getNextMove();
        m_board->makeMove(move, tmpBoard);
        const BITBOARD bb = tmpBoard->getHashKey();

        if (findDBKey(bb) < DB_SIZE) {
            moveArr[iCnt++] = move;
        }
    }
    if (iCnt == 0) {
        DEBUG_PRINT("No move found in openingsdatabase\n", 0);
        return 0;
    }

    DEBUG_PRINT("Choosing from %d moves\n", iCnt);

    timeval time;
    gettimeofday(&time, NULL);
    srand((unsigned int) time.tv_usec);
    int i = rand() % iCnt;
    return moveArr[i];
}

// House variant search
// allowAttack as for putPieceHouse
/*
int Game::searchHouse(boolean allowAttack)
{
        return 0;
}
*/

// allowAttack, if a new piece on the board is allowed to attack immediatly,
// for crazyhouse that's ok, for parachute not
boolean Game::putPieceHouse(const int pos, const int piece, const boolean allowAttack) {
    ChessBoard *nextBoard = new ChessBoard();
    if (m_board->putHouse(pos, piece, nextBoard, m_boardRefurbish, allowAttack)) {
        m_board = nextBoard;
        return true;
    }
    delete nextBoard;
    return false;
}

#pragma endregion

#pragma region Database methods

void Game::loadDB(const char *sFile, int depth) {
    if (DB_FP != NULL) {
        fclose(DB_FP);
        DB_FP = NULL;
        DEBUG_PRINT("Closing database...\n", 0);
    }
    DB_DEPTH = depth;
    DB_SIZE = 0;
    DB_FP = fopen(sFile, "rb");
    if (DB_FP != NULL) {
        fseek(DB_FP, 0, SEEK_END);
        DB_SIZE = ftell(DB_FP) / 8;
        rewind(DB_FP);

        /*
        BITBOARD bb, bbPrev = 0;
        for(int i = 0; i < DB_SIZE; i++){

            if(readDBAt(i, bb)){
                if(bb > bbPrev)
                    DEBUG_PRINT("DB %d = %lld bigger\n", i, bb);
                else
                    DEBUG_PRINT("DB %d = %lld SMALLER\n", i, bb);

                bbPrev = bb;
            }
        }

        long pos = findDBKey(bb);
        if(pos){

            DEBUG_PRINT("FOUND KEY %lld...%ld\n", bb, pos);
        }
        */
        DEBUG_PRINT("Set filepointer ok, filesize %ld...\n", DB_SIZE);

    } else {
        DEBUG_PRINT("Could not open file %s\n", sFile);
    }
}

// Binary search returns leftmost entry or size of db
long Game::findDBKey(BITBOARD bbKey) {
    int left, right, mid;
    BITBOARD bb;

    left = 0;
    right = DB_SIZE - 1;

    while (left < right) {
        mid = (left + right) / 2;

        if (readDBAt(mid, bb)) {
            // DEBUG_PRINT("Binary search %d-%d = %lld\n", left, right, bb);

            if (bbKey <= bb) {
                right = mid;
            } else {
                left = mid + 1;
            }
        } else {
            return DB_SIZE;
        }
    }

    if (readDBAt(left, bb)) {
        return (bb == bbKey) ? left : DB_SIZE;
    } else {
        return DB_SIZE;
    }
}

boolean Game::readDBAt(int iPos, BITBOARD &bb) {
    if (iPos < DB_SIZE && fseek(DB_FP, iPos * 8, SEEK_SET) == 0) {
        static char buf[8];
        int cnt = fread(buf, 1, 8, DB_FP);

        if (cnt != 8) {
            DEBUG_PRINT("Could not read database at %d\n", iPos);
            return false;
        }

        // memcpy(&bb, buf, 8);
        bb = 0;
        bb |= ((BITBOARD) buf[0] & 0xFF) << 56;
        bb |= ((BITBOARD) buf[1] & 0xFF) << 48;
        bb |= ((BITBOARD) buf[2] & 0xFF) << 40;
        bb |= ((BITBOARD) buf[3] & 0xFF) << 32;
        bb |= ((BITBOARD) buf[4] & 0xFF) << 24;
        bb |= ((BITBOARD) buf[5] & 0xFF) << 16;
        bb |= ((BITBOARD) buf[6] & 0xFF) << 8;
        bb |= ((BITBOARD) buf[7] & 0xFF);

        return true;
        // sprintf(s, "===\n %lld {%d %d %d %d %d %d %d %d}\n", bb, buf[0], buf[1], buf[2], buf[3],
        // buf[4], buf[5], buf[6], buf[7]); DEBUG_PRINT(s);
    }
    DEBUG_PRINT("Position not found\n", 0);
    return false;
}

#pragma endregion

#pragma region Search timing functions

void Game::startTime() {
    timeval time;
    gettimeofday(&time, NULL);
    m_millies = (time.tv_sec * 1000) + (time.tv_usec / 1000);
}

boolean Game::timeUp() {
    timeval time;
    gettimeofday(&time, NULL);
    return (m_milliesGiven < ((time.tv_sec * 1000) + (time.tv_usec / 1000) - m_millies));
}

// return true if we consumed more than x'd of tme
boolean Game::usedTime() {
    timeval time;
    gettimeofday(&time, NULL);
    return ((m_milliesGiven / 3) < ((time.tv_sec * 1000) + (time.tv_usec / 1000) - m_millies));
}

long Game::timePassed() {
    timeval time;
    gettimeofday(&time, NULL);
    return ((time.tv_sec * 1000) + (time.tv_usec / 1000) - m_millies);
}

#pragma endregion
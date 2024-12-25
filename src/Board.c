// Board.cpp

#include "stdafx.h"

#include "Board.h"

#include "BitBoard.h"
#include "Def.h"
#include "Gen.h"
#include "Hash.h"
#include "Move.h"
#include "NNUE2.h"
#include "Types.h"

const char* BoardName[64] = {
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1"
};

const char PiecesCharWhite[6] = { 'P', 'N', 'B', 'R', 'Q', 'K' };
const char PiecesCharBlack[6] = { 'p', 'n', 'b', 'r', 'q', 'k' };

// Castle permission flags (part 2)

const int CastleMask[64] = {
     7, 15, 15, 15,  3, 15, 15, 11,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    13, 15, 15, 15, 12, 15, 15, 14
};

/*
    Algebraic notation (https://en.wikipedia.org/wiki/Algebraic_notation_(chess))

    Examples:
        Moves: c5, Nf3, Bdb8, R1a3, Qh4e1
        Captures: exd5, Bxe5, Bdxb8, R1xa3, Qh4xe1
        Pawn promotion: e8=Q
        Castling: O-O, O-O-O
        Check: +
        Checkmate: #
*/
const char MoveFirstChar[] = "abcdefghNBRQKO";
const char MoveSubsequentChar[] = "abcdefgh012345678x=NBRQ-O+#";

const char StartFen[] = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

BoardItem CurrentBoard;

#ifdef LATE_MOVE_PRUNING
int LateMovePruningTable[7]; // [LMP depth]
#endif // LATE_MOVE_PRUNING

#ifdef LATE_MOVE_REDUCTION
int LateMoveReductionTable[64][64]; // [LMR depth][Move number]
#endif // LATE_MOVE_REDUCTION

BOOL IsSquareAttacked(const BoardItem* Board, const int Square, const int Color)
{
    assert(Square >= 0 && Square <= 63);
    assert(Color == WHITE || Color == BLACK);

    int EnemyColor = CHANGE_COLOR(Color);

    U64 Attackers = 0UL;

    // Pawns
    Attackers |= PawnAttacks(BB_SQUARE(Square), Color) & Board->BB_Pieces[EnemyColor][PAWN];

    // Knights
    Attackers |= KnightAttacks(Square) & Board->BB_Pieces[EnemyColor][KNIGHT];

    // Bishops or Queens
    Attackers |= BishopAttacks(Square, (Board->BB_WhitePieces | Board->BB_BlackPieces)) & (Board->BB_Pieces[EnemyColor][BISHOP] | Board->BB_Pieces[EnemyColor][QUEEN]);

    // Rooks or Queens
    Attackers |= RookAttacks(Square, (Board->BB_WhitePieces | Board->BB_BlackPieces)) & (Board->BB_Pieces[EnemyColor][ROOK] | Board->BB_Pieces[EnemyColor][QUEEN]);

    // Kings
    Attackers |= KingAttacks(Square) & Board->BB_Pieces[EnemyColor][KING];

    return !!Attackers;
}

BOOL IsInCheck(const BoardItem* Board, const int Color)
{
    assert(Color == WHITE || Color == BLACK);

    int KingSquare = LSB(Board->BB_Pieces[Color][KING]);

    return IsSquareAttacked(Board, KingSquare, Color);
}

BOOL HasLegalMoves(BoardItem* Board)
{
    int GenMoveCount;
    MoveItem MoveList[MAX_GEN_MOVES];

    GenMoveCount = 0;
    GenerateAllMoves(Board, NULL, MoveList, &GenMoveCount);

    for (int MoveNumber = 0; MoveNumber < GenMoveCount; ++MoveNumber) {
        MakeMove(Board, MoveList[MoveNumber]);

        if (IsInCheck(Board, CHANGE_COLOR(Board->CurrentColor))) { // Illegal move
            UnmakeMove(Board);

            continue; // Next move
        }

        // Legal move

        UnmakeMove(Board);

        return TRUE;
    }

    // No legal moves

    return FALSE;
}

void NotateMove(BoardItem* Board, const MoveItem Move, char* Result)
{
    int From = MOVE_FROM(Move.Move);
    int To = MOVE_TO(Move.Move);

    int PieceTypeFrom = PIECE_TYPE(Board->Pieces[From]);

    int GenMoveCount;
    MoveItem MoveList[MAX_GEN_MOVES];

    BOOL GiveCheck;
    BOOL LegalMoves = FALSE;

    BOOL Ambiguous = FALSE;
    BOOL AmbiguousFile = FALSE;
    BOOL AmbiguousRank = FALSE;

    BOOL ShowFileFrom = FALSE;
    BOOL ShowRankFrom = FALSE;

    if (PieceTypeFrom == PAWN && (Move.Type & MOVE_CAPTURE)) {
        ShowFileFrom = TRUE;
    }

    GenMoveCount = 0;
    GenerateAllMoves(Board, NULL, MoveList, &GenMoveCount);

    for (int MoveNumber = 0; MoveNumber < GenMoveCount; ++MoveNumber) {
        if (MOVE_TO(MoveList[MoveNumber].Move) != To) {
            continue; // Next move
        }

        if (MOVE_FROM(MoveList[MoveNumber].Move) == From) {
            continue; // Next move
        }

        if (PIECE_TYPE(Board->Pieces[MOVE_FROM(MoveList[MoveNumber].Move)]) != PieceTypeFrom) {
            continue; // Next move
        }

        if ((MoveList[MoveNumber].Type & MOVE_PAWN_PROMOTE) && MOVE_PROMOTE_PIECE_TYPE(MoveList[MoveNumber].Move) != QUEEN) {
            continue; // Next move
        }

        MakeMove(Board, MoveList[MoveNumber]);

        if (IsInCheck(Board, CHANGE_COLOR(Board->CurrentColor))) { // Illegal move
            UnmakeMove(Board);

            continue; // Next move
        }

        Ambiguous = TRUE;

        if (FILE(MOVE_FROM(MoveList[MoveNumber].Move)) == FILE(From)) {
            AmbiguousFile = TRUE;
        }

        if (RANK(MOVE_FROM(MoveList[MoveNumber].Move)) == RANK(From)) {
            AmbiguousRank = TRUE;
        }

        UnmakeMove(Board);
    }

    if (Ambiguous) {
        if (AmbiguousFile && AmbiguousRank) {
            ShowFileFrom = TRUE;
            ShowRankFrom = TRUE;
        }
        else if (AmbiguousFile) {
            ShowRankFrom = TRUE;
        }
        else if (AmbiguousRank) {
            ShowFileFrom = TRUE;
        }
        else {
            ShowFileFrom = TRUE;
        }
    }

    MakeMove(Board, Move);

    if (IsInCheck(Board, CHANGE_COLOR(Board->CurrentColor))) { // Illegal move
        UnmakeMove(Board);

        strcpy(Result, "(none)\0");

        return;
    }

    GiveCheck = IsInCheck(Board, Board->CurrentColor);

    if (GiveCheck) {
        LegalMoves = HasLegalMoves(Board);
    }

    UnmakeMove(Board);

    if (Move.Type == MOVE_CASTLE_KING) {
        strcpy(Result, "O-O\0");
        Result += 3;
    }
    else if (Move.Type == MOVE_CASTLE_QUEEN) {
        strcpy(Result, "O-O-O\0");
        Result += 5;
    }
    else {
        switch (PieceTypeFrom) {
            case KNIGHT:
                *Result++ = 'N';
                break;

            case BISHOP:
                *Result++ = 'B';
                break;

            case ROOK:
                *Result++ = 'R';
                break;

            case QUEEN:
                *Result++ = 'Q';
                break;

            case KING:
                *Result++ = 'K';
                break;
        }

        if (ShowFileFrom) {
            *Result++ = 'a' + (char)FILE(From);
        }

        if (ShowRankFrom) {
            *Result++ = '8' - (char)RANK(From);
        }

        if (Move.Type & MOVE_CAPTURE) {
            *Result++ = 'x';
        }

        *Result++ = 'a' + (char)FILE(To);
        *Result++ = '8' - (char)RANK(To);

        if (Move.Type & MOVE_PAWN_PROMOTE) {
            *Result++ = '=';

            switch (MOVE_PROMOTE_PIECE_TYPE(Move.Move)) {
                case KNIGHT:
                    *Result++ = 'N';
                    break;

                case BISHOP:
                    *Result++ = 'B';
                    break;

                case ROOK:
                    *Result++ = 'R';
                    break;

                case QUEEN:
                    *Result++ = 'Q';
                    break;
            }
        }
    }

    if (GiveCheck) {
        if (LegalMoves) { // Check
            *Result++ = '+';
        }
        else { // Checkmate
            *Result++ = '#';
        }
    }

    *Result = '\0'; // Nul
}

int PositionRepeat1(const BoardItem* Board)
{
    if (Board->FiftyMove < 4) {
        return 0;
    }

    for (int Index = Board->HalfMoveNumber - 2; Index >= Board->HalfMoveNumber - Board->FiftyMove; Index -= 2) {
        if (Board->MoveTable[Index].Hash == Board->Hash) {
            return 1;
        }
    }

    return 0;
}

int PositionRepeat2(const BoardItem* Board)
{
    int RepeatCounter = 0;

    if (Board->FiftyMove < 4) {
        return 0;
    }

    for (int Index = Board->HalfMoveNumber - 2; Index >= Board->HalfMoveNumber - Board->FiftyMove; Index -= 2) {
        if (Board->MoveTable[Index].Hash == Board->Hash) {
            ++RepeatCounter;
        }
    }

    return RepeatCounter;
}

BOOL IsInsufficientMaterial(const BoardItem* Board)
{
    if (POPCNT(Board->BB_WhitePieces | Board->BB_BlackPieces) <= 3) { // KK or KxK
        if (Board->BB_Pieces[WHITE][PAWN] | Board->BB_Pieces[BLACK][PAWN]) { // KPK
            return FALSE;
        }

        if (Board->BB_Pieces[WHITE][ROOK] | Board->BB_Pieces[BLACK][ROOK]) { // KRK
            return FALSE;
        }

        if (Board->BB_Pieces[WHITE][QUEEN] | Board->BB_Pieces[BLACK][QUEEN]) { // KQK
            return FALSE;
        }

        return TRUE;
    }

    return FALSE;
}

void PrintBoard(BoardItem* Board)
{
    printf("\n");

    printf("     a   b   c   d   e   f   g   h\n");

    for (int Square = 0; Square < 64; ++Square) {
        if (Square % 8 == 0) {
            printf("   +---+---+---+---+---+---+---+---+\n");
            printf(" %d |", 8 - Square / 8);
        }

        if (Board->Pieces[Square] == NO_PIECE) {
            printf("   |");
        }
        else if (PIECE_COLOR(Board->Pieces[Square]) == WHITE) {
            printf(" %c |", PiecesCharWhite[PIECE_TYPE(Board->Pieces[Square])]);
        }
        else { // BLACK
            printf(" %c |", PiecesCharBlack[PIECE_TYPE(Board->Pieces[Square])]);
        }

        if ((Square + 1) % 8 == 0) {
            printf(" %d\n", 8 - Square / 8);
        }
    }

    printf("   +---+---+---+---+---+---+---+---+\n");
    printf("     a   b   c   d   e   f   g   h\n");

//    printf("\n");

//    printf("Hash = 0x%016lx\n", Board->Hash);

    printf("\n");

    printf("Static evaluate = %.2f\n", (double)Evaluate(Board) / 100.0);
}

void PrintBitMask(const U64 Mask)
{
    for (int Square = 0; Square < 64; ++Square) {
        if (Square > 0 && (Square % 8) == 0) {
            printf("\n");
        }

        if (Mask & BB_SQUARE(Square)) {
            printf("x");
        }
        else {
            printf("-");
        }
    }

    printf("\n");
}

int SetFen(BoardItem* Board, const char* Fen)
{
    const char* Part = Fen;

    int Square;

    int File;
    int Rank;

    int MoveNumber;

    // Clear board

    for (Square = 0; Square < 64; ++Square) {
        Board->Pieces[Square] = NO_PIECE;
    }

    Board->BB_WhitePieces = 0UL;
    Board->BB_BlackPieces = 0UL;

    for (int Color = 0; Color < 2; ++Color) {
        for (int Piece = 0; Piece < 6; ++Piece) {
            Board->BB_Pieces[Color][Piece] = 0UL;
        }
    }

    // Load pieces

    Square = 0;

    while (*Part != ' ') {
        if (*Part == 'P') { // White pawn
            Board->Pieces[Square] = PIECE_CREATE(PAWN, WHITE);

            Board->BB_WhitePieces |= BB_SQUARE(Square);
            Board->BB_Pieces[WHITE][PAWN] |= BB_SQUARE(Square);

            ++Square;
        }
        else if (*Part == 'p') { // Black pawn
            Board->Pieces[Square] = PIECE_CREATE(PAWN, BLACK);

            Board->BB_BlackPieces |= BB_SQUARE(Square);
            Board->BB_Pieces[BLACK][PAWN] |= BB_SQUARE(Square);

            ++Square;
        }
        else if (*Part == 'N') { // White knight
            Board->Pieces[Square] = PIECE_CREATE(KNIGHT, WHITE);

            Board->BB_WhitePieces |= BB_SQUARE(Square);
            Board->BB_Pieces[WHITE][KNIGHT] |= BB_SQUARE(Square);

            ++Square;
        }
        else if (*Part == 'n') { // Black knight
            Board->Pieces[Square] = PIECE_CREATE(KNIGHT, BLACK);

            Board->BB_BlackPieces |= BB_SQUARE(Square);
            Board->BB_Pieces[BLACK][KNIGHT] |= BB_SQUARE(Square);

            ++Square;
        }
        else if (*Part == 'B') { // White bishop
            Board->Pieces[Square] = PIECE_CREATE(BISHOP, WHITE);

            Board->BB_WhitePieces |= BB_SQUARE(Square);
            Board->BB_Pieces[WHITE][BISHOP] |= BB_SQUARE(Square);

            ++Square;
        }
        else if (*Part == 'b') { // Black bishop
            Board->Pieces[Square] = PIECE_CREATE(BISHOP, BLACK);

            Board->BB_BlackPieces |= BB_SQUARE(Square);
            Board->BB_Pieces[BLACK][BISHOP] |= BB_SQUARE(Square);

            ++Square;
        }
        else if (*Part == 'R') { // White rook
            Board->Pieces[Square] = PIECE_CREATE(ROOK, WHITE);

            Board->BB_WhitePieces |= BB_SQUARE(Square);
            Board->BB_Pieces[WHITE][ROOK] |= BB_SQUARE(Square);

            ++Square;
        }
        else if (*Part == 'r') { // Black rook
            Board->Pieces[Square] = PIECE_CREATE(ROOK, BLACK);

            Board->BB_BlackPieces |= BB_SQUARE(Square);
            Board->BB_Pieces[BLACK][ROOK] |= BB_SQUARE(Square);

            ++Square;
        }
        else if (*Part == 'Q') { // White queen
            Board->Pieces[Square] = PIECE_CREATE(QUEEN, WHITE);

            Board->BB_WhitePieces |= BB_SQUARE(Square);
            Board->BB_Pieces[WHITE][QUEEN] |= BB_SQUARE(Square);

            ++Square;
        }
        else if (*Part == 'q') { // Black queen
            Board->Pieces[Square] = PIECE_CREATE(QUEEN, BLACK);

            Board->BB_BlackPieces |= BB_SQUARE(Square);
            Board->BB_Pieces[BLACK][QUEEN] |= BB_SQUARE(Square);

            ++Square;
        }
        else if (*Part == 'K') { // White king
            Board->Pieces[Square] = PIECE_CREATE(KING, WHITE);

            Board->BB_WhitePieces |= BB_SQUARE(Square);
            Board->BB_Pieces[WHITE][KING] |= BB_SQUARE(Square);

            ++Square;
        }
        else if (*Part == 'k') { // Black king
            Board->Pieces[Square] = PIECE_CREATE(KING, BLACK);

            Board->BB_BlackPieces |= BB_SQUARE(Square);
            Board->BB_Pieces[BLACK][KING] |= BB_SQUARE(Square);

            ++Square;
        }
        else if (*Part >= '1' && *Part <= '8') { // Empty square(s)
            Square += *Part - '0';
        }

        ++Part;
    } // while

    // Load color

    ++Part; // Space

    if (*Part == 'w') {
        Board->CurrentColor = WHITE;

        ++Part;
    }
    else { // 'b'
        Board->CurrentColor = BLACK;

        ++Part;
    }

    // Load castle flags

    ++Part; // Space

    Board->CastleFlags = 0;

    if (*Part == '-') {
        ++Part;
    }
    else {
        while (*Part != ' ') {
            if (*Part == 'K') { // White O-O
                Board->CastleFlags |= CASTLE_WHITE_KING;
            }
            else if (*Part == 'Q') { // White O-O-O
                Board->CastleFlags |= CASTLE_WHITE_QUEEN;
            }
            else if (*Part == 'k') { // Black O-O
                Board->CastleFlags |= CASTLE_BLACK_KING;
            }
            else if (*Part == 'q') { // Black O-O-O
                Board->CastleFlags |= CASTLE_BLACK_QUEEN;
            }

            ++Part;
        }
    }

    // Load en passant square

    ++Part; // Space

    if (*Part == '-') {
        Board->PassantSquare = -1;

        ++Part;
    }
    else {
        File = Part[0] - 'a';
        Rank = 7 - (Part[1] - '1');

        Board->PassantSquare = SQUARE_CREATE(File, Rank);

        Part += 2;
    }

    // Load fifty move

    ++Part; // Space

    Board->FiftyMove = atoi(Part);

    while (*Part != ' ') {
        ++Part;
    }

    // Load move number

    ++Part; // Space

    MoveNumber = atoi(Part);

    Board->HalfMoveNumber = (MoveNumber - 1) * 2 + Board->CurrentColor;

    while (*Part != ' ' && *Part != '\r' && *Part != '\n' && *Part != '\0') {
        ++Part;
    }

    InitHash(Board);

    memset(Board->MoveTable, 0, sizeof(Board->MoveTable));

#ifdef USE_NNUE_UPDATE
    Board->Accumulator.AccumulationComputed = FALSE;
#endif // USE_NNUE_UPDATE

    return (int)(Part - Fen);
}

void GetFen(const BoardItem* Board, char* Fen)
{
    int EmptyCount = 0;

    // Save pieces

    for (int Square = 0; Square < 64; ++Square) {
        if (Square > 0 && (Square % 8) == 0) {
            if (EmptyCount > 0) {
                Fen += sprintf(Fen, "%d", EmptyCount);

                EmptyCount = 0;
            }

            *Fen++ = '/';
        }

        if (Board->Pieces[Square] == NO_PIECE) {
            ++EmptyCount;

            continue; // Next square
        }

        if (EmptyCount > 0) {
            Fen += sprintf(Fen, "%d", EmptyCount);

            EmptyCount = 0;
        }

        if (PIECE_COLOR(Board->Pieces[Square]) == WHITE) {
            Fen += sprintf(Fen, "%c", PiecesCharWhite[PIECE_TYPE(Board->Pieces[Square])]);
        }
        else { // BLACK
            Fen += sprintf(Fen, "%c", PiecesCharBlack[PIECE_TYPE(Board->Pieces[Square])]);
        }
    }

    if (EmptyCount > 0) {
        Fen += sprintf(Fen, "%d", EmptyCount);
    }

    // Save color

    *Fen++ = ' ';

    if (Board->CurrentColor == WHITE) {
        *Fen++ = 'w';
    }
    else { // BLACK
        *Fen++ = 'b';
    }

    // Save castle flags

    *Fen++ = ' ';

    if (!Board->CastleFlags) {
        *Fen++ = '-';
    }
    else {
        if (Board->CastleFlags & CASTLE_WHITE_KING) { // White O-O
            *Fen++ = 'K';
        }

        if (Board->CastleFlags & CASTLE_WHITE_QUEEN) { // White O-O-O
            *Fen++ = 'Q';
        }

        if (Board->CastleFlags & CASTLE_BLACK_KING) { // Black O-O
            *Fen++ = 'k';
        }

        if (Board->CastleFlags & CASTLE_BLACK_QUEEN) { // Black O-O-O
            *Fen++ = 'q';
        }
    }

    // Save en passant square

    *Fen++ = ' ';

    if (Board->PassantSquare == -1) {
        *Fen++ = '-';
    }
    else {
        Fen += sprintf(Fen, "%s", BoardName[Board->PassantSquare]);
    }

    // Save fifty move

    *Fen++ = ' ';

    Fen += sprintf(Fen, "%d", Board->FiftyMove);

    // Save move number

    *Fen++ = ' ';

    Fen += sprintf(Fen, "%d", Board->HalfMoveNumber / 2 + 1);

    *Fen = '\0'; // Nul
}

U64 CountLegalMoves(BoardItem* Board, const int Depth)
{
    int GenMoveCount;
    MoveItem MoveList[MAX_GEN_MOVES];

    U64 LegalMoveCount = 0UL;

    if (Depth == 0) {
        return 1UL;
    }

    GenMoveCount = 0;
    GenerateAllMoves(Board, NULL, MoveList, &GenMoveCount);

    for (int MoveNumber = 0; MoveNumber < GenMoveCount; ++MoveNumber) {
        MakeMove(Board, MoveList[MoveNumber]);

        if (!IsInCheck(Board, CHANGE_COLOR(Board->CurrentColor))) { // Legal move
            LegalMoveCount += CountLegalMoves(Board, Depth - 1);
        }

        UnmakeMove(Board);
    }

    return LegalMoveCount;
}
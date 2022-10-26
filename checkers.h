#ifndef CHECKERS_H
#define CHECKERS_H

#define Empty 0
#define Piece 1
#define King 2

#define HUMAN 1
#define COMPUTER 2

struct Square {
        Widget widget;
        int val;
        int state;
        int col;
        int hilite;
};

/** Available to all **/
void StopGame(void);
void NewGame(void);
void SquareChosen(struct Square *sq);

/** Used internally **/
void SquaresToBoard(int board[8][8]);
void AddMove(int move[2]);
void FindKingMoves(int board[8][8], int x, int y);
void FindMoves(int player, int board[8][8], int x, int y);
void AddJump(int move[12]);
int FindKingJump(int player, int board[8][8], int move[12], int len, int x, int y);
int FindJump(int player, int board[8][8], int move[12], int len, int x, int y);
int FindLegalMoves(int player);
int Match(int *move1, int *move2);
int IsLegal(int move[12], int mlen);
void NumberToXY(int num, int *x, int *y);
int TextToMove(char *mtext, int move[12]);
void PerformMove(int move[12], int mlen);

#endif

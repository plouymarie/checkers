#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/times.h>
#include <time.h>
#include <pthread.h>
#include <setjmp.h>
#include <limits.h>
#include "myprog.h"


int maxInt = 2147483647;
float SecPerMove;
char board[8][8];
char bestmove[12];
int me,cutoff,endgame = 0;
long NumNodes;
int MaxDepth;
int RED = 1;
int WHITE = 2;
int threeSecondDepth = 6;
int oneSecondDepth = 4;
int tenSecondDepth = 7;
jmp_buf env;
int globalPlayer;
int latestMove1;
int latestMove2;

/*** For timing ***/
clock_t start_t;
struct tms bff;

/*** For the jump list ***/
int jumpptr = 0;
int jumplist[48][12];

/*** For the move list ***/
int numLegalMoves = 0;
int movelist[48][12];

void PrintTime(void)
{
    clock_t current;
    float total;

    current = clock();
    total = (float) ((float)current-(float)start_t)/(int)CLOCKS_PER_SEC;
    fprintf(stderr, "Time = %f\n", total);
}
void PrintBoard(State *currBoard)
{
    int y,x;
    for(y=0; y<8; y++)
    {
        for(x=0; x<8; x++)
        {
            if(x%2 != y%2)
            {
                if(king(currBoard->board[y][x]))
                {
                    if(currBoard->board[y][x] & White) fprintf(stderr,"B");
                    else fprintf(stderr,"A");
                }
                else if(piece(currBoard->board[y][x]))
                {
                    if(currBoard->board[y][x] & White) fprintf(stderr,"b");
                    else fprintf(stderr,"a");
                }
                else fprintf(stderr," ");
            }
            else fprintf(stderr," ");
        }
        fprintf(stderr,"\n");
    }
}

int LowOnTime(void)
{

    clock_t current;
    float total;

    current = clock();
    total = (float) ((float)current-(float)start_t)/CLOCKS_PER_SEC;
    if(total >= (SecPerMove- SecPerMove != 1 ? .5 : .25))
    {
    	fprintf(stderr, "YES, I AM LOW ON TIME\n");
    	return 1;
    }
    else return 0;
}

/* Copy a square state */
void CopyState(char *dest, char src)
{
    char state;
    
    *dest &= Clear;
    state = src & 0xE0;
    *dest |= state;
}

/* Reset board to initial configuration */
void ResetBoard(void)
{
        int x,y;
    char pos;

        pos = 0;
        for(y=0; y<8; y++)
        for(x=0; x<8; x++)
        {
                if(x%2 != y%2) {
                        board[y][x] = pos;
                        if(y<3 || y>4) board[y][x] |= Piece; else board[y][x] |= Empty;
                        if(y<3) board[y][x] |= Red; 
                        if(y>4) board[y][x] |= White;
                        pos++;
                } else board[y][x] = 0;
        }
    endgame = 0;
}

/* Add a move to the legal move list */
void AddMove(char move[12])
{
    int i;

    for(i=0; i<12; i++) movelist[numLegalMoves][i] = move[i];
    numLegalMoves++;
}

/* Finds legal non-jump moves for the King at position x,y */
void FindKingMoves(char board[8][8], int x, int y) 
{
    int i,j,x1,y1;
    char move[12];

    memset(move,0,12*sizeof(char));

    /* Check the four adjacent squares */
    for(j=-1; j<2; j+=2)
    for(i=-1; i<2; i+=2)
    {
        y1 = y+j; x1 = x+i;
        /* Make sure we're not off the edge of the board */
        if(y1<0 || y1>7 || x1<0 || x1>7) continue; 
        if(empty(board[y1][x1])) {  /* The square is empty, so we can move there */
            move[0] = number(board[y][x])+1;
            move[1] = number(board[y1][x1])+1;    
            AddMove(move);
        }
    }
}

/* Finds legal non-jump moves for the Piece at position x,y */
void FindMoves(int player, char board[8][8], int x, int y) 
{
    int i,j,x1,y1;
    char move[12];

    memset(move,0,12*sizeof(char));

    /* Check the two adjacent squares in the forward direction */
    if(player == 1) j = 1; else j = -1;
    for(i=-1; i<2; i+=2)
    {
        y1 = y+j; x1 = x+i;
        /* Make sure we're not off the edge of the board */
        if(y1<0 || y1>7 || x1<0 || x1>7) continue; 
        if(empty(board[y1][x1])) {  /* The square is empty, so we can move there */
            move[0] = number(board[y][x])+1;
            move[1] = number(board[y1][x1])+1;    
            AddMove(move);
        }
    }
}

/* Adds a jump sequence the the legal jump list */
void AddJump(char move[12])
{
    int i;
    
    for(i=0; i<12; i++) jumplist[jumpptr][i] = move[i];
    jumpptr++;
}

/* Finds legal jump sequences for the King at position x,y */
int FindKingJump(int player, char board[8][8], char move[12], int len, int x, int y) 
{
    int i,j,x1,y1,x2,y2,FoundJump = 0;
    char one,two,mymove[12],myboard[8][8];

    memcpy(mymove,move,12*sizeof(char));

    /* Check the four adjacent squares */
    for(j=-1; j<2; j+=2)
    for(i=-1; i<2; i+=2)
    {
        y1 = y+j; x1 = x+i;
        y2 = y+2*j; x2 = x+2*i;
        /* Make sure we're not off the edge of the board */
        if(y2<0 || y2>7 || x2<0 || x2>7) continue; 
        one = board[y1][x1];
        two = board[y2][x2];
        /* If there's an enemy piece adjacent, and an empty square after hum, we can jump */
        if(!empty(one) && color(one) != player && empty(two)) {
            /* Update the state of the board, and recurse */
            memcpy(myboard,board,64*sizeof(char));
            myboard[y][x] &= Clear;
            myboard[y1][x1] &= Clear;
            mymove[len] = number(board[y2][x2])+1;
            FoundJump = FindKingJump(player,myboard,mymove,len+1,x+2*i,y+2*j);
            if(!FoundJump) {
                FoundJump = 1;
                AddJump(mymove);
            }
        }
    }
    return FoundJump;
}

/* Finds legal jump sequences for the Piece at position x,y */
int FindJump(int player, char board[8][8], char move[12], int len, int x, int y) 
{
    int i,j,x1,y1,x2,y2,FoundJump = 0;
    char one,two,mymove[12],myboard[8][8];

    memcpy(mymove,move,12*sizeof(char));

    /* Check the two adjacent squares in the forward direction */
    if(player == 1) j = 1; else j = -1;
    for(i=-1; i<2; i+=2)
    {
        y1 = y+j; x1 = x+i;
        y2 = y+2*j; x2 = x+2*i;
        /* Make sure we're not off the edge of the board */
        if(y2<0 || y2>7 || x2<0 || x2>7) continue; 
        one = board[y1][x1];
        two = board[y2][x2];
        /* If there's an enemy piece adjacent, and an empty square after hum, we can jump */
        if(!empty(one) && color(one) != player && empty(two)) {
            /* Update the state of the board, and recurse */
            memcpy(myboard,board,64*sizeof(char));
            myboard[y][x] &= Clear;
            myboard[y1][x1] &= Clear;
            mymove[len] = number(board[y2][x2])+1;
            FoundJump = FindJump(player,myboard,mymove,len+1,x+2*i,y+2*j);
            if(!FoundJump) {
                FoundJump = 1;
                AddJump(mymove);
            }
        }
    }
    return FoundJump;
}

/* Determines all of the legal moves possible for a given state */
int FindLegalMoves(struct State *state)
{
    int x,y;
    char move[12], board[8][8];

    memset(move,0,12*sizeof(char));
    jumpptr = numLegalMoves = 0;
    memcpy(board,state->board,64*sizeof(char));
    memset(movelist,0,48*12*sizeof(char));
    memset(jumplist,0,48*12*sizeof(char));

    /* Loop through the board array, determining legal moves/jumps for each piece */
    for(y=0; y<8; y++)
    for(x=0; x<8; x++)
    {
        if(x%2 != y%2 && color(board[y][x]) == state->player && !empty(board[y][x])) {
            if(king(board[y][x])) { /* King */
                move[0] = number(board[y][x])+1;
                FindKingJump(state->player,board,move,1,x,y);
                if(!jumpptr) FindKingMoves(board,x,y);
            } 
            else if(piece(board[y][x])) { /* Piece */
                move[0] = number(board[y][x])+1;
                FindJump(state->player,board,move,1,x,y);
                if(!jumpptr) FindMoves(state->player,board,x,y);    
            }
        }    
    }
    if(jumpptr) {
        for(x=0; x<jumpptr; x++) 
        for(y=0; y<12; y++) 
        state->movelist[x][y] = jumplist[x][y];
        state->numLegalMoves = jumpptr;
    } 
    else {
        for(x=0; x<numLegalMoves; x++) 
        for(y=0; y<12; y++) 
        state->movelist[x][y] = movelist[x][y];
        state->numLegalMoves = numLegalMoves;
    }
    return (jumpptr+numLegalMoves);
}

/* Converts a square label to it's x,y position */
void NumberToXY(char num, int *x, int *y)
{
    int i=0,newy,newx;

    for(newy=0; newy<8; newy++)
    for(newx=0; newx<8; newx++)
    {
        if(newx%2 != newy%2) {
            i++;
            if(i==(int) num) {
                *x = newx;
                *y = newy;
                return;
            }
        }
    }
    *x = 0; 
    *y = 0;
}

/* Returns the length of a move */
int MoveLength(char move[12])
{
    int i = 0;
    while(i<12 && move[i]) i++;
    return i;
}    

/* Converts the text version of a move to its integer array version */
int TextToMove(char *mtext, char move[12])
{
    int i=0,len=0,last;
    char val,num[64];

    while(mtext[i] != '\0') {
        last = i;
        while(mtext[i] != '\0' && mtext[i] != '-') i++;
        strncpy(num,&mtext[last],i-last);
        num[i-last] = '\0';
        val = (char) atoi(num);
        if(val <= 0 || val > 32) return 0;
        move[len] = val;
        len++;
        if(mtext[i] != '\0') i++;
    }
    if(len<2 || len>12) return 0; else return len;
}

/* Converts the integer array version of a move to its text version */
void MoveToText(char move[12], char *mtext)
{
    int i;
    char temp[8];

    mtext[0] = '\0';
    for(i=0; i<12; i++) {
        if(move[i]) {
            sprintf(temp,"%d",(int)move[i]);
            strcat(mtext,temp);
            strcat(mtext,"-");
        }
    }
    mtext[strlen(mtext)-1] = '\0';
}

void performMove(char board[8][8], char move[12], int mlen, int player)
{
    int i,j,x,y,x1,y1,x2,y2;

    NumberToXY(move[0],&x,&y);
    NumberToXY(move[mlen-1],&x1,&y1);
    CopyState(&board[y1][x1],board[y][x]);
    if(y1 == 0 || y1 == 7) board[y1][x1] |= King;
    board[y][x] &= Clear;
    NumberToXY(move[1],&x2,&y2);
    if(abs(x2-x) == 2) {
        for(i=0,j=1; j<mlen; i++,j++) {
            if(move[i] > move[j]) {
                y1 = -1; 
                if((move[i]-move[j]) == 9) x1 = -1; else x1 = 1;
            }
            else {
                y1 = 1;
                if((move[j]-move[i]) == 7) x1 = -1; else x1 = 1;
            }
            NumberToXY(move[i],&x,&y);
            board[y+y1][x+x1] &= Clear;
        }
    }
}
int main(int argc, char *argv[])
{
    char buf[1028],move[12];
    int len,mlen,player1;
    // MaxDepth = 10;

    /* Convert command line parameters */
    SecPerMove = (float) atof(argv[1]); /* Time allotted for each move */
    MaxDepth = (argc == 3) ? atoi(argv[2]) : 5;

    /* Determine if I am player 1 (red) or player 2 (white) */
    //fgets(buf, sizeof(buf), stdin);
    len=read(STDIN_FILENO,buf,1028);
    buf[len]='\0';
    if(!strncmp(buf,"Player1", strlen("Player1"))) 
    {
        player1 = 1; 
    }
    else 
    {
        player1 = 0;
    }
    if(player1) me = 1; else me = 2;

    /* Set up the board */ 
    ResetBoard();
    srand((unsigned int)time(0));

    if (player1) {
        start_t = times(&bff);
        goto determine_next_move;
    }

fprintf(stderr,"Starting game\n");fflush(stderr);

    for(;;) {
        /* Read the other player's move from the pipe */
        //fgets(buf, sizeof(buf), stdin);
        len=read(STDIN_FILENO,buf,1028);
        buf[len]='\0';
        start_t = times(&bff);
        memset(move,0,12*sizeof(char));

        /* Update the board to reflect opponents move */
        mlen = TextToMove(buf,move);
        performMove(board,move,mlen, me);
        
determine_next_move:
        /* Find my move, update board, and write move to pipe */
        if(player1) FindBestMove(1); else FindBestMove(2);

        fprintf(stderr,"Found bestmove: %d\n", bestmove[0]);fflush(stderr);
        if(bestmove[0] != 0) { /* There is a legal move */
            mlen = MoveLength(bestmove);    
            performMove(board,bestmove,mlen, me);
            MoveToText(bestmove,buf);
        }
        else exit(1); /* No legal moves available, so I have lost */

        /* Write the move to the pipe */
        //printf("%s", buf);
        write(STDOUT_FILENO,buf,strlen(buf));
        fflush(stdout);
    }

    return 0;
}

// void FindBestMove(int player)
int maxd;
/* Employ your favorite search to find the best move.  This code is an example     */
/* of an alpha/beta search, except I have not provided the MinVal,MaxVal,EVAL      */
/* functions.  This example code shows you how to call the FindLegalMoves function */
/* and the PerformMove function */
void *FindBestMoveThread(void *p)
{
    int player = globalPlayer;
    int x,currBestMove = -1;
    double currBestVal = INT_MIN;
    struct State state; 
    int oldState;
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldState);

    /* Set up the current state */
    state.player = player;
    memcpy(state.board, board, 64 * sizeof(char));

    /* Find the legal moves for the current state */
    FindLegalMoves(&state);

    memset(bestmove, 0, 12 * sizeof(char));
    for(int maxDepth = 3;;maxDepth++){
        currBestMove = -1;
        // if(LowOnTime() != 0){
        //     fprintf(stderr,"Low On Time 1\n");fflush(stderr);
        //     break;
        // }
        int *indexes = (int *) malloc (sizeof(int)*state.numLegalMoves);
        for(x = 0; x < state.numLegalMoves; x++){
            indexes[x] = x;
        }
        for(x = 0; x < state.numLegalMoves*2; x++){
            int temp, i, j;
            i = rand()%state.numLegalMoves;
            j = rand()%state.numLegalMoves;
            temp = indexes[i];
            indexes[i]=indexes[j];
            indexes[j]=temp;
        }
        for (x = 0; x < state.numLegalMoves; x++) {
            double rval = 0;
            char nextBoard[8][8];
            memcpy(nextBoard, state.board, 64 * sizeof(char));
            performMove(nextBoard, state.movelist[indexes[x]], MoveLength(state.movelist[indexes[x]]), player);
            rval = minVal(nextBoard, player, INT_MIN, INT_MAX, maxDepth);
            if (currBestVal < rval) {
                currBestVal = rval;
                currBestMove = indexes[x];
            }
        }
            // if(latestMove2 == currBestMove && latestMove1 == currBestMove && !jumplist[currBestMove]){
            //     currBestMove = prevBestMove;
            //     // currBestMove = rand()%state.numLegalMoves;
            // }
            // latestMove2 = latestMove1;
            // latestMove1 = currBestMove;
        memcpy(bestmove, state.movelist[currBestMove],MoveLength(state.movelist[currBestMove]));
    }
    // memcpy(bestmove, state.movelist[currBestMove],MoveLength(state.movelist[currBestMove]));
}

double minVal(char currBoard[8][8], int player, double alpha, double beta, int depth){
    struct State state; 
    depth--;
    /* Set up the current state */
    state.player = me == RED ? WHITE : RED ;
    memcpy(state.board, currBoard, 64 * sizeof(char));

    if(depth <= 0){
        return evalBoard(&state);
    }

    /* Find the legal moves for the current state */
    FindLegalMoves(&state);

    // This loop isn't doing anything, but it shows you how to copy the board state and perform a move on it
    for (int x = 0; x < state.numLegalMoves; x++){
        char nextBoard[8][8];

        // prep data
        memcpy(nextBoard,state.board,64*sizeof(char));
        performMove(nextBoard,state.movelist[x],MoveLength(state.movelist[x]), state.player);

        // Do your mini-max alpha-beta pruning search here 
        beta = MIN(beta, maxVal(nextBoard,player, alpha, beta, depth));
        if(beta <= alpha){
            return alpha;
        }
    }
    
    return beta;
}

double maxVal(char currBoard[8][8], int player, double alpha, double beta, int depth){
    struct State state;
    depth--;
    /* Set up the current state */
    state.player = me;
    memcpy(state.board,currBoard,64*sizeof(char));

    if(depth <= 0){
        return evalBoard(&state);
    }

    /* Find the legal moves for the current state */
    FindLegalMoves(&state);

    // This loop isn't doing anything, but it shows you how to copy the board state and perform a move on it
    for (int x = 0; x < state.numLegalMoves; x++){

        char nextBoard[8][8];

        // prep data
        memcpy(nextBoard,state.board,64*sizeof(char));
        performMove(nextBoard,state.movelist[x],MoveLength(state.movelist[x]), state.player);

        // Do your mini-max alpha-beta pruning search here 
        alpha = MAX(alpha, minVal(nextBoard, player, alpha, beta, depth));
        if(alpha >= beta){
            return beta;
        }
    }
    return alpha;
}

void *timerThread(void *p)
{
    // int wait = (int)SecPerMove * 1000000 - 300000;
    int wait = (int)SecPerMove * 100000;
    usleep(wait);
    return NULL;
}

//int pthread_create (pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
void TimedFindBestMove(int player)
{
    // char buf[1024];
    // int rval;

    globalPlayer = player;

    pthread_t timer, fbmt;

    fprintf(stderr,"Creating timer thread\n");fflush(stderr);
    // start_t_t kludge timer thread
    pthread_create(&timer, NULL, timerThread, NULL);

    // create find best move thread (fbmt)
    pthread_create(&fbmt, NULL, FindBestMoveThread, NULL); 

    // detach fbmt cuz we don't want to join with it, we'll just kill it when we run out of time
    pthread_detach(fbmt); 

    // wait for timer thread to come back, yawn
    pthread_join(timer,NULL);
    // pthread_join(fbmt,NULL);

    fprintf(stderr,"Done waiting for timer thread\n");fflush(stderr);

    // smack find best move thread upside the head
    pthread_cancel(fbmt);

    fprintf(stderr,"Exiting TimedFindBestMove\n");fflush(stderr);

//MoveToText(bestmove,buf);
// fprintf(stderr,"max depth = %i\n", MaxDepth);fflush(stderr);
}

void FindBestMove(int player)
{
    memset(bestmove,0,12*sizeof(char));
    // brokenFindBestMove(player);
    TimedFindBestMove(player);
}

double evalBoard(State * state) {
    int row, column, p1Score = 0, p2Score = 0, numWhitePieces = 0, numRedPieces = 0;
	int KING_MATERIAL_ADV = 10;
	int PAWN_MATERIAL_ADV = 5;

	if (!endgame)
	for (row = 0; row < 8; row++)
		for (column = 0; column < 8; column++) {
			if (row % 2 != column % 2 && !empty(state->board[row][column])) {
				if (color(state->board[row][column]) == RED) {
					if (king(state->board[row][column]))
					{
						p1Score += KING_MATERIAL_ADV;
					}
					else if (piece(state->board[row][column]))
					{
						p1Score += PAWN_MATERIAL_ADV;
					}
					++numRedPieces;
				}
				else
				{
					if (king(state->board[row][column]))
					{
						p2Score += KING_MATERIAL_ADV;
					}
					else if (piece(state->board[row][column]))
					{
						p2Score += PAWN_MATERIAL_ADV;
					}
					++numWhitePieces;
				}
			}
		}
	int difference = p1Score - p2Score;
	return me == RED ? difference : -1*difference;

//      int y,x;
//     double score=0.0;

// //fprintf(stderr,"**********************\n");
// //fprintf(stderr,"**********************\n");
// //PrintBoard(currBoard);

//     for(y=0; y<8; y++) for(x=0; x<8; x++) if(x%2 != y%2)
//     {
//         if(king(state->board[y][x]))
//         {
//             if(state->board[y][x] & White) score += 2.0;
//             else score -= 2.0;
//         }
//         else if(piece(state->board[y][x]))
//         {
//             if(state->board[y][x] & White) score += 1.0;
//             else score -= 1.0;
//         }
//     }

//     //if not 0, return -score, else score
//     //Change if it doesn't work
//     score = me==1 ? -score : score;

// //fprintf(stderr,"score = %lg\n", score);
// //fprintf(stderr,"**********************\n");
// //fprintf(stderr,"**********************\n");
//     return score;
}

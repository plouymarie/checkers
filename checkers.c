#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/times.h>
#include <fcntl.h>
#include <time.h>
#include <Xm/Xm.h>
#include <pthread.h>

#include "checkers.h"
#include "graphics.h"

#define ILLEGAL 1
#define PARTIAL 2
#define FULL 3
#ifndef CLK_TCK
#define CLK_TCK CLOCKS_PER_SEC
#endif

struct Square square[16][16];
float SecPerMove;
int turn,playing=0;
int player[2];
int MaxDepth;

int player1Java;
int player2Java;

/* Made these global (used to be declared in NewGame) */
char player1[128],player2[128];

/*** For the pipes ***/
int readfd[2],writefd[2];
pid_t pid[2];

/*** For the jump list ***/
int jumpptr = 0;
int jumplist[256][12];

/*** For the move list ***/
int moveptr = 0;
int movelist[48][12];

/*** For human moves ***/
int hlen = 0;
int hmove[12];
int HumanMoved = 0;

void Usage(char *str)
{
   printf("Usage: %s player1 player2 SecPerMove [-MaxDepth x]\n",str);
   exit(0);
}

void PrintBoard()
{
    int board[8][8];
    int x,y;
    char ch = 127;

    for(y=0; y<8; y++)
    {
       for(x=0; x<8; x++)
       {
           if(x%2 != y%2) {
               if(square[y][x].state) {
                   if(square[y][x].col) 
                   {
                      if(square[y][x].state == King) board[y][x] = 'B';
                      else board[y][x] = 'b';
                   }
                   else
                   {
                      if(square[y][x].state == King) board[y][x] = 'A';
                      else board[y][x] = 'a';
                   }
               } else board[y][x] = ' ';
           } else board[y][x] = ch;
           printf("%c",board[y][x]);
       }
       printf("\n");
    }
}

/* Converts a board in the 'square structure' format to a board in a   */
/* simple array format where 0 = empty, 1 = red piece, 2 = white piece */
void SquaresToBoard(int board[8][8])
{
    int x,y;

    for(y=0; y<8; y++)
    for(x=0; x<8; x++)
    {
        if(x%2 != y%2) {
            if(square[y][x].state) {
                board[y][x] = square[y][x].col+1;
            } else board[y][x] = 0;
        } else board[y][x] = 0;
    }
}

/* Adds a move to the legal moves list */
void AddMove(int move[12])
{
    int i;

    for(i=0; i<12; i++) movelist[moveptr][i] = move[i];
    moveptr++;
}

/* Finds legal non-jump moves for the King at position x,y */ 
void FindKingMoves(int board[8][8], int x, int y) 
{
    int i,j,x1,y1,move[12];

    memset(move,0,12*sizeof(int));
    /* Check the four adjacent squares */
    for(j=-1; j<2; j+=2)
    for(i=-1; i<2; i+=2)
    {
        y1 = y+j; x1 = x+i;
        /* Make sure we're not off the edge of the board */
        if(y1<0 || y1>7 || x1<0 || x1>7) continue; 
        if(!board[y1][x1]) { /* The square is empty, so we can move there */
            move[0] = square[y][x].val;
            move[1] = square[y1][x1].val;    
            AddMove(move);
        }
    }
}

/* Finds legal non-jump moves for the Piece at position x,y */
void FindMoves(int player, int board[8][8], int x, int y) 
{
    int i,j,x1,y1,move[12];

    memset(move,0,12*sizeof(int));
    if(player == 1) j = 1; else j = -1;
    /* Check the two adjacent squares in the forward direction */
    for(i=-1; i<2; i+=2)
    {
        y1 = y+j; x1 = x+i;
        /* Make sure we're not off the edge of the board */
        if(y1<0 || y1>7 || x1<0 || x1>7) continue; 
        if(!board[y1][x1]) { /* The square is empty, so we can move there */ 
            move[0] = square[y][x].val;
            move[1] = square[y1][x1].val;    
            AddMove(move);
        }
    }
}

/* Adds a jump sequence to the list of legal jumps */
void AddJump(int move[12])
{
    int i;
    
    for(i=0; i<12; i++) jumplist[jumpptr][i] = move[i];
    jumpptr++;
}

/* Finds legal jump sequences for the King at position x,y */ 
int FindKingJump(int player, int board[8][8], int move[12], int len, int x, int y) 
{
    int i,j,x1,y1,x2,y2,one,two;
    int myboard[8][8],mymove[12],FoundJump = 0;

    memcpy(mymove,move,12*sizeof(int));
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
        /* If there's an enemy piece adjacent, and an empty square after him, we can jump */
        if(one && one != player && !two) {
            /* Update state of board, and recurse */
            memcpy(myboard,board,64*sizeof(int));
            myboard[y][x] = 0;
            myboard[y1][x1] = 0;
            mymove[len] = square[y2][x2].val;
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
int FindJump(int player, int board[8][8], int move[12], int len, int x, int y) 
{
    int i,j,x1,y1,x2,y2,one,two;
    int myboard[8][8],mymove[12],FoundJump = 0;

    memcpy(mymove,move,12*sizeof(int));
    if(player == 1) j = 1; else j = -1;
    /* Check the two adjacent squares in the forward direction */
    for(i=-1; i<2; i+=2)
    {
        y1 = y+j; x1 = x+i;
        y2 = y+2*j; x2 = x+2*i;
        /* Make sure we're not off the edge of the board */
        if(y2<0 || y2>7 || x2<0 || x2>7) continue; 
        one = board[y1][x1];
        two = board[y2][x2];
        /* If there's an enemy piece adjacent, and an empty square after him, we can jump */
        if(one && one != player && !two) { 
            /* Update state of board, and recurse */
            memcpy(myboard,board,64*sizeof(int));
            myboard[y][x] = 0;
            myboard[y1][x1] = 0;
            mymove[len] = square[y2][x2].val;
            FoundJump = FindJump(player,myboard,mymove,len+1,x+2*i,y+2*j);
            if(!FoundJump) {
                FoundJump = 1;
                AddJump(mymove);
            }
        }
    }
    return FoundJump;
}

/* Determines all of the legal moves possible for a player given the current board */
int FindLegalMoves(int player)
{
    int x,y,board[8][8],move[12];

    SquaresToBoard(board);
    memset(move,0,12*sizeof(int));
    jumpptr = moveptr = 0;

    /* Loop through board array, calling FindJump, FindMoves, etc. for each piece */
    for(y=0; y<8; y++)
    for(x=0; x<8; x++)
    {
        if(x%2 != y%2 && board[y][x] == player) {
            if(square[y][x].state == King) { /* King */
                move[0] = square[y][x].val;
                FindKingJump(player,board,move,1,x,y);
                if(!jumpptr) FindKingMoves(board,x,y);
            } 
            else { /* Piece */
                move[0] = square[y][x].val;
                FindJump(player,board,move,1,x,y);
                if(!jumpptr) FindMoves(player,board,x,y);    
            }
        }    
    }
    return (jumpptr+moveptr);
}

/* Determines if two moves are identical */
int Match(int move1[12], int move2[12])
{
    int i;

    for(i=0; i<12; i++) if(move1[i] != move2[i]) return 0;
    return 1;
}

/* Determines if a certain move is legal */
int IsLegal(int move[12], int mlen)
{
    int i;

    /* Check jumps first, since jumps must always be made */
    if(jumpptr) {
        for(i=0; i<jumpptr; i++) if(Match(jumplist[i],move)) return 1;
    }
    else {
        for(i=0; i<moveptr; i++) if(Match(movelist[i],move)) return 1;
    }
    return 0; 
}

/* Converts a square's label (number) to its x,y coordinates in the array */
void NumberToXY(int num, int *x, int *y)
{
    int i=0,newy,newx;

    for(newy=0; newy<8; newy++)
    for(newx=0; newx<8; newx++)
    {
        if(newx%2 != newy%2) {
            i++;
            if(i==num) {
                *x = newx;
                *y = newy;
                return;
            }
        }
    }
    *x = 0; 
    *y = 0;
}

/* Converts a text version of a move to its integer array version */
int TextToMove(char *mtext, int move[12])
{
    int i=0,len=0,last,val;
    char number[64];

    while (mtext[i] != '\0') {
        last = i;
        while(mtext[i] != '\0' && mtext[i] != '-') i++;
        strncpy(number,&mtext[last],i-last);
        number[i-last] = '\0';
        val = atoi(number);
        if(val <= 0 || val > 32) return 0;
        move[len] = val;
        len++;
        if(mtext[i] != '\0') i++;
    }
    if(len<2 || len>12) return 0; else return len;
}

/* Converts an integer array version of a move to its text version */
void MoveToText(int move[12], char *mtext)
{
    int i;
    char temp[8];

    mtext[0] = '\0';
    for(i=0; i<12; i++) {
        if(move[i]) {
            sprintf(temp,"%d",move[i]);
            strcat(mtext,temp);
            strcat(mtext,"-");
        }
    }

    mtext[strlen(mtext) - 1] = '\0';
    mtext[strlen(mtext)] = '\0';
}

/* Performs a move on the board, updating the state of the board */
void PerformMove(int move[12], int mlen)
{
    int i,j,x,y,x1,y1;

    NumberToXY(move[0],&x,&y);
    NumberToXY(move[mlen-1],&x1,&y1);
    square[y1][x1].state = square[y][x].state;
    if(y1 == 0 || y1 == 7) square[y1][x1].state = King;
    square[y1][x1].col = square[y][x].col;
    square[y][x].state = Empty;
    if(jumpptr) {
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
            square[y+y1][x+x1].state = Empty;
        }
    }
}

/* Determines if 'move1' is a subsequence of length 'len' of 'move2' */
int Partial(int move1[12], int move2[12], int len)
{
    int i;

    for(i=0; i<len; i++) if(move1[i] != move2[i]) return 0;
    return 1;
}

/* Checks the human's current move sequence to see if it matches or partially matches */
/* a valid (legal) move. */ 
int CheckHumanMove(void)
{
    int i,full=0,partial=0;

    if(jumpptr) {
        for(i=0; i<jumpptr; i++) {
            if(Match(jumplist[i],hmove)) full=1; else
            if(Partial(jumplist[i],hmove,hlen)) partial=1;
        }
    }
    else {
        for(i=0; i<moveptr; i++) if(Match(movelist[i],hmove)) full = 1;
    }
    if(full) return FULL; else
    if(partial) return PARTIAL; else
    return ILLEGAL;
}

/* After the human has completed a move, or made an illegal one, unhighlight the */
/* squares he/she has chosen */
void UnHighlightAll(void)
{
    int x,y;

        for(y=0; y<8; y++)
        for(x=0; x<8; x++)
        if(x%2 != y%2) {
        square[y][x].hilite = Green;
        }
    UpdateBoard();
}

/* This function is called when a human player clicks on a square with the mouse */
void SquareChosen(struct Square *sq)
{
    int result;

    if(playing && player[turn] == HUMAN) {
        if(hlen == 0) {    /* This is the first square chosen */
            if(sq->state && sq->col == turn) {
                hmove[hlen] = sq->val;
                hlen++;
                sq->hilite = turn;
                UpdateBoard();
            }
        } 
        else {    /* This is NOT the first square chosen */
            if(!sq->state) {
                /* Add square to move list and check to see if the current move */
                /* is illegal, partially complete, or a completed legal move */
                hmove[hlen] = sq->val;
                hlen++;
                result = CheckHumanMove();
                if(result == ILLEGAL) {
                    hlen = 0;
                    UnHighlightAll();
                    memset(hmove,0,12*sizeof(int));
                }
                else if(result == PARTIAL) {
                    sq->hilite = turn;
                    UpdateBoard();
                }
                else if(result == FULL) {
                    UnHighlightAll();
                    HumanMoved = 1;
                }    
            } 
            else {
                hlen = 0;
                UnHighlightAll();
                memset(hmove,0,12*sizeof(int));
            }
        }    
    }
}

/* Called when the 'Stop Game' menu item is selected */
void StopGame(void)
{
    char command[32];
    int i;

    if(playing) {
        for(i=0; i<2; i++) {
            if(player[i] == COMPUTER) {
                close(writefd[i]);
                close(readfd[i]);
                sprintf(command,"kill -9 %d\n",(int)pid[i]);
                system(command);
            }
        }
        playing = 0;
    }
#ifndef GRAPHICS
exit(0);
#endif
}

/* Called when the 'New Game' menu item is selected */
void NewGame(void)
{
    char arg1[16],arg2[16], arg01[16], arg02[16];
    int i;
#ifdef GRAPHICS
    if(!NewDialog(player1,player2,&SecPerMove)) return;
    if(!strcmp(player1,"human")) player[0] = HUMAN; else player[0] = COMPUTER;
    if(!strcmp(player2,"human")) player[1] = HUMAN; else player[1] = COMPUTER;
#else
   player[0]=COMPUTER;
   player[1]=COMPUTER;
#endif


    arg01[0]=0;
    arg02[0]=0;
    player1Java=0;
    player2Java=0;
    if(!strncmp(player1,"java",4)) 
    {
        player1Java=1;
        fprintf(stderr,"Player1 is java player\n");
        strcpy(arg01,&(player1[5]));
        player1[4]=0;
        strcpy(player1,"/usr/bin/java");
        fprintf(stderr,"%s %s\n", player1, arg01);
    }
    if(!strncmp(player2,"java",4)) 
    {
        player2Java=1;
        fprintf(stderr,"Player2 is java player\n");
        strcpy(arg02,&(player2[5]));
        //player2[4]=0;
        strcpy(player2,"/usr/bin/java");
        fprintf(stderr,"%s %s\n", player2, arg02);
    }

    /* If 'New Game' is chosen while a game is in progress, stop the game */
    if (playing) StopGame();

    // Set up the pipes necessary for communication with computer players.
    for (i = 0; i < 2; i++) {
        int to_proc[2];
        int from_proc[2];

        if (player[i] == HUMAN) {
            // Pipes aren't needed for human players.
            continue;
        }

        pipe(to_proc);
        pipe(from_proc);

        writefd[i] = to_proc[1];
        readfd[i] = from_proc[0];

        if (fcntl(readfd[i],F_SETFL,(int)O_NDELAY) < 0) {
            printf("fcntl failed\n");
            return;
        }

        /* Fork a child process. We will then overlay the computer program */
        if((pid[i] = fork()) == (pid_t)0) {
            int temp;
            dup2(to_proc[0], STDIN_FILENO);
            close(to_proc[0]);

            dup2(from_proc[1], STDOUT_FILENO);
            close(from_proc[1]);

            sprintf(arg1,"%.2f",SecPerMove);
            if(MaxDepth >= 0)
            {
               sprintf(arg2,"%d", MaxDepth);
               if((i==0 && player1Java ) || (i==1 && player2Java))
                   temp = execl(i?player2:player1,i?player2:player1, i?arg02:arg01,arg1,arg2, NULL);
               else
                   temp = execl(i?player2:player1,i?player2:player1, arg1,arg2,(char *)0);
               if(temp)
               {
                   fprintf(stderr, "exec for %s failed\n",i?player2:player1);
                   exit(0);
               }
            }
            else
            {
               if((i==0 && player1Java ) || (i==1 && player2Java))
                   temp = execl(i?player2:player1,i?player2:player1, i?arg02:arg01,arg1, NULL);
               else 
                   temp = execl(i?player2:player1,i?player2:player1, arg1,(char *)0);
               if(temp)
               {
                   fprintf(stderr, "exec for %s failed\n",i?player2:player1);
                   exit(0);
               }
            }
        }
    }
    ResetBoard();
    playing = 1;
    turn = 0;
    hlen = 0;
    memset(hmove,0,12*sizeof(int));
    FindLegalMoves(turn+1);

    /* Tell the computer programs which player they are */
    if(player[0] == COMPUTER) 
    {
       write(writefd[0],"Player1",7);
       fsync(writefd[0]);
    }
    if(player[1] == COMPUTER) 
    {
       write(writefd[1],"Player2",7);
       fsync(writefd[1]);
    }
}


void *timer(void *timeup)
{
    //usleep(1000 + SecPerMove * 1000);
    int sleeptime = 1+SecPerMove;
    sleep(sleeptime);
    *((int*)timeup) = 1;
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    char text[1028],temptext[1028], str[1028];
    int tlen,mlen,move[12],numlegal,done;
    if(argc>=3)
    {
       if(!strncasecmp("-MaxDepth",argv[argc-2], strlen("-MaxDepth")))
       {
          MaxDepth = atoi(argv[argc-1]);
          argc-=2;
       }
       else MaxDepth = -1;
    }
    else MaxDepth = -1;

#ifndef GRAPHICS
    printf("No graphics\n");
    if(argc != 4) Usage(argv[0]);
    strcpy(player1,argv[1]);
    strcpy(player2,argv[2]);
    SecPerMove = atof(argv[3]);
#endif

#ifdef GRAPHICS
    printf("Graphics\n");
    InitGraphics(argc,argv);
#else
      NewGame();
      {
     int x,y;
     /* I'll wait a bit to make sure both oponents are ready to go */
     printf("waiting\n");
     sleep(1);
     for(x=0;x<1000;x++)
     for(y=0;y<10000;y++);
      }
#endif
    ResetBoard();
    for(;;) {
        pthread_t thread;
        int rc, dummy;
        HandleEvents();
        if(playing) {
            sprintf(str,"Waiting for player %d",turn+1);
            Message(str);
            HumanMoved = done = 0;
            //start = times(&bff);
            rc = pthread_create(&thread, NULL, timer, (void*)&done);
            pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &dummy);
            do {
                HandleEvents();
                /* Give humans all the time they want to move */
                if(player[turn] == HUMAN) done = HumanMoved;
                else if(player[turn] == COMPUTER) 
                {
                    char *ptr;
                    memset(temptext,0,sizeof(temptext));
                    tlen = read(readfd[turn],temptext,1028);
                    if(tlen > 0) 
                    {
                        ptr = temptext;
                        while(*ptr == 10 && *ptr !=0) ptr++;
                        strcpy(text,ptr);
                        if(strlen(text)) done=1;
                    }
                }
            } while(playing && !done);
            pthread_cancel(thread);
            if(!playing) continue;
            if(player[turn] == COMPUTER && tlen <= 0) {
                sprintf(str,"Player %d has lost the game (time limit reached).",turn+1);
                Message(str);
                StopGame();
            }    
            else {
                if(player[turn] == COMPUTER) {
                    text[tlen] = '\0';
                    memset(move,0,12*sizeof(int));
                    mlen = TextToMove(text,move);
                }
                else if(player[turn] == HUMAN) {
                    mlen = hlen;
                    memcpy(move,hmove,12*sizeof(int));
                    hlen = 0;
                    memset(hmove,0,12*sizeof(int));
                    MoveToText(move,text);
                }

                if(!mlen) { /* Illegal move check 1 */
                    /*char temp[1000];
                    char *ptr1, *ptr2;
                    ptr1=text;
                    temp[0] = 0;
                    ptr2=temp;
                    while(*ptr1) 
                    {
                        sprintf(ptr2,"%i, ", *ptr1);
                        ptr1++;
                        ptr2 = &(ptr2[strlen(ptr2)]);
                    }*/
                    //sprintf(str,"Player %d has lost the game (illegal move %s %s submitted).",turn+1,text, temp);
                    sprintf(str,"Player %d has lost the game (illegal move %s submitted).",turn+1,text);
                    Message(str);
                    StopGame();
                }
                else {
                    if(!IsLegal(move,mlen)) { /* Illegal move check 2 */
                       /*char temp[1000];
                       char *ptr1, *ptr2;
                       ptr1=text;
                       temp[0] = 0;
                       ptr2=temp;
                       while(*ptr1) 
                       {
                           sprintf(ptr2,"%i, ", *ptr1);
                           ptr1++;
                           ptr2 = &(ptr2[strlen(ptr2)]);
                       }
                        sprintf(str,"Player %d has lost the game (illegal move %s %s submitted).",turn+1,text, temp);*/
                        sprintf(str,"Player %d has lost the game (illegal move %s submitted).",turn+1,text);
                        Message(str);
                        StopGame();
                    }
                    else {  /* Legal Move */
                        PerformMove(move,mlen);
#ifdef GRAPHICS
                        UpdateBoard();
#else
                        printf("Move: %s\n",text);
                        PrintBoard();
#endif
                        if(turn) turn=0; else turn=1;
    
                        /* Check to see if other player has now lost */
                        numlegal = FindLegalMoves(turn+1);
                        if(!numlegal) {
                            sprintf(str,"Player %d has lost the game.",turn+1);
                            Message(str);
                            StopGame();
                        }
                        else if(player[turn] == COMPUTER) {
                            write(writefd[turn],text,strlen(text));
                        }
                    }
                }
            }
        }            
    }
}

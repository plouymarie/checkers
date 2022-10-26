#ifndef _graphics_h_
#define _graphics_h_

#define Red 0
#define White 1
#define Black 2
#define Green 3
#define Tan 4

void UpdateSquare(int x, int y);
void UpdateBoard(void);
void ResetBoard(void);
void ClearBoard(void);
int NewDialog(char *player1, char *player2, float *SecPerMove);
void Message(char *str);
void HandleEvents(void);
void InitGraphics(int argc, char *argv[]);

#endif

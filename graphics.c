#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/times.h>
#include <X11/Xlib.h>
#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/Form.h>
#include <Xm/DialogS.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>
#include <Xm/MessageB.h>

#include "checkers.h"
#include "graphics.h"

int Color[5][3] = { {255,  0,  0},  /* Red     */
                    {255,255,255},  /* White   */
                    {  0,  0,  0},  /* Black   */
                    { 90,140, 80},  /* Green   */
                    {210,200,110}}; /* Tan     */

XtAppContext app;
Display *dpy; 
GC gc;
Widget toplevel, mainWidget, menubar, workwin, message;
Colormap colormap;
extern struct Square square[16][16];
unsigned long color[256];
int squaresize,circle1,circle2;
int dialog_open,cancelled;
char xval[3][128];

void SetColor(int col) 
{
    XSetForeground (dpy, gc, color[col]);
}

void buttonpress(Widget widget, XtPointer client, XEvent *event) 
{
    int x,y;

    for(y=0; y<8; y++)
    for(x=0; x<8; x++)
    if(x%2 != y%2 && widget == square[y][x].widget) {
        SquareChosen(&square[y][x]);
    }
}

void UpdateSquare(int x, int y)
{
    if(x%2 != y%2) {
        if(square[y][x].state) {
            SetColor(square[y][x].col);
            XFillArc(dpy,XtWindow(square[y][x].widget),gc,
                circle1,circle1,circle2,circle2,0,360*64);
            SetColor(Black);
            XDrawArc(dpy,XtWindow(square[y][x].widget),gc,
                circle1,circle1,circle2,circle2,0,360*64);
            if(square[y][x].state == King) {
                SetColor(square[y][x].col);
                XFillArc(dpy,XtWindow(square[y][x].widget),gc,
                    circle1+4,circle1+4,circle2,circle2,0,360*64);
                SetColor(Black);
                XDrawArc(dpy,XtWindow(square[y][x].widget),gc,
                    circle1+4,circle1+4,circle2,circle2,0,360*64);
            }
        } 
        else {
            SetColor(Green);
            XFillArc(dpy,XtWindow(square[y][x].widget),gc,
                circle1-2,circle1-2,circle2+8,circle2+8,0,360*64);
        }
        SetColor(square[y][x].hilite);
        XDrawRectangle(dpy,XtWindow(square[y][x].widget),gc,0,0,squaresize,squaresize);
    }
    XSync(dpy,False);
}

void updatesquare(Widget widget, XtPointer client, XEvent *event)
{
    int x,y;

    for(y=0; y<8; y++)
    for(x=0; x<8; x++)
    if(x%2 != y%2 && widget == square[y][x].widget) {
        UpdateSquare(x,y);
        break;
    }
}

void UpdateBoard(void)
{
    int x,y;

    for(y=0; y<8; y++)
    for(x=0; x<8; x++)
    if(x%2 != y%2) UpdateSquare(x,y);
}

void ResetBoard(void)
{
    int x,y,pos;

    ClearBoard();
    pos = 1;
    for(y=0; y<8; y++)
    for(x=0; x<8; x++)
    {
        if(x%2 != y%2) {
            square[y][x].val = pos;
            if(y<3 || y>4) square[y][x].state = Piece; else square[y][x].state = Empty;
            if(y<3) square[y][x].col = Red; else
            if(y>4) square[y][x].col = White; else
            square[y][x].col = Black;
            square[y][x].hilite = Green;
#ifdef GRAPHICS
            UpdateSquare(x,y);
#endif
            pos++;
        } 
    }
}

void ClearBoard(void)
{
    int x,y;
    
    for(y=0; y<8; y++)
    for(x=0; x<8; x++)
    {
        if(x%2 != y%2) {
            square[y][x].state = Empty;
            square[y][x].col = Black;
#ifdef GRAPHICS
            UpdateSquare(x,y);
#endif
        } 
    }
}

void Message(char *str)
{
#ifdef GRAPHICS
    XmString mess;
    mess = XmStringCreateLocalized(str);
    XtVaSetValues(message,XmNlabelString,mess,NULL);
    XmStringFree(mess);
#else
   printf("%s\n",str);
#endif
}

Widget LocateShell(Widget w)
{
    if(!w) return (Widget)0;
    while(w) {
        if(XtIsShell(w) || XmIsDialogShell(w)) return w;
        w = XtParent(w);
    }
    return (Widget)0;
}

void newcallback(Widget w, XtPointer client, XtPointer call)
{
    XmAnyCallbackStruct *cbs = (XmAnyCallbackStruct *)call;

    switch(cbs->reason) {
        case XmCR_CANCEL: cancelled = 1; 
        case XmCR_OK:    XtPopdown(XtParent(w));
                  dialog_open = 0;
                  break;
    }
}

int NewDialog(char *player1, char *player2, float *SecPerMove)
{
    Widget dialog,form,subform,label,text[3];
    Arg args[6];
    XmString ok,cancel,title;
    int i,n=0;
    char buf[32],*field1,*field2,*field3;
    
    ok = XmStringCreateLocalized("OK");
    cancel = XmStringCreateLocalized("Cancel");
    title = XmStringCreateLocalized("New Game");
    XtSetArg(args[n],XmNautoUnmanage, False); n++;
    XtSetArg(args[n],XmNokLabelString, ok); n++;
    XtSetArg(args[n],XmNcancelLabelString, cancel); n++;
    XtSetArg(args[n],XmNdialogTitle, title); n++;
    XtSetArg(args[n],XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL); n++;
    dialog = (Widget) XmCreateTemplateDialog(mainWidget,"new",args,n);
    XtAddCallback(dialog,XmNokCallback, newcallback, NULL);
    XtAddCallback(dialog,XmNcancelCallback, newcallback, NULL);
    XmStringFree(ok);
    XmStringFree(cancel);
    XmStringFree(title);

    form = XtVaCreateWidget("form",
        xmFormWidgetClass,dialog,
        XmNautoUnmanage, False,
        NULL);

    for(i=0; i<3; i++) {
        subform = XtVaCreateWidget("subform",
            xmFormWidgetClass, form,
            XmNtopAttachment, i ? XmATTACH_WIDGET : XmATTACH_FORM,
            XmNtopWidget, subform,
            XmNleftAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            NULL);

        if(i<2) sprintf(buf,"Player %d:",i+1); 
            else sprintf(buf,"Seconds per move:");
        label = XtVaCreateManagedWidget(buf,
            xmLabelGadgetClass, subform,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_FORM,
            XmNalignment, XmALIGNMENT_BEGINNING,
            NULL);

        sprintf(buf, "text_%d",i);
        text[i] = XtVaCreateManagedWidget(buf,
            xmTextFieldWidgetClass, subform,
            XmNtopAttachment, XmATTACH_FORM,
            XmNbottomAttachment, XmATTACH_FORM,
            XmNrightAttachment, XmATTACH_FORM,
            XmNleftAttachment, XmATTACH_WIDGET,
            XmNleftWidget, label,
            XmNvalue, xval[i],
            NULL);
        XtManageChild(subform);
    }
    XtManageChild(dialog);
    XtManageChild(form);
    XtRealizeWidget(dialog);
    dialog_open = 1;    
    cancelled = 0;
    while(dialog_open) HandleEvents();
    field1 = XmTextFieldGetString(text[0]);
    field2 = XmTextFieldGetString(text[1]);
    field3 = XmTextFieldGetString(text[2]);    
    XtDestroyWidget(dialog);
    strcpy(player1,field1); strcpy(xval[0],field1); XtFree(field1);
    strcpy(player2,field2); strcpy(xval[1],field2); XtFree(field2);
    *SecPerMove = (float) atof(field3); strcpy(xval[2],field3); XtFree(field3);
    if(cancelled) return 0; else return 1;
}

void MapColor(Colormap cmap, unsigned short i, int red, int green, int blue)
{
    XColor col;

    if(red > 255) red = 255;
    if(green > 255) green = 255;
    if(blue > 255) blue = 255;

    col.pixel = (unsigned long) i;
    col.red = (unsigned short) (red * 65535/256);
    col.green = (unsigned short) (green * 65535/256);
    col.blue = (unsigned short) (blue * 65535/256);
    col.flags = DoRed | DoBlue | DoGreen;

    XAllocColor(dpy,cmap, &col);
    color[i] = col.pixel;
}

Colormap CreateColorMap(Widget main)
{
    Colormap cmap,def_cmap;
    XColor Colors[256];
    int i,ncolors;
    unsigned long pixels[256],planes[1];

    /* Find out how many colors are available on the default colormap */
        cmap = DefaultColormap (dpy, DefaultScreen (dpy));
        ncolors = 256;
        while(ncolors>1) {
                if (XAllocColorCells(dpy,cmap,0,planes,0,pixels,ncolors)) break;
                ncolors--;
        }
        if(ncolors>1) XFreeColors(dpy,cmap,pixels,ncolors,0);

    /* If there are not enough colors available, create a new colormap */
    if(ncolors < 5) {
        ncolors = 5;
        def_cmap = DefaultColormap (dpy, DefaultScreen(dpy));
        for(i=0; i<200; i++) {
            Colors[i].pixel = i;
            Colors[i].flags = DoRed|DoGreen|DoBlue;
        }
        XQueryColors(dpy,def_cmap,Colors,200);
        cmap = XCreateColormap(dpy,DefaultRootWindow(dpy),
            DefaultVisual(dpy,DefaultScreen(dpy)),
            AllocNone);
        XAllocColorCells(dpy,cmap,1,planes,0,pixels,200);
        //XStoreColors(dpy,cmap,Colors,200);
    }

    /* Map the colors we will use */
    memset(color,0,sizeof(unsigned long)*256);
    for(i=0; i<5; i++) MapColor(cmap,i,Color[i][0],Color[i][1],Color[i][2]);

    /* Return the colormap */
    return(cmap);
}

void file_cb(Widget widget, XtPointer client, XtPointer call)
{
    int selection = (int) client;

    switch(selection) {
        case 0:    /* New Game */
            NewGame();
            break;
        case 1: /* Stop Game */
            StopGame();
            Message("Game stopped");
            break;
        case 2:    /* Quit */
            ClearBoard();
            StopGame();
            exit(0);
    }
}
    
Widget CreateMenuBar(Widget main)
{
    Widget menubar;
    XmString file,mode,step,options,open,stop,quit;

    /* Create the menubar */
    file = XmStringCreateLocalized("File");
    mode = XmStringCreateLocalized("Mode");
    step = XmStringCreateLocalized("Step");
    options = XmStringCreateLocalized("Options");
    menubar = XmVaCreateSimpleMenuBar(main,"menubar",
        XmVaCASCADEBUTTON, file, 'F',
        XmVaCASCADEBUTTON, mode, 'M',
        XmVaCASCADEBUTTON, step, 'S',
        XmVaCASCADEBUTTON, options, 'O',
        NULL);
    XmStringFree(file);
    XmStringFree(mode);
    XmStringFree(step);
    XmStringFree(options);

    /* File Menu */
    open = XmStringCreateLocalized("New Game...");
    stop = XmStringCreateLocalized("Stop Game");
    quit = XmStringCreateLocalized("Quit");
    XmVaCreateSimplePulldownMenu(menubar,"file_menu",0,file_cb,
        XmVaPUSHBUTTON,open,'O',NULL,NULL,
        XmVaPUSHBUTTON,stop,'S',NULL,NULL,
        XmVaSEPARATOR,
        XmVaPUSHBUTTON,quit,'Q',NULL,NULL,
        NULL);
    XmStringFree(open);
    XmStringFree(stop);
    XmStringFree(quit);
        
    /* Manage the menubar and return the widget */
    XtManageChild(menubar);
    return menubar;
}

Widget CreateWorkWin(Widget main)
{
    Widget workwin;
    int x,y,col,width,height,pos;
    char label[16];

    /* Determine size of board */
    height = HeightOfScreen(XtScreen(main));
    width = WidthOfScreen(XtScreen(main));
    if(width < 640 || height < 640) squaresize=40; else
    if(width < 800 || height < 800) squaresize=50; else
    if(width < 960 || height < 960) squaresize=60; else
    if(width < 1120 || height < 1120) squaresize=70; else
    squaresize=80;

    /* Determine circle coordinates for drawing pieces */
    circle1 = squaresize/5;
    circle2 = squaresize - circle1*2;

    /* Create the work window */
    workwin = XtVaCreateManagedWidget("workwin",
        xmFormWidgetClass, main,
        XmNwidth, squaresize*8,
        XmNheight, squaresize*8,
        XmNfractionBase, 8,
        NULL);

    /* Set up the checker board in the work window */
    pos = 1;
    for(y=0; y<8; y++)
    for(x=0; x<8; x++)
    {
        if(x%2 != y%2) {
            col = color[Green];
            sprintf(label,"%d",pos);
            pos++;
        }
        else {
            col = color[Tan]; 
            strcpy(label," ");
        }
        square[y][x].widget = XtVaCreateManagedWidget(label,
            xmLabelWidgetClass, workwin,
            XmNheight, squaresize,
            XmNwidth, squaresize,
            XmNtopAttachment, XmATTACH_POSITION,
            XmNtopPosition, y,
            XmNbottomAttachment, XmATTACH_POSITION,
            XmNbottomPosition, y+1,
            XmNleftAttachment, XmATTACH_POSITION,
            XmNleftPosition, x,
            XmNrightAttachment, XmATTACH_POSITION,
            XmNrightPosition, x+1,
            XmNmarginTop, 2,
            XmNmarginBottom, squaresize-20,
            XmNmarginRight, squaresize-20, 
            XmNmarginLeft, 2,
            XmNbackground, col,
            NULL);
        XtAddEventHandler(square[y][x].widget,ExposureMask,FALSE,
            (XtEventHandler)updatesquare,(XtPointer) 0);
        XtAddEventHandler(square[y][x].widget,ButtonPressMask,FALSE,
            (XtEventHandler)buttonpress,(XtPointer) 0);
    }

    /* Manage the work window and return the widget */
    XtManageChild(workwin);
    return workwin;
}

Widget CreateMessage(Widget main)
{
    Widget message;
    XmString mess;

    /* Create the message label */
    mess = XmStringCreateLocalized("Welcome to checkers");
    message = XtVaCreateManagedWidget("message",
        xmLabelGadgetClass,main,
        XmNalignment,XmALIGNMENT_BEGINNING,
        XmNmarginTop, 5,
        XmNmarginBottom, 5,
        XmNmarginRight, 5,
        XmNmarginLeft, 5,
        XmNlabelString, mess,
        NULL);
    XmStringFree(mess);

    /* Manage the message label and return the widget */
    XtManageChild(message);
    return message;
}

void HandleEvents(void)
{
#ifdef GRAPHICS
    XEvent event;
    while(XtAppPending(app)) {
        XtAppNextEvent (app, &event); 
        XtDispatchEvent (&event);
    }
#endif
}

void InitGraphics(int argc, char *argv[])
{
    Dimension width, height;
    XEvent event;
    static String fallback_resources[] = {
        "*xcheck*background: #e0e0e0",
        "*xcheck*fontList: -*-helvetica-bold-r-normal--14-*-*-*-*-*-*-*",
        "*xcheck*font: -*-helvetica-bold-r-normal--14-*-*-*-*-*-*-*",
        "*xcheck*XmText.background:  #b0e0c0",
        NULL,
    };

    XtSetLanguageProc(NULL,NULL,NULL);
    toplevel = XtVaAppInitialize(
        &app,"App-Class",
        NULL,0,
        &argc,argv,
        fallback_resources,
        NULL);
    mainWidget = XtVaCreateManagedWidget("mw", xmMainWindowWidgetClass, toplevel, NULL);
    dpy = XtDisplay(mainWidget);
    gc = XCreateGC(dpy,DefaultRootWindow(dpy),NULL,NULL);
    XSetLineAttributes(dpy,gc,2,LineSolid,CapButt,JoinMiter);
    colormap = CreateColorMap(mainWidget);
    menubar = CreateMenuBar(mainWidget);
    workwin = CreateWorkWin(mainWidget);
    message = CreateMessage(mainWidget);
    XtVaSetValues(mainWidget,
        XmNcolormap,colormap,
        XmNmenuBar,menubar,
        XmNworkWindow,workwin,
        XmNmessageWindow,message,
        NULL);
    XtRealizeWidget(toplevel);
    do {
        XtAppNextEvent(app,&event);
        XtDispatchEvent(&event);
    }
    while(event.type != Expose);

    XtVaGetValues(toplevel,XmNheight,&height,XmNwidth,&width,NULL);
    XtVaSetValues(toplevel,
        XmNwidth, width,
        XmNheight, height,
        XmNmaxWidth, width,
        XmNmaxHeight, height,
        XmNminWidth, width,
        XmNminHeight, height,
        XmNallowShellResize, False,
        NULL);
    HandleEvents();

    strcpy(xval[0],"human");
    strcpy(xval[1],"computer");
    strcpy(xval[2],"10.0");
}

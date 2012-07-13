#define TITLE "Hot Scroll PM Loader"
#define INI_FILENAME "hotscrl.ini"
#define CLIENTCLASS "Hot Scroll PM Loader (C) 1998 Samuel Audet <guardia@cam.org>"
#define FONT1 "9.WarpSans"
#define FONT2 "8.Helv"

#define INCL_PM
#define INCL_DOS
#include <os2.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "pmloader.h"
#include "hotscroll.h"

BOOL EXPENTRY HotScrollHookInit(void);
BOOL EXPENTRY HotScrollHookKill(void);
BOOL EXPENTRY HotScrollWaitLoaded(void);
BOOL EXPENTRY HotScrollLoaded(void);
MRESULT EXPENTRY FrameWndProc(HWND hwnd,ULONG msg,MPARAM mp1,MPARAM mp2);
BOOL CreateNotebook(HWND hwnd);
HWND LoadNotebook(HWND hwndNB);
MRESULT EXPENTRY wpActivation( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 );
MRESULT EXPENTRY wpKeyEntryField( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 );
MRESULT EXPENTRY wpPtrFileEntryField( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 );
MRESULT EXPENTRY wpOptions( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 );
MRESULT EXPENTRY wpAppEntryField( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 );
MRESULT EXPENTRY wpAppListBox( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 );
MRESULT EXPENTRY wpTimerOptions( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 );
char *getINIfile(void);
char *getEXEpath(void);

struct KEYOPT keyopt = DEFAULT_KEYOPT;
char ptrfile[2*CCHMAXPATH];
char cursorfile[2*CCHMAXPATH];

struct APPOPT defappopt = DEFAULT_APPOPT;
struct APPOPT initialappopt = INIT_APPOPT;
struct APPOPT *curappopt = &defappopt;

ULONG temp;

/* application wide used variables */
HAB hab;
char *EXEpath, *INIfile;

/* font to be used as system font */
char *font;

/* for the window sizes */
#define FRAME 0
#define OKPB 1
#define LOADPB 2
#define UNLOADPB 3

POINTL size[4];

/* error display handling */
char errormsg[100]; /* error message */
int errornum;       /* error counter */
RECTL location;     /* error message location in window */

RECTL loadedpaint;  /* display if hotscroll is loaded or not */

/* used to sublass hotkey/application entry field */
FNWP *WinOldEntryFieldWindowProc,
     *WinOldListBoxWindowProc;

/* main window handles */
HWND hwndFrame, hwndClient;

/* notebook page information, see hotcorn.h */

NBPAGE nbpage[] =
{
   {wpActivation,                "Tab 1 of 3", "Ac~tivation",    ID_ACTIVATION,    FALSE, BKA_MAJOR, 0, 0},
   {wpOptions,      "Page 1 of 2  Tab 2 of 3", "O~ptions",       ID_OPTIONS,       FALSE, BKA_MAJOR, 0, 0},
   {wpTimerOptions, "Page 2 of 2  Tab 2 of 3", "Timer ~Scrolling Options", ID_TIMER_OPTIONS, FALSE, BKA_MINOR, 0, 0},
   {NULL,                        "Tab 3 of 3", "A~bout",         ID_ABOUT,         FALSE, BKA_MAJOR, 0, 0}
};

/* some constants so that we don't have to modify code if we add pages */
#define PAGE_COUNT (sizeof( nbpage ) / sizeof( NBPAGE ))
#define PAGE_ACTIVATION    0
#define PAGE_OPTIONS       1
#define PAGE_TIMER_OPTIONS 2
#define PAGE_ABOUT         3

int main()
{
   HMQ hmq;

   ULONG frameFlags = FCF_TITLEBAR | FCF_SYSMENU | FCF_MINBUTTON |/* FCF_MAXBUTTON | */
                      /* FCF_SIZEBORDER  | FCF_MENU | */ FCF_ACCELTABLE | FCF_ICON |
                      /* FCF_SHELLPOSITION | */ FCF_TASKLIST | FCF_DLGBORDER;
   QMSG qmsg;
   HEV hev;

   HPS hps;
   LONG fontcounter = 0;

   /* if this gets the value of NULL, the program should use
      the current dir, ie.: no path assignment before filename */
   EXEpath = getEXEpath();
   /* this finds INI_FILENAME where hotscrl.dll resides */
   INIfile = getINIfile();

   /* window size in dialog units */
   size[FRAME].x = 395;   size[FRAME].y = 220;

   /* button size in dialog units */
   size[OKPB].x = 24;     size[OKPB].y = 14;
   size[LOADPB].x = 38;   size[LOADPB].y = 14;
   size[UNLOADPB].x = 32; size[UNLOADPB].y = 14;

   /* convert these sizes in pixel */
   WinMapDlgPoints(HWND_DESKTOP, size, 4, TRUE);

   /* init of error display message handling in the main window */
   errornum = 1;
   strcpy(errormsg,"");
   location.xRight = 400; /* put a lot */
   /* locate error message after the buttons */
   location.xLeft = 5+size[OKPB].x+5+size[LOADPB].x+5+size[UNLOADPB].x+5;
   location.yTop = 35;
   location.yBottom = location.yTop - 15;

   loadedpaint.xRight = location.xRight;
   loadedpaint.xLeft = location.xLeft;
   loadedpaint.yTop = location.yBottom;
   loadedpaint.yBottom = loadedpaint.yTop - 15;

   /* decide which font to use */
   hps = WinGetPS(HWND_DESKTOP);

   if(GpiQueryFonts(hps, QF_PUBLIC,strchr(FONT1,'.')+1, &fontcounter, 0, NULL))
      font = FONT1;
   else
      font = FONT2;

   WinReleasePS(hps);

   /* already loaded detecter */
   if(DosCreateEventSem("\\SEM32\\HOTSCROLL\\RUNNING.SEM",&hev,0,FALSE))
   {
      DosBeep(600,200);
      sprintf(errormsg,"%d. Warning, already running",errornum++);
   }

   /* initialize PM */
   hab = WinInitialize(0);

   hmq = WinCreateMsgQueue(hab, 0);

   WinRegisterClass( hab, CLIENTCLASS, FrameWndProc,
                     CS_CLIPCHILDREN | CS_SIZEREDRAW, 0 );

   hwndFrame = WinCreateStdWindow (HWND_DESKTOP, 0, &frameFlags, CLIENTCLASS,
                                   TITLE, 0, 0, ID_MAIN, &hwndClient);

   if (hwndFrame)
   {
      SWP pos;
      HINI INIhandle;
      ULONG state;

      /* get window position from INI file */
      if (INIhandle = PrfOpenProfile(hab,INIfile))
      {
         ULONG datasize;

         PrfQueryProfileSize(INIhandle, "Position", "state", &datasize);
            if (datasize == sizeof(state))
               PrfQueryProfileData(INIhandle, "Position", "state", &state, &datasize);
            else state = 0;

         PrfQueryProfileSize(INIhandle, "Position", "x", &datasize);
            if (datasize == sizeof(pos.x))
               PrfQueryProfileData(INIhandle, "Position", "x", &pos.x, &datasize);
            else pos.x = 30;

         PrfQueryProfileSize(INIhandle, "Position", "y", &datasize);
            if (datasize == sizeof(pos.y))
               PrfQueryProfileData(INIhandle, "Position", "y", &pos.y, &datasize);
            else pos.y = 30;

         PrfCloseProfile(INIhandle);
      }

      /* set window position */
      WinSetWindowPos( hwndFrame, NULLHANDLE,
                       pos.x, pos.y, size[FRAME].x, size[FRAME].y,
                       SWP_SIZE | SWP_MOVE | SWP_SHOW | state );

      while( WinGetMsg( hab, &qmsg, 0L, 0, 0 ) )
         WinDispatchMsg( hab, &qmsg );

      DosCloseEventSem(hev);

      HotScrollHookKill();

      /* save window position in INI file */
      if (INIhandle = PrfOpenProfile(hab,INIfile))
      {
         if (WinQueryWindowPos(hwndFrame, &pos))
         {
            state = 0;
            if (pos.fl & SWP_MINIMIZE)
            {
               pos.x = (LONG) WinQueryWindowUShort(hwndFrame, QWS_XRESTORE);
               pos.y = (LONG) WinQueryWindowUShort(hwndFrame, QWS_YRESTORE);
               state = SWP_MINIMIZE;
            }
            else if (pos.fl & SWP_MAXIMIZE)
            {
               pos.x = (LONG) WinQueryWindowUShort(hwndFrame, QWS_XRESTORE);
               pos.y = (LONG) WinQueryWindowUShort(hwndFrame, QWS_YRESTORE);
               state = SWP_MAXIMIZE;
            }

            PrfWriteProfileData(INIhandle, "Position","state", &state, sizeof(state));
            PrfWriteProfileData(INIhandle, "Position","x", &pos.x, sizeof(pos.x));
            PrfWriteProfileData(INIhandle, "Position","y", &pos.y, sizeof(pos.y));
         }
         PrfCloseProfile(INIhandle);
      }

      WinDestroyWindow(hwndFrame);        /* Tidy up...                    */
   }
   WinDestroyMsgQueue( hmq );             /* Tidy up...                    */
   WinTerminate( hab );                   /* Terminate the application     */

   return 1;
}

MRESULT EXPENTRY FrameWndProc(HWND hwnd,ULONG msg,MPARAM mp1,MPARAM mp2)
{
   switch (msg)
   {

   case WM_CHAR:
      if (!(SHORT1FROMMP(mp1) & KC_KEYUP))
      {
         /* implent tab and arrow movements around the dialog items */
         if (SHORT1FROMMP(mp1) & KC_VIRTUALKEY)
            switch(SHORT2FROMMP(mp2))
            {
               case VK_RIGHT:
                  WinSetFocus(HWND_DESKTOP,WinEnumDlgItem(hwnd,WinQueryFocus(HWND_DESKTOP), EDI_NEXTGROUPITEM));
                  break;
               case VK_LEFT:
                  WinSetFocus(HWND_DESKTOP,WinEnumDlgItem(hwnd,WinQueryFocus(HWND_DESKTOP), EDI_PREVGROUPITEM));
                  break;
               case VK_DOWN:
                  WinSetFocus(HWND_DESKTOP,WinEnumDlgItem(hwnd,WinQueryFocus(HWND_DESKTOP), EDI_NEXTGROUPITEM));
                  break;
               case VK_UP:
                  WinSetFocus(HWND_DESKTOP,WinEnumDlgItem(hwnd,WinQueryFocus(HWND_DESKTOP), EDI_PREVGROUPITEM));
                  break;
               case VK_TAB:
                  WinSetFocus(HWND_DESKTOP,WinEnumDlgItem(hwnd,WinQueryFocus(HWND_DESKTOP), EDI_NEXTTABITEM));
                  break;
               case VK_BACKTAB:
                  WinSetFocus(HWND_DESKTOP,WinEnumDlgItem(hwnd,WinQueryFocus(HWND_DESKTOP), EDI_PREVTABITEM));
                  break;
            }

         /* match mnemonic of buttons, and send BM_CLICK */
         else if (SHORT1FROMMP(mp1) & KC_CHAR)
         {
            HWND itemhwnd = WinEnumDlgItem(hwnd, 0, EDI_FIRSTTABITEM);

            do if ((BOOL)WinSendMsg(itemhwnd, WM_MATCHMNEMONIC, mp2, 0))
                  WinSendMsg(itemhwnd, BM_CLICK, SHORT1FROMMP(FALSE), 0);
            while((itemhwnd = WinEnumDlgItem(hwnd, itemhwnd, EDI_NEXTTABITEM))
                   != WinEnumDlgItem(hwnd, 0, EDI_FIRSTTABITEM));
         }
      }
      break;

   case WM_COMMAND:
   {
      switch (SHORT1FROMMP(mp1))
      {
         case ID_OKPB:
            /* save in INI file */
            {
               HINI INIhandle;
               BOOL rc = 0;

               if (INIhandle = PrfOpenProfile(hab,INIfile))
               {
                  USHORT i,size;
                  char *buffer;
                  HWND hwndl = WinWindowFromID(nbpage[PAGE_OPTIONS].hwnd,ID_APP_LIST);

                  rc += !PrfWriteProfileData(INIhandle,"Options",NULL,NULL,0);

                  for(i = 0 ; i < SHORT1FROMMR(WinSendMsg(hwndl,LM_QUERYITEMCOUNT,0,0)) ; i++)
                  {
                     size = SHORT1FROMMR(WinSendMsg(hwndl,LM_QUERYITEMTEXTLENGTH,MPFROMSHORT(i),0))+1;
                     buffer = alloca(size);
                     WinSendMsg(hwndl,LM_QUERYITEMTEXT,MPFROM2SHORT(i,size),MPFROMP(buffer));
                     rc += !PrfWriteProfileData(INIhandle,"Options",buffer,PVOIDFROMMR(WinSendMsg(hwndl,LM_QUERYITEMHANDLE,MPFROMSHORT(i),0)),sizeof(struct APPOPT));
                  }

                  rc += !PrfWriteProfileData(INIhandle,"Activation","Settings",&keyopt,sizeof(keyopt));
                  if(ptrfile[0])
                     rc += !PrfWriteProfileString(INIhandle,"Activation","Pointer",ptrfile);
                  else
                     PrfWriteProfileString(INIhandle,"Activation","Pointer",NULL);

                  if(cursorfile[0])
                     rc += !PrfWriteProfileString(INIhandle,"Activation","Cursor",cursorfile);
                  else
                     PrfWriteProfileString(INIhandle,"Activation","Cursor",NULL);


                  if (rc > 0)
                  {
                     sprintf(errormsg,"%d. Error saving INI file",errornum++);
                     WinInvalidateRect( hwnd, &location, FALSE );
                  }
               }
               else
               {
                  sprintf(errormsg,"%d. Error opening INI file",errornum++);
                  WinInvalidateRect( hwnd, &location, FALSE );
               }
            }

         case ID_LOADPB:
            if(HotScrollHookInit())
               HotScrollWaitLoaded();
            else if (!HotScrollLoaded())
            {
               sprintf(errormsg,"%d. Hook NOT successfully loaded.",errornum++);
               WinInvalidateRect( hwnd, &location, FALSE );
            }
            WinInvalidateRect( hwnd, &loadedpaint, FALSE );
            break;

         case ID_UNLOADPB:
            if(!HotScrollHookKill() && HotScrollLoaded())
            {
               sprintf(errormsg,"%d. Hook NOT successfully unloaded.",errornum++);
               WinInvalidateRect( hwnd, &location, FALSE );
            }
            WinInvalidateRect( hwnd, &loadedpaint, FALSE );
            break;

         case ID_ACTIVATION:
            WinSendDlgItemMsg(hwnd, ID_NB, BKM_TURNTOPAGE,
                    MPFROMLONG(nbpage[PAGE_ACTIVATION].ulPageId), 0);
            WinSetFocus(HWND_DESKTOP,WinWindowFromID(hwnd,ID_NB));
            break;

         case ID_OPTIONS:
            WinSendDlgItemMsg(hwnd, ID_NB, BKM_TURNTOPAGE,
                    MPFROMLONG(nbpage[PAGE_OPTIONS].ulPageId), 0);
            WinSetFocus(HWND_DESKTOP,WinWindowFromID(hwnd,ID_NB));
            break;

         case ID_TIMER_OPTIONS:
            WinSendDlgItemMsg(hwnd, ID_NB, BKM_TURNTOPAGE,
                    MPFROMLONG(nbpage[PAGE_TIMER_OPTIONS].ulPageId), 0);
            WinSetFocus(HWND_DESKTOP,WinWindowFromID(hwnd,ID_NB));
            break;

         case ID_ABOUT:
            WinSendDlgItemMsg(hwnd, ID_NB, BKM_TURNTOPAGE,
                    MPFROMLONG(nbpage[PAGE_ABOUT].ulPageId), 0);
            WinSetFocus(HWND_DESKTOP,WinWindowFromID(hwnd,ID_NB));
            break;

         /* alt-arrows up and down */
         case ID_NBFOCUS:
            WinSetFocus(HWND_DESKTOP,WinWindowFromID(hwnd,ID_NB));
            break;
         case ID_OKFOCUS:
            WinSetFocus(HWND_DESKTOP,WinWindowFromID(hwnd,ID_OKPB));
            break;

      }
      return 0;
   }

   case WM_CREATE:
   {
      HINI INIhandle;
      BOOL rc = 0;
      HWND hwndNB;

      WinSetPresParam(hwnd, PP_FONTNAMESIZE, strlen(font)+1, font);

      if (!(hwndNB = CreateNotebook(hwnd)))
         return (MRESULT) TRUE;

      /* load INI file */
      if (INIhandle = PrfOpenProfile(hab,INIfile))
      {
         ULONG datasize;
         char *buffer;
         HWND hwndl;
         PAGESELECTNOTIFY pageselect;
         struct APPOPT *appopt;
         short item;

         /* gotta load notebook page with listbox */
         pageselect.hwndBook = WinWindowFromID(hwnd,ID_NB);
         pageselect.ulPageIdNew = pageselect.ulPageIdCur = nbpage[PAGE_OPTIONS].ulPageId;
         WinSendMsg(hwnd,WM_CONTROL,MPFROM2SHORT(ID_NB,BKN_PAGESELECTEDPENDING),MPFROMP(&pageselect));

         hwndl = WinWindowFromID(nbpage[PAGE_OPTIONS].hwnd,ID_APP_LIST);

         PrfQueryProfileSize(INIhandle, "Options", NULL, &datasize);
         buffer = alloca(datasize);
         PrfQueryProfileData(INIhandle, "Options", NULL, buffer, &datasize);

         while(buffer[0])
         {
            /* load text */
            PrfQueryProfileSize(INIhandle, "Options", buffer, &datasize);

            if(datasize == sizeof(struct APPOPT))
            {
               appopt = (struct APPOPT *) malloc(datasize);
               PrfQueryProfileData(INIhandle, "Options", buffer, appopt, &datasize);

               if(!stricmp(buffer,"Default"))
               {
                  defappopt = *appopt;
                  free(appopt);
               }
               else
               {
                  item = (SHORT) WinSendMsg(hwndl, LM_INSERTITEM, MPFROMSHORT(LIT_END), MPFROMP(buffer));
                  if((item != LIT_ERROR) && (item != LIT_MEMERROR))
                     WinSendMsg(hwndl, LM_SETITEMHANDLE, MPFROMSHORT(item), MPFROMLONG(appopt));
               }
            }

            buffer = strchr(buffer,'\0') + 1;
         }

         /* select Default */
         WinSendMsg(hwndl, LM_SELECTITEM, 0, MPFROMLONG(TRUE));

         PrfQueryProfileSize(INIhandle, "Activation", "Settings", &datasize);
         if(datasize == sizeof(keyopt))
            PrfQueryProfileData(INIhandle, "Activation", "Settings", &keyopt, &datasize);

         PrfQueryProfileString(INIhandle, "Activation", "Pointer", "", ptrfile, sizeof(ptrfile));
         PrfQueryProfileString(INIhandle, "Activation", "Cursor", "", cursorfile, sizeof(cursorfile));

         PrfCloseProfile(INIhandle);

         if (rc > 0)
         {
            sprintf(errormsg,"%d. Error loading INI file",errornum++);
            WinInvalidateRect( hwnd, &location, FALSE );
         }
      }
      else
      {
         sprintf(errormsg,"%d. Error opening INI file",errornum++);
         WinInvalidateRect( hwnd, &location, FALSE );
      }

      /* load hook */
      WinSendMsg(hwnd, WM_COMMAND, MPFROMSHORT(ID_LOADPB), NULL);

      /* turn to first notebook page */
      WinSendMsg( hwndNB, BKM_TURNTOPAGE, MPFROMLONG(

         (ULONG) WinSendMsg(hwndNB, BKM_QUERYPAGEID, NULL,
                MPFROM2SHORT(BKA_FIRST, BKA_MAJOR))

                                                              ), NULL );

      return 0;
   }
   /* received from the notebook window, allows loading and refreshing of pages */
   case WM_CONTROL:
   {
      switch(SHORT2FROMMP(mp1))
      {
      case BKN_PAGESELECTEDPENDING:
      {
         PAGESELECTNOTIFY *pageselect = PVOIDFROMMP(mp2);
         /* get page window handle */
         HWND FrameHwnd = (HWND) LONGFROMMR(WinSendMsg(pageselect->hwndBook, BKM_QUERYPAGEWINDOWHWND,
                                    MPFROMLONG(pageselect->ulPageIdNew), NULL));

         NBPAGE *pagedata = (NBPAGE *) PVOIDFROMMR(WinSendMsg(pageselect->hwndBook, BKM_QUERYPAGEDATA,
                              MPFROMLONG(pageselect->ulPageIdNew), NULL));

         if (!pagedata) return 0;

         /* skip dummy (major) pages that we want to skip to only show their minor pages */
         if(pagedata->skip)
         {
            ULONG ulPageNext = (ULONG) WinSendMsg( pageselect->hwndBook, BKM_QUERYPAGEID,
                                        MPFROMLONG( pageselect->ulPageIdNew ),
                                        MPFROM2SHORT( BKA_NEXT, 0 ) );

            /* if the next page was the previously viewed page, means the user is
               going backward, so we show the previous page */
            if(ulPageNext == pageselect->ulPageIdCur)
               WinSendMsg(pageselect->hwndBook, BKM_TURNTOPAGE, MPFROMLONG(

                   (ULONG) WinSendMsg(pageselect->hwndBook, BKM_QUERYPAGEID,
                      MPFROMLONG(pageselect->ulPageIdNew), MPFROM2SHORT(BKA_PREV, 0))

                                         ), NULL);
            /* else we want to show the next page */
            else
               WinSendMsg( pageselect->hwndBook, BKM_TURNTOPAGE, MPFROMLONG(ulPageNext), NULL);

            /* prevents the current page from showing at all */
            pageselect->ulPageIdNew = 0;
            break;
         }
         else if (!FrameHwnd)
         {
            /* loading dialog page frame */
            pagedata->hwnd = WinLoadDlg(pageselect->hwndBook, pageselect->hwndBook,
                                   pagedata->pfnwpDlg, 0, pagedata->idDlg, NULL);

            WinSetPresParam(pagedata->hwnd, PP_FONTNAMESIZE, strlen(font)+1, font);

            WinSendMsg(pageselect->hwndBook, BKM_SETPAGEWINDOWHWND,
               MPFROMLONG(pageselect->ulPageIdNew), MPFROMHWND(pagedata->hwnd));

            /* I want the focus on the notebook tab after load */
            WinSetFocus(HWND_DESKTOP, pageselect->hwndBook);
         }
         else
            /* send refresh */
            WinSendMsg(FrameHwnd, WM_USER, 0, 0);
         break;
      }
      }
      return 0;
   }

   case WM_ERASEBACKGROUND:
      /* fill leftout (see WM_SIZE) background with gray color */
      WinFillRect(HWNDFROMMP(mp1), PVOIDFROMMP(mp2), SYSCLR_FIELDBACKGROUND);
      return 0;

   case WM_PAINT:
   {
      /* display error messages */
      HPS hps = WinGetPS(hwnd);
      WinFillRect(hps, &location, SYSCLR_FIELDBACKGROUND);
      if (errormsg[0] != '\0')
         WinDrawText(hps, -1, errormsg, &location, CLR_RED, BM_LEAVEALONE, 0);

      WinFillRect(hps, &loadedpaint, SYSCLR_FIELDBACKGROUND);
      if(HotScrollLoaded())
         WinDrawText(hps, -1, "Hot Scroll is currently loaded.", &loadedpaint, CLR_BLACK, BM_LEAVEALONE, 0);
      else
         WinDrawText(hps, -1, "Hot Scroll NOT is currently loaded.", &loadedpaint, CLR_BLACK, BM_LEAVEALONE, 0);

      WinReleasePS(hps);
      break; /* to let the default window procedure draw the notebook */
   }

   case WM_BUTTON1CLICK:
      if( (SHORT1FROMMP(mp1) > location.xLeft) &&
          (SHORT1FROMMP(mp1) < location.xRight) &&
          (SHORT2FROMMP(mp1) > location.yBottom) &&
          (SHORT2FROMMP(mp1) < location.yTop) )
      {
         errormsg[0] = '\0';
         WinInvalidateRect( hwnd, &location, FALSE );
      }
      break;

   case WM_SIZE:
      /* position notebook in the main client window over the buttons */
      WinSetWindowPos(WinWindowFromID(hwnd, ID_NB), 0,
                                      5, /* x position */
                        size[OKPB].y+10, /* y position */
                   SHORT1FROMMP(mp2)-10, /* x width */
      SHORT2FROMMP(mp2)-size[OKPB].y-15, /* y width */
      SWP_SIZE | SWP_SHOW | SWP_MOVE);

      return 0;

   case WM_MINMAXFRAME:
   {
      /* switch to the app that is just behind us when minimizing */
      PSWP pswp = PVOIDFROMMP(mp1);
      if (pswp->fl & SWP_MINIMIZE)
      /* first gets the previous frame on top (frame behind this frame, this API
         is a tricky one) then sets that window active */
         WinSetActiveWindow(HWND_DESKTOP, WinQueryWindow(hwnd, QW_NEXTTOP));
      return 0;
   }

   }

   return (MRESULT)   WinDefWindowProc(hwnd,msg,mp1,mp2);
}

BOOL CreateNotebook(HWND hwnd)
{
   HWND hwndNB;
   ULONG xspace;  /* the space at the left for the next button */

   /* Create buttons */

   /* since I'm not in a dialog box, the tab are not automatically prosseced,
      and it seems WinEnumDlgItem processes the groups backward from their load, so
      I load them from the last to first one of the group.  Warp 4 style notebook
      processes these buttons at one time when tabbing around, but Warp 3 style
      does not, but it doesn't really matter, since we can still navigate with
      alt-arrows accelerator keys I have implemented */
   xspace = 5 + size[OKPB].x + 5 + size[LOADPB].x + 5;
   WinCreateWindow(hwnd, WC_BUTTON, "~Unload", BS_PUSHBUTTON | WS_TABSTOP | WS_VISIBLE,
                   xspace, 5, size[UNLOADPB].x, size[UNLOADPB].y, hwnd, HWND_TOP, ID_UNLOADPB, NULL, NULL);

   xspace -= size[LOADPB].x + 5;
   WinCreateWindow(hwnd, WC_BUTTON, "(Re)~Load", BS_PUSHBUTTON | WS_TABSTOP | WS_VISIBLE,
                   xspace, 5, size[LOADPB].x, size[LOADPB].y, hwnd, HWND_TOP, ID_LOADPB, NULL, NULL);

   xspace -= size[OKPB].x + 5;
   WinCreateWindow(hwnd, WC_BUTTON, "~OK", BS_PUSHBUTTON | WS_TABSTOP | WS_VISIBLE | WS_GROUP,
                   xspace, 5, size[OKPB].x, size[OKPB].y, hwnd, HWND_TOP, ID_OKPB, NULL, NULL);

   /* create notebook, here if the warp 4 doesn't show, I am trying to minic it
      with the old style one */
   hwndNB =  WinCreateWindow( hwnd, WC_NOTEBOOK, NULL,
             WS_VISIBLE | 0x800 | WS_GROUP | WS_TABSTOP |
             /* needed for old style customizeable notebook */
             BKS_BACKPAGESTR | BKS_MAJORTABTOP | BKS_ROUNDEDTABS |
             BKS_STATUSTEXTCENTER | BKS_SPIRALBIND | BKS_TABTEXTLEFT,
             0, 0, 0, 0, hwnd, HWND_TOP, ID_NB, NULL, NULL );

  /* change colors for old style notebook not to look ugly */
  WinSendMsg(hwndNB, BKM_SETNOTEBOOKCOLORS, MPFROMLONG(SYSCLR_FIELDBACKGROUND),
             MPFROMSHORT(BKA_BACKGROUNDPAGECOLORINDEX));

  /* change tab width for old style notebook to something OK for most font size */
  WinSendMsg(hwndNB, BKM_SETDIMENSIONS, MPFROM2SHORT(80,25),
             MPFROMSHORT(BKA_MAJORTAB));

  /* no minor */
  WinSendMsg(hwndNB, BKM_SETDIMENSIONS, 0, MPFROMSHORT(BKA_MINORTAB));

  /* load notebook pages */
  return LoadNotebook(hwndNB);
}

HWND LoadNotebook(HWND hwndNB)
{
   int i;

   for(i = 0; i < PAGE_COUNT; i++)
   {
      nbpage[i].ulPageId = (LONG)WinSendMsg(hwndNB,  BKM_INSERTPAGE, NULL,
           MPFROM2SHORT((BKA_STATUSTEXTON | BKA_AUTOPAGESIZE | nbpage[i].usTabType),
           BKA_LAST));

      if ( !nbpage[i].ulPageId)
        return FALSE;

      if ( !WinSendMsg(hwndNB, BKM_SETSTATUSLINETEXT, MPFROMLONG(nbpage[i].ulPageId),
           MPFROMP(nbpage[i].szStatusLineText)))
        return FALSE;

      if ( !WinSendMsg(hwndNB, BKM_SETTABTEXT, MPFROMLONG(nbpage[i].ulPageId),
           MPFROMP(nbpage[i].szTabText)))
        return FALSE;

      if (!WinSendMsg( hwndNB, BKM_SETPAGEDATA, MPFROMLONG(nbpage[i].ulPageId),
                       MPFROMP(&nbpage[i])))
        return FALSE;
   }

   /* return success (notebook window handle) */
   return hwndNB;
}

MRESULT EXPENTRY wpActivation( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
   switch( msg )
   {
      case WM_INITDLG:
      {
         ULONG keyflags = 0;

         if(keyopt.custom_key) keyflags = keyflags | KC_SCANCODE;
         if(keyopt.virtualkey) keyflags = keyflags | KC_VIRTUALKEY;
         if(keyopt.character) keyflags = keyflags | KC_CHAR;

         WinOldEntryFieldWindowProc = WinSubclassWindow(WinWindowFromID(hwnd,ID_CUSTOMK_FIELD),(PFNWP)wpKeyEntryField);
         WinSendDlgItemMsg(hwnd,ID_CUSTOMK_FIELD,WM_CHAR,MPFROMSH2CH(keyflags,1,keyopt.custom_key),
                                 MPFROM2SHORT(keyopt.character,keyopt.virtualkey));


         if(keyopt.custom_key2) keyflags = keyflags | KC_SCANCODE;
         if(keyopt.virtualkey2) keyflags = keyflags | KC_VIRTUALKEY;
         if(keyopt.character2) keyflags = keyflags | KC_CHAR;

         WinOldEntryFieldWindowProc = WinSubclassWindow(WinWindowFromID(hwnd,ID_CUSTOMK_FIELD2),(PFNWP)wpKeyEntryField);
         WinSendDlgItemMsg(hwnd,ID_CUSTOMK_FIELD2,WM_CHAR,MPFROMSH2CH(keyflags,1,keyopt.custom_key2),
                                 MPFROM2SHORT(keyopt.character2,keyopt.virtualkey2));


         WinOldEntryFieldWindowProc = WinSubclassWindow(WinWindowFromID(hwnd,ID_PTRFILE),(PFNWP)wpPtrFileEntryField);
         WinSendDlgItemMsg(hwnd,ID_PTRFILE,EM_SETTEXTLIMIT,MPFROMLONG(2*CCHMAXPATH),0);

         WinOldEntryFieldWindowProc = WinSubclassWindow(WinWindowFromID(hwnd,ID_CURSORFILE),(PFNWP)wpPtrFileEntryField);
         WinSendDlgItemMsg(hwnd,ID_CURSORFILE,EM_SETTEXTLIMIT,MPFROMLONG(2*CCHMAXPATH),0);
      }

      /* refresh */
      case WM_USER:

         WinSendDlgItemMsg(hwnd, ID_SCROLL_LOCK, BM_SETCHECK, MPFROMSHORT(keyopt.scroll_lock),0);

         WinSendDlgItemMsg(hwnd, ID_NORMAL, BM_SETCHECK, MPFROMSHORT(keyopt.normal),0);
         WinSendDlgItemMsg(hwnd, ID_ALT, BM_SETCHECK, MPFROMSHORT(keyopt.alt),0);
         WinSendDlgItemMsg(hwnd, ID_CTRL, BM_SETCHECK, MPFROMSHORT(keyopt.ctrl),0);
         WinSendDlgItemMsg(hwnd, ID_SHIFT, BM_SETCHECK, MPFROMSHORT(keyopt.shift),0);
         WinSendDlgItemMsg(hwnd, ID_1, BM_SETCHECK, MPFROMSHORT(keyopt.button_1),0);
         WinSendDlgItemMsg(hwnd, ID_2, BM_SETCHECK, MPFROMSHORT(keyopt.button_2),0);
         WinSendDlgItemMsg(hwnd, ID_3, BM_SETCHECK, MPFROMSHORT(keyopt.button_3),0);
         WinSendDlgItemMsg(hwnd, ID_CUSTOMK, BM_SETCHECK, MPFROMSHORT(keyopt.custom_key_toggle),0);
         WinEnableWindow(WinWindowFromID(hwnd,ID_CUSTOMK_FIELD),keyopt.custom_key_toggle);

         WinSendDlgItemMsg(hwnd, ID_TIMER, BM_SETCHECK, MPFROMSHORT(keyopt.timer),0);
         WinSendDlgItemMsg(hwnd, ID_ALT2, BM_SETCHECK, MPFROMSHORT(keyopt.alt2),0);
         WinSendDlgItemMsg(hwnd, ID_CTRL2, BM_SETCHECK, MPFROMSHORT(keyopt.ctrl2),0);
         WinSendDlgItemMsg(hwnd, ID_SHIFT2, BM_SETCHECK, MPFROMSHORT(keyopt.shift2),0);
         WinSendDlgItemMsg(hwnd, ID_12, BM_SETCHECK, MPFROMSHORT(keyopt.button_12),0);
         WinSendDlgItemMsg(hwnd, ID_22, BM_SETCHECK, MPFROMSHORT(keyopt.button_22),0);
         WinSendDlgItemMsg(hwnd, ID_32, BM_SETCHECK, MPFROMSHORT(keyopt.button_32),0);
         WinSendDlgItemMsg(hwnd, ID_CUSTOMK2, BM_SETCHECK, MPFROMSHORT(keyopt.custom_key_toggle2),0);
         WinEnableWindow(WinWindowFromID(hwnd,ID_CUSTOMK_FIELD2),keyopt.custom_key_toggle2);

         WinSetDlgItemText(hwnd, ID_PTRFILE, ptrfile);
         WinSetDlgItemText(hwnd, ID_CURSORFILE, cursorfile);

         if(SHORT1FROMMR(WinSendDlgItemMsg(hwnd,ID_NORMAL, BM_QUERYCHECK, 0, 0)))
         {
            WinEnableWindow(WinWindowFromID(hwnd,ID_ALT),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_CTRL),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_SHIFT),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_1),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_2),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_3),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_CUSTOMK),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_CUSTOMK_FIELD),TRUE);
         }
         else
         {
            WinEnableWindow(WinWindowFromID(hwnd,ID_ALT),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_CTRL),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_SHIFT),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_1),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_2),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_3),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_CUSTOMK),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_CUSTOMK_FIELD),FALSE);
         }

         if(SHORT1FROMMR(WinSendDlgItemMsg(hwnd,ID_TIMER, BM_QUERYCHECK, 0, 0)))
         {
            WinEnableWindow(WinWindowFromID(hwnd,ID_ALT2),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_CTRL2),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_SHIFT2),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_12),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_22),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_32),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_CUSTOMK2),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_CUSTOMK_FIELD2),TRUE);
         }
         else
         {
            WinEnableWindow(WinWindowFromID(hwnd,ID_ALT2),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_CTRL2),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_SHIFT2),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_12),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_22),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_32),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_CUSTOMK2),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_CUSTOMK_FIELD2),FALSE);
         }


         return 0;

      case WM_CONTROL:
         if((SHORT2FROMMP(mp1) == BN_CLICKED) || (SHORT2FROMMP(mp1) == BN_DBLCLICKED))
            switch (SHORT1FROMMP(mp1))
            {
               case ID_NORMAL: keyopt.normal = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0));
                               WinSendMsg(hwnd,WM_USER,0,0); break;
               case ID_SCROLL_LOCK: keyopt.scroll_lock = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0)); break;
               case ID_ALT: keyopt.alt = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0)); break;
               case ID_CTRL: keyopt.ctrl = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0)); break;
               case ID_SHIFT: keyopt.shift = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0)); break;
               case ID_1: keyopt.button_1 = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0)); break;
               case ID_2: keyopt.button_2 = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0)); break;
               case ID_3: keyopt.button_3 = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0)); break;
               case ID_CUSTOMK:
                  keyopt.custom_key_toggle = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0));
                  WinEnableWindow(WinWindowFromID(hwnd,ID_CUSTOMK_FIELD),keyopt.custom_key_toggle); break;

               case ID_TIMER: keyopt.timer = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0));
                              WinSendMsg(hwnd,WM_USER,0,0); break;
               case ID_ALT2: keyopt.alt2 = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0)); break;
               case ID_CTRL2: keyopt.ctrl2 = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0)); break;
               case ID_SHIFT2: keyopt.shift2 = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0)); break;
               case ID_12: keyopt.button_12 = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0)); break;
               case ID_22: keyopt.button_22 = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0)); break;
               case ID_32: keyopt.button_32 = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0)); break;
               case ID_CUSTOMK2:
                  keyopt.custom_key_toggle2 = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0));
                  WinEnableWindow(WinWindowFromID(hwnd,ID_CUSTOMK_FIELD2),keyopt.custom_key_toggle2); break;

            }

            if((SHORT1FROMMP(mp1) == ID_PTRFILE) && (SHORT2FROMMP(mp1) == EN_CHANGE))
               WinQueryWindowText(HWNDFROMMP(mp2),sizeof(ptrfile),ptrfile);

            if((SHORT1FROMMP(mp1) == ID_CURSORFILE) && (SHORT2FROMMP(mp1) == EN_CHANGE))
               WinQueryWindowText(HWNDFROMMP(mp2),sizeof(cursorfile),cursorfile);

            break;

      return 0;

      case WM_CHAR:
         if(SHORT2FROMMP(mp2) == VK_ESC) return 0;

   }
   return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}

MRESULT EXPENTRY wpPtrFileEntryField( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
   switch( msg )
   {
      case DM_DRAGOVER:
      {
         DRAGINFO *draginfo = (DRAGINFO *)mp1;
         USHORT accept;

         DrgAccessDraginfo(draginfo);

         if((DrgQueryDragitemCount(draginfo) > 1) ||
             !DrgVerifyRMF(DrgQueryDragitemPtr(draginfo, 0), "DRM_OS2FILE", NULL))
            accept = DOR_NEVERDROP;
         else
            accept = DOR_DROP;

         DrgFreeDraginfo(draginfo);

         return (MRFROM2SHORT(accept, DO_COPY));
      }
      case DM_DROP:
      {
         DRAGINFO *draginfo = (DRAGINFO *)mp1;
         USHORT accept;
         char buffer[2*CCHMAXPATH];
         DRAGITEM *dragitem;

         DrgAccessDraginfo(draginfo);
         dragitem = DrgQueryDragitemPtr(draginfo, 0);

         if((DrgQueryDragitemCount(draginfo) == 1) &&
             DrgVerifyRMF(dragitem, "DRM_OS2FILE", NULL))
         {
            DrgQueryStrName(dragitem->hstrContainerName,2*CCHMAXPATH,buffer);
            DrgQueryStrName(dragitem->hstrSourceName,CCHMAXPATH,strchr(buffer,'\0'));
            WinSetWindowText(hwnd,buffer);
         }

         DrgFreeDraginfo(draginfo);

         return 0;
      }

      case WM_CONTEXTMENU:
      {
         if (SHORT2FROMMP(mp2) == TRUE)
         WinPopupMenu(hwnd, hwnd, WinLoadMenu(hwnd, 0, ID_POPUPMENU), 10, 10, 0,
                      PU_HCONSTRAIN | PU_VCONSTRAIN | PU_MOUSEBUTTON1 | PU_KEYBOARD);
         else
         WinPopupMenu(hwnd, hwnd, WinLoadMenu(hwnd, 0, ID_POPUPMENU), SHORT1FROMMP(mp1), SHORT2FROMMP(mp1), 0,
                      PU_HCONSTRAIN | PU_VCONSTRAIN | PU_MOUSEBUTTON1 | PU_KEYBOARD);
         return 0;
      }

      case WM_COMMAND:
         switch(SHORT1FROMMP(mp1))
         {
            case ID_ADD:
            {
               int i;
               FILEDLG filedlginfo = {0};
               filedlginfo.cbSize = sizeof(FILEDLG);
               filedlginfo.fl = FDS_CENTER | FDS_OPEN_DIALOG;
               filedlginfo.pszTitle = "Select Icon or Pointer";
               strcpy(filedlginfo.szFullFile,"*.ptr;*.ico");
               filedlginfo.pszOKButton = "OK";

               WinFileDlg(HWND_DESKTOP,hwnd,&filedlginfo);
               if(filedlginfo.lReturn == DID_OK)
                  WinSetWindowText(hwnd, filedlginfo.szFullFile);
            }
         }
         return 0;

      case WM_CHAR:
         if (!(SHORT1FROMMP(mp1) & KC_KEYUP))
            if (SHORT1FROMMP(mp1) & KC_VIRTUALKEY)
               switch (SHORT2FROMMP(mp2))
               {
                  case VK_INSERT:
                     if( !(SHORT1FROMMP(mp1) & KC_SHIFT) &&
                         !(SHORT1FROMMP(mp1) & KC_ALT)   &&
                         !(SHORT1FROMMP(mp1) & KC_CTRL)   )
                     {
                        WinSendMsg(hwnd, WM_COMMAND, MPFROMSHORT(ID_ADD), 0);
                        return 0;
                     }
                     break;
                  case VK_F11:
                     if (SHORT1FROMMP(mp1) & KC_SHIFT)
                        WinSendMsg(hwnd, WM_CONTEXTMENU, 0, MPFROM2SHORT(0,TRUE));
                     return 0;
               }
         break;

   }

   return WinOldEntryFieldWindowProc( hwnd, msg, mp1, mp2 );
}


MRESULT EXPENTRY wpKeyEntryField( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
   switch( msg )
   {
      case WM_CHAR:

         if(!(SHORT1FROMMP(mp1) & KC_KEYUP))
         {
            char buffer[20] = {0};

            keyopt.virtualkey = SHORT2FROMMP(mp2);
            keyopt.character = 0;
            keyopt.custom_key = CHAR4FROMMP(mp1);

            switch(SHORT2FROMMP(mp2))
            {
               case VK_BREAK    : strcpy(buffer,"Break");       break;
               case VK_BACKSPACE: strcpy(buffer,"Backspace");   break;
               case VK_TAB      : strcpy(buffer,"Tab");         break;
               case VK_BACKTAB  : strcpy(buffer,"BackTab");     break;
               case VK_NEWLINE  : strcpy(buffer,"Enter");       break;
               case VK_SHIFT    : strcpy(buffer,"Shift");       break;
               case VK_CTRL     : strcpy(buffer,"Ctrl");        break;
               case VK_ALT      : strcpy(buffer,"Alt");         break;
               case VK_ALTGRAF  : strcpy(buffer,"Alt Char");    break;
               case VK_PAUSE    : strcpy(buffer,"Pause");       break;
               case VK_CAPSLOCK : strcpy(buffer,"Capslock");    break;
               case VK_ESC      : strcpy(buffer,"Esc");         break;
               case VK_SPACE    : strcpy(buffer,"Space");       break;
               case VK_PAGEUP   : strcpy(buffer,"Page Up");     break;
               case VK_PAGEDOWN : strcpy(buffer,"Page Down");   break;
               case VK_END     :  strcpy(buffer,"End");         break;
               case VK_HOME    :  strcpy(buffer,"Home");        break;
               case VK_LEFT    :  strcpy(buffer,"Left");        break;
               case VK_UP      :  strcpy(buffer,"Up");          break;
               case VK_RIGHT   :  strcpy(buffer,"Right");       break;
               case VK_DOWN    :  strcpy(buffer,"Down");        break;
               case VK_PRINTSCRN: strcpy(buffer,"Print Screen");break;
               case VK_INSERT  :  strcpy(buffer,"Insert");      break;
               case VK_DELETE  :  strcpy(buffer,"Delete");      break;
               case VK_SCRLLOCK:  strcpy(buffer,"Scroll Lock"); break;
               case VK_NUMLOCK:   strcpy(buffer,"Numlock");break;
               case VK_ENTER:     strcpy(buffer,"Enter"); break;
               case VK_SYSRQ:     strcpy(buffer,"Sys Rq");break;
               case VK_F1  :      strcpy(buffer,"F1");  break;
               case VK_F2  :      strcpy(buffer,"F2");  break;
               case VK_F3  :      strcpy(buffer,"F3");  break;
               case VK_F4  :      strcpy(buffer,"F4");  break;
               case VK_F5  :      strcpy(buffer,"F5");  break;
               case VK_F6  :      strcpy(buffer,"F6");  break;
               case VK_F7  :      strcpy(buffer,"F7");  break;
               case VK_F8  :      strcpy(buffer,"F8");  break;
               case VK_F9  :      strcpy(buffer,"F9");  break;
               case VK_F10 :      strcpy(buffer,"F10"); break;
               case VK_F11 :      strcpy(buffer,"F11"); break;
               case VK_F12 :      strcpy(buffer,"F12"); break;
               case VK_F13 :      strcpy(buffer,"F13"); break;
               case VK_F14 :      strcpy(buffer,"F14"); break;
               case VK_F15 :      strcpy(buffer,"F15"); break;
               case VK_F16 :      strcpy(buffer,"F16"); break;
               case VK_F17 :      strcpy(buffer,"F17"); break;
               case VK_F18 :      strcpy(buffer,"F18"); break;
               case VK_F19 :      strcpy(buffer,"F19"); break;
               case VK_F20 :      strcpy(buffer,"F20"); break;
               case VK_F21 :      strcpy(buffer,"F21"); break;
               case VK_F22 :      strcpy(buffer,"F22"); break;
               case VK_F23 :      strcpy(buffer,"F23"); break;
               case VK_F24 :      strcpy(buffer,"F24"); break;
               default :
                  keyopt.virtualkey = 0;
                  if (SHORT1FROMMP(mp1) & KC_CHAR)
                  {
                     buffer[0] = CHAR1FROMMP(mp2);
                     buffer[1] = '\0';
                     keyopt.character = CHAR1FROMMP(mp2);
                  }
                  else if (SHORT1FROMMP(mp1) & KC_SCANCODE)
                  {
                     strcpy(buffer,"SC: ");
                     _itoa(CHAR4FROMMP(mp1),&buffer[4],10);
                  }
                  break;
            }

            WinSetWindowText(hwnd,buffer);
         }

      return 0;

   }

   return WinOldEntryFieldWindowProc( hwnd, msg, mp1, mp2 );
}

MRESULT EXPENTRY wpOptions( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
   switch( msg )
   {
      case WM_INITDLG:
      {
         WinSendDlgItemMsg(hwnd,ID_NONPROPSPIN, SPBM_OVERRIDESETLIMITS, MPFROMLONG(255),0);
         WinSendDlgItemMsg(hwnd,ID_SPEEDSPIN, SPBM_OVERRIDESETLIMITS, MPFROMLONG(1000),0);
         WinSendDlgItemMsg(hwnd,ID_APP_FIELD, EM_SETTEXTLIMIT, MPFROMSHORT(256),0);

         WinOldEntryFieldWindowProc = WinSubclassWindow(WinWindowFromID(hwnd,ID_APP_FIELD),(PFNWP)wpAppEntryField);
         WinOldListBoxWindowProc = WinSubclassWindow(WinWindowFromID(hwnd,ID_APP_LIST),(PFNWP)wpAppListBox);

         WinSendDlgItemMsg(hwnd, ID_APP_LIST, LM_INSERTITEM, 0, MPFROMP("Default"));
         WinSendDlgItemMsg(hwnd, ID_APP_LIST, LM_SETITEMHANDLE, 0, MPFROMLONG(&defappopt));
         WinSendDlgItemMsg(hwnd, ID_APP_LIST, LM_SELECTITEM, 0, MPFROMLONG(TRUE));
      }

      /* refresh */
      case WM_USER:
         WinSendDlgItemMsg(hwnd, ID_ENABLE, BM_SETCHECK, MPFROMSHORT(curappopt->enabled),0);
         /* remember this is a 3 state button, returns value 0,1,2 */
         if(!SHORT1FROMMR(WinSendDlgItemMsg(hwnd,ID_ENABLE, BM_QUERYCHECK, 0, 0)))
         {
            WinEnableWindow(WinWindowFromID(hwnd,ID_VERT),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_HORZ),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_FOCUS_LOCK),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_LEASH),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_REDRAW),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_REVERSED),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_SPEED),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_SPEEDSPIN),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_NONPROP),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_NONPROPSPIN),FALSE);
         }
         else
         {
            WinEnableWindow(WinWindowFromID(hwnd,ID_VERT),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_HORZ),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_FOCUS_LOCK),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_LEASH),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_REDRAW),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_REVERSED),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_SPEED),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_SPEEDSPIN),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_NONPROP),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_NONPROPSPIN),TRUE);
         }

         WinSendDlgItemMsg(hwnd, ID_VERT, BM_SETCHECK, MPFROMSHORT(curappopt->vertical),0);
         WinSendDlgItemMsg(hwnd, ID_HORZ, BM_SETCHECK, MPFROMSHORT(curappopt->horizontal),0);
         WinSendDlgItemMsg(hwnd, ID_FOCUS_LOCK, BM_SETCHECK, MPFROMSHORT(curappopt->no_focus_lock),0);
         WinSendDlgItemMsg(hwnd, ID_LEASH, BM_SETCHECK, MPFROMSHORT(curappopt->mouse_leash),0);
         WinSendDlgItemMsg(hwnd, ID_REDRAW, BM_SETCHECK, MPFROMSHORT(curappopt->fake_dynamic),0);
         WinSendDlgItemMsg(hwnd, ID_REVERSED, BM_SETCHECK, MPFROMSHORT(curappopt->reversed),0);

         /* display default value in spin if box is disabled */
         WinSendDlgItemMsg(hwnd, ID_SPEED, BM_SETCHECK, MPFROMSHORT(curappopt->speed_toggle),0);
         if(SHORT1FROMMR(WinSendDlgItemMsg(hwnd,ID_SPEED, BM_QUERYCHECK, 0, 0)) == 1)
            WinSendDlgItemMsg(hwnd, ID_SPEEDSPIN, SPBM_SETCURRENTVALUE, MPFROMLONG(curappopt->speed),0);
         else
         {
            WinSendDlgItemMsg(hwnd, ID_SPEEDSPIN, SPBM_SETCURRENTVALUE, MPFROMLONG(defappopt.speed),0);
            WinEnableWindow(WinWindowFromID(hwnd,ID_SPEEDSPIN),FALSE);
         }

         WinSendDlgItemMsg(hwnd, ID_NONPROP, BM_SETCHECK, MPFROMSHORT(curappopt->nonprop),0);
         if(SHORT1FROMMR(WinSendDlgItemMsg(hwnd,ID_NONPROP, BM_QUERYCHECK, 0, 0)) == 1)
            WinSendDlgItemMsg(hwnd, ID_NONPROPSPIN, SPBM_SETCURRENTVALUE, MPFROMLONG(curappopt->posperpix),0);
         else
         {
            WinSendDlgItemMsg(hwnd, ID_NONPROPSPIN, SPBM_SETCURRENTVALUE, MPFROMLONG(defappopt.posperpix),0);
            WinEnableWindow(WinWindowFromID(hwnd,ID_NONPROPSPIN),FALSE);
         }

         return 0;

      case WM_CONTROL:
         /* checkbox toggling */
         if((SHORT2FROMMP(mp1) == BN_CLICKED) || (SHORT2FROMMP(mp1) == BN_DBLCLICKED))
         {
            /* for default settings, we cannot set the options to default as they
               are the default on which other pages are going to depend on. */

            if( (curappopt == &defappopt) && /* we are setting default option */
                (SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0)) == 2) )
               WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), BM_SETCHECK, MPFROMSHORT(0),0);

            switch (SHORT1FROMMP(mp1))
            {
               case ID_ENABLE: curappopt->enabled = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0));
                               WinSendMsg(hwnd,WM_USER,0,0); break;

               case ID_VERT: curappopt->vertical = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0)); break;
               case ID_HORZ: curappopt->horizontal = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0)); break;
               case ID_FOCUS_LOCK: curappopt->no_focus_lock = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0)); break;
               case ID_LEASH:  curappopt->mouse_leash = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0)); break;
               case ID_REDRAW:  curappopt->fake_dynamic = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0)); break;
               case ID_REVERSED:  curappopt->reversed = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0)); break;

               case ID_SPEED:
                  /* SPEED can't be disabled */
                  if(!SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0)))
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), BM_SETCHECK, MPFROMSHORT(1),0);

                  curappopt->speed_toggle = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0));
                  if(curappopt->speed_toggle == 1)
                  {
                     WinEnableWindow(WinWindowFromID(hwnd,ID_SPEEDSPIN),TRUE);
                     WinSendDlgItemMsg(hwnd, ID_SPEEDSPIN, SPBM_QUERYVALUE, MPFROMP(&temp),0);
                     curappopt->speed = temp;
                  }
                  else
                     WinEnableWindow(WinWindowFromID(hwnd,ID_SPEEDSPIN),FALSE);
                  break;

               case ID_NONPROP:
                  curappopt->nonprop = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0));
                  if(curappopt->nonprop == 1)
                  {
                     WinEnableWindow(WinWindowFromID(hwnd,ID_NONPROPSPIN),TRUE);
                     WinSendDlgItemMsg(hwnd, ID_NONPROPSPIN, SPBM_QUERYVALUE, MPFROMP(&temp),0);
                     curappopt->posperpix = temp;
                  }
                  else
                     WinEnableWindow(WinWindowFromID(hwnd,ID_NONPROPSPIN),FALSE);
                  break;
            }
         }

         /* spinning numbers */
         switch(SHORT1FROMMP(mp1))
         {
            case ID_NONPROPSPIN:
               switch(SHORT2FROMMP(mp1))
               {
                  case SPBN_CHANGE:
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_QUERYVALUE, MPFROMP(&temp),0);
                     curappopt->posperpix = temp;
                     break;
               }
               break;

            case ID_SPEEDSPIN:
               switch(SHORT2FROMMP(mp1))
               {
                  case SPBN_CHANGE:
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_QUERYVALUE, MPFROMP(&temp),0);
                     curappopt->speed = temp;
                     break;
                  /* jump up and down by 10 */
                  case SPBN_UPARROW:
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_QUERYVALUE, MPFROMP(&temp),0);
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_SETCURRENTVALUE, MPFROMLONG(temp+9),0);
                     break;
                  case SPBN_DOWNARROW:
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_QUERYVALUE, MPFROMP(&temp),0);
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_SETCURRENTVALUE, MPFROMLONG(temp-9),0);
                     break;
               }
               break;

            /* application being selected */
            case ID_APP_LIST:
               if(SHORT2FROMMP(mp1) == LN_SELECT)
               {
                  short item;
                  item = (SHORT) WinSendMsg(HWNDFROMMP(mp2),LM_QUERYSELECTION,MPFROMSHORT(LIT_FIRST),0);
                  curappopt = (struct APPOPT *) WinSendMsg(HWNDFROMMP(mp2),LM_QUERYITEMHANDLE,MPFROMSHORT(item),0);
                  WinSendMsg(hwnd,WM_USER,0,0);
               }
               break;
         }
      return 0;

      /* adding/deleting application */
      case WM_COMMAND:
         switch(SHORT1FROMMP(mp1))
         {
            case ID_APP_ADD:
            {
               char *buffer;
               USHORT item,size;

               /* find text */
               size = WinQueryDlgItemTextLength(hwnd,ID_APP_FIELD)+1;
               buffer = alloca(size);
               WinQueryDlgItemText(hwnd,ID_APP_FIELD,size,buffer);
               if(buffer[0] && stricmp(buffer,"Default"))
               {
                  /* insert item after item 0, which is Default */
                  item = (SHORT) WinSendDlgItemMsg(hwnd,ID_APP_LIST, LM_INSERTITEM, MPFROMSHORT(1), MPFROMP(buffer));
                  if((item != LIT_ERROR) && (item != LIT_MEMERROR))
                  {
                     struct APPOPT *appopt;

                     /* allocate memory and set default variables */
                     appopt = (struct APPOPT *) malloc(sizeof(struct APPOPT));
                     *appopt = initialappopt;

                     WinSendDlgItemMsg(hwnd, ID_APP_LIST, LM_SETITEMHANDLE, MPFROMSHORT(item), MPFROMLONG(appopt));
                     WinSendDlgItemMsg(hwnd, ID_APP_LIST, LM_SELECTITEM, MPFROMSHORT(item),MPFROMLONG(TRUE));

                     WinSetDlgItemText(hwnd,ID_APP_FIELD,"");
                  }
               }
            }
            break;

            case ID_APP_DEL:
            {
               HWND hwndl = WinWindowFromID(hwnd,ID_APP_LIST);
               SHORT item = (SHORT) WinSendMsg(hwndl,LM_QUERYSELECTION,MPFROMSHORT(LIT_FIRST),0);

               if(item) /* it's NOT the Default which item = 0 */
               {
                  free((struct APPOPT *) WinSendMsg(hwndl,LM_QUERYITEMHANDLE,MPFROMSHORT(item),0));
                  WinSendMsg(hwndl, LM_DELETEITEM, MPFROMSHORT(item),0);
                  curappopt = (struct APPOPT *) WinSendMsg(hwndl,LM_QUERYITEMHANDLE,MPFROMSHORT(0),0);
                  WinSendMsg(hwndl, LM_SELECTITEM, MPFROMSHORT(0),MPFROMLONG(TRUE));
                  WinSendMsg(hwnd,WM_USER,0,0);
               }
            }
         }

      return 0;

      case WM_CHAR:
         if(SHORT2FROMMP(mp2) == VK_ESC) return 0;

   }
   return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}

MRESULT EXPENTRY wpAppListBox( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
   if(  (msg == WM_CHAR) &&
       !(SHORT1FROMMP(mp1) & KC_KEYUP) &&
        (SHORT2FROMMP(mp2) == VK_DELETE)  )
      {
         WinSendMsg(nbpage[PAGE_OPTIONS].hwnd,WM_COMMAND,MPFROMSHORT(ID_APP_DEL),0);
         return 0;
      }
   return WinOldListBoxWindowProc( hwnd, msg, mp1, mp2 );
}

MRESULT EXPENTRY wpAppEntryField( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
   switch( msg )
   {
      case WM_CHAR:

         if(!(SHORT1FROMMP(mp1) & KC_KEYUP) &&
              ((SHORT2FROMMP(mp2) == VK_NEWLINE) ||
               (SHORT2FROMMP(mp2) == VK_ENTER)))
         {
            WinSendMsg(nbpage[PAGE_OPTIONS].hwnd,WM_COMMAND,MPFROMSHORT(ID_APP_ADD),0);
            return 0;
         }
      break;
   }

   return WinOldEntryFieldWindowProc( hwnd, msg, mp1, mp2 );
}


MRESULT EXPENTRY wpTimerOptions( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
   switch( msg )
   {
      case WM_INITDLG:

         WinSendDlgItemMsg(hwnd,ID_MSRANGE, SPBM_OVERRIDESETLIMITS, MPFROMLONG(10000),MPFROMLONG(1));
         WinSendDlgItemMsg(hwnd,ID_MSPIX, SPBM_OVERRIDESETLIMITS, MPFROMLONG(100000),MPFROMLONG(1));
         WinSendDlgItemMsg(hwnd,ID_RANGEFRACTION, SPBM_OVERRIDESETLIMITS, MPFROMLONG(1000),MPFROMLONG(1));
         WinSendDlgItemMsg(hwnd,ID_POSPERTIMEOUT, SPBM_OVERRIDESETLIMITS, MPFROMLONG(1000),MPFROMLONG(1));
         WinSendDlgItemMsg(hwnd,ID_PIXMIN, SPBM_OVERRIDESETLIMITS, MPFROMLONG(1000),MPFROMLONG(0));
         WinSendDlgItemMsg(hwnd,ID_PIXMAX, SPBM_OVERRIDESETLIMITS, MPFROMLONG(10000),MPFROMLONG(0));

      case WM_USER:
      {
         char buffer[20];

         WinSendDlgItemMsg(hwnd, ID_TIMERDEFAULT, BM_SETCHECK, MPFROMLONG(curappopt->timerdefault),0);

         if( (curappopt == &defappopt) || /* we are setting default option */
             (nbpage[PAGE_OPTIONS].hwnd &&
               !SHORT1FROMMR(WinSendDlgItemMsg(nbpage[PAGE_OPTIONS].hwnd,ID_ENABLE, BM_QUERYCHECK, 0, 0)))
           )
            WinEnableWindow(WinWindowFromID(hwnd,ID_TIMERDEFAULT),FALSE);
         else
            WinEnableWindow(WinWindowFromID(hwnd,ID_TIMERDEFAULT),TRUE);


         if(curappopt == &defappopt || SHORT1FROMMR(WinSendDlgItemMsg(hwnd,ID_TIMERDEFAULT, BM_QUERYCHECK, 0, 0)))
         {
            WinSendDlgItemMsg(hwnd, ID_LINEAR_INVERSE_ON, BM_SETCHECK, MPFROMSHORT(defappopt.linear_inverse),0);
            WinSendDlgItemMsg(hwnd, ID_LINEAR_INVERSE_OFF, BM_SETCHECK, MPFROMSHORT(!defappopt.linear_inverse),0);
            WinSendDlgItemMsg(hwnd, ID_MSRANGE, SPBM_SETCURRENTVALUE, MPFROMLONG(defappopt.msrange),0);
            _gcvt(defappopt.slope,4,buffer);
            if(*(strchr(buffer,0)-1) == '.') *(strchr(buffer,0)-1) = 0;
            WinSetDlgItemText(hwnd, ID_SLOPE, buffer);
            WinSendDlgItemMsg(hwnd, ID_MSPIX, SPBM_SETCURRENTVALUE, MPFROMLONG(defappopt.mspix),0);

            WinSendDlgItemMsg(hwnd, ID_TIMER_NONPROP_ON, BM_SETCHECK, MPFROMSHORT(defappopt.timer_nonprop),0);
            WinSendDlgItemMsg(hwnd, ID_TIMER_NONPROP_OFF, BM_SETCHECK, MPFROMSHORT(!defappopt.timer_nonprop),0);
            WinSendDlgItemMsg(hwnd, ID_POSPERTIMEOUT, SPBM_SETCURRENTVALUE, MPFROMSHORT(defappopt.pospertimeout),0);
            WinSendDlgItemMsg(hwnd, ID_RANGEFRACTION, SPBM_SETCURRENTVALUE, MPFROMSHORT(defappopt.rangefraction),0);
            WinSendDlgItemMsg(hwnd, ID_PIXMIN, SPBM_SETCURRENTVALUE, MPFROMLONG(defappopt.pixmin),0);
            WinSendDlgItemMsg(hwnd, ID_PIXMAX, SPBM_SETCURRENTVALUE, MPFROMLONG(defappopt.pixmax),0);
         }
         else
         {
            WinSendDlgItemMsg(hwnd, ID_LINEAR_INVERSE_ON, BM_SETCHECK, MPFROMSHORT(curappopt->linear_inverse),0);
            WinSendDlgItemMsg(hwnd, ID_LINEAR_INVERSE_OFF, BM_SETCHECK, MPFROMSHORT(!curappopt->linear_inverse),0);
            WinSendDlgItemMsg(hwnd, ID_MSRANGE, SPBM_SETCURRENTVALUE, MPFROMLONG(curappopt->msrange),0);
            _gcvt(curappopt->slope,4,buffer);
            if(*(strchr(buffer,0)-1) == '.') *(strchr(buffer,0)-1) = 0;
            WinSetDlgItemText(hwnd, ID_SLOPE, buffer);
            WinSendDlgItemMsg(hwnd, ID_MSPIX, SPBM_SETCURRENTVALUE, MPFROMLONG(curappopt->mspix),0);

            WinSendDlgItemMsg(hwnd, ID_TIMER_NONPROP_ON, BM_SETCHECK, MPFROMSHORT(curappopt->timer_nonprop),0);
            WinSendDlgItemMsg(hwnd, ID_TIMER_NONPROP_OFF, BM_SETCHECK, MPFROMSHORT(!curappopt->timer_nonprop),0);
            WinSendDlgItemMsg(hwnd, ID_POSPERTIMEOUT, SPBM_SETCURRENTVALUE, MPFROMSHORT(curappopt->pospertimeout),0);
            WinSendDlgItemMsg(hwnd, ID_RANGEFRACTION, SPBM_SETCURRENTVALUE, MPFROMSHORT(curappopt->rangefraction),0);

            WinSendDlgItemMsg(hwnd, ID_PIXMIN, SPBM_SETCURRENTVALUE, MPFROMLONG(curappopt->pixmin),0);
            WinSendDlgItemMsg(hwnd, ID_PIXMAX, SPBM_SETCURRENTVALUE, MPFROMLONG(curappopt->pixmax),0);
         }

         if( (nbpage[PAGE_OPTIONS].hwnd &&
              !SHORT1FROMMR(WinSendDlgItemMsg(nbpage[PAGE_OPTIONS].hwnd,ID_ENABLE, BM_QUERYCHECK, 0, 0))) ||
            (SHORT1FROMMR(WinSendDlgItemMsg(hwnd,ID_TIMERDEFAULT, BM_QUERYCHECK, 0, 0)) && curappopt != &defappopt)
           )
         {
            WinEnableWindow(WinWindowFromID(hwnd,ID_LINEAR_INVERSE_ON),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_LINEAR_INVERSE_OFF),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_TIMER_NONPROP_ON),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_TIMER_NONPROP_OFF),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_MSRANGE),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_SLOPE),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_MSPIX),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_RANGEFRACTION),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_POSPERTIMEOUT),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_PIXMIN),FALSE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_PIXMAX),FALSE);
         }
         else
         {
            WinEnableWindow(WinWindowFromID(hwnd,ID_LINEAR_INVERSE_ON),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_LINEAR_INVERSE_OFF),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_TIMER_NONPROP_ON),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_TIMER_NONPROP_OFF),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_MSRANGE),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_SLOPE),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_MSPIX),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_RANGEFRACTION),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_POSPERTIMEOUT),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_PIXMIN),TRUE);
            WinEnableWindow(WinWindowFromID(hwnd,ID_PIXMAX),TRUE);

            if(SHORT1FROMMR(WinSendDlgItemMsg(hwnd,ID_LINEAR_INVERSE_ON, BM_QUERYCHECK, 0, 0)) == 1)
               WinEnableWindow(WinWindowFromID(hwnd,ID_MSPIX),FALSE);
            else
            {
               WinEnableWindow(WinWindowFromID(hwnd,ID_MSRANGE),FALSE);
               WinEnableWindow(WinWindowFromID(hwnd,ID_SLOPE),FALSE);
            }

            if(SHORT1FROMMR(WinSendDlgItemMsg(hwnd,ID_TIMER_NONPROP_ON, BM_QUERYCHECK, 0, 0)) == 1)
               WinEnableWindow(WinWindowFromID(hwnd,ID_RANGEFRACTION),FALSE);
            else
               WinEnableWindow(WinWindowFromID(hwnd,ID_POSPERTIMEOUT),FALSE);
         }

         return 0;
      }

      case WM_CONTROL:

         /* checkbox toggling */
         if((SHORT2FROMMP(mp1) == BN_CLICKED) || (SHORT2FROMMP(mp1) == BN_DBLCLICKED))
         {
            /* for default settings, we cannot set the options to default as they
               are the default on which other entries are going to depend on. */

            if( (curappopt == &defappopt) && /* we are setting default option */
                (SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0)) == 2) )
               WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), BM_SETCHECK, MPFROMSHORT(0),0);

            switch (SHORT1FROMMP(mp1))
            {
               case ID_TIMERDEFAULT:
                  curappopt->timerdefault = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,SHORT1FROMMP(mp1), BM_QUERYCHECK, 0, 0));
                  WinSendMsg(hwnd,WM_USER,0,0); break;

               case ID_LINEAR_INVERSE_ON:
                  curappopt->linear_inverse = 1;
                  WinEnableWindow(WinWindowFromID(hwnd,ID_MSRANGE),TRUE);
                  WinEnableWindow(WinWindowFromID(hwnd,ID_SLOPE),TRUE);
                  WinEnableWindow(WinWindowFromID(hwnd,ID_MSPIX),FALSE);
                  break;

               case ID_LINEAR_INVERSE_OFF:
                  curappopt->linear_inverse = 0;
                  WinEnableWindow(WinWindowFromID(hwnd,ID_MSRANGE),FALSE);
                  WinEnableWindow(WinWindowFromID(hwnd,ID_SLOPE),FALSE);
                  WinEnableWindow(WinWindowFromID(hwnd,ID_MSPIX),TRUE);
                  break;

               case ID_TIMER_NONPROP_ON:
                  curappopt->timer_nonprop = 1;
                  WinEnableWindow(WinWindowFromID(hwnd,ID_RANGEFRACTION),FALSE);
                  WinEnableWindow(WinWindowFromID(hwnd,ID_POSPERTIMEOUT),TRUE);
                  break;

               case ID_TIMER_NONPROP_OFF:
                  curappopt->timer_nonprop = 0;
                  WinEnableWindow(WinWindowFromID(hwnd,ID_RANGEFRACTION),TRUE);
                  WinEnableWindow(WinWindowFromID(hwnd,ID_POSPERTIMEOUT),FALSE);
                  break;
            }
         }

         /* spinning numbers and entryfield */
         switch(SHORT1FROMMP(mp1))
         {
            case ID_MSRANGE:
               switch(SHORT2FROMMP(mp1))
               {
                  case SPBN_CHANGE:
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_QUERYVALUE, MPFROMP(&curappopt->msrange),0);
                     break;
                  /* jump up and down by 10 */
                  case SPBN_UPARROW:
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_QUERYVALUE, MPFROMP(&temp),0);
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_SETCURRENTVALUE, MPFROMLONG(temp+9),0);
                     break;
                  case SPBN_DOWNARROW:
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_QUERYVALUE, MPFROMP(&temp),0);
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_SETCURRENTVALUE, MPFROMLONG(temp-9),0);
                     break;
               }
               break;

            case ID_SLOPE:
               if(SHORT2FROMMP(mp1) == EN_CHANGE)
               {
                  char *buffer;
                  USHORT item,size;

                  /* find text */
                  size = WinQueryWindowTextLength(HWNDFROMMP(mp2))+1;
                  buffer = alloca(size);
                  WinQueryWindowText(HWNDFROMMP(mp2),size,buffer);

                  curappopt->slope = atof(buffer);
               }
               break;

            case ID_MSPIX:
               switch(SHORT2FROMMP(mp1))
               {
                  case SPBN_CHANGE:
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_QUERYVALUE, MPFROMP(&curappopt->mspix),0);
                     break;
                  /* jump up and down by 100 */
                  case SPBN_UPARROW:
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_QUERYVALUE, MPFROMP(&temp),0);
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_SETCURRENTVALUE, MPFROMLONG(temp+99),0);
                     break;
                  case SPBN_DOWNARROW:
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_QUERYVALUE, MPFROMP(&temp),0);
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_SETCURRENTVALUE, MPFROMLONG(temp-99),0);
                     break;
               }
               break;

            case ID_RANGEFRACTION:
               switch(SHORT2FROMMP(mp1))
               {
                  case SPBN_CHANGE:
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_QUERYVALUE, MPFROMP(&temp),0);
                     curappopt->rangefraction = temp;
                     break;
               }
               break;

            case ID_POSPERTIMEOUT:
               switch(SHORT2FROMMP(mp1))
               {
                  case SPBN_CHANGE:
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_QUERYVALUE, MPFROMP(&temp),0);
                     curappopt->pospertimeout = temp;
                     break;
               }
               break;

            case ID_PIXMIN:
               switch(SHORT2FROMMP(mp1))
               {
                  case SPBN_CHANGE:
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_QUERYVALUE, MPFROMP(&curappopt->pixmin),0);
                     break;
               }
               break;

            case ID_PIXMAX:
               switch(SHORT2FROMMP(mp1))
               {
                  case SPBN_CHANGE:
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_QUERYVALUE, MPFROMP(&curappopt->pixmax),0);
                     break;
                  /* jump up and down by 10 */
                  case SPBN_UPARROW:
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_QUERYVALUE, MPFROMP(&temp),0);
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_SETCURRENTVALUE, MPFROMLONG(temp+9),0);
                     break;
                  case SPBN_DOWNARROW:
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_QUERYVALUE, MPFROMP(&temp),0);
                     WinSendDlgItemMsg(hwnd, SHORT1FROMMP(mp1), SPBM_SETCURRENTVALUE, MPFROMLONG(temp-9),0);
                     break;
               }
               break;
         }

         return 0;

      case WM_CHAR:
         if(SHORT2FROMMP(mp2) == VK_ESC) return 0;
   }

   return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}


#ifdef NEED_DUMMY

/* used for windows with no action to give focus to the next dialog item after
   the notebook and with a tabstop to prevent a keyboard pitfall */
MRESULT EXPENTRY wpDummy( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
   if(msg == WM_CHAR)
      WinSetFocus(HWND_DESKTOP,WinEnumDlgItem(hwndClient,WinWindowFromID(hwndClient,ID_NB),EDI_NEXTTABITEM));
   return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}

#endif


char *getINIfile(void)
{
   char *INIfile = malloc(512);
   HMODULE hmDll;

   if(DosQueryModuleHandle("hotscrl", &hmDll))
   {
      sprintf(errormsg,"%d. Error querying hotscrl.dll handle.",errornum++);
      WinInvalidateRect( hwndClient, &location, FALSE );
      return "";
   }
   if(DosQueryModuleName(hmDll, 512, INIfile))
   {
      sprintf(errormsg,"%d. Error querying hotscrl.dll path.",errornum++);
      WinInvalidateRect( hwndClient, &location, FALSE );
      return "";
   }

   strcpy(strrchr(INIfile,'\\')+1,INI_FILENAME);

   return INIfile;
}

char *getEXEpath()
{
   ULONG PathLength;
   char *Path = NULL;

   /* query max path length */

   if (!DosQuerySysInfo(QSV_MAX_PATH_LENGTH, QSV_MAX_PATH_LENGTH, &PathLength, sizeof(ULONG)))
      Path = (char *) malloc(2*PathLength+1);

   /* multiplied by 2 to include the filename length too */

   if(Path)
   {
      PPIB ppib;
      PTIB ptib;

      DosGetInfoBlocks(&ptib, &ppib);
      DosQueryModuleName(ppib->pib_hmte, 2*PathLength+1, Path);

      *(strrchr(Path,'\\')) = 0;

      Path = (char *)realloc(Path, strlen(Path)+1);
   }

   return Path;
}


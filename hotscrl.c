#define pointer 243
#define INCL_GPI
#define INCL_WIN
#define INCL_DOS

#include <os2.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pm\hotscroll.h"

void endscroll(void);
BOOL findscrollbars(HWND hwndparent, POINTL mouse);
void setcurappopt(void);

int _CRT_init(void);
void _CRT_term(void);

#pragma data_seg(SHAREDMEM)

HAB mainhab = 0;
HWND desktophwnd = 0;
HMODULE hmDll = 0;
BOOL loaded = FALSE;

HWND hwndvowner = 0,
     hwndhowner = 0,
     oldhwndowner = 0, /* this is for fake dynamic scrolling, checks if we
                          are still moving around the same window, so we
                          don't always have to load the parameters */
     hwndhscroll = 0,
     hwndvscroll = 0,
     hwndparent = 0; /* this is temporary variable to find scroll bars */

BOOL found = FALSE;

/* mouse pointer position fun */
POINTL oldmoose,initialmoose,virtualmoose,oldvirtmoose,tempmoose;

/* scroll bar width, length, range and positions */
SWP hswp,vswp;
ULONG hrange,vrange;
SHORT initialhpos, initialvpos, hpos, vpos;

/* timer scrolling things */
BOOL dotimer = FALSE;
USHORT vtimerid = 0, htimerid = 0;
LONG vtimerms, htimerms;
LONG newvtimerms, newhtimerms;
struct
{
   enum {UP, DOWN} v;
   enum {LEFT, RIGHT} h;
} direction;

#define INI_FILENAME "hotscrl.ini"

struct KEYOPT keyopt = DEFAULT_KEYOPT;

/* this corresponds to the listbox in the pm loader */
struct APPLIST
{
   struct APPOPT appopt;
   char *exename;
};

struct APPLIST *applist[200] = {0};
struct APPOPT defappopt = DEFAULT_APPOPT,
              curappopt = DEFAULT_APPOPT;

#ifdef pointer
HPOINTER mousepointer = NULLHANDLE;
#endif

FNWP *oldframe;
HWND timercursorhwnd = NULLHANDLE;
HPOINTER timercursor = NULLHANDLE;
SIZEL cursorsize;


PPIB ppib;
PTIB ptib;
char EXEname[2*CCHMAXPATH];

#pragma data_seg()


MRESULT EXPENTRY drawicon(HWND hwnd,ULONG msg,MPARAM mp1,MPARAM mp2)
{
   switch(msg)
   {
      case WM_PAINT:
      {
         HPS hps;
         RECTL rcl;
         oldframe(hwnd,msg,mp1,mp2);

         hps = WinGetPS(hwnd);
         WinQueryWindowRect(hwnd,&rcl);
         WinFillRect(hps,&rcl,CLR_WHITE);
         WinDrawPointer(hps, 0, 0, timercursor, DP_NORMAL);
         WinReleasePS(hps);
         return 0;
      }

      case WM_USER:
         WinDestroyWindow(hwnd);
         return 0;

      case WM_USER+1:
         WinSetWindowPos(hwnd,HWND_TOP,0,0,0,0,SWP_ZORDER);
         return 0;

      case WM_ACTIVATE:

      case WM_BUTTON1UP:
      case WM_BUTTON1DOWN:
      case WM_BUTTON2UP:
      case WM_BUTTON2DOWN:
      case WM_BUTTON3UP:
      case WM_BUTTON3DOWN:

      case WM_BUTTON1CLICK:
      case WM_BUTTON1DBLCLK:
      case WM_BUTTON2CLICK:
      case WM_BUTTON2DBLCLK:
      case WM_BUTTON3CLICK:
      case WM_BUTTON3DBLCLK:
         return 0;
   }

   return oldframe(hwnd,msg,mp1,mp2);
}


VOID EXPENTRY HotScrollHookSend(HAB hab, PSMHSTRUCT psmh, BOOL fInterTask)
{

   /* need to receive WM_FOCUSCHANGE via send msg hook for NO_FOCUS_LOCK */

   if( (curappopt.no_focus_lock) &&
       (psmh->msg == WM_FOCUSCHANGE) &&
       !SHORT1FROMMP(psmh->mp2) &&  /* means psmh->mp1 is getting focus */
       (keyopt.scroll_lock)  && /* scroll lock active */
       (WinGetKeyState(HWND_DESKTOP,VK_SCRLLOCK) & 0x0001) &&  /* if on */
       !WinIsChild(hwndvscroll,psmh->mp1) &&
       !WinIsChild(hwndhscroll,psmh->mp1)
     )
   {
      setcurappopt();

      if(curappopt.enabled)
      {
         BOOL didtimer = dotimer;
         hwndparent = HWNDFROMMP(psmh->mp1);

         endscroll();
         dotimer = didtimer;

         found = findscrollbars(hwndparent,oldmoose);

         while( (hwndparent != WinQueryDesktopWindow(mainhab,NULLHANDLE)) && !found)
         {
            hwndparent = WinQueryWindow(hwndparent,QW_PARENT);
            found = findscrollbars(hwndparent,oldmoose);
         }

         if(found)
         {
            if(hwndvscroll && WinIsWindowEnabled(hwndvscroll))
               hwndvowner = WinQueryWindow(hwndvscroll,QW_OWNER);

            if(hwndhscroll && WinIsWindowEnabled(hwndhscroll))
               hwndhowner = WinQueryWindow(hwndhscroll,QW_OWNER);

            WinQueryPointerPos(HWND_DESKTOP,&initialmoose);

            if(dotimer && timercursor)
            {
               timercursorhwnd = WinCreateWindow(HWND_DESKTOP, WC_STATIC,
                                 "bleah", WS_VISIBLE,
                                 initialmoose.x-16, initialmoose.y-16,
                                 cursorsize.cx, cursorsize.cy,
                                 NULLHANDLE, HWND_TOP, 2355, NULL, NULL);
               oldframe = WinSubclassWindow(timercursorhwnd, drawicon);
               WinInvalidateRect(timercursorhwnd,NULL,TRUE);
            }
         }
      }

      return;
   }

   /* this is to make the cursor window float on top */

   switch(psmh->msg)
   {
      case WM_WINDOWPOSCHANGED:

         if(desktophwnd == WinQueryWindow(psmh->hwnd,QW_PARENT) && timercursorhwnd)
         {
            HWND temp;
            HENUM henum;
            BOOL done = FALSE;

            henum = WinBeginEnumWindows(HWND_DESKTOP);
            while((temp = WinGetNextWindow(henum)) && !done)
            {
               if(temp == timercursorhwnd)
                  done = TRUE;
               else if(WinIsWindowShowing(temp))
               {
                  /* post msg to client since send hook doesn't like winsetwindowpos */
                  WinPostMsg(timercursorhwnd,WM_USER+1,0,0);
                  done = TRUE;
               }
            }
            WinEndEnumWindows(henum);
         }

         return;

      case WM_ADJUSTWINDOWPOS:
      {
         SWP *swp = PVOIDFROMMP(psmh->mp1);

         /* we only process windows that have the desktop window as parent */
         if(swp && timercursorhwnd && (desktophwnd == WinQueryWindow(swp->hwnd,QW_PARENT)))
         {
            /* the window to float on top */
            if(timercursorhwnd == swp->hwnd)
            {
               swp->fl |= SWP_ZORDER;
               if( (swp->fl & SWP_ACTIVATE) || (swp->fl & SWP_FOCUSACTIVATE) )
               {
                  swp->fl &= ~SWP_ACTIVATE & ~SWP_FOCUSACTIVATE;
                  WinFocusChange(HWND_DESKTOP,swp->hwnd,FC_NOBRINGTOTOP);

               }
               swp->hwndInsertBehind = HWND_TOP;
            }

            /* if it's some other window, let's place them properly */
            else
            {
               /* looks like windows being opened need that */
               if(swp->fl & SWP_SHOW)
               {
                  swp->fl |= SWP_ZORDER;
                  swp->hwndInsertBehind = timercursorhwnd;
               }
               else if(swp->fl & SWP_SIZE)
                  swp->hwndInsertBehind = timercursorhwnd;

               if( (swp->fl & SWP_ACTIVATE) || (swp->fl & SWP_FOCUSACTIVATE) )
               {
                  swp->fl |= SWP_ZORDER;
                  swp->fl &= ~SWP_ACTIVATE & ~SWP_FOCUSACTIVATE;
                  swp->hwndInsertBehind = timercursorhwnd;
                  WinFocusChange(HWND_DESKTOP,swp->hwnd,FC_NOBRINGTOTOP);
               }
               else if(swp->hwndInsertBehind == HWND_TOP)
               {
                  swp->fl |= SWP_ZORDER;
                  swp->hwndInsertBehind = timercursorhwnd;
               }
            }
         }
         return;
      }
   }

}


BOOL EXPENTRY HotScrollHookInput(HAB hab,PQMSG pqmsg,USHORT usRemove )
{
   BOOL setmousepointer = FALSE;
   loaded = TRUE;

/* scroll lock keyboard activation */

   if ( (pqmsg->msg == WM_CHAR)                 &&
        !(SHORT1FROMMP(pqmsg->mp1) & KC_KEYUP)  &&
        (CHAR4FROMMP(pqmsg->mp1) == 70)         &&
        ((keyopt.scroll_lock == 1 && keyopt.normal) ||
         (keyopt.scroll_lock == 2 && keyopt.timer))
      )
   {
      setcurappopt();

      if(curappopt.enabled)
      {
         if(WinGetKeyState(HWND_DESKTOP,VK_SCRLLOCK) & 0x0001) /* if on */
         {
            hwndparent = pqmsg->hwnd;

            found = findscrollbars(hwndparent,pqmsg->ptl);

            while( (hwndparent != WinQueryDesktopWindow(mainhab,NULLHANDLE)) && !found)
            {
               hwndparent = WinQueryWindow(hwndparent,QW_PARENT);
               found = findscrollbars(hwndparent,pqmsg->ptl);
            }

            if(found)
            {
               initialmoose.y = pqmsg->ptl.y;

               if(hwndvscroll && WinIsWindowEnabled(hwndvscroll))
               {
                  hwndvowner = WinQueryWindow(hwndvscroll,QW_OWNER);

                  initialvpos = SHORT1FROMMR(WinSendMsg(hwndvscroll,SBM_QUERYPOS,0,0));
                  WinQueryWindowPos(hwndvscroll,&vswp);
                  vrange = LONGFROMMR(WinSendMsg(hwndvscroll,SBM_QUERYRANGE,0,0));

                  if(vswp.cy && (HIUSHORT(vrange) - LOUSHORT(vrange)))
                  {
                     if(curappopt.reversed)
                        initialmoose.y = vswp.cy * initialvpos / (HIUSHORT(vrange) - LOUSHORT(vrange));
                     else
                        initialmoose.y = vswp.cy - vswp.cy * initialvpos / (HIUSHORT(vrange) - LOUSHORT(vrange));

                     WinMapWindowPoints(hwndvscroll, HWND_DESKTOP, &initialmoose, 1);
                  }
               }

               tempmoose.y = initialmoose.y;

               initialmoose.x = pqmsg->ptl.x;

               if(hwndhscroll && WinIsWindowEnabled(hwndhscroll))
               {
                  hwndhowner = WinQueryWindow(hwndhscroll,QW_OWNER);

                  initialhpos = SHORT1FROMMR(WinSendMsg(hwndhscroll,SBM_QUERYPOS,0,0));
                  WinQueryWindowPos(hwndhscroll,&hswp);
                  hrange = LONGFROMMR(WinSendMsg(hwndhscroll,SBM_QUERYRANGE,0,0));

                  if(hswp.cx && (HIUSHORT(hrange) - LOUSHORT(hrange)))
                  {
                     if(curappopt.reversed)
                        initialmoose.x = hswp.cx - hswp.cx * initialhpos / (HIUSHORT(hrange) - LOUSHORT(hrange));
                     else
                        initialmoose.x = hswp.cx * initialhpos / (HIUSHORT(hrange) - LOUSHORT(hrange));

                     WinMapWindowPoints(hwndhscroll, HWND_DESKTOP, &initialmoose, 1);
                  }
               }

               initialmoose.y = tempmoose.y;

               if(keyopt.scroll_lock == 1)
                  dotimer = FALSE;
               else /* == 2 */
                  dotimer = TRUE;

               if(curappopt.mouse_leash || dotimer)
               {
                  virtualmoose = oldvirtmoose = initialmoose;
                  initialmoose = pqmsg->ptl;
               }
               else
                  WinSetPointerPos(HWND_DESKTOP,initialmoose.x,initialmoose.y);

               if(dotimer && timercursor)
               {
                  timercursorhwnd = WinCreateWindow(HWND_DESKTOP, WC_STATIC,
                                    "bleah", WS_VISIBLE,
                                    initialmoose.x-16, initialmoose.y-16,
                                    cursorsize.cx, cursorsize.cy,
                                    NULLHANDLE, HWND_TOP, 2355, NULL, NULL);
                  oldframe = WinSubclassWindow(timercursorhwnd, drawicon);
                  WinInvalidateRect(timercursorhwnd,NULL,TRUE);
               }
            }

         } /* if off */
         else
            endscroll();
      }
      return FALSE;
   }

/* endscroll on key or button release in non-scroll lock mode  */

   if( ( ((pqmsg->msg == WM_CHAR) && (SHORT1FROMMP(pqmsg->mp1) & KC_KEYUP)) ||
         (pqmsg->msg == WM_BUTTON1UP) ||
         (pqmsg->msg == WM_BUTTON2UP) ||
         (pqmsg->msg == WM_BUTTON3UP) )

        &&

       (hwndvscroll || hwndhscroll) /* scroll bar active */

        &&

      !( (keyopt.scroll_lock) &&                          /* if scroll lock option off, */
         (WinGetKeyState(HWND_DESKTOP,VK_SCRLLOCK) & 1) ) /* don't matter, but if on,   */
                                                          /* need scroll lock off       */
        &&

       /* check if any of the key we monitor went up */

        (  (  !dotimer &&

              !keyopt.button_1 || (keyopt.button_1 &&
              (WinGetKeyState(HWND_DESKTOP,VK_BUTTON1) & 0x8000) ) &&

              !keyopt.button_2 || (keyopt.button_2 &&
              (WinGetKeyState(HWND_DESKTOP,VK_BUTTON2) & 0x8000) ) &&

              !keyopt.button_3 || (keyopt.button_3  &&
              (WinGetKeyState(HWND_DESKTOP,VK_BUTTON3) & 0x8000) ) &&

              !keyopt.alt || (keyopt.alt &&
              (WinGetKeyState(HWND_DESKTOP,VK_ALT) & 0x8000) ) &&

              !keyopt.ctrl || (keyopt.ctrl &&
              (WinGetKeyState(HWND_DESKTOP,VK_CTRL) & 0x8000) ) &&

              !keyopt.shift || (keyopt.shift &&
              (WinGetKeyState(HWND_DESKTOP,VK_SHIFT) & 0x8000) ) &&

              !keyopt.custom_key_toggle || (keyopt.custom_key_toggle &&
              (WinGetPhysKeyState(HWND_DESKTOP,keyopt.custom_key) & 0x8000) )
           )

           ||

           (  dotimer &&

              !keyopt.button_12 || (keyopt.button_12 &&
              (WinGetKeyState(HWND_DESKTOP,VK_BUTTON1) & 0x8000) ) &&

              !keyopt.button_22 || (keyopt.button_22 &&
              (WinGetKeyState(HWND_DESKTOP,VK_BUTTON2) & 0x8000) ) &&

              !keyopt.button_32 || (keyopt.button_32  &&
              (WinGetKeyState(HWND_DESKTOP,VK_BUTTON3) & 0x8000) ) &&

              !keyopt.alt2 || (keyopt.alt2 &&
              (WinGetKeyState(HWND_DESKTOP,VK_ALT) & 0x8000) ) &&

              !keyopt.ctrl2 || (keyopt.ctrl2 &&
              (WinGetKeyState(HWND_DESKTOP,VK_CTRL) & 0x8000) ) &&

              !keyopt.shift2 || (keyopt.shift2 &&
              (WinGetKeyState(HWND_DESKTOP,VK_SHIFT) & 0x8000) ) &&

              !keyopt.custom_key_toggle2 || (keyopt.custom_key_toggle2 &&
              (WinGetPhysKeyState(HWND_DESKTOP,keyopt.custom_key2) & 0x8000) )
           )
        )
     )
   {
      endscroll();
      return FALSE;
   }

/* scrolling routines */

   if(
       (pqmsg->msg == WM_MOUSEMOVE) &&
       pqmsg->hwnd && /* hwnd is 0 while resizing frames */
       ((pqmsg->ptl.y != oldmoose.y) ||
        (pqmsg->ptl.x != oldmoose.x))
     )
   {
      oldmoose = pqmsg->ptl;

      /* checks if we should start to find some new scroll bars */

      /* starting timer scrolling */

      if(  !hwndvscroll && !hwndhscroll && /* no scroll bars found yet */

           keyopt.timer && /* user enabled timer scrolling */

           !( (keyopt.scroll_lock) &&                         /* if scroll lock option off, */
              (WinGetKeyState(HWND_DESKTOP,VK_SCRLLOCK) & 1) ) && /* don't matter, but if on,   */
                                                                  /* need scroll lock off       */
           (  !(keyopt.button_12) ||
             ( (keyopt.button_12) &&
               (WinGetKeyState(HWND_DESKTOP,VK_BUTTON1) & 0x8000) ) ) &&
           (  !(keyopt.button_22) ||
             ( (keyopt.button_22) &&
               (WinGetKeyState(HWND_DESKTOP,VK_BUTTON2) & 0x8000) ) ) &&
           (  !(keyopt.button_32) ||
             ( (keyopt.button_32) &&
               (WinGetKeyState(HWND_DESKTOP,VK_BUTTON3) & 0x8000) ) ) &&
           (  !(keyopt.alt2) ||
             ( (keyopt.alt2) &&
               (WinGetKeyState(HWND_DESKTOP,VK_ALT) & 0x8000) ) ) &&
           (  !(keyopt.ctrl2) ||
             ( (keyopt.ctrl2) &&
               (WinGetKeyState(HWND_DESKTOP,VK_CTRL) & 0x8000) ) ) &&
           (  !(keyopt.shift2) ||
             ( (keyopt.shift2) &&
               (WinGetKeyState(HWND_DESKTOP,VK_SHIFT) & 0x8000) ) ) &&
           (  !(keyopt.custom_key_toggle2) ||
             ( (keyopt.custom_key_toggle2) &&
               (WinGetPhysKeyState(HWND_DESKTOP,keyopt.custom_key) & 0x8000) ) ) )

      {
         setcurappopt();

         if(curappopt.enabled)
         {
            hwndparent = pqmsg->hwnd;

            found = findscrollbars(hwndparent,pqmsg->ptl);

            while( (hwndparent != WinQueryDesktopWindow(mainhab,NULLHANDLE)) && !found)
            {
               hwndparent = WinQueryWindow(hwndparent,QW_PARENT);
               found = findscrollbars(hwndparent,pqmsg->ptl);
            }

            if(found)
            {
               tempmoose = initialmoose = virtualmoose = oldvirtmoose = pqmsg->ptl;

               if(hwndvscroll && WinIsWindowEnabled(hwndvscroll))
               {
                  hwndvowner = WinQueryWindow(hwndvscroll,QW_OWNER);
                  initialvpos = SHORT1FROMMR(WinSendMsg(hwndvscroll,SBM_QUERYPOS,0,0));
                  WinMapWindowPoints(HWND_DESKTOP, hwndvowner, &tempmoose, 1);
               }

               tempmoose = initialmoose;

               if(hwndhscroll && WinIsWindowEnabled(hwndhscroll))
               {
                  hwndhowner = WinQueryWindow(hwndhscroll,QW_OWNER);
                  initialhpos = SHORT1FROMMR(WinSendMsg(hwndhscroll,SBM_QUERYPOS,0,0));
                  WinMapWindowPoints(HWND_DESKTOP, hwndhowner, &tempmoose, 1);
               }

               dotimer = TRUE;

               if(timercursor)
               {
                  timercursorhwnd = WinCreateWindow(HWND_DESKTOP, WC_STATIC,
                                    "bleah", WS_VISIBLE,
                                    initialmoose.x-16, initialmoose.y-16,
                                    cursorsize.cx, cursorsize.cy,
                                    NULLHANDLE, HWND_TOP, 2355, NULL, NULL);
                  oldframe = WinSubclassWindow(timercursorhwnd, drawicon);
                  WinInvalidateRect(timercursorhwnd,NULL,TRUE);
               }
            }
         }
      }

      /* normal scrolling... if the key combination of normal scrolling is
         a superset of timer scrolling, it won't work */

      else if(  !hwndvscroll && !hwndhscroll && /* no scroll bars found yet */

           keyopt.normal && /* user enabled normal scrolling */

           !( (keyopt.scroll_lock) &&                         /* if scroll lock option off, */
              (WinGetKeyState(HWND_DESKTOP,VK_SCRLLOCK) & 1) ) && /* don't matter, but if on,   */
                                                                  /* need scroll lock off       */
           (  !(keyopt.button_1) ||
             ( (keyopt.button_1) &&
               (WinGetKeyState(HWND_DESKTOP,VK_BUTTON1) & 0x8000) ) ) &&
           (  !(keyopt.button_2) ||
             ( (keyopt.button_2) &&
               (WinGetKeyState(HWND_DESKTOP,VK_BUTTON2) & 0x8000) ) ) &&
           (  !(keyopt.button_3) ||
             ( (keyopt.button_3) &&
               (WinGetKeyState(HWND_DESKTOP,VK_BUTTON3) & 0x8000) ) ) &&
           (  !(keyopt.alt) ||
             ( (keyopt.alt) &&
               (WinGetKeyState(HWND_DESKTOP,VK_ALT) & 0x8000) ) ) &&
           (  !(keyopt.ctrl) ||
             ( (keyopt.ctrl) &&
               (WinGetKeyState(HWND_DESKTOP,VK_CTRL) & 0x8000) ) ) &&
           (  !(keyopt.shift) ||
             ( (keyopt.shift) &&
               (WinGetKeyState(HWND_DESKTOP,VK_SHIFT) & 0x8000) ) ) &&
           (  !(keyopt.custom_key_toggle) ||
             ( (keyopt.custom_key_toggle) &&
               (WinGetPhysKeyState(HWND_DESKTOP,keyopt.custom_key) & 0x8000) ) ) )
      {
         setcurappopt();

         if(curappopt.enabled)
         {
            hwndparent = pqmsg->hwnd;

            found = findscrollbars(hwndparent,pqmsg->ptl);

            while( (hwndparent != WinQueryDesktopWindow(mainhab,NULLHANDLE)) && !found)
            {
               hwndparent = WinQueryWindow(hwndparent,QW_PARENT);
               found = findscrollbars(hwndparent,pqmsg->ptl);
            }

            if(found)
            {
               if(hwndvscroll && WinIsWindowEnabled(hwndvscroll))
               {
                  hwndvowner = WinQueryWindow(hwndvscroll,QW_OWNER);
                  initialvpos = SHORT1FROMMR(WinSendMsg(hwndvscroll,SBM_QUERYPOS,0,0));
               }

               if(hwndhscroll && WinIsWindowEnabled(hwndhscroll))
               {
                  hwndhowner = WinQueryWindow(hwndhscroll,QW_OWNER);
                  initialhpos = SHORT1FROMMR(WinSendMsg(hwndhscroll,SBM_QUERYPOS,0,0));
               }

               initialmoose = virtualmoose = oldvirtmoose = pqmsg->ptl;

               dotimer = FALSE;
            }
         }
      }


      if(hwndvscroll && WinIsWindowEnabled(hwndvscroll) && curappopt.vertical)
      {
         WinQueryWindowPos(hwndvscroll,&vswp);

         if(vswp.cy)
         {
            /* see the WM_TIMER hooking */
            if(dotimer)
            {
               if(curappopt.mouse_leash)
               {
                  virtualmoose.y += (pqmsg->ptl.y - initialmoose.y);
                  pqmsg->ptl.y = virtualmoose.y;
                  WinSetPointerPos(HWND_DESKTOP,initialmoose.x,initialmoose.y);
               }

               if(abs(initialmoose.y - pqmsg->ptl.y) > curappopt.pixmin)
               {
                  if(curappopt.reversed)
                     direction.v = ( (initialmoose.y - pqmsg->ptl.y) > 0 ? DOWN : UP);
                  else
                     direction.v = ( (initialmoose.y - pqmsg->ptl.y) > 0 ? UP : DOWN);

                  if(curappopt.pixmax && (abs(initialmoose.y - pqmsg->ptl.y) > curappopt.pixmax))
                     pqmsg->ptl.y = initialmoose.y - curappopt.pixmax;

                  if(curappopt.linear_inverse)
                     newvtimerms = curappopt.msrange - curappopt.slope * abs(initialmoose.y - pqmsg->ptl.y);
                  else
                     newvtimerms = curappopt.mspix / abs(initialmoose.y - pqmsg->ptl.y);

                  /* 1 ms is the lowest natural number before 0 ms */
                  if(newvtimerms <= 0)
                     newvtimerms = 1;

                  /* if there is already a timer working, we'll leave it change
                     the timer timeout itself.  like this, we don't get a "bunch"
                     of scrolling when the user is moving the mouse */
                  if(!vtimerid)
                  {
                     vtimerms = newvtimerms;
                     vtimerid = WinStartTimer(mainhab,0,0,vtimerms);
                     WinPostMsg(0,WM_TIMER,MPFROMSHORT(vtimerid),0); /* start NOW */
                  }
               }
               else
               {
                  newvtimerms = 0;

                  if(vtimerid)
                  {
                     WinStopTimer(mainhab,0,vtimerid);
                     vtimerid = 0;
                  }
               }

               if(mousepointer)
                  setmousepointer = TRUE;
            }
            else
            {
               vrange = LONGFROMMR(WinSendMsg(hwndvscroll,SBM_QUERYRANGE,0,0));

               if(curappopt.mouse_leash)
               {
                  /* don't update the leash position if it does no good */

                  if(( (vpos > LOUSHORT(vrange)) &&
                       (vpos < HIUSHORT(vrange))  ) ||

                      ((curappopt.reversed) &&
                       (((vpos == LOUSHORT(vrange)) &&
                        ((pqmsg->ptl.y - initialmoose.y) > 0)) ||
                       ((vpos == HIUSHORT(vrange)) &&
                        ((pqmsg->ptl.y - initialmoose.y) < 0))) ) ||

                      (!(curappopt.reversed) &&
                       (((vpos == LOUSHORT(vrange)) &&
                        ((pqmsg->ptl.y - initialmoose.y) < 0)) ||
                       ((vpos == HIUSHORT(vrange)) &&
                        ((pqmsg->ptl.y - initialmoose.y) > 0))) )  )

                     virtualmoose.y += (pqmsg->ptl.y - initialmoose.y);

                  pqmsg->ptl.y = virtualmoose.y;
                  WinSetPointerPos(HWND_DESKTOP,initialmoose.x,initialmoose.y);
               }

               /* make the scrolling position follow the mouse pointer in
                  scroll lock mode but not in key down mode */

               if ( (keyopt.scroll_lock) &&    /* scroll lock active */
                    (WinGetKeyState(HWND_DESKTOP,VK_SCRLLOCK) & 1) )
               {
                  tempmoose = pqmsg->ptl;
                  WinMapWindowPoints(HWND_DESKTOP, hwndvscroll, &tempmoose, 1);
                  if(curappopt.reversed)
                     vpos = (HIUSHORT(vrange) - LOUSHORT(vrange)) * curappopt.speed / 100 * tempmoose.y / vswp.cy;
                  else
                     vpos = (HIUSHORT(vrange) - LOUSHORT(vrange)) * curappopt.speed / 100 * (vswp.cy - tempmoose.y) / vswp.cy;
               }
               else
               {
                  if(curappopt.reversed)
                     pqmsg->ptl.y = 2 * initialmoose.y - pqmsg->ptl.y;

                  if(curappopt.nonprop)
                     vpos = (initialmoose.y - pqmsg->ptl.y) * curappopt.posperpix + initialvpos;
                  else
                     vpos = (HIUSHORT(vrange) - LOUSHORT(vrange)) * curappopt.speed / 100 * (initialmoose.y - pqmsg->ptl.y) / vswp.cy + initialvpos;
               }

               if(vpos < LOUSHORT(vrange))
                  vpos = LOUSHORT(vrange);
               else if(vpos > HIUSHORT(vrange))
                  vpos = HIUSHORT(vrange);

            /* update the slider position before notifying the app */

               WinPostMsg(hwndvscroll, SBM_SETPOS, MPFROMSHORT(vpos),0);

               WinPostMsg(hwndvowner,WM_VSCROLL,
                          MPFROMSHORT(WinQueryWindowUShort(hwndvscroll, QWS_ID)),
                          MPFROM2SHORT(vpos, SB_SLIDERTRACK));

               if(mousepointer)
                  setmousepointer = TRUE;
            }
         }
      }

      if(hwndhscroll && WinIsWindowEnabled(hwndhscroll) && curappopt.horizontal)
      {
         WinQueryWindowPos(hwndhscroll,&hswp);

         if(hswp.cx)
         {
            /* see the WM_TIMER hooking */
            if(dotimer)
            {
               if(curappopt.mouse_leash)
               {
                  virtualmoose.x += (pqmsg->ptl.x - initialmoose.x);
                  pqmsg->ptl.x = virtualmoose.x;
                  WinSetPointerPos(HWND_DESKTOP,initialmoose.x,initialmoose.y);
               }

               if(abs(initialmoose.x - pqmsg->ptl.x) > curappopt.pixmin)
               {
                  if(curappopt.reversed)
                     direction.h = ( (initialmoose.x - pqmsg->ptl.x) > 0 ? RIGHT: LEFT);
                  else
                     direction.h = ( (initialmoose.x - pqmsg->ptl.x) > 0 ? LEFT : RIGHT);

                  if(abs(initialmoose.x - pqmsg->ptl.x) > curappopt.pixmax)
                     pqmsg->ptl.x = initialmoose.x - curappopt.pixmax;

                  if(curappopt.linear_inverse)
                     newhtimerms = curappopt.msrange - curappopt.slope * abs(initialmoose.x - pqmsg->ptl.x);
                  else
                     newhtimerms = curappopt.mspix / abs(initialmoose.x - pqmsg->ptl.x);

                  /* 1 ms is the lowest natural number before 0 ms */
                  if(newhtimerms <= 0)
                     newhtimerms = 1;

                  /* if there is already a timer working, we'll leave it change
                     the timer timeout itself.  like this, we don't get a "bunch"
                     of scrolling when the user is moving the mouse */
                  if(!htimerid)
                  {
                     htimerms = newhtimerms;
                     htimerid = WinStartTimer(mainhab,0,0,htimerms);
                     WinPostMsg(0,WM_TIMER,MPFROMSHORT(htimerid),0); /* start NOW */
                  }
               }
               else
               {
                  newhtimerms = 0;

                  if(htimerid)
                  {
                     WinStopTimer(mainhab,0,htimerid);
                     htimerid = 0;
                  }
               }

               if(mousepointer)
                  setmousepointer = TRUE;
            }
            else
            {
               hrange = LONGFROMMR(WinSendMsg(hwndhscroll,SBM_QUERYRANGE,0,0));

               if(curappopt.mouse_leash)
               {
                  /* don't update the leash position if it does no good */

                  if(( (hpos > LOUSHORT(hrange)) &&
                       (hpos < HIUSHORT(hrange))  ) ||

                      ((curappopt.reversed) &&
                       (((hpos == LOUSHORT(hrange)) &&
                        ((pqmsg->ptl.x - initialmoose.x) < 0)) ||
                       ((hpos == HIUSHORT(hrange)) &&
                        ((pqmsg->ptl.x - initialmoose.x) > 0))) ) ||

                      (!(curappopt.reversed) &&
                       (((hpos == LOUSHORT(hrange)) &&
                        ((pqmsg->ptl.x - initialmoose.x) > 0)) ||
                       ((hpos == HIUSHORT(hrange)) &&
                        ((pqmsg->ptl.x - initialmoose.x) < 0))) )  )

                     virtualmoose.x += (pqmsg->ptl.x - initialmoose.x);

                  pqmsg->ptl.x = virtualmoose.x;
                  WinSetPointerPos(HWND_DESKTOP,initialmoose.x,initialmoose.y);
               }

               if ( (keyopt.scroll_lock) &&    /* scroll lock active */
                    (WinGetKeyState(HWND_DESKTOP,VK_SCRLLOCK) & 1) )
               {
                  tempmoose = pqmsg->ptl;
                  WinMapWindowPoints(HWND_DESKTOP, hwndhscroll, &tempmoose, 1);
                  if(curappopt.reversed)
                     hpos = (HIUSHORT(hrange) - LOUSHORT(hrange)) * curappopt.speed / 100 * (hswp.cx - tempmoose.x) / hswp.cx;
                  else
                     hpos = (HIUSHORT(hrange) - LOUSHORT(hrange)) * curappopt.speed / 100 * (tempmoose.x) / hswp.cx;
               }
               else
               {
                  if(curappopt.reversed)
                     pqmsg->ptl.x = 2 * initialmoose.x - pqmsg->ptl.x;

                  if(curappopt.nonprop)
                     hpos = (pqmsg->ptl.x - initialmoose.x) * curappopt.posperpix + initialhpos;
                  else
                     hpos = (HIUSHORT(hrange) - LOUSHORT(hrange)) * curappopt.speed / 100 * (pqmsg->ptl.x - initialmoose.x) / hswp.cx + initialhpos;
               }

               if(hpos < LOUSHORT(hrange))
                  hpos = LOUSHORT(hrange);
               else if(hpos > HIUSHORT(hrange))
                  hpos = HIUSHORT(hrange);

            /* update the slider position before notifying the app */

               WinPostMsg(hwndhscroll, SBM_SETPOS, MPFROMSHORT(hpos),0);

               WinPostMsg(hwndhowner,WM_HSCROLL,
                          MPFROMSHORT(WinQueryWindowUShort(hwndhscroll, QWS_ID)),
                          MPFROM2SHORT(hpos,SB_SLIDERTRACK));

               if(mousepointer)
                  setmousepointer = TRUE;
            }
         }
      }

      /* update mouse pointer */
      if(setmousepointer)
      {
         WinLockPointerUpdate(HWND_DESKTOP,mousepointer,100);
         WinDispatchMsg(mainhab, pqmsg);
         WinSetPointer(HWND_DESKTOP, mousepointer);
         return TRUE;
      }
      return FALSE;
   }

/* let's fake dynamic scrolling */

   if(((pqmsg->msg == WM_VSCROLL) || (pqmsg->msg == WM_HSCROLL)) &&
       (SHORT2FROMMP(pqmsg->mp2) == SB_SLIDERTRACK))
   {
      if(pqmsg->hwnd != oldhwndowner)
      {
         setcurappopt();
         oldhwndowner = pqmsg->hwnd;
      }

      if(curappopt.fake_dynamic && curappopt.enabled)
         WinPostMsg(pqmsg->hwnd,pqmsg->msg,pqmsg->mp1,MPFROM2SHORT(SHORT1FROMMP(pqmsg->mp2),SB_SLIDERPOSITION));

      return FALSE;
   }

/* this is for the timer scrolling */

   if((pqmsg->msg == WM_TIMER) && (pqmsg->hwnd == 0))
   {
      /* kludge for nasty applications like PMMail and Comm/2 which use my own timer ID */
      static ULONG lastvtime = 0, lasthtime = 0;
      ULONG currentvtime = WinGetCurrentTime(mainhab),
            currenthtime = WinGetCurrentTime(mainhab);

      if(SHORT1FROMMP(pqmsg->mp1) == vtimerid && (currentvtime - lastvtime) >= vtimerms)
      {
         lastvtime = currentvtime;
         vrange = LONGFROMMR(WinSendMsg(hwndvscroll,SBM_QUERYRANGE,0,0));
         vpos = SHORT1FROMMR(WinSendMsg(hwndvscroll,SBM_QUERYPOS,0,0));

         if(curappopt.timer_nonprop)
         {
            if(direction.v == UP)
               vpos += curappopt.pospertimeout;
            else if(direction.v == DOWN)
               vpos -= curappopt.pospertimeout;
         }
         else
         {
            /* we add 1 in case the division gives 0 */
            if(direction.v == UP)
               vpos += (HIUSHORT(vrange) - LOUSHORT(vrange)) / curappopt.rangefraction + 1;
            else if(direction.v == DOWN)
               vpos -= (HIUSHORT(vrange) - LOUSHORT(vrange)) / curappopt.rangefraction + 1;
         }

         if(vpos < LOUSHORT(vrange))
            vpos = LOUSHORT(vrange);
         else if(vpos > HIUSHORT(vrange))
            vpos = HIUSHORT(vrange);

      /* update the slider position before notifying the app */

         WinPostMsg(hwndvscroll, SBM_SETPOS, MPFROMSHORT(vpos),0);

         WinPostMsg(hwndvowner,WM_VSCROLL,
                    MPFROMSHORT(WinQueryWindowUShort(hwndvscroll, QWS_ID)),
                    MPFROM2SHORT(vpos, SB_SLIDERTRACK));

      /* if we should now change our timeout value, do it */

         if(newvtimerms != vtimerms)
         {
            WinStopTimer(mainhab,0,vtimerid);
            vtimerid = 0;
            vtimerms = newvtimerms;

            if(vtimerms)
               vtimerid = WinStartTimer(mainhab,0,0,abs(vtimerms));
         }

         if(mousepointer) WinLockPointerUpdate(HWND_DESKTOP,mousepointer,100);
         return(TRUE);
      }
      else if(SHORT1FROMMP(pqmsg->mp1) == htimerid && (currenthtime - lasthtime) >= htimerms)
      {
         lasthtime = currenthtime;
         hrange = LONGFROMMR(WinSendMsg(hwndhscroll,SBM_QUERYRANGE,0,0));
         hpos = SHORT1FROMMR(WinSendMsg(hwndhscroll,SBM_QUERYPOS,0,0));

         if(curappopt.timer_nonprop)
         {
            if(direction.h == RIGHT)
               hpos += curappopt.pospertimeout;
            else if(direction.h == LEFT)
               hpos -= curappopt.pospertimeout;
         }
         else
         {
            /* we add 1 in case the division gives 0 */
            if(direction.h == RIGHT)
               hpos += (HIUSHORT(hrange) - LOUSHORT(hrange)) / curappopt.rangefraction + 1;
            else if(direction.h == LEFT)
               hpos -= (HIUSHORT(hrange) - LOUSHORT(hrange)) / curappopt.rangefraction + 1;
         }

         if(hpos < LOUSHORT(hrange))
            hpos = LOUSHORT(hrange);
         else if(hpos > HIUSHORT(hrange))
            hpos = HIUSHORT(hrange);

      /* update the slider position before notifying the app */

         WinPostMsg(hwndhscroll, SBM_SETPOS, MPFROMSHORT(hpos),0);

         WinPostMsg(hwndhowner,WM_HSCROLL,
                    MPFROMSHORT(WinQueryWindowUShort(hwndhscroll, QWS_ID)),
                    MPFROM2SHORT(hpos, SB_SLIDERTRACK));

      /* if we should now change our timeout value, do it */

         if(newhtimerms != htimerms)
         {
            WinStopTimer(mainhab,0,htimerid);
            htimerid = 0;
            htimerms = newhtimerms;

            if(htimerms)
               htimerid = WinStartTimer(mainhab,0,0,abs(htimerms));
         }

         if(mousepointer) WinLockPointerUpdate(HWND_DESKTOP,mousepointer,100);
         return(TRUE);
      }
   }

   return(FALSE);
}

/* sets the variables to stop scrolling */

void endscroll(void)
{
   if(vtimerid)
      WinStopTimer(mainhab,0,vtimerid);
   vtimerid = 0;

   if(htimerid)
      WinStopTimer(mainhab,0,htimerid);
   vtimerid = 0;

   if(hwndvscroll && WinIsWindowEnabled(hwndvscroll) && curappopt.vertical)
      WinPostMsg(hwndvowner,WM_VSCROLL,
                 MPFROMSHORT(WinQueryWindowUShort(hwndvscroll, QWS_ID)),
                 MPFROM2SHORT(SHORT1FROMMR(WinSendMsg(hwndvscroll,SBM_QUERYPOS,0,0)), SB_SLIDERPOSITION));
   hwndvscroll = 0;

   if(hwndhscroll && WinIsWindowEnabled(hwndhscroll) && curappopt.horizontal)
      WinPostMsg(hwndhowner,WM_HSCROLL,
                 MPFROMSHORT(WinQueryWindowUShort(hwndhscroll, QWS_ID)),
                 MPFROM2SHORT(SHORT1FROMMR(WinSendMsg(hwndhscroll,SBM_QUERYPOS,0,0)),SB_SLIDERPOSITION));
   hwndhscroll = 0;

   /* update mouse pointer */
   if(mousepointer)
      WinSetPointer(HWND_DESKTOP, WinQuerySysPointer(HWND_DESKTOP, SPTR_ARROW, FALSE));

   if(dotimer)
   {
      dotimer = FALSE;
      /* destroy */
      if(timercursorhwnd)
      {
         WinSendMsg(timercursorhwnd, WM_USER, 0, 0);
         timercursorhwnd = 0;
      }
   }
}

/* finds scroll bar of hwndparent around the mouse position given */

BOOL findscrollbars(HWND hwndparent, POINTL mouse)
{
   BOOL found = FALSE;
   HENUM henum;
   SWP swp;
   HWND hwndtemp;
   CHAR achClass[256] = {0};
   POINTL convmouse;

   henum = WinBeginEnumWindows(hwndparent);

   while((hwndtemp = WinGetNextWindow(henum)) != NULLHANDLE)
   {
      WinQueryClassName(hwndtemp,sizeof(achClass),achClass);
      if(WinFindAtom(WinQuerySystemAtomTable(),achClass) == LOUSHORT(WC_SCROLLBAR))
      {

/* tricky one here, SBS_HORZ is actually 0, so check for SBS_VERT instead */

         found = TRUE;
         WinQueryWindowPos(hwndtemp,&swp);
         convmouse = mouse;
         WinMapWindowPoints(HWND_DESKTOP, hwndtemp, &convmouse, 1);

         if(WinQueryWindowULong(hwndtemp,QWL_STYLE) & SBS_VERT)
         {
            if((convmouse.y > swp.y) && (convmouse.y < swp.y+swp.cy))
               hwndvscroll = hwndtemp;
            else if(!hwndvscroll)
               hwndvscroll = hwndtemp;
         }
         else
         {
             if((convmouse.x > swp.x) && (convmouse.x < swp.x+swp.cx))
               hwndhscroll = hwndtemp;
            else if(!hwndhscroll)
               hwndhscroll = hwndtemp;
         }
      }
   }

   WinEndEnumWindows(henum);

   return found;
}

ULONG dontfreepid = 0;

/* find EXE specific options */
void setcurappopt(void)
{
   int i;

   DosGetInfoBlocks(&ptib, &ppib);
   DosQueryModuleName(ppib->pib_hmte, sizeof(EXEname), EXEname);
   curappopt = defappopt;

   if(applist[0] && !DosGetSharedMem(applist[0],PAG_READ) &&
      applist[0]->exename && !DosGetSharedMem(applist[0]->exename,PAG_READ))
   {
      for(i = 0 ; applist[i] ; i++)
      {
         /* need a match that doesn't match Default */
         if(strstr(EXEname,applist[i]->exename) && strcmp(EXEname,"DEFAULT"))
         {
            curappopt = applist[i]->appopt;
            if(curappopt.enabled == 2)
               curappopt.enabled = defappopt.enabled;
            if(curappopt.mouse_leash == 2)
               curappopt.mouse_leash = defappopt.mouse_leash;
            if(curappopt.no_focus_lock == 2)
               curappopt.no_focus_lock = defappopt.no_focus_lock;
            if(curappopt.fake_dynamic == 2)
               curappopt.fake_dynamic = defappopt.fake_dynamic;
            if(curappopt.reversed == 2)
               curappopt.reversed = defappopt.reversed;
            if(curappopt.vertical == 2)
               curappopt.vertical = defappopt.vertical;
            if(curappopt.horizontal == 2)
               curappopt.horizontal = defappopt.horizontal;
            if(curappopt.speed_toggle == 2)
               curappopt.speed = defappopt.speed;
            if(curappopt.nonprop == 2)
            {
               curappopt.nonprop = defappopt.nonprop;
               curappopt.posperpix = defappopt.posperpix;
            }
            if(curappopt.timerdefault)
            {
               curappopt.msrange = defappopt.msrange;
               curappopt.slope = defappopt.slope;
               curappopt.mspix = defappopt.mspix;
               curappopt.rangefraction = defappopt.rangefraction;
               curappopt.pospertimeout = defappopt.pospertimeout;
               curappopt.pixmin = defappopt.pixmin;
               curappopt.pixmax = defappopt.pixmax;
               curappopt.linear_inverse = defappopt.linear_inverse;
               curappopt.timer_nonprop = defappopt.timer_nonprop;
            }
            break;
         }
      }
      if(dontfreepid != ppib->pib_ulpid)
      {
         DosFreeMem(applist[0]->exename);
         DosFreeMem(applist[0]);
      }
   }
}

BOOL EXPENTRY HotScrollHookInit(void)
{
   int i;
   HINI INIhandle;
   char inifile[512];
   char ptrfile[2*CCHMAXPATH];
   char cursorfile[2*CCHMAXPATH];

   /* find desktop anchor and handle */
   mainhab = WinQueryAnchorBlock(HWND_DESKTOP);
   desktophwnd = WinQueryDesktopWindow(mainhab,NULLHANDLE);

   if(DosQueryModuleHandle("hotscrl", &hmDll))
       return FALSE;

   DosQueryModuleName(hmDll, 512, inifile);
   strcpy(strrchr(inifile,'\\')+1,INI_FILENAME);

   if(applist[0])
   {
      int i = 0;
      if(!DosGetSharedMem(applist[0],PAG_WRITE))
         DosFreeMem(applist[0]->exename);

      DosFreeMem(applist[0]);
      DosFreeMem(applist[0]);
      memset(applist, 0, sizeof(applist));
   }

   if(INIhandle = PrfOpenProfile(mainhab,inifile))
   {
      ULONG datasize;
      PPIB ppib;
      PTIB ptib;

      DosGetInfoBlocks(&ptib, &ppib);
      dontfreepid = ppib->pib_ulpid;

      PrfQueryProfileSize(INIhandle, "Options", NULL, &datasize);
      if(datasize)
      {
         char *buffer,*count;

         DosAllocSharedMem((void *) &buffer,NULL,datasize,PAG_COMMIT | OBJ_GETTABLE | PAG_WRITE);
         PrfQueryProfileData(INIhandle, "Options", NULL, buffer, &datasize);

         for(i = 0, count = buffer ; count[0] ; count = strchr(count,'\0') + 1, i++);
         DosAllocSharedMem((void *) &applist[0],NULL,sizeof(struct APPLIST)*(i+1),PAG_COMMIT | OBJ_GETTABLE | PAG_WRITE);

         for(i = 0 ; buffer[0] ; buffer = strchr(buffer,'\0') + 1, i++)
         {
            /* load options corresponding to the EXE */
            PrfQueryProfileSize(INIhandle, "Options", buffer, &datasize);

            if(datasize == sizeof(struct APPOPT))
            {
               applist[i] = applist[0] + i; /* use memory after the previous one */
               PrfQueryProfileData(INIhandle, "Options", buffer, &applist[i]->appopt, &datasize);
               strupr(buffer);
               applist[i]->exename = buffer;

               if(!strcmp(buffer,"DEFAULT"))
                  defappopt = applist[i]->appopt;
            }
         }
      }

      PrfQueryProfileSize(INIhandle, "Activation", "Settings", &datasize);
      if(datasize == sizeof(keyopt))
         PrfQueryProfileData(INIhandle, "Activation", "Settings", &keyopt, &datasize);

      PrfQueryProfileString(INIhandle, "Activation", "Pointer", "", ptrfile, sizeof(ptrfile));
      PrfQueryProfileString(INIhandle, "Activation", "Cursor", "", cursorfile, sizeof(cursorfile));

      PrfCloseProfile(INIhandle);
   }
   else
      DosBeep(600,200);

#ifdef pointer
   if(mousepointer)
   {
      PPIB ppib;
      PTIB ptib;

      DosGetInfoBlocks(&ptib, &ppib);
      WinSetPointerOwner(mousepointer, ppib->pib_ulpid, TRUE);
      WinFreeFileIcon(mousepointer);
      mousepointer = NULLHANDLE;
   }
   if(ptrfile && *ptrfile)
      mousepointer = WinLoadFileIcon(ptrfile,FALSE);
   if(mousepointer)
      WinSetPointerOwner(mousepointer, 0, FALSE);
#endif

   if(timercursor)
   {
      PPIB ppib;
      PTIB ptib;

      DosGetInfoBlocks(&ptib, &ppib);
      WinSetPointerOwner(timercursor, ppib->pib_ulpid, TRUE);
      WinFreeFileIcon(timercursor);
      timercursor = NULLHANDLE;
   }
   if(cursorfile && *cursorfile)
      timercursor = WinLoadFileIcon(cursorfile,FALSE);
   if(timercursor)
   {
      WinSetPointerOwner(timercursor, 0, FALSE);
//      WinQueryPointerInfo(timercursor,&pointerinfo);
      cursorsize.cx = WinQuerySysValue(HWND_DESKTOP,SV_CXPOINTER);
      cursorsize.cy = WinQuerySysValue(HWND_DESKTOP,SV_CYPOINTER);
   }


   if(!loaded)
   {
      /* let's also get initial pointer position */
      WinQueryPointerPos(HWND_DESKTOP,&oldmoose);

      /* setting hooks */
      if(WinSetHook(mainhab, NULLHANDLE, HK_INPUT, (PFN)HotScrollHookInput, hmDll) &&
         WinSetHook(mainhab, NULLHANDLE, HK_SENDMSG, (PFN)HotScrollHookSend, hmDll))
         return TRUE;
      else
         return FALSE;
   }

   return FALSE;
}


BOOL EXPENTRY HotScrollHookKill(void)
{
   loaded = FALSE;

   #ifdef pointer
   if(mousepointer)
   {
      PPIB ppib;
      PTIB ptib;

      DosGetInfoBlocks(&ptib, &ppib);
      WinSetPointerOwner(mousepointer, ppib->pib_ulpid, TRUE);
      WinFreeFileIcon(mousepointer);
   }
   #endif

   if(timercursor)
   {
      PPIB ppib;
      PTIB ptib;

      DosGetInfoBlocks(&ptib, &ppib);
      WinSetPointerOwner(timercursor, ppib->pib_ulpid, TRUE);
      WinFreeFileIcon(timercursor);
   }


   if(applist[0])
   {
      int i = 0;
      if(!DosGetSharedMem(applist[0],PAG_WRITE))
         DosFreeMem(applist[0]->exename);

      DosFreeMem(applist[0]);
      DosFreeMem(applist[0]);
      memset(applist, 0, sizeof(applist));
   }

   if(WinReleaseHook(mainhab, NULLHANDLE, HK_INPUT, (PFN)HotScrollHookInput, hmDll) &&
      WinReleaseHook(mainhab, NULLHANDLE, HK_SENDMSG, (PFN)HotScrollHookSend, hmDll) )
      return TRUE;
   else
      return FALSE;
}

BOOL EXPENTRY HotScrollWaitLoaded(void)
{
   while(!loaded) DosSleep(10);
   return TRUE;
}

BOOL EXPENTRY HotScrollLoaded(void)
{
   return loaded;
}

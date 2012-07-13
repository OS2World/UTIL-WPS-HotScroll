#define MAXSELECT 100

/* main client window frame and notebook */
#define ID_MAIN 1
#define ID_NB 42

#define ID_DUMMY 20000

/* notebook page frame */
#define ID_ACTIVATION 100
#define ID_OPTIONS	 101
#define ID_TIMER_OPTIONS    102
#define ID_ABOUT      103

/* dialog elements */

#define ID_SCROLL_LOCK	 200

#define ID_NORMAL        201
#define ID_ALT           202
#define ID_CTRL          203
#define ID_SHIFT         204
#define ID_1             205
#define ID_2             206
#define ID_3             207
#define ID_CUSTOMK       208
#define ID_CUSTOMK_FIELD 209

#define ID_TIMER          210
#define ID_ALT2           211
#define ID_CTRL2          212
#define ID_SHIFT2         213
#define ID_12             214
#define ID_22             215
#define ID_32             216
#define ID_CUSTOMK2       217
#define ID_CUSTOMK_FIELD2 218

#define ID_PTRFILE       219
#define ID_CURSORFILE    220
#define ID_POPUPMENU     221
#define ID_ADD           222

#define ID_ENABLE 		 300
#define ID_VERT			 301
#define ID_HORZ			 302
#define ID_LEASH			 303
#define ID_FOCUS_LOCK	 304
#define ID_REDRAW 		 305
#define ID_REVERSED		 306
#define ID_SPEED			 307
#define ID_SPEEDSPIN 	 308
#define ID_NONPROP		 309
#define ID_NONPROPSPIN	 310
#define ID_APP_FIELD 	 311
#define ID_APP_ADD		 312
#define ID_APP_DEL		 313
#define ID_APP_LIST      314

#define ID_TIMERDEFAULT       412
#define ID_LINEAR_INVERSE_ON  401
#define ID_LINEAR_INVERSE_OFF 402
#define ID_MSRANGE            403
#define ID_SLOPE              404
#define ID_MSPIX              405
#define ID_TIMER_NONPROP_ON   406
#define ID_TIMER_NONPROP_OFF  407
#define ID_RANGEFRACTION      408
#define ID_POSPERTIMEOUT      409
#define ID_PIXMIN             410
#define ID_PIXMAX             411

/* push buttons */
#define ID_OKPB 500
#define ID_LOADPB 501
#define ID_UNLOADPB 502

#define ID_NBFOCUS 600
#define ID_OKFOCUS 601

/* notebook page info structure */

typedef struct _NBPAGE
{
	 PFNWP	 pfnwpDlg;					/* Window procedure address for the dialog */
	 PSZ		 szStatusLineText;		/* Text to go on status line */
	 PSZ		 szTabText; 				/* Text to go on major tab */
	 ULONG	 idDlg;						/* ID of the dialog box for this page */
	 BOOL 	 skip;						/* skip this page (for major pages with minor pages) */
	 USHORT	 usTabType; 				/* BKA_MAJOR or BKA_MINOR */
	 ULONG	 ulPageId;					/* notebook page ID */
	 HWND 	 hwnd;						/* set when page frame is loaded */

} NBPAGE, *PNBPAGE;


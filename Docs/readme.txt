Hot Scroll 1.1      (C) 1998  Samuel Audet <guardia@cam.org>

THIS IS A FREEWARE UTILITY!!  Please send an e-mail if are using this
program, thank you!  If you can spare some $MONEY$ for a university
student, it would be very much appreciated.  See Contacting the Author
section.

Contents
========

   1. Introduction
   2. Usage
   3. Tested Software
   4. Things to come
   5. Legal Stuff
   6. For Programmers
   7. Acknowledgments
   8. Contacting the Author


Introduction
============

Hot Scroll works similar to Microsoft IntelliMouse (and FlyWheel), Logitech
FirstMouse+/MouseMan+, Mouse Systems ProAgios/ScrollMouse, Kinsington
Internet Mouse, Fellowes Browser, Genius EasyScroll/NetScroll/NetMouse, and
IBM ScrollPoint, but instead of using a second roller or button, it uses
the mouse movement itself requiring no special hardware. Also, Hot Scroll
works for both horizontal and vertical scrolling AT THE SAME TIME.

New in 1.1 is the ability to do timer scrolling like in StarOffice 5.0 and
Microsoft Office 9x.  Also new is the ability to include a different mouse
pointer when scrolling.

I first saw such a feature in an Amiga browser, IBrowse in Summer 1997 at
PowerCon '97 (PowerPC Convention).
http://www.amiga.org/news/powercon97.html As I learned programming, I
forgot about it, but then I saw Microsoft made such a beast, but ah! with
special hardware, for specific software and with limited features of
course... Intellimoose. :) This reminded me of what I had seen on the Amiga
and I thought I might as well make such a thing, but for OS/2. If you are
also using a Mac with MacOS, check Scroll Magick!!
http://www.edenware.com/scrollmagick.html  I also have a list of
alternatives for Windows:

http://www.zdnet.com/pcmag/pctech/content/17/06/ut1706.001.html
Scroll++ http://www.pointix.com/
MOUSEHOK.ZIP from Rossen Assenov <rossen_assenov@hotmail.com>
Flywheel http://www.plannetarium.com/flywheel.htm

You'll notice the mouse doesn't follow the scroll bars like usual.. I have
a problem.  Scroll bars don't have anything for pixels.  In any case,
knowing the physical length of the scroll bar and its range, I was able to
do something not too shabby.  It works quite well, as you can see below.

Check my home page for more great PM/WPS Enhancers, BackTalk, PM123 and
CD2MP3 PM!


Usage
=====

Loading/Installation
---------------------

Load using "loader l" or by running the PM Loader.  You can also add the
DLL to the LoadOneTime key in the SYS_DLLS application of OS2.INI.
"Install.cmd" is provided for this purpose.  Note that if you run the
loader and Hot Scroll has already been loaded from OS2.INI, it will be
unloaded on loader termination.

How to use it, two methods
--------------------------

1. Give focus to a window with scroll bars and activate Scroll Lock.  The
   pointer will be positioned in the best position to scroll (unless the
   leash is active), and this will leave both the mouse and keyboard free.
   Move the mouse around, type, click, etc. Deactivate Scroll Lock.

2. Put the pointer over a window to scroll, keep the selected keys and
   buttons down, and move around.  Remember that key combination leaves the
   mouse free (well, I'm not sure of it's usefulness with some keys like
   alt, ctrl or shift are down) and that mouse combination leaves the
   keyboard free, unless both are used.  Also, actions that were already
   performed by some keys or buttons are still and always active.

Note: If more than one scroll bar has the same parent, Hot Scroll will use
      the one under or beside the mouse pointer when activated.

Options
-------

Hot Scroll comes with a bunch of user definable options using loader.exe
parameters or pmloader.exe notebook page.  The options  are saved in
HOTSCRL.INI.  Make sure it always follows HOTSCRL.DLL if you decide to
"install.cmd" it. Because the text loader now offers liltle advantage over
the PM loader after implementing shared memory, I haven't bullet proofed
it, so don't panic or report bugs if it crashes for improperly used
parameters.

Note: I have not updated loader.exe to include the new features in 1.1.  If
this bothers anybody, please let me know.

Syntax for loader.exe:

loader <l|s|d> [/lacfk123[-|+]] [/i] [/p <profile name>] [/ehvomdrs[-|+|*]]
               [/s[+|*] <speed %>] [/n[-|+|*] <pos/pix>] [/?]

Note: Follow options by + to activate, - to deactivate, * to use default
      value found in the "Default" profile, or nothing to toggle.

l   Loads Hot Scroll hook.
s   Saves key combinations and options.
d   Deletes the current profile's options.

/p  Specify the profile (application EXE substring) to set or view.  This
    profile string will be used to case-insensitively match a substring in
    the memory stored pathname for each process.  For example, the WPS is
    usually "X:\OS2\PMSHELL.EXE".  Use "pstat.exe /c" to see those strings.
    A special profile is "Default".

/i  List currently configured profiles in INI file.

Activation - These can only be enabled or disabled.

   /l  Scroll Lock method.

   Keys that must be down to scroll.

   /a  Alt
   /c  Ctrl
   /f  Shift
   /k  Defines a custom key.  When activating this parameter, you will be
       asked to type a key in.
   /1  Mouse Button 1
   /2  Mouse Button 2
   /3  Mouse Button 3  If you are having problems getting your third
       button to work, try Rodent:
       ftp://hobbes.nmsu.edu/pub/os2/system/drivers/mouse/rodnt100.zip

   /?  Shows the help/info screen.


Options - The Default profile should not be set with "use default values".

   /e  Enables Hot Scroll for a particular profile.
   /h  Enables horizontal scrolling.
   /v  Enables vertical scrolling.

   /o  No focus lock. This will make Hot Scroll follow focus changes for the
       Scroll Lock method without the need to press Scroll Lock again.  Can
       be useful when surfing pages with multiple frames in Navigator for
       example.

   /m  Mouse Leash.  I don't know of any API that actually stops the mouse, so
       what I had to do is to reset the mouse position constantly.  You will
       see the mouse trying to escape.

   /d  Fake Dynamic Scrolling.  WARNING this is a kludge!!  It may give
       screen corruption (or even system hang?) in certain situations. This
       gives Dynamic Scrolling to apps that normally don't have any when
       using Hot Scroll, for example DeScribe, FaxWorks, and OS/2 System
       Editor. Unfortunately, this breaks the scrolling of a few other
       applications that already have dynamic scrolling like HELPMGR.DLL.
       An interesting side effect is that it makes folders scroll smoother.

   /r  Reversed Scrolling.  Not very useful IMHO, unless you want to get
       confused, but someone wanted this, so it's there.  You can get somekind
       of pseudo-"hand manipulation" with this and the non-proportional mode,
       but the latter needs a different value for _everything_.

   /s  Factor for the speed of the scrolling.  As a reference, a value of
       100% requires the mouse to travel from one end to the other of the
       scroll bar to entirely scroll the window.

   /n  Instead of scrolling proportionally to the scroll bar range and length,
       here you can define how many positions the scrolling is to move for
       each pixel of mouse movement (you'll have to see by yourself what it
       does in your applications).  This is never active in Scroll Lock mode
       as it doesn't make any sense.


Examples
--------

   Sets Mouse Button 1 + Mouse Button 2, a Default profile with No Focus
   Lock and Speed of 200%, and a describe.exe profile with Fake Dynamic
   Scrolling and Mouse Leash:

      [C:\]loader s /1+ /2+ /o+ /s 200
      [C:\]loader s /p describe.exe /d+ /m+
      [C:\]loader l

   Non-proportional mode of 5 positions per pixel with Reversed Scrolling
   and Fake Dynamic Scrolling for faxworks.exe:

      [C:\]loader s /n+ 5 /r+ /d+
      [C:\]loader l


Unloading/Uninstallion
----------------------

You can unload it by terminating either loader or pmloader.  If it was
added to OS2.INI, you can uninstall it with "uninst.cmd".

Using the PM interface
----------------------

Activation

   - Click whatever you want, and type in the custom key in the entry
     fields. Note that the Timer Scrolling key combination has priority over
     the Normal Scrolling.  This means setting Normal Scrolling to MB3 and
     Timer Scrolling to Atl-MB3 will work but not the contrary for example.
   - Mouse Pointer File will be shown as the mouse pointer when scrolling (at
     least it tries).
   - Timer Scrolling Cursor File will display a window containing that icon
     at the initial mouse position when using timer scrolling so that you
     can find the center much easier.

Options

   Add or Delete profiles.
   Options are on when checked off, off when not, and set to default when
   grayed out.
   Spin the speed and non-proportional spinboxes.

Timer Options

   The options in this page are not described in loader.exe section.

   The way Timer Scrolling functions is that it calculates a certain
   timeout value depending on the numbers you entered for the respective
   functions (linear or inverse) and using that timeout value, it starts a
   timer which will scroll at each timeout.  You can tell this timer to
   scroll proportional of a certain fraction of the scrolling area or
   non-proportionally (application dependent by trial and error).  You can
   also specify the minimum distance from the initial mouse position to
   start scrolling and the maximum amount of pixels included in the
   calculation of the timeout.

About

   Copy and Paste my e-mail address, and mail me!!

Press OK to activate the changes you have made.

If error messages appear in red at the bottom of the screen, you can
dismiss them by clicking on them.

The program will keep its window state saved after being closed.  That
means that it will start minimized if it was closed minimized (like at
shutdown).

Yes I used a BeOS Icon... arg, icons are hard to do.  If you can make a
good one, send it over!

I hope keyboard junkies will like the functionality. ;)


Tested Software
===============

Here's software I've tried Hot Scroll with.

Work
----

   Netscape 2.02 - Now try what's called REAL browsing!  *COOL*!
   Communicator/2
   The Workplace Shell - Folders, Window List, etc.
       - The Window List doesn't "erase" the Timer Scrolling Cursor when
         scrolling.
   Mesa 2
   PMMail 1.9
   VisualAge C++ 3.0 - ICSDEBUG only works with Fake Dynamic Scrolling
   VIO windows - scrolling conflict when scrolling outside of the
                 window with MB2 and MB3 only.
   PMView
   The File Dialog
   OS/2 System Editor
   DeScribe 5 - The slider/content flickers?  The frame seems to receive the
                scrolling messages instead of the client window, weird.
                Problems also occurs on blank parts of documents.
   OpenChat
   UniMaint 5
   Partition Magic 3
   FaxWorks 3
   Seek and Scan Files
   HELPMGR.DLL (used by help screens and VIEW.EXE/VIEWDOC.EXE)
      - Content window doesn't work with Fake Dynamic Scrolling.
      - Some content/scroll bar position consistency
        problems with No Focus Lock and Timer Scrolling
   EPM 6 - The content wraps to the beginning after scrolling to the end ??
   HyperView PM 3.4
   ZOC 3 - The Back Log doesn't "erase" the Timer Scrolling Cursor when scrolling.
   MR/2 ICE
   Graphical File Comparison
   Maple V R4
   American Heritage Dictionary
   ICQ Java
   PM123, CD2MP3 PM, WarpAMP, PM Checksum, Memsize, WarpZip, KAZip, Archive
   Folder, Enhanced E/EE, Zip Control 2.5, lst/pm, FM/2 2.5, PC/2 2.0,
   Ultimod, DH-Grep-PM

Tricky
------

   Reason:     Windows with scroll bars cannot have focus.
   Workaround: Keep the slider down while pressing scroll lock.
               Use "key/button down" method.

   GTIRC 2 and 3
   WebExplorer


Don't work
----------

   Acrobat Reader - The scroll bars all have different parents, so you
                    can't use both vertical and horizontal at the same
                    time.  And the parents are hidden, the only way to make
                    it work in any (even there not so) useful way is with
                    scroll lock while keeping a scroll bar down.  Also, the
                    right horizontal slider blinks.  Fiuf... I wonder how
                    this program even works.  It's best just to use the
                    already present "hand manipulation".  On the other
                    hand, Fake Dynamic Scrolling works great when using the
                    scroll bars.

   ProNews/2      - Now that's another weird one... for one thing, all
                    scroll bars have the same parent (??!).  Maybe I'll
                    figure it out if I'm lucky, but I don't want to lose
                    time on one program. I've managed to make it half work,
                    but there are still a lot of conflict between the
                    scroll bars I still don't understand.  Ask Panacea to
                    fix that up a bit.

   Mixomat        - Scroll Bars aren't used for scrolling.

   StarOffice 5.0 - Doesn't use OS/2 scroll bars.


Things to come  Remember that $MONEY$ makes miracles for new features!!
==============

(The following is very uncertain due to implementation complications.)
- "Hand manipulation" like in Acrobat Reader.
- Make the scrolling follow a bit more the slider sizes.

ie.: probably nothing, this is the last release


Legal stuff
===========

This freeware product is used at your own risk, although it is highly
improbable it can cause any damage.

If you plan on copying me, please give me the credits, thanks.


For Programmers
===============

Source code is free of charge, as long as it's not to make money. Send me a
sign of life and I will send it to you.


Acknowledgments
===============

Robert Mahoney <rmahoney@netusa.net>

Thanks to the following for financial or gift support!

   Wayne Westerman <westerma@ee.udel.edu>
   Hauke Laging <hauke@haui.allcon.com>
   Alan R. Balmer <albalmer@worldnet.att.net>
   Hidetoshi Kumura <kumurahi@gld.mmtr.or.jp>
   Gilles Tschopp <blueneon@bluewin.ch>
   Nancy E. VanAbrahams
   William Meertens <76125.702@compuserve.com>


Contacting the Author
=====================
If you have any suggestions, comments or bug reports, please let me know.

Samuel Audet

E-mail:      guardia@cam.org
Homepage:    http://www.cam.org/~guardia
IRC nick:    Guardian_ (be sure it's me before starting asking questions though)
Talk Client: Guardian@guardian.dyn.ml.org

Snail Mail:

   377, rue D'Argenteuil
   Laval, Quebec
   H7N 1P7   CANADA

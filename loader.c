#define INCL_KBD
#define INCL_DOS
#define INCL_WIN
#include <os2.h>
#include <stdio.h>
#include <conio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "pm\hotscroll.h"

BOOL EXPENTRY HotScrollHookInit(void);
BOOL EXPENTRY HotScrollHookKill(void);
BOOL EXPENTRY HotScrollWaitLoaded(void);
BOOL EXPENTRY HotScrollLoaded(void);
int setoption(char *arg, int option);
void help(char *program);

#define INI_FILENAME "hotscrl.ini"

struct KEYOPT keyopt = DEFAULT_KEYOPT;
struct APPOPT appopt = DEFAULT_APPOPT;
struct APPOPT initialappopt = INIT_APPOPT;
char *app = "Default";

#ifdef pointer
char *ptrfile = NULL;
#endif

enum actions { nothing, load, save, delete } todo = nothing;

int main(int argc, char *argv[])
{
   int i;
   HINI INIhandle;
   HAB hab = WinQueryAnchorBlock(HWND_DESKTOP);
   ULONG datasize;
   char inifile[512];
   HMODULE hmDll;

   char *buffer;
   BOOL listapp = FALSE,
        found = FALSE;

   if(DosQueryModuleHandle("hotscrl", &hmDll))
   {
      printf("Error querying hotscrl.dll handle.");
      return 3;
   }
   if(DosQueryModuleName(hmDll, 512, inifile))
   {
      printf("Error querying hotscrl.dll path.");
      return 2;
   }
   strcpy(strrchr(inifile,'\\')+1,INI_FILENAME);

   if(!(INIhandle = PrfOpenProfile(hab,inifile)))
   {
      printf("Error opening %s",inifile);
      return 1;
   }

   for(i = 1; i < argc; i++)
      if ((argv[i][0] == '/') || (argv[i][0] == '-'))
         switch(toupper(argv[i][1]))
         {
            case 'P': app = argv[++i]; break;
            case 'I': printf("Configured profiles:\n");
                      listapp = TRUE; break;
         }

   PrfQueryProfileSize(INIhandle, "Options", NULL, &datasize);
   buffer = alloca(datasize);
   PrfQueryProfileData(INIhandle, "Options", NULL, buffer, &datasize);

   while(buffer[0])
   {
      if(listapp) printf("%s\n",buffer);

      if(!stricmp(buffer,app))
      {
         strcpy(app,buffer); /* INI files are case sensitive */

         PrfQueryProfileSize(INIhandle, "Options", buffer, &datasize);

         if(datasize == sizeof(struct APPOPT))
            PrfQueryProfileData(INIhandle, "Options", buffer, &appopt, &datasize);

         found = TRUE;
      }          

      buffer = strchr(buffer,'\0') + 1;
   }

   printf("\n");

   if(!found && stricmp(app,"Default")) /* if new one and not default */
      appopt = initialappopt;

   PrfQueryProfileSize(INIhandle, "Activation", "Settings", &datasize);
   if(datasize == sizeof(keyopt))
      PrfQueryProfileData(INIhandle, "Activation", "Settings", &keyopt, &datasize);

   if(argc == 1) { help(argv[0]); return 0; }

   for(i = 1; i < argc; i++)
      if ((argv[i][0] == '/') || (argv[i][0] == '-'))
         switch(toupper(argv[i][1]))
         {
            case 'E': appopt.enabled = setoption(argv[i], appopt.enabled); break;
            case 'H': appopt.horizontal = setoption(argv[i], appopt.horizontal); break;
            case 'V': appopt.vertical = setoption(argv[i], appopt.vertical); break;
            case 'L': keyopt.scroll_lock = setoption(argv[i], keyopt.scroll_lock); break;
            case 'A': keyopt.alt = setoption(argv[i], keyopt.alt); break;
            case 'C': keyopt.ctrl = setoption(argv[i], keyopt.ctrl); break;
            case 'F': keyopt.shift = setoption(argv[i], keyopt.shift); break;
            case 'K': switch(argv[i][2])
                      {
                        case '-':  keyopt.custom_key_toggle = 0; break;
                        case '+':
                        {
                           KBDKEYINFO keyinfo;

                           printf("Press the wanted key: ");
                           fflush(stdout);
                           if(KbdCharIn(&keyinfo,IO_WAIT,0) == 0)
                           {
                              keyopt.custom_key_toggle = 1;
                              keyopt.custom_key = keyinfo.chScan;
                              printf("%c\nKey with scancode %d successfully set.\n",keyinfo.chChar,keyopt.custom_key);
                           }
                           else
                              printf("\nKey not successfully set, error occured.\n");

                           break;
                        }
                        case '*': keyopt.custom_key_toggle = 2; break;
                      }
                      break;
            case '1': keyopt.button_1 = setoption(argv[i], keyopt.button_1); break;
            case '2': keyopt.button_2 = setoption(argv[i], keyopt.button_2); break;
            case '3': keyopt.button_3 = setoption(argv[i], keyopt.button_3); break;
            case 'M': appopt.mouse_leash = setoption(argv[i], appopt.mouse_leash); break;
            case 'O': appopt.no_focus_lock = setoption(argv[i], appopt.no_focus_lock); break;
            case 'D': appopt.fake_dynamic = setoption(argv[i], appopt.fake_dynamic); break;
            case 'R': appopt.reversed = setoption(argv[i], appopt.reversed); break;
            case 'S': appopt.speed_toggle = setoption(argv[i], appopt.speed_toggle);
                      appopt.speed = atoi(argv[++i]); break;
            case 'N': appopt.nonprop = setoption(argv[i], appopt.nonprop);
                      appopt.posperpix = atoi(argv[++i]); break;
            #ifdef pointer
            case 'P': ptrfile = argv[++i]; break;
            #endif

            /* already processed */
            case 'P': i++; break;
            case 'I': break;

            case '?': help(argv[0]); return 0;
            default : printf("Unrecognized Option: %s\n",argv[i]); break;
         }
      else
         switch(toupper(argv[i][0]))
         {
            case 'L': todo = load; break;
            case 'S': todo = save; break;
            case 'D': todo = delete; break;
            default : printf("Unrecognized Action: %s\n",argv[i]); break;
         }

   switch(todo)
   {
      case save:
         PrfWriteProfileData(INIhandle,"Options",app,&appopt,sizeof(appopt));
         PrfWriteProfileData(INIhandle,"Activation","Settings",&keyopt,sizeof(keyopt));
         printf("Parameters saved.\n");
         PrfCloseProfile(INIhandle);
         break;

      case delete:
         PrfWriteProfileData(INIhandle,"Options",app,NULL,0);
         printf("Profile for %s deleted.\n",app);
         PrfCloseProfile(INIhandle);
         break;

      case load:
         PrfCloseProfile(INIhandle);
         if (HotScrollHookInit() || HotScrollLoaded())
         {
            HotScrollWaitLoaded();
            printf("Hook successfully loaded.\n");
         }
         else
            printf("Hook NOT successfully loaded.\n");

         printf("Parameters activated.\n");
         printf("Press any key to terminate.\n");
         fflush(stdout);
         _getch();

         if (HotScrollHookKill() || !HotScrollLoaded())
            printf("Hook successfully unloaded.\n");
         else
            printf("Hook NOT successfully unloaded.\n");
         break;

      case nothing:
         PrfCloseProfile(INIhandle);
         printf("Done nothing! Better luck next time.\n"); break;
   }

   return 0;
}

int setoption(char *arg, int option)
{
   if(arg[2] == '-')
      return 0;
   else if (arg[2] == '+')
      return 1;
   else if (arg[2] == '*')
      return 2;
   else if(option == 1)
      return 0;
   else
      return 1;
}

char *OnOff(short option)
{
   switch(option)
   {
      case 0: return("off");
      case 1: return("on ");
      case 2: return("def");
      default: return("error");
   }
}

void help(char *program)
{
   printf("\nHot Scroll 1.1 Loader (C) 1998 Samuel Audet <guardia@cam.org> \n"
          "\n"
          "%s <l|s|d> [/lacfk123[-|+]] [/i] [/p <profile name>] [/ehvomdrs[-|+|*]] [/s[+|*] <speed %%>] [/n[-|+|*] <pos/pix>] [/?] \n"
          "                                                                 currently\n"
          " l  load          /p  current profile:     /e  enable               %s\n"
          " s  save              %-20s /h  scroll horizontally  %s\n"
          " d  delete        /i  list profiles        /v  scroll vertically    %s\n"
          "                                           /l  scroll lock          %s\n"
          "    Keys that must be down to scroll       /o  no focus lock        %s\n"
          " /a  alt             %s                   /m  mouse leash          %s\n"
          " /c  ctrl            %s                   /d  fake dynamic scroll  %s\n"
          " /f  shift           %s                   /r  reversed scrolling   %s\n"
          " /k  custom key      %s   scancode: %-3d   /s  speed of scrolling   %d%%\n"
          " /1  mouse button 1  %s                   /n  non-proportional     %s\n"
          " /2  mouse button 2  %s                       %d positions/pixel     \n"
          " /3  mouse button 3  %s\n"
          "                               Don't forget to send mail, and\n"
          " /?  this screen               money is always appreciated.\n\n",
                                        program,
                                        OnOff(appopt.enabled),
                                        app,
                                        OnOff(appopt.horizontal),
                                        OnOff(appopt.vertical),
                                        OnOff(keyopt.scroll_lock),
                                        OnOff(appopt.no_focus_lock),
                                        OnOff(keyopt.alt),
                                        OnOff(appopt.mouse_leash),
                                        OnOff(keyopt.ctrl),
                                        OnOff(appopt.fake_dynamic),
                                        OnOff(keyopt.shift),
                                        OnOff(appopt.reversed),
                                        OnOff(keyopt.custom_key_toggle),
                                        keyopt.custom_key,
                                        appopt.speed,
                                        OnOff(keyopt.button_1),
                                        OnOff(appopt.nonprop),
                                        OnOff(keyopt.button_2),
                                        appopt.posperpix,
                                        OnOff(keyopt.button_3));

          if(HotScrollLoaded())
             printf("Hot Scroll is currently loaded.\n");
          else
             printf("Hot Scroll is not currently loaded.\n");
}

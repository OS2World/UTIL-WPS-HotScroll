/* Install 'dll' entry into USER INI file */

call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs

dll = 'HOTSCRL'

/* get INI entry, and strip out null terminator */
string = SysIni("USER","SYS_DLLS","LoadOneTime")
string = left(string,length(string)-1)

flag = 0
do wrd = 1 to WORDS(string)
  if word(string,wrd) = dll then
     flag = 1
end
if flag = 0 then
do
   if SysIni("USER","SYS_DLLS","LoadOneTime",string dll||d2c(0)) = '' then
   do
      say dll 'successfully installed.'
      say 'Make sure you have copied' dll 'in \OS2\DLL.'
   end
   else
      say 'An error occured while installing' dll 'in OS2.INI.'
end
else
   say dll 'already installed.'

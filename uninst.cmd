/* Install 'dll' entry into USER INI file */

call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs

dll = 'HOTSCRL'

/* get INI entry, and strip out null terminator */
string = SysIni("USER","SYS_DLLS","LoadOneTime")
string = left(string,length(string)-1)

uninst = 0
do wrd = 1 to WORDS(string)
  if word(string,wrd) = dll then
  do
     if SysIni("USER","SYS_DLLS","LoadOneTime",delword(string,wrd,1)||d2c(0)) = '' then
        say dll 'successfully uninstalled.'
     uninst = 1
  end
end

if uninst = 0 then
   say 'Nothing to do.'

@echo off
setlocal

if exist "G:\C++\My custom language\see-lang\msvc-wdk\setup_x64.bat" call "G:\C++\My custom language\see-lang\msvc-wdk\setup_x64.bat" 2>nul

rc.exe /nologo /fo src\version.res src\version.rc

cl.exe /nologo /std:c++20 /O2 /Oi /Ot /EHsc /MT /GS /guard:cf /sdl /I src ^
    src\main.cpp ^
    src\common\types.cpp ^
    src\common\crypto.cpp ^
    src\parser\lexer.cpp ^
    src\parser\parser.cpp ^
    src\compiler\bytecode.cpp ^
    src\compiler\optimizer.cpp ^
    src\compiler\icon.cpp ^
    src\compiler\builder.cpp ^
    src\runtime\env.cpp ^
    src\runtime\math.cpp ^
    src\runtime\vm.cpp ^
    src\cli\cmdline.cpp ^
    /Fe:syn.bat.compiler.exe ^
    /link /SUBSYSTEM:CONSOLE ^
    /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /GUARD:CF ^
    src\version.res ^
    kernel32.lib user32.lib shell32.lib /OPT:REF /OPT:ICF

del *.obj 2>nul

if exist syn.bat.compiler.exe (
    echo built syn.bat.compiler.exe
) else (
    echo build failed
)

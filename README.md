# syn bat compiler

standalone compiler converting batch scripts into encrypted native executables running via built-in vm runtime.

## how to use

interactive / drag-and-drop:
- run `syn.bat.compiler.exe` and drag a `.bat` file into the window, or drop a `.bat` file onto `syn.bat.compiler.exe`

cli:

```bash
syn.bat.compiler.exe input.bat [flags]
```

## flags

- `/Fe:<path>`, `-o <path>`: output `.exe` path
- `/icon:<file.ico>`: embed custom icon
- `/res:<file.res|file.rc>`: embed custom `.res` or `.rc` resource
- `/invisible`, `-i`: stealth mode (no console window)
- `/pad:<kb>`, `-p <kb>`: pad binary size in kb
- `/key:<string>`: custom encryption key
- `/nologo`: suppress banner

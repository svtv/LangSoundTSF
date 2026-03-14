# LangSoundTSF

Windows provides various ways to add audible feedback for state toggles (Caps Lock, Num Lock, etc.), but it does not provide a built-in sound for input language switching in multilingual setups. This project fills that gap by playing a short sound when the active input language changes.

LangSoundTSF is a Windows Text Services Framework (TSF) text service that plays a short sound when the current input language changes.

The service compares the currently active keyboard layout language with the system default input language:

- If the active language is not the default input language, it plays `NonDefault.wav`.
- If the active language is the default input language, it plays `Default.wav`.

The WAV files are embedded into the DLL as resources.

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 (C++23)

## Build

Open `LangSoundTSF.sln` in Visual Studio and build the `Release | x64` configuration.

## Install (register)

Registration requires administrative privileges because TSF TIP registration writes under `HKLM\SOFTWARE\Microsoft\CTF\TIP`.

1. Get the source code:

   - Clone:

     ```bat
     git clone https://github.com/svtv/LangSoundTSF.git
     ```

   - Or download a ZIP archive from the GitHub repository page.

2. Build the DLL (see the Build section).

3. From an elevated Command Prompt, register the DLL:

```bat
C:\Windows\System32\regsvr32.exe "full\path\to\LangSoundTSF.dll"
```

## Uninstall (unregister)

From an elevated Command Prompt:

```bat
C:\Windows\System32\regsvr32.exe /u "full\path\to\LangSoundTSF.dll"
```

## Notes

- This project is an in-process COM server (TSF text service). The DLL may stay loaded while referenced by TSF-enabled processes.
- If you modify embedded sounds, rebuild the DLL so resources are updated.

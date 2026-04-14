# STM32 Unified Toolchain Setup (CubeCLT + EIDE + CLion)

This setup keeps one consistent STM32 CLI stack across IDEs:

- `arm-none-eabi-*` from CubeCLT
- `ST-LINK_gdbserver` from CubeCLT
- `OpenOCD` and `JLinkGDBServerCL` from STM32CubeIDE plugins
- `cmake` from your standalone system install

## 0) VS Code profile switch (recommended)

Use VS Code `Profile` binding (the UI shown in your screenshot):

1. Keep an `EIDE` profile for STM32 projects
2. Keep an `ESP-IDF` profile for ESP projects
3. Bind each folder/workspace to the matching profile under `Folders & Workspaces`

This avoids per-workspace setting conflicts.

## 1) Recommended paths on this machine

Resolved from `tools/stm32_unified_env.ps1`:

- `cmake.exe`:
  `D:\ProgrammingTools\cmake\bin\cmake.exe`
- `arm-none-eabi-gcc.exe`:
  `D:\ProgrammingTools\STM32CubeCLT_1.21.0\GNU-tools-for-STM32\bin\arm-none-eabi-gcc.exe`
- `arm-none-eabi-gdb.exe`:
  `D:\ProgrammingTools\STM32CubeCLT_1.21.0\GNU-tools-for-STM32\bin\arm-none-eabi-gdb.exe`
- `STM32_Programmer_CLI.exe`:
  `D:\ProgrammingTools\STM32CubeCLT_1.21.0\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe`
- `ST-LINK_gdbserver.exe`:
  `D:\ProgrammingTools\STM32CubeCLT_1.21.0\STLink-gdb-server\bin\ST-LINK_gdbserver.exe`
- `openocd.exe`:
  `D:\ProgrammingTools\STM32CubeIDE_2.0.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.openocd.win32_2.4.300.202509300731\tools\bin\openocd.exe`
- `OpenOCD scripts`:
  `D:\ProgrammingTools\STM32CubeIDE_2.0.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.debug.openocd_2.3.200.202510310951\resources\openocd\st_scripts`
- `JLinkGDBServerCL.exe`:
  `D:\ProgrammingTools\STM32CubeIDE_2.0.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.jlink.win32_2.5.100.202509120932\tools\bin\JLinkGDBServerCL.exe`

Notes:

- CubeCLT `1.21.0` does not include an `OpenOCD` folder in your install.
- Using OpenOCD/J-Link from STM32CubeIDE keeps the stack STM32-official and newer than your EIDE-bundled tools.

## 2) EIDE configuration

Set EIDE tools to the paths above and avoid mixed sources:

- GCC root:
  `D:\ProgrammingTools\STM32CubeCLT_1.21.0\GNU-tools-for-STM32\bin`
- GDB:
  `D:\ProgrammingTools\STM32CubeCLT_1.21.0\GNU-tools-for-STM32\bin\arm-none-eabi-gdb.exe`
- OpenOCD executable:
  `...\externaltools.openocd...\tools\bin\openocd.exe`
- OpenOCD scripts:
  `...\debug.openocd...\resources\openocd\st_scripts`
- ST-Link GDB Server:
  `D:\ProgrammingTools\STM32CubeCLT_1.21.0\STLink-gdb-server\bin\ST-LINK_gdbserver.exe`
- J-Link GDB Server:
  `...\externaltools.jlink...\tools\bin\JLinkGDBServerCL.exe`
- CubeProgrammer:
  `D:\ProgrammingTools\STM32CubeCLT_1.21.0\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe`

If your EIDE project has per-project tool paths, update them there first, then update global defaults.

## 3) CLion configuration

### Toolchain

In `Settings -> Build, Execution, Deployment -> Toolchains`:

- CMake:
  `D:\ProgrammingTools\cmake\bin\cmake.exe`
- C Compiler:
  `D:\ProgrammingTools\STM32CubeCLT_1.21.0\GNU-tools-for-STM32\bin\arm-none-eabi-gcc.exe`
- C++ Compiler:
  `D:\ProgrammingTools\STM32CubeCLT_1.21.0\GNU-tools-for-STM32\bin\arm-none-eabi-g++.exe`
- Debugger:
  `D:\ProgrammingTools\STM32CubeCLT_1.21.0\GNU-tools-for-STM32\bin\arm-none-eabi-gdb.exe`

### CMake profile

Use:

- `-DCMAKE_TOOLCHAIN_FILE=<repo>/cmake/stm32-gcc-toolchain.cmake`

Optional (if you want explicit override):

- `-DARM_GCC_BIN=D:/ProgrammingTools/STM32CubeCLT_1.21.0/GNU-tools-for-STM32/bin`

### Debug server choice

- OpenOCD server command:
  `...\openocd.exe -s ...\st_scripts -f interface/stlink.cfg -f target/<your-target>.cfg`
- ST-Link GDB Server command:
  `...\ST-LINK_gdbserver.exe -p 61234 -cp D:\ProgrammingTools\STM32CubeCLT_1.21.0\STM32CubeProgrammer\bin`

Pick one server per debug profile and keep `arm-none-eabi-gdb.exe` the same.

## 4) Optional: export environment in one shot

Run in PowerShell:

```powershell
. .\tools\stm32_unified_env.ps1 -ExportToSession
```

This exports:

- `STM32_CMAKE`
- `STM32_GCC_BIN`
- `STM32_GDB`
- `STM32_OPENOCD`
- `STM32_OPENOCD_SCRIPTS`
- `STM32_STLINK_GDB_SERVER`
- `STM32_JLINK_GDB_SERVER`
- `STM32_PROGRAMMER_CLI`


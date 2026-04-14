param(
    [string]$CubeCltRoot = "D:\ProgrammingTools\STM32CubeCLT_1.21.0",
    [string]$CubeIdeRoot = "D:\ProgrammingTools\STM32CubeIDE_2.0.0\STM32CubeIDE",
    [string]$SystemCMake = "D:\ProgrammingTools\cmake\bin\cmake.exe",
    [switch]$ExportToSession
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-LatestPluginDir {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PluginsRoot,
        [Parameter(Mandatory = $true)]
        [string]$Filter
    )

    if (-not (Test-Path $PluginsRoot)) {
        return $null
    }

    return Get-ChildItem -Path $PluginsRoot -Directory -Filter $Filter -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending |
        Select-Object -First 1
}

function Get-FirstExistingPath {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Candidates
    )

    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return $candidate
        }
    }

    return $null
}

$pluginsRoot = Join-Path $CubeIdeRoot "plugins"
$openocdToolPlugin = Get-LatestPluginDir -PluginsRoot $pluginsRoot -Filter "com.st.stm32cube.ide.mcu.externaltools.openocd.win32_*"
$openocdScriptPlugin = Get-LatestPluginDir -PluginsRoot $pluginsRoot -Filter "com.st.stm32cube.ide.mcu.debug.openocd_*"
$jlinkPlugin = Get-LatestPluginDir -PluginsRoot $pluginsRoot -Filter "com.st.stm32cube.ide.mcu.externaltools.jlink.win32_*"

$resolved = [ordered]@{
    cmake_exe = Get-FirstExistingPath -Candidates @(
        $SystemCMake,
        (Join-Path $CubeCltRoot "CMake\bin\cmake.exe")
    )
    gcc_bin_dir = Get-FirstExistingPath -Candidates @(
        (Join-Path $CubeCltRoot "GNU-tools-for-STM32\bin"),
        "$env:USERPROFILE\.eide\tools\gcc_arm\bin"
    )
    arm_none_eabi_gcc = $null
    arm_none_eabi_gdb = $null
    stm32_programmer_cli = Get-FirstExistingPath -Candidates @(
        (Join-Path $CubeCltRoot "STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"),
        "$env:USERPROFILE\.eide\tools\st_cube_programer\bin\STM32_Programmer_CLI.exe"
    )
    stlink_gdb_server = Get-FirstExistingPath -Candidates @(
        (Join-Path $CubeCltRoot "STLink-gdb-server\bin\ST-LINK_gdbserver.exe")
    )
    openocd_exe = Get-FirstExistingPath -Candidates @(
        $(if ($openocdToolPlugin) { Join-Path $openocdToolPlugin.FullName "tools\bin\openocd.exe" }),
        "D:\ProgrammingTools\xpack-openocd-0.12.0-7\bin\openocd.exe",
        "$env:USERPROFILE\.eide\tools\openocd_7a1adfbec_mingw32\bin\openocd.exe"
    )
    openocd_scripts = Get-FirstExistingPath -Candidates @(
        $(if ($openocdScriptPlugin) { Join-Path $openocdScriptPlugin.FullName "resources\openocd\st_scripts" }),
        "D:\ProgrammingTools\xpack-openocd-0.12.0-7\openocd\scripts",
        "$env:USERPROFILE\.eide\tools\openocd_7a1adfbec_mingw32\scripts"
    )
    jlink_gdb_server = Get-FirstExistingPath -Candidates @(
        $(if ($jlinkPlugin) { Join-Path $jlinkPlugin.FullName "tools\bin\JLinkGDBServerCL.exe" }),
        "$env:USERPROFILE\.eide\tools\jlink\JLinkGDBServerCL.exe"
    )
}

if ($resolved.gcc_bin_dir) {
    $resolved.arm_none_eabi_gcc = Join-Path $resolved.gcc_bin_dir "arm-none-eabi-gcc.exe"
    $resolved.arm_none_eabi_gdb = Join-Path $resolved.gcc_bin_dir "arm-none-eabi-gdb.exe"
}

$missing = @()
foreach ($key in $resolved.Keys) {
    $value = $resolved[$key]
    if (-not $value -or -not (Test-Path $value)) {
        $missing += $key
    }
}

if ($ExportToSession) {
    if ($resolved.cmake_exe) { $env:STM32_CMAKE = $resolved.cmake_exe }
    if ($resolved.gcc_bin_dir) { $env:STM32_GCC_BIN = $resolved.gcc_bin_dir }
    if ($resolved.arm_none_eabi_gdb) { $env:STM32_GDB = $resolved.arm_none_eabi_gdb }
    if ($resolved.openocd_exe) { $env:STM32_OPENOCD = $resolved.openocd_exe }
    if ($resolved.openocd_scripts) { $env:STM32_OPENOCD_SCRIPTS = $resolved.openocd_scripts }
    if ($resolved.stlink_gdb_server) { $env:STM32_STLINK_GDB_SERVER = $resolved.stlink_gdb_server }
    if ($resolved.jlink_gdb_server) { $env:STM32_JLINK_GDB_SERVER = $resolved.jlink_gdb_server }
    if ($resolved.stm32_programmer_cli) { $env:STM32_PROGRAMMER_CLI = $resolved.stm32_programmer_cli }

    $pathEntries = @(
        $resolved.gcc_bin_dir,
        $(if ($resolved.openocd_exe) { Split-Path $resolved.openocd_exe -Parent }),
        $(if ($resolved.stlink_gdb_server) { Split-Path $resolved.stlink_gdb_server -Parent }),
        $(if ($resolved.jlink_gdb_server) { Split-Path $resolved.jlink_gdb_server -Parent }),
        $(if ($resolved.stm32_programmer_cli) { Split-Path $resolved.stm32_programmer_cli -Parent }),
        $(if ($resolved.cmake_exe) { Split-Path $resolved.cmake_exe -Parent })
    ) | Where-Object { $_ } | Select-Object -Unique

    $env:Path = (($pathEntries + @($env:Path)) -join ";")
}

$result = [ordered]@{
    resolved_paths = $resolved
    missing_keys = $missing
    export_to_session = [bool]$ExportToSession
}

$result | ConvertTo-Json -Depth 6

#!/bin/sh
# Compiles and installs start-everything.wh.cpp the way Windhawk's own
# compiler does, registers it, and bumps SettingsChangeTime to make the engine
# hot-reload it. Development tooling, not part of the mod.
#
# Every registry path below is written out in full and touched with
# Set-ItemProperty on a single named key. Nothing here builds a path by
# interpolation and nothing recurses -- an earlier version of this pattern
# deleted every installed mod's registration.
set -e
cd "$(dirname "$0")"

CXX="/c/Program Files/Windhawk/Compiler/bin/clang++.exe"
INC="/c/Program Files/Windhawk/Compiler/include"
ENGDIR="/c/Program Files/Windhawk/Engine/1.7.3/64"
MODS="/c/ProgramData/Windhawk/Engine/Mods/64"
SRCDIR="/c/ProgramData/Windhawk/ModsSource"

# A fresh filename every build: Windhawk keeps the previous image mapped, so
# rewriting the same path would be refused.
NAME="local@start-everything_0.1_$(date +%H%M%S).dll"

"$CXX" --target=x86_64-w64-mingw32 -shared -O2 -std=c++23 \
  -DUNICODE -D_UNICODE -mwindows \
  -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 \
  -D_WIN32_IE=0x0A00 -DNTDDI_VERSION=0x0A000008 \
  -D__USE_MINGW_ANSI_STDIO=0 -DWH_MOD \
  '-DWH_MOD_ID=L"local@start-everything"' '-DWH_MOD_VERSION=L"0.1"' \
  -include windhawk_api.h -I"$INC" \
  -L"$ENGDIR" \
  -Wl,--export-all-symbols \
  -o "$MODS/$NAME" \
  -x c++ start-everything.wh.cpp \
  -lwindhawk -lole32 -loleaut32 -lruntimeobject

cp start-everything.wh.cpp "$SRCDIR/local@start-everything.wh.cpp"

powershell.exe -NoProfile -Command "
  \$k = 'HKLM:\SOFTWARE\Windhawk\Engine\Mods\local@start-everything';
  if (-not (Test-Path \$k)) { New-Item -Path \$k | Out-Null }
  \$s = 'HKLM:\SOFTWARE\Windhawk\Engine\Mods\local@start-everything\Settings';
  if (-not (Test-Path \$s)) { New-Item -Path \$s | Out-Null }
  Set-ItemProperty \$k -Name 'LibraryFileName' -Value '$NAME';
  Set-ItemProperty \$k -Name 'Include' -Value 'StartMenuExperienceHost.exe';
  Set-ItemProperty \$k -Name 'Exclude' -Value '';
  Set-ItemProperty \$k -Name 'Architecture' -Value 'x86-64';
  Set-ItemProperty \$k -Name 'Version' -Value '0.1';
  Set-ItemProperty \$k -Name 'Disabled' -Value 0 -Type DWord;
  Set-ItemProperty \$k -Name 'LoggingEnabled' -Value 0 -Type DWord;
  Set-ItemProperty \$k -Name 'DebugLoggingEnabled' -Value 1 -Type DWord;
  if (-not (Get-ItemProperty \$s -Name 'filesColumnPercent' -EA SilentlyContinue)) {
    Set-ItemProperty \$s -Name 'filesColumnPercent' -Value 58 -Type DWord;
    Set-ItemProperty \$s -Name 'showIcons' -Value 1 -Type DWord;
    Set-ItemProperty \$s -Name 'rowHeight' -Value 40 -Type DWord;
  }
  Set-ItemProperty \$k -Name 'SettingsChangeTime' -Value ([int][DateTimeOffset]::UtcNow.ToUnixTimeSeconds()) -Type DWord
" >/dev/null
echo "installed $NAME"

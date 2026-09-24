$timestamp = (Get-Date -Format 'HHmmss')
$name = "local@start-everything_1.0_$timestamp.dll"
$modsDir = "C:\ProgramData\Windhawk\Engine\Mods\64"
$outPath = Join-Path $modsDir $name

Write-Host "Compiling unified mod $outPath..."

$cxx = "C:\Program Files\Windhawk\Compiler\bin\clang++.exe"
$args = @(
    "--target=x86_64-w64-mingw32",
    "-shared",
    "-O2",
    "-std=c++23",
    "-DUNICODE",
    "-D_UNICODE",
    "-mwindows",
    "-DWINVER=0x0A00",
    "-D_WIN32_WINNT=0x0A00",
    "-D_WIN32_IE=0x0A00",
    "-DNTDDI_VERSION=0x0A000008",
    "-D__USE_MINGW_ANSI_STDIO=0",
    "-DWH_MOD",
    "-include", "windhawk_api.h",
    "-I", "C:\Program Files\Windhawk\Compiler\include",
    "-L", "C:\Program Files\Windhawk\Engine\1.7.3\64",
    "-Wl,--export-all-symbols",
    "-o", $outPath,
    "-x", "c++", (Join-Path $PSScriptRoot "start-everything.wh.cpp"),
    "-lwindhawk", "-lole32", "-loleaut32", "-lruntimeobject", "-luuid", "-lshell32", "-lshlwapi", "-lcomctl32", "-ldwmapi", "-luser32", "-liphlpapi", "-lgdi32"
)

& $cxx @args
if ($LASTEXITCODE -ne 0) {
    Write-Error "Compilation failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

Copy-Item (Join-Path $PSScriptRoot "start-everything.wh.cpp") "C:\ProgramData\Windhawk\ModsSource\local@start-everything.wh.cpp" -Force

# 1. Configure unified start-everything mod in Windhawk
$k = 'HKLM:\SOFTWARE\Windhawk\Engine\Mods\local@start-everything'
if (-not (Test-Path $k)) { New-Item -Path $k | Out-Null }
Set-ItemProperty $k -Name 'LibraryFileName' -Value $name
Set-ItemProperty $k -Name 'Include' -Value 'StartMenuExperienceHost.exe|SearchHost.exe|explorer.exe'
Set-ItemProperty $k -Name 'Exclude' -Value ''
Set-ItemProperty $k -Name 'Architecture' -Value 'x86-64'
Set-ItemProperty $k -Name 'Version' -Value '1.0'
Set-ItemProperty $k -Name 'Disabled' -Value 0 -Type DWord
Set-ItemProperty $k -Name 'SettingsChangeTime' -Value ([int][DateTimeOffset]::UtcNow.ToUnixTimeSeconds()) -Type DWord

# 1b. Populate default 6 unitConversions into Settings registry so Windhawk UI shows each item individually
$kSettings = 'HKLM:\SOFTWARE\Windhawk\Engine\Mods\local@start-everything\Settings'
if (-not (Test-Path $kSettings)) { New-Item -Path $kSettings | Out-Null }

$conversions = @(
    @{ fromUnit = 'km'; toUnit = 'miles'; formula = 'x * 0.621371'; category = 'Distance' },
    @{ fromUnit = 'c'; toUnit = "$([char]0x00B0)F"; formula = 'x * 9 / 5 + 32'; category = 'Temperature' },
    @{ fromUnit = 'kg'; toUnit = 'lbs'; formula = 'x * 2.20462'; category = 'Weight' },
    @{ fromUnit = 'm'; toUnit = 'feet'; formula = 'x * 3.28084'; category = 'Length' },
    @{ fromUnit = 'cm'; toUnit = 'in'; formula = 'x / 2.54'; category = 'Length' },
    @{ fromUnit = 'mb'; toUnit = 'GB'; formula = 'x / 1024'; category = 'Storage' }
)

$curProps = (Get-ItemProperty -Path $kSettings).psobject.Properties | Where-Object { $_.Name -like 'unitConversions*' }
foreach ($p in $curProps) {
    Remove-ItemProperty -Path $kSettings -Name $p.Name -ErrorAction SilentlyContinue
}

for ($idx = 0; $idx -lt $conversions.Count; $idx++) {
    Set-ItemProperty -Path $kSettings -Name "unitConversions[$idx].fromUnit" -Value $conversions[$idx].fromUnit
    Set-ItemProperty -Path $kSettings -Name "unitConversions[$idx].toUnit" -Value $conversions[$idx].toUnit
    Set-ItemProperty -Path $kSettings -Name "unitConversions[$idx].formula" -Value $conversions[$idx].formula
    Set-ItemProperty -Path $kSettings -Name "unitConversions[$idx].category" -Value $conversions[$idx].category
}

# 1c. Populate default filterNoisyPaths and 14 excludedPaths patterns into Settings registry
Set-ItemProperty $kSettings -Name 'filterNoisyPaths' -Value 1 -Type DWord

$noisyPaths = @(
    '\node_modules\',
    '\.git\',
    '\.gradle\',
    '\appdata\local\temp\',
    '\appdata\local\packages\',
    '\__pycache__\',
    '\.venv\',
    '\site-packages\',
    '\.cache\',
    '\build\intermediates\',
    '\obj\debug\',
    '\obj\release\',
    '\windows\winsxs\',
    '\windows\servicing\'
)

$curExcluded = (Get-ItemProperty -Path $kSettings).psobject.Properties | Where-Object { $_.Name -like 'excludedPaths*' }
foreach ($p in $curExcluded) {
    Remove-ItemProperty -Path $kSettings -Name $p.Name -ErrorAction SilentlyContinue
}

for ($idx = 0; $idx -lt $noisyPaths.Count; $idx++) {
    Set-ItemProperty -Path $kSettings -Name "excludedPaths[$idx]" -Value $noisyPaths[$idx]
}

# 2. Disable old standalone and test mods to avoid duplicate hooks
foreach ($oldMod in @('local@searchhost-disconnect', 'local@prevent-searchhost-focus', 'local@start-everything-notap', 'local@start-everything-fix')) {
    $kOld = "HKLM:\SOFTWARE\Windhawk\Engine\Mods\$oldMod"
    if (Test-Path $kOld) {
        Set-ItemProperty $kOld -Name 'Disabled' -Value 1 -Type DWord
        Set-ItemProperty $kOld -Name 'SettingsChangeTime' -Value ([int][DateTimeOffset]::UtcNow.ToUnixTimeSeconds()) -Type DWord
    }
}

Write-Host "SUCCESS: $name registered and active"
Write-Host "Stand-alone mods disabled in registry"

# 3. Recycle target processes to pick up the consolidated mod
Stop-Process -Name StartMenuExperienceHost, SearchHost -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 1500
Get-Process -Name StartMenuExperienceHost, SearchHost -ErrorAction SilentlyContinue | Select-Object Id, ProcessName

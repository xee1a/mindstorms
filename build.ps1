# Configure, build, test and package NXT Studio.
#   .\build.ps1            Release build + tests, then a runnable folder in dist\
#   .\build.ps1 -Debug     Debug build
param([switch]$Debug, [string]$QtDir = "C:\Qt\6.8.3\msvc2022_64")

$ErrorActionPreference = "Stop"
$config = if ($Debug) { "Debug" } else { "Release" }

cmake -S $PSScriptRoot -B "$PSScriptRoot\build" -G "Visual Studio 17 2022" -A x64 "-DCMAKE_PREFIX_PATH=$QtDir"
if ($LASTEXITCODE) { exit $LASTEXITCODE }
cmake --build "$PSScriptRoot\build" --config $config --parallel
if ($LASTEXITCODE) { exit $LASTEXITCODE }
ctest --test-dir "$PSScriptRoot\build" -C $config --output-on-failure
if ($LASTEXITCODE) { exit $LASTEXITCODE }

# Standalone folder: the exe plus the Qt DLLs it needs.
$dist = "$PSScriptRoot\dist"
New-Item -ItemType Directory -Force $dist | Out-Null
Copy-Item "$PSScriptRoot\build\$config\NXTStudio.exe" $dist -Force
# The NXC compiler used by "Upload to NXT".
New-Item -ItemType Directory -Force "$dist\tools" | Out-Null
Copy-Item "$PSScriptRoot\third_party\nbc\nbc.exe" "$dist\tools\nbc.exe" -Force
& "$QtDir\bin\windeployqt.exe" --no-compiler-runtime --no-opengl-sw --no-system-d3d-compiler --translations pl "$dist\NXTStudio.exe"
if ($LASTEXITCODE) { exit $LASTEXITCODE }
# The Visual C++ runtime next to the exe, so dist\ also runs on PCs without the VC++ Redistributable.
if (-not $Debug) {
    $vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
    $crt = Get-ChildItem "$vs\VC\Redist\MSVC\*\x64\Microsoft.VC143.CRT" -Directory | Sort-Object FullName | Select-Object -Last 1
    if ($crt) { Copy-Item "$($crt.FullName)\*.dll" $dist -Force }
    else { Write-Warning "Visual C++ runtime not found; dist\ needs the VC++ Redistributable on other PCs." }
}
Write-Host "Ready: $dist\NXTStudio.exe"

# The same app as one downloadable file (unpacks itself on first start), kept in the repo under release\.
if (-not $Debug) {
    New-Item -ItemType Directory -Force "$PSScriptRoot\release" | Out-Null
    & "$PSScriptRoot\build\Release\nxtpack.exe" $dist "$PSScriptRoot\build\Release\nxtlauncher.exe" "$PSScriptRoot\release\NXTStudio.exe"
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
    Write-Host "Single file: $PSScriptRoot\release\NXTStudio.exe"

    # And as an installer, when a Python with msilib (3.12 or older) is around.
    $version = (Select-String -Path "$PSScriptRoot\CMakeLists.txt" -Pattern 'project\(NXTStudio VERSION ([\d.]+)').Matches[0].Groups[1].Value
    $hasMsilib = $false
    if (Get-Command python -ErrorAction SilentlyContinue) {
        & python -W ignore -c "import msilib" 2>$null
        $hasMsilib = $LASTEXITCODE -eq 0
    }
    if ($hasMsilib) {
        & python "$PSScriptRoot\tools\make_msi.py" $dist "$PSScriptRoot\release\NXTStudio.msi" $version
        if ($LASTEXITCODE) { exit $LASTEXITCODE }
    } else { Write-Warning "No Python 3.12 with msilib; skipping release\NXTStudio.msi." }
}

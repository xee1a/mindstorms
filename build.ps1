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
& "$QtDir\bin\windeployqt.exe" --no-compiler-runtime --no-opengl-sw --no-system-d3d-compiler --translations pl "$dist\NXTStudio.exe"
if ($LASTEXITCODE) { exit $LASTEXITCODE }
Write-Host "Ready: $dist\NXTStudio.exe"

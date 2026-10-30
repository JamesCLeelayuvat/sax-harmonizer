# Builds (Release, so timings mean something) and runs the pitch-detection benchmark. Usage (from anywhere):
#   powershell -ExecutionPolicy Bypass -File Tests/Benchmark/run_benchmark.ps1
$ErrorActionPreference = "Stop"
$msbuild = "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/amd64/MSBuild.exe"
$sln = Join-Path $PSScriptRoot "Builds/VisualStudio2026/Benchmark.sln"

# Projucer targets VS 2026 (v145); only VS 2022 Build Tools (v143) are installed
& $msbuild $sln -p:Configuration=Release -p:Platform=x64 -p:PlatformToolset=v143 -m -nologo -v:minimal
if ($LASTEXITCODE -ne 0) { Write-Host "Build failed"; exit $LASTEXITCODE }

& (Join-Path $PSScriptRoot "Builds/VisualStudio2026/x64/Release/ConsoleApp/Benchmark.exe")
exit $LASTEXITCODE

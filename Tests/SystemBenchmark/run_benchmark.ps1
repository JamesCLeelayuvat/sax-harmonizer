# Builds both configurations, runs the whole-application benchmark, and plots it. The CSV and PDF land in the
# current working directory. Usage (from the project root):
#   powershell -ExecutionPolicy Bypass -File Tests/SystemBenchmark/run_benchmark.ps1
$ErrorActionPreference = "Stop"
$msbuild = "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/MSBuild/Current/Bin/amd64/MSBuild.exe"
$sln = Join-Path $PSScriptRoot "Builds/VisualStudio2026/SystemBenchmark.sln"

foreach ($config in "Release", "Debug") {
    # Projucer targets VS 2026 (v145); only VS 2022 Build Tools (v143) are installed
    & $msbuild $sln -p:Configuration=$config -p:Platform=x64 -p:PlatformToolset=v143 -m -nologo -v:minimal
    if ($LASTEXITCODE -ne 0) { Write-Host "Build failed"; exit $LASTEXITCODE }
}

# Release runs every test. Debug adds its latency rows, the only test the build changes.
& (Join-Path $PSScriptRoot "Builds/VisualStudio2026/x64/Release/ConsoleApp/SystemBenchmark.exe")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $PSScriptRoot "Builds/VisualStudio2026/x64/Debug/ConsoleApp/SystemBenchmark.exe") --tests=latency --append
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

python (Join-Path $PSScriptRoot "plot_results.py")
exit $LASTEXITCODE

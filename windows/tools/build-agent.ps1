[CmdletBinding()]
param(
  [ValidateSet('Debug', 'Release')]
  [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswherePath)) { throw 'Visual Studio Installer vswhere.exe was not found.' }
$installation = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | ConvertFrom-Json
if (-not $installation) { throw 'MSVC Build Tools were not found.' }
$installation = @($installation)[0]
$cmakePath = Join-Path $installation.installationPath 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$ctestPath = Join-Path (Split-Path -Parent $cmakePath) 'ctest.exe'
if (-not (Test-Path -LiteralPath $cmakePath)) { throw 'The Visual Studio CMake component was not found.' }
$majorVersion = ([version]$installation.installationVersion).Major
$generator = switch ($majorVersion) {
  16 { 'Visual Studio 16 2019' }
  17 { 'Visual Studio 17 2022' }
  default { throw "Unsupported Visual Studio major version: $majorVersion" }
}
$buildDirectory = Join-Path $repositoryRoot "build/native-vs$majorVersion"
& $cmakePath -S (Join-Path $repositoryRoot 'agent') -B $buildDirectory -G $generator -A x64 "-DCMAKE_GENERATOR_INSTANCE=$($installation.installationPath)"
if ($LASTEXITCODE -ne 0) { throw 'Native configure failed.' }
& $cmakePath --build $buildDirectory --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }
& $ctestPath --test-dir $buildDirectory -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Native preflight tests failed.' }

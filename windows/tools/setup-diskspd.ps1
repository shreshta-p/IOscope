param([switch]$AcceptDiskSpdLicense)
$ErrorActionPreference = 'Stop'
$projectDirectory = Split-Path $PSScriptRoot -Parent
$pin = Get-Content -LiteralPath (Join-Path $projectDirectory 'agent/diskspd.lock.json') -Raw | ConvertFrom-Json
if (-not $AcceptDiskSpdLicense) {
  throw 'Read the Microsoft DiskSpd binary license in the official v2.3 release, then rerun with -AcceptDiskSpdLicense if you accept it: https://github.com/microsoft/diskspd/releases/tag/v2.3. IOscope does not redistribute this binary.'
}
$buildDirectory = Join-Path $projectDirectory 'build'
$archivePath = Join-Path $buildDirectory 'DiskSpd-v2.3.zip'
$dependencyDirectory = Join-Path $buildDirectory 'diskspd-v2.3'
$executablePath = Join-Path $dependencyDirectory 'amd64/diskspd.exe'
New-Item -ItemType Directory -Path $buildDirectory -Force | Out-Null
if (-not (Test-Path -LiteralPath $archivePath)) { Invoke-WebRequest -Uri $pin.url -OutFile $archivePath }
if ((Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pin.archiveSha256) { throw 'DiskSpd archive hash mismatch; no files extracted.' }
if (-not (Test-Path -LiteralPath $dependencyDirectory)) { Expand-Archive -LiteralPath $archivePath -DestinationPath $dependencyDirectory }
if (-not (Test-Path -LiteralPath $executablePath)) { throw 'Pinned amd64 executable missing; inspect the dependency directory.' }
if ((Get-FileHash -LiteralPath $executablePath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pin.executableSha256) { throw 'DiskSpd executable hash mismatch.' }
Write-Output 'Pinned DiskSpd 2.3 verified. No workload was started.'

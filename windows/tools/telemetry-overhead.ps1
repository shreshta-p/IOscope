$ErrorActionPreference = 'Stop'
$agentPid = [int](Get-Content "$PSScriptRoot/../out/agent.pid")
$initial = Get-Process -Id $agentPid
$startCpu = $initial.TotalProcessorTime.TotalSeconds
$timer = [Diagnostics.Stopwatch]::StartNew()
$samples = @()
for ($index=0; $index -lt 60; $index++) {
  Start-Sleep -Seconds 1
  $observed = Get-Process -Id $agentPid
  $samples += @{ elapsedSeconds=$timer.Elapsed.TotalSeconds; cpuSeconds=$observed.TotalProcessorTime.TotalSeconds; privateBytes=$observed.PrivateMemorySize64 }
}
$logicalCpuCount = [Environment]::ProcessorCount
$percent = 100 * ($samples[-1].cpuSeconds-$startCpu) / $timer.Elapsed.TotalSeconds / $logicalCpuCount
$maximumPrivate = ($samples.privateBytes | Measure-Object -Maximum).Maximum
$result = @{ durationSeconds=$timer.Elapsed.TotalSeconds; logicalCpuCount=$logicalCpuCount; normalizedCpuPercent=$percent; maximumPrivateBytes=$maximumPrivate; samples=$samples; method='Read-only agent sampling; browser and tests may coexist. No throughput-distortion claim.' }
$result | ConvertTo-Json -Depth 4 | Set-Content "$PSScriptRoot/../out/telemetry-overhead.json"
$result | Select-Object durationSeconds,normalizedCpuPercent,maximumPrivateBytes | ConvertTo-Json
if ($percent -ge 1 -or $maximumPrivate -ge 150MB) { throw 'Collector budget exceeded' }

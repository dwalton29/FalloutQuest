param(
    [ValidateRange(10,100)][int]$Seconds = 60,
    [string]$OutputDirectory = ".",
    [switch]$AllowDebug
)
$ErrorActionPreference = "Stop"
$adbCandidates = @((Join-Path $PSScriptRoot "adb.exe"), (Join-Path (Get-Location).Path "adb.exe"))
$script:AdbPath = $adbCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if (!$script:AdbPath) {
    $adbCommand = Get-Command adb -ErrorAction SilentlyContinue
    if (!$adbCommand) { throw "Cannot find adb.exe. Put this script in your platform-tools folder or add platform-tools to PATH." }
    $script:AdbPath = $adbCommand.Source
}
function Invoke-Adb {
    param([string[]]$Arguments)
    $result = & $script:AdbPath @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) { throw "adb failed: $($result -join ' ')" }
    return $result
}
# Run with the headset awake, FalloutQuest focused and the scene already loaded.
$null = Invoke-Adb -Arguments @("get-state")
$package = Invoke-Adb -Arguments @("shell", "dumpsys", "package", "com.falloutquest.app")
$packageText = $package -join "`n"
if ($packageText -notmatch 'versionName=0\.93\.0-reuse-water-depth') {
    throw "Install v234 before capturing. The currently installed package is not v234."
}
if (!$AllowDebug -and $packageText -notmatch 'versionName=0\.93\.0-reuse-water-depth-O2') {
    throw "Install the v234 O2 APK, or explicitly pass -AllowDebug for an unoptimised comparison."
}
$token = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds().ToString()
$null = Invoke-Adb -Arguments @("shell", "setprop", "debug.falloutquest.perf_capture", $token)
$started = $false
for ($i=0; $i -lt 8; $i++) {
    Start-Sleep -Seconds 1
    $events = (Invoke-Adb -Arguments @("logcat", "-d", "-s", "FQPerf:I", "*:S")) -join "`n"
    if ($events -match "CAPTURE_START id=$token\b") { $started = $true; break }
}
try {
    if (!$started) { throw "No capture start received. Keep FalloutQuest running and the headset awake. No old CSV will be downloaded." }
    Write-Host "Recording $Seconds seconds. Play normally with the headset awake."
    Start-Sleep -Seconds $Seconds
} finally {
    $null = Invoke-Adb -Arguments @("shell", "setprop", "debug.falloutquest.perf_capture", "0")
}
$saved = $false
for ($i=0; $i -lt 10; $i++) {
    Start-Sleep -Seconds 1
    $events = (Invoke-Adb -Arguments @("logcat", "-d", "-s", "FQPerf:I", "*:S")) -join "`n"
    if ($events -match "CAPTURE_END id=$token\b[^\r\n]*saved=1\b") { $saved = $true; break }
}
if (!$saved) { throw "No successful save for this capture. Keep the game focused; no old CSV will be downloaded." }
$csv = (Invoke-Adb -Arguments @("exec-out", "run-as", "com.falloutquest.app", "cat", "files/performance.csv")) -join "`n"
$header = ($csv -split "`n")[0]
if ($header -notmatch '^# format=233\b' -or $header -notmatch "session=$token\b") {
    throw "Downloaded CSV is stale or from the wrong build. It has not been saved as a new capture."
}
if (!$AllowDebug -and $header -notmatch '\bnative=profile-O2\b') {
    throw "The native library is not the O2 profile build. Download rejected."
}
$null = New-Item -ItemType Directory -Force -Path $OutputDirectory
$prefix = Join-Path $OutputDirectory "megaton-v234-$token"
$utf8 = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText("$prefix.csv", $csv + "`n", $utf8)
$log = (Invoke-Adb -Arguments @("logcat", "-d")) -join "`n"
[System.IO.File]::WriteAllText("$prefix-logcat.txt", $log + "`n", $utf8)
Write-Host $header
Write-Host ($events -split "`n" | Where-Object { $_ -match "CAPTURE_END id=$token\b" })
Write-Host "Saved $prefix.csv and $prefix-logcat.txt"

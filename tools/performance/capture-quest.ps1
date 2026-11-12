param(
    [ValidateRange(10,100)][int]$Seconds = 30,
    [string]$OutputDirectory = ".",
    [ValidatePattern('^[a-zA-Z0-9_-]{1,48}$')][string]$Scene = "megaton",
    [switch]$AllowDebug
)
$ErrorActionPreference = "Stop"
$token = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds().ToString()
$destination = New-Item -ItemType Directory -Force -Path $OutputDirectory
$prefix = Join-Path $destination.FullName "$Scene-$token"
$utf8 = New-Object System.Text.UTF8Encoding($false)
$script:AdbPath = $null
$versionCode = "unknown"
$versionName = "unknown"
$captureError = $null

function Invoke-Adb {
    param([string[]]$Arguments)
    # Windows PowerShell can wrap native stderr as error records.
    $ErrorActionPreference = "Continue"
    $result = & $script:AdbPath @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) { throw "adb $($Arguments[0]) failed: $($result -join ' ')" }
    return $result
}

try {
    $adbCandidates = @((Join-Path $PSScriptRoot "adb.exe"), (Join-Path (Get-Location).Path "adb.exe"))
    $script:AdbPath = $adbCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
    if (!$script:AdbPath) {
        $adbCommand = Get-Command adb -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
        if (!$adbCommand) { throw "Cannot find adb.exe. Put this script in your platform-tools folder or add platform-tools to PATH." }
        $script:AdbPath = $adbCommand.Source
    }

    # Run with the headset awake, FalloutQuest focused and the scene already loaded.
    $null = Invoke-Adb -Arguments @("get-state")
    $packageText = (Invoke-Adb -Arguments @("shell", "dumpsys", "package", "com.falloutquest.app")) -join "`n"
    $versionCode = [regex]::Match($packageText, '(?m)^\s*versionCode=(\d+)\b').Groups[1].Value
    $versionName = [regex]::Match($packageText, '(?m)^\s*versionName=(\S+)').Groups[1].Value
    if (!$versionCode -or !$versionName) { throw "FalloutQuest is not installed on the connected headset." }
    $prefix = Join-Path $destination.FullName "$Scene-v$versionCode-$token"
    if (!$AllowDebug -and !$versionName.EndsWith("-O2")) {
        throw "Install the FalloutQuest O2 profile APK, or pass -AllowDebug to capture a debug build."
    }

    $null = Invoke-Adb -Arguments @("shell", "setprop", "debug.falloutquest.perf_capture", $token)
    try {
        $started = $false
        for ($i=0; $i -lt 8; $i++) {
            Start-Sleep -Seconds 1
            $events = (Invoke-Adb -Arguments @("logcat", "-d", "-s", "FQPerf:I", "*:S")) -join "`n"
            if ($events -match "CAPTURE_START id=$token\b") { $started = $true; break }
        }
        if (!$started) { throw "No CAPTURE_START received. Keep FalloutQuest in focus and the headset awake." }
        Write-Host "Recording $Seconds seconds in $Scene on v$versionCode. Keep the game in focus."
        Start-Sleep -Seconds $Seconds
    } finally {
        $null = Invoke-Adb -Arguments @("shell", "setprop", "debug.falloutquest.perf_capture", "0")
    }

    $completion = $null
    for ($i=0; $i -lt 10; $i++) {
        Start-Sleep -Seconds 1
        $events = (Invoke-Adb -Arguments @("logcat", "-d", "-s", "FQPerf:I", "*:S")) -join "`n"
        $completion = [regex]::Match($events, "CAPTURE_END id=$token\b[^\r\n]*")
        if ($completion.Success) { break }
    }
    if (!$completion.Success) { throw "No CAPTURE_END received. Keep FalloutQuest running until the recording is saved." }
    if ($completion.Value -notmatch '\bsaved=1\b' -or $completion.Value -notmatch '\bframes=[1-9][0-9]*\b') {
        throw "The headset did not save a capture with frames. $($completion.Value)"
    }

    $csv = @(Invoke-Adb -Arguments @("exec-out", "run-as", "com.falloutquest.app", "cat", "files/performance.csv"))
    $header = $csv[0]
    if ($csv.Count -lt 3 -or $header -notmatch '^# format=233\b' -or $header -notmatch "session=$token\b") {
        throw "The CSV is empty, stale, or from the wrong build."
    }
    if (!$AllowDebug -and $header -notmatch '\bnative=profile-O2\b') {
        throw "The native library is not the O2 profile build."
    }
    [System.IO.File]::WriteAllLines("$prefix.csv", [string[]]$csv, $utf8)
    Write-Host $header
    Write-Host $completion.Value
    Write-Host "Saved $prefix.csv"
} catch {
    $captureError = $_
} finally {
    $log = ""
    if ($script:AdbPath) {
        try {
            $log = (Invoke-Adb -Arguments @("logcat", "-d", "-v", "threadtime", "-s", "FalloutQuest:I", "FQPerf:I", "*:S")) -join "`n"
        } catch {
            $log = "Could not read headset logcat: $($_.Exception.Message)"
        }
    }
    $status = if ($captureError) { "Capture failed: $($captureError.Exception.Message)" } else { "Capture saved." }
    [System.IO.File]::WriteAllText("$prefix-logcat.txt", "versionCode=$versionCode`nversionName=$versionName`nsession=$token`n$status`n$log`n", $utf8)
    Write-Host "Saved diagnostic log to $prefix-logcat.txt"
}
if ($captureError) { throw $captureError }

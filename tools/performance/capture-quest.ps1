param(
    [ValidateRange(10,100)][int]$Seconds = 30,
    [string]$OutputDirectory = ".",
    [ValidatePattern('^[a-zA-Z0-9_-]{1,48}$')][string]$Scene = "megaton",
    [switch]$AllowDebug,
    [ValidateSet('16','20')][string]$SolverIterations,
    [ValidateSet('On','Off')][string]$BufferBindReuse
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
$deviceNotes = New-Object System.Collections.Generic.List[string]

function Invoke-Adb {
    param([string[]]$Arguments)
    # Windows PowerShell can wrap native stderr as error records.
    $ErrorActionPreference = "Continue"
    $result = & $script:AdbPath @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) { throw "adb $($Arguments[0]) failed: $($result -join ' ')" }
    return $result
}

function Get-CaptureStatus {
    try {
        $lines = @(Invoke-Adb -Arguments @("exec-out", "run-as", "com.falloutquest.app", "cat", "files/performance-status.txt"))
        $status = @{}
        foreach ($line in $lines) { if ([string]$line -match '^([^=]+)=(.*)$') { $status[$Matches[1]] = $Matches[2] } }
        if ($status.session -eq $token -and $status.state) { return $status }
    } catch { } # Older APKs use logcat markers instead.
    return $null
}

function Add-DeviceSnapshot([string]$Label) {
    $deviceNotes.Add("--- $Label $([DateTime]::UtcNow.ToString('o')) ---")
    foreach ($service in @("thermalservice", "battery", "cpuinfo", "meminfo com.falloutquest.app")) {
        try {
            $deviceNotes.Add("dumpsys $service")
            $deviceNotes.Add((Invoke-Adb -Arguments (@("shell", "dumpsys") + $service.Split(' '))) -join "`n")
        } catch { $deviceNotes.Add($_.Exception.Message) }
    }
}

function Get-Percentile($Values, [double]$P) {
    $sorted = @($Values | Sort-Object)
    $position = ($sorted.Count - 1) * $P
    $low = [int][Math]::Floor($position)
    $high = [int][Math]::Ceiling($position)
    return [double]$sorted[$low] + ([double]$sorted[$high] - [double]$sorted[$low]) * ($position - $low)
}

function Write-CaptureSummary($Csv, [string]$Path) {
    $rows = @($Csv | Where-Object { $_ -notmatch '^#' } | ConvertFrom-Csv)
    $gameplay = @($rows | Where-Object { $_.loading -eq '0' -and $_.blocked -eq '0' -and $_.submitted -eq '1' -and (!$_.focused -or $_.focused -eq '1') })
    $report = New-Object System.Collections.Generic.List[string]
    $report.Add([string]$Csv[0])
    $report.Add("Frames: $($rows.Count). Active gameplay: $($gameplay.Count). Loading, blocked, unfocused and unsubmitted frames excluded below.")
    $report.Add("Times are milliseconds. Scopes include children and must not be added together. threadCpu is render-thread execution, work is elapsed time excluding xrWaitFrame.")
    $report.Add("GPU passes are sampled every 8 frames. -1 means unavailable or pending. The final in-flight GPU submissions can be missing. App cadence is not a compositor dropped-frame counter.")
    $report.Add("Performance summary: metric | samples | median ms | p95 ms | max ms")
    if ($gameplay.Count) {
        if ('motionRejectReason' -in $rows[0].PSObject.Properties.Name) {
            $rejected = @($gameplay | Where-Object { $_.motionRejectReason -ne 'none' })
            $report.Add("SpaceWarp history rejected records on $($rejected.Count)/$($gameplay.Count) gameplay frames.")
            foreach ($group in $rejected | Group-Object motionRejectReason | Sort-Object Count -Descending) {
                $row = $group.Group[0]
                $report.Add(("{0} on {1} frames. First example: object {2}, ref {3:X8}, mesh {4}, {5} objects, {6} bytes in use, {7} bytes for this record." -f $group.Name,$group.Count,$row.motionRejectObject,([uint32]$row.motionRejectRef),$row.motionRejectMesh,$row.motionRejectObjects,$row.motionRejectUsedBytes,$row.motionRejectRecordBytes))
            }
        }
        if ('vkBufferBindsIssued' -in $rows[0].PSObject.Properties.Name) {
            $issued = ($gameplay | Measure-Object -Property vkBufferBindsIssued -Sum).Sum
            $skipped = ($gameplay | Measure-Object -Property vkBufferBindsSkipped -Sum).Sum
            $draws = ($gameplay | Measure-Object -Property vkSceneDraws -Sum).Sum
            $report.Add("Vulkan scene draws: $draws. Vertex and index buffer binds issued: $issued. Skipped: $skipped.")
        }
        $measurements = @()
        $primary = @('workUs','threadCpuUs','xrWaitUs','xrEndUs','swapchainAcquireWaitUs','vkWorldGpuUs','vkUiGpuUs','vkReuseWaitUs','vkFinishWaitUs','gpuEyesUs')
        foreach ($key in $rows[0].PSObject.Properties.Name) {
            if ($key -notmatch 'Us$' -or $key -match 'MaxUs$' -or $key -in @('periodUs','cadenceUs','elapsedUs')) { continue }
            $values = @($gameplay | ForEach-Object { [double]::Parse($_.$key, [Globalization.CultureInfo]::InvariantCulture) } | Where-Object { $_ -ge 0 })
            if (!$values.Count) { continue }
            $median = Get-Percentile $values 0.5
            $p95 = Get-Percentile $values 0.95
            $maximum = ($values | Measure-Object -Maximum).Maximum
            $measurements += [pscustomobject]@{ Key=$key; P95=$p95; Line=("{0} | {1} | {2:F3} | {3:F3} | {4:F3}" -f $key,$values.Count,($median/1000),($p95/1000),($maximum/1000)) }
        }
        foreach ($m in $measurements | Where-Object { $_.Key -in $primary }) { $report.Add($m.Line) }
        if ('rigidUpdateBoundsUs' -in $rows[0].PSObject.Properties.Name) {
            $report.Add("Bullet timings per gameplay frame (rigidSolveUs includes all phases)")
            foreach ($m in $measurements | Where-Object { $_.Key -in @('rigidSolveUs','rigidPredictiveContactsUs','rigidUpdateBoundsUs','rigidOverlapPairsUs','rigidContactDispatchUs','rigidIslandsUs','rigidConstraintsUs','rigidIntegrateUs') }) { $report.Add($m.Line) }
            if ('rigidGroupSetupUs' -in $rows[0].PSObject.Properties.Name) {
                $report.Add("Within rigidConstraintsUs per frame (remainder includes island grouping, sorting and dispatch)")
                foreach ($m in $measurements | Where-Object { $_.Key -in @('rigidGroupSetupUs','rigidGroupIterationsUs','rigidGroupFinishUs','rigidConstraintRemainderUs') }) { $report.Add($m.Line) }
                $groups = ($gameplay | Measure-Object -Property rigidGroupIterationsUsCalls -Sum).Sum
                $iterations = ($gameplay | Measure-Object -Property solverVelocityIterations -Sum).Sum
                $report.Add("$iterations velocity iterations across $groups solver group calls. Iteration time also includes split-impulse work.")
            }
            foreach ($r in $gameplay | Where-Object { [int]$_.rigidFullRefreshUsCalls -gt 0 }) {
                $report.Add("Full scenery refresh on frame $($r.serial) took $($r.rigidFullRefreshUs) us. Frame totals: $($r.staticRefsReused) reused and $($r.staticRefsBuilt) built.")
            }
        }
        $report.Add("Largest instrumented scopes by p95 (inclusive, may overlap):")
        foreach ($m in $measurements | Where-Object { $_.Key -notin $primary -and $_.P95 -gt 0 } | Sort-Object P95 -Descending | Select-Object -First 15) { $report.Add($m.Line) }
        $report.Add("Slowest gameplay frames: serial | cell | world | work ms | thread CPU ms | residents | draws | slow object reference (decimal)")
        foreach ($row in $gameplay | Sort-Object { [double]::Parse($_.workUs,[Globalization.CultureInfo]::InvariantCulture) } -Descending | Select-Object -First 10) {
            $threadMs = if ($row.threadCpuUs -and [double]::Parse($row.threadCpuUs,[Globalization.CultureInfo]::InvariantCulture) -ge 0) { [Math]::Round([double]::Parse($row.threadCpuUs,[Globalization.CultureInfo]::InvariantCulture)/1000,3) } else { "unavailable" }
            $report.Add("$($row.serial) | $($row.cell) | $($row.world) | $([Math]::Round([double]::Parse($row.workUs,[Globalization.CultureInfo]::InvariantCulture)/1000,3)) | $threadMs | $($row.residents) | $($row.draws) | $($row.slowObjectRef)")
        }
    } else { $report.Add("No active gameplay frames. Keep the headset awake and the game in focus while recording.") }
    $report.Add("For full phase distributions: python summarize_capture.py <capture.csv>")
    [System.IO.File]::WriteAllLines($Path,[string[]]$report,$utf8)
    Write-Host ($report -join "`n")
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
    $request = [string]$token
    if ($SolverIterations) {
        if ([int]$versionCode -lt 363) { throw "Solver iteration selection requires v363 or newer." }
        $prefix = Join-Path $destination.FullName "$Scene-v$versionCode-i$SolverIterations-$token"
        $request = "${token}:$SolverIterations"
    }
    if ($BufferBindReuse) {
        if ([int]$versionCode -lt 364) { throw "Buffer bind comparison requires v364 or newer." }
        $bindMode = [int]($BufferBindReuse -eq 'On')
        $request = "${request}:binds$bindMode"
        $prefix = $prefix -replace "-$token$", "-binds$bindMode-$token"
    }
    if (!$AllowDebug -and !$versionName.EndsWith("-O2")) {
        throw "Install the FalloutQuest O2 profile APK, or pass -AllowDebug to capture a debug build."
    }

    Add-DeviceSnapshot "before capture"
    $null = Invoke-Adb -Arguments @("shell", "setprop", "debug.falloutquest.perf_capture", $request)
    try {
        $started = $false
        for ($i=0; $i -lt 8; $i++) {
            Start-Sleep -Seconds 1
            $status = Get-CaptureStatus
            if ($status -and $status.state -eq 'recording') { $started = $true; break }
            $events = (Invoke-Adb -Arguments @("logcat", "-d", "-s", "FQPerf:I", "*:S")) -join "`n"
            if ($events -match "CAPTURE_START id=$token\b") { $started = $true; break }
        }
        if (!$started) { throw "No CAPTURE_START received. Keep FalloutQuest in focus and the headset awake." }
        Write-Host "Recording $Seconds seconds in $Scene on v$versionCode. Keep the game in focus."
        # Check once a second so a full bounded buffer can finish early.
        for ($elapsed=0; $elapsed -lt $Seconds; $elapsed++) {
            Start-Sleep -Seconds 1
            $status = Get-CaptureStatus
            if ($status -and $status.state -eq 'complete') { break }
        }
    } finally {
        $null = Invoke-Adb -Arguments @("shell", "setprop", "debug.falloutquest.perf_capture", "0")
    }

    $completion = $null
    for ($i=0; $i -lt 10; $i++) {
        Start-Sleep -Seconds 1
        $status = Get-CaptureStatus
        if ($status -and $status.state -eq 'complete') {
            $completion = [regex]::Match("CAPTURE_END id=$token frames=$($status.frames) saved=$($status.saved) error=$($status.error) reason=$($status.reason)", 'CAPTURE_END.*')
            break
        }
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
    if ($csv.Count -lt 3 -or $header -notmatch '^# format=(233|351)\b' -or $header -notmatch "session=$token\b") {
        throw "The CSV is empty, stale, or from the wrong build."
    }
    $expectedFrames = [int][regex]::Match($completion.Value, '\bframes=(\d+)\b').Groups[1].Value
    if ($csv.Count - 2 -ne $expectedFrames) { throw "The CSV was truncated. Expected $expectedFrames frames, received $($csv.Count - 2)." }
    if (!$AllowDebug -and $header -notmatch '\bnative=profile-O2\b') {
        throw "The native library is not the O2 profile build."
    }
    if ($SolverIterations -and $header -notmatch "\bsolverIterationLimit=$SolverIterations\b") {
        throw "The headset did not confirm the requested solver iteration limit of $SolverIterations."
    }
    if ($BufferBindReuse -and ($header -notmatch '\brenderer=vulkan\b' -or $header -notmatch "\bvkBufferBindReuse=$bindMode\b")) {
        throw "The Vulkan renderer did not confirm buffer bind reuse $BufferBindReuse."
    }
    [System.IO.File]::WriteAllLines("$prefix.csv", [string[]]$csv, $utf8)
    Write-Host $header
    Write-Host $completion.Value
    Write-Host "Saved $prefix.csv"
    Write-CaptureSummary $csv "$prefix-summary.txt"
    Write-Host "Saved $prefix-summary.txt"
} catch {
    $captureError = $_
} finally {
    $log = ""
    if ($script:AdbPath) {
        Add-DeviceSnapshot "after capture"
        try {
            $log = (Invoke-Adb -Arguments @("logcat", "-d", "-v", "threadtime", "-s", "FalloutQuest:I", "FQPerf:I", "AndroidRuntime:E", "libc:F", "DEBUG:F", "*:S")) -join "`n"
        } catch {
            $log = "Could not read headset logcat: $($_.Exception.Message)"
        }
    }
    $status = if ($captureError) { "Capture failed: $($captureError.Exception.Message)" } else { "Capture saved." }
    [System.IO.File]::WriteAllText("$prefix-logcat.txt", "versionCode=$versionCode`nversionName=$versionName`nsession=$token`n$status`n$log`n", $utf8)
    [System.IO.File]::WriteAllLines("$prefix-device.txt", [string[]]$deviceNotes, $utf8)
    Write-Host "Saved device diagnostics to $prefix-device.txt"
    Write-Host "Saved diagnostic log to $prefix-logcat.txt"
}
if ($captureError) { throw $captureError }

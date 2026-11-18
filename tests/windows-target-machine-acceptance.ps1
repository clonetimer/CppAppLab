[CmdletBinding()]
param(
    [string]$Distro,
    [string]$RepoPath = "~/work/CppAppLab",
    [string]$EvidenceDirectory
)

$ErrorActionPreference = "Stop"

function Assert-Condition {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$launcherPath = Join-Path $repositoryRoot "Start-CppAppLab.cmd"
Assert-Condition (Test-Path -LiteralPath $launcherPath -PathType Leaf) "Run this script from a CppAppLab checkout; Start-CppAppLab.cmd is missing."

$os = Get-CimInstance -ClassName Win32_OperatingSystem
$build = [int]$os.BuildNumber
Assert-Condition ($build -ge 22000) "P8-E target acceptance requires Windows 11 (build 22000 or later). Detected: $($os.Caption), build $build."

$wsl = Get-Command -Name "wsl.exe" -ErrorAction SilentlyContinue
Assert-Condition ($null -ne $wsl) "WSL was not found. Install WSL and Ubuntu 22.04/24.04 first."

$wslLines = @(& $wsl.Source --list --verbose 2>&1 | ForEach-Object { ($_.ToString() -replace "\x00", "").Trim() })
Assert-Condition ($LASTEXITCODE -eq 0) "Cannot list WSL distributions. Run 'wsl -l -v' and repair WSL before retrying."

$installed = @()
foreach ($line in $wslLines) {
    if ($line -match '^\*?\s*(?<Name>.+?)\s+(?<State>Running|Stopped)\s+(?<Version>[12])\s*$') {
        $name = $Matches.Name.Trim()
        $version = $Matches.Version
        if ($name -match '^(Ubuntu|Ubuntu[- ]22[.]04|Ubuntu[- ]24[.]04)$') {
            $installed += [pscustomobject]@{ Name = $name; WslVersion = $version }
        }
    }
}
Assert-Condition ($installed.Count -gt 0) "No supported Ubuntu distribution was found. Install Ubuntu 22.04 or 24.04 with WSL2."

if ([string]::IsNullOrWhiteSpace($Distro)) {
    if ($installed.Count -eq 1) {
        $Distro = $installed[0].Name
    } else {
        Write-Host "Select the Ubuntu distribution for target-machine acceptance:"
        for ($i = 0; $i -lt $installed.Count; $i++) {
            Write-Host ("  {0}. {1} (WSL {2})" -f ($i + 1), $installed[$i].Name, $installed[$i].WslVersion)
        }
        $selection = Read-Host "Ubuntu distribution number"
        $number = 0
        Assert-Condition ([int]::TryParse($selection, [ref]$number) -and $number -ge 1 -and $number -le $installed.Count) "Invalid Ubuntu selection."
        $Distro = $installed[$number - 1].Name
    }
}

$selected = @($installed | Where-Object { $_.Name -eq $Distro })
Assert-Condition ($selected.Count -eq 1) "The selected distribution '$Distro' is not in the supported installed Ubuntu list."
Assert-Condition ($selected[0].WslVersion -eq "2") "The selected distribution '$Distro' uses WSL $($selected[0].WslVersion), not WSL2. Convert it with: wsl --set-version $Distro 2"

$ubuntuInfo = @(& $wsl.Source --distribution $Distro --exec bash -lc 'source /etc/os-release && printf "%s|%s" "$ID" "$VERSION_ID"' 2>&1)
Assert-Condition ($LASTEXITCODE -eq 0) "Could not read Ubuntu release information from '$Distro'."
$ubuntuParts = ($ubuntuInfo -join "").Trim() -split "\|"
Assert-Condition ($ubuntuParts.Count -eq 2 -and $ubuntuParts[0] -eq "ubuntu" -and $ubuntuParts[1] -in @("22.04", "24.04")) "Unsupported target distribution. Expected Ubuntu 22.04 or 24.04; received '$($ubuntuInfo -join '')'."

if ([string]::IsNullOrWhiteSpace($EvidenceDirectory)) {
    $EvidenceDirectory = Join-Path $repositoryRoot "artifacts/windows-target-machine"
}
New-Item -ItemType Directory -Path $EvidenceDirectory -Force | Out-Null
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$transcriptPath = Join-Path $EvidenceDirectory "p8e-target-$stamp-transcript.txt"
$evidencePath = Join-Path $EvidenceDirectory "p8e-target-$stamp.json"
$transcriptStarted = $false

Write-Host "P8-E Windows 11 + WSL2 target-machine acceptance"
Write-Host ("Windows: {0} (build {1})" -f $os.Caption, $build)
Write-Host ("Ubuntu: {0} / WSL2" -f $Distro)
Write-Host ("Evidence: {0}" -f $EvidenceDirectory)

try {
    Start-Transcript -Path $transcriptPath -Force | Out-Null
    $transcriptStarted = $true

    Write-Host "[RUN] Invoking the actual Windows double-click launcher entry."
    & $launcherPath -Distro $Distro -RepoPath $RepoPath
    $launcherExit = $LASTEXITCODE
    Assert-Condition ($launcherExit -eq 0) "Windows launcher exited with code $launcherExit. Inspect $transcriptPath."

    $health = Invoke-RestMethod -Uri "http://127.0.0.1:8765/api/v1/health" -Method Get -TimeoutSec 8
    Assert-Condition ($health.status -eq "ok") "Workbench health status is not ok."
    Assert-Condition ($health.service -eq "CppAppLab") "Port 8765 is not serving the CppAppLab service."
    Assert-Condition ([int]$health.task_count -eq 23) "Expected 23 registered learning tasks; received $($health.task_count)."

    $page = Invoke-WebRequest -Uri "http://127.0.0.1:8765/" -UseBasicParsing -TimeoutSec 8
    Assert-Condition ([int]$page.StatusCode -eq 200) "The browser workbench page did not return HTTP 200."
    Assert-Condition ($page.Content -match "CppAppLab") "The served page does not identify CppAppLab."

    Stop-Transcript | Out-Null
    $transcriptStarted = $false
    $transcriptText = Get-Content -LiteralPath $transcriptPath -Raw
    $browserOpened = $transcriptText.Contains("[PASS] Opened the workbench in the Windows default browser.")
    $browserFallback = $transcriptText.Contains("[PASS] Opened the workbench with wslview.")
    Assert-Condition $browserOpened "The Windows default-browser interop was not confirmed. wslview fallback=$browserFallback. Check WSL interop and retry."

    $evidence = [ordered]@{
        schema = "cppapplab.p8e.target-machine.v1"
        result = "PASS"
        timestamp_utc = (Get-Date).ToUniversalTime().ToString("o")
        windows_caption = $os.Caption
        windows_build = $build
        wsl_distribution = $Distro
        wsl_version = 2
        ubuntu_version = $ubuntuParts[1]
        repository_path = $RepoPath
        service_url = "http://127.0.0.1:8765/"
        health_status = $health.status
        service_identity = $health.service
        registered_task_count = [int]$health.task_count
        page_http_status = [int]$page.StatusCode
        windows_default_browser_opened = $browserOpened
        transcript = (Split-Path -Leaf $transcriptPath)
        note = "The workbench is intentionally left running. This test does not stop the service or modify learner workspaces."
    }
    $evidence | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $evidencePath -Encoding UTF8
    Write-Host "[PASS] Windows 11 + WSL2 target-machine browser startup accepted."
    Write-Host ("[PASS] 23-task catalog health and HTTP page verified: {0}" -f $evidencePath)
    Write-Host "[INFO] The workbench remains running to preserve the learner session."
} catch {
    if ($transcriptStarted) {
        Write-Host ("[FAIL] {0}" -f $_.Exception.Message)
        Stop-Transcript | Out-Null
        $transcriptStarted = $false
    }
    throw
}

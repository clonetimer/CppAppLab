[CmdletBinding()]
param(
    [string]$Distro,
    [string]$RepoPath = "~/work/CppAppLab",
    [switch]$Setup
)

$ErrorActionPreference = "Stop"

$wslCommand = Get-Command -Name "wsl.exe" -ErrorAction SilentlyContinue
if ($null -eq $wslCommand) {
    throw "WSL was not found. Install WSL from an elevated PowerShell session with: wsl --install"
}

$rawDistros = @(& $wslCommand.Source --list --quiet 2>&1)
if ($LASTEXITCODE -ne 0) {
    throw "Cannot list WSL distributions. Run 'wsl --status' and repair the WSL installation."
}

$ubuntuDistros = @(
    $rawDistros |
        ForEach-Object { ($_.ToString() -replace "\x00", "").Trim() } |
        Where-Object { $_ -match '^Ubuntu(?:[- ].*)?$' }
)

if ([string]::IsNullOrWhiteSpace($Distro)) {
    if ($ubuntuDistros.Count -eq 0) {
        Write-Host "No Ubuntu distribution was found."
        Write-Host "Install the supported distribution with: wsl --install -d Ubuntu-24.04"
        throw "Install Ubuntu 24.04, complete its first-run setup, then run this launcher again."
    }

    if ($ubuntuDistros.Count -eq 1) {
        $Distro = $ubuntuDistros[0]
    } else {
        Write-Host "Select the Ubuntu distribution to use:"
        for ($index = 0; $index -lt $ubuntuDistros.Count; $index++) {
            Write-Host ("  {0}. {1}" -f ($index + 1), $ubuntuDistros[$index])
        }

        $selection = Read-Host "Ubuntu distribution number"
        $selectedIndex = 0
        $selectionIsNumeric = [int]::TryParse($selection, [ref]$selectedIndex)
        if (-not $selectionIsNumeric -or
            $selectedIndex -lt 1 -or $selectedIndex -gt $ubuntuDistros.Count) {
            throw "Invalid Ubuntu distribution selection."
        }
        $Distro = $ubuntuDistros[$selectedIndex - 1]
    }
} elseif ($ubuntuDistros -notcontains $Distro) {
    throw "The WSL distribution '$Distro' was not found among the installed Ubuntu distributions."
}

$verboseDistros = @(
    & $wslCommand.Source --list --verbose 2>&1 |
        ForEach-Object { ($_.ToString() -replace "\x00", "").Trim() }
)
if ($LASTEXITCODE -ne 0) {
    throw "Cannot inspect WSL distribution versions. Run 'wsl -l -v' manually."
}

$escapedDistro = [regex]::Escape($Distro)
$versionPattern = '^\*?\s*' + $escapedDistro + '\s+\S+\s+(?<Version>\d+)\s*$'
$wslVersion = $null
foreach ($line in $verboseDistros) {
    if ($line -match $versionPattern) {
        $wslVersion = $Matches["Version"]
        break
    }
}

if ($null -eq $wslVersion) {
    throw "Could not determine the WSL version for '$Distro'. Run 'wsl -l -v' and check the distribution name."
}
if ($wslVersion -ne "2") {
    throw ("The '$Distro' distribution uses WSL $wslVersion. Convert it with: wsl --set-version " + $Distro + " 2")
}

if (-not ($RepoPath.StartsWith("~/") -or $RepoPath.StartsWith("/"))) {
    throw "RepoPath must be a Linux path such as '~/work/CppAppLab' or '/home/user/work/CppAppLab'."
}

$repoPathBase64 = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($RepoPath))
$forceSetup = if ($Setup) { "1" } else { "0" }

$bootstrapScriptPath = Join-Path $PSScriptRoot "cppapplab-wsl-launcher.sh"
if (-not (Test-Path -LiteralPath $bootstrapScriptPath -PathType Leaf)) {
    throw "The WSL bootstrap script is missing: $bootstrapScriptPath"
}
$bootstrapScript = [System.IO.File]::ReadAllText($bootstrapScriptPath)
$bashScriptBase64 = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($bootstrapScript))
$bootstrapCommand = "printf '%s' '$bashScriptBase64' | base64 --decode | bash -s -- '$repoPathBase64' '$forceSetup'"

Write-Host "CppAppLab - Windows / WSL2 launcher"
Write-Host ("Distribution: {0}" -f $Distro)
Write-Host ("Linux checkout: {0}" -f $RepoPath)
if ($Setup) {
    Write-Host "Mode: force setup, build and test"
} else {
    Write-Host "Mode: open existing build, or run setup if this is the first launch"
}

& $wslCommand.Source --distribution $Distro --exec bash -lc $bootstrapCommand
$launcherExitCode = $LASTEXITCODE
exit $launcherExitCode

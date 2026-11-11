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

$bashScript = @'
set -Eeuo pipefail

repo_path="$(printf '%s' '__REPO_PATH_BASE64__' | base64 --decode)"
case "$repo_path" in
    "~/"*) repo_path="$HOME/${repo_path#~/}" ;;
    "~") repo_path="$HOME" ;;
esac

if [[ ! -r /etc/os-release ]]; then
    echo "[FAIL] Cannot identify the Ubuntu distribution." >&2
    exit 2
fi
# shellcheck disable=SC1091
source /etc/os-release
if [[ "${ID:-}" != "ubuntu" || "${VERSION_ID:-}" != "24.04" ]]; then
    echo "[FAIL] CppAppLab requires Ubuntu 24.04; detected ${PRETTY_NAME:-unknown}." >&2
    exit 2
fi

if ! command -v git >/dev/null 2>&1; then
    echo "[INFO] Git is missing; installing it inside Ubuntu."
    sudo apt-get update
    sudo apt-get install -y git
fi

if [[ -e "$repo_path" ]] && ! git -C "$repo_path" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "[FAIL] The target path exists but is not a Git checkout: $repo_path" >&2
    echo "No existing files were changed. Choose another path with -RepoPath." >&2
    exit 3
fi

if ! git -C "$repo_path" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    mkdir -p "$(dirname "$repo_path")"
    echo "[INFO] Cloning the private CppAppLab repository into $repo_path"
    if ! git clone https://github.com/clonetimer/CppAppLab.git "$repo_path"; then
        echo "[FAIL] Git clone failed. Authenticate Git inside WSL (for example, with 'gh auth login') and retry." >&2
        exit 5
    fi
fi

remote_url="$(git -C "$repo_path" remote get-url origin 2>/dev/null | tr '[:upper:]' '[:lower:]' || true)"
case "$remote_url" in
    https://github.com/clonetimer/cppapplab.git|https://github.com/clonetimer/cppapplab|git@github.com:clonetimer/cppapplab.git|ssh://git@github.com/clonetimer/cppapplab.git)
        ;;
    *)
        echo "[FAIL] The checkout at $repo_path is not the clonetimer/CppAppLab repository." >&2
        echo "No files were changed. Set -RepoPath to the correct Linux checkout." >&2
        exit 6
        ;;
esac

cd "$repo_path"
if [[ "__FORCE_SETUP__" == "1" || ! -x build/cppapplab ]]; then
    echo "[INFO] Running first-time setup/build/test."
    exec bash scripts/quickstart.sh
fi

echo "[INFO] Opening the existing CppAppLab build."
echo "[INFO] No automatic git pull is performed; local changes are preserved."
exec ./build/cppapplab
'@

$bashScript = $bashScript.Replace("__REPO_PATH_BASE64__", $repoPathBase64)
$bashScript = $bashScript.Replace("__FORCE_SETUP__", $forceSetup)
$bashScriptBase64 = [Convert]::ToBase64String([System.Text.Encoding]::UTF8.GetBytes($bashScript))
$bootstrapCommand = 'bash -c "$(printf ''%s'' ''' + $bashScriptBase64 + ''' | base64 --decode)"'

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

param(
    [string]$Binary = "",
    [string]$Python = "python.exe",
    [int]$Port = 8765
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not $Binary) { $Binary = Join-Path $root 'build\cppapplab.exe' }
$Binary = (Resolve-Path -LiteralPath $Binary).Path
if ($Port -lt 1 -or $Port -gt 65535) { throw 'Port must be in 1..65535.' }
$pythonCommand = Get-Command $Python -CommandType Application -ErrorAction Stop
$server = Join-Path $root 'web\server\cppapplab_web.py'
if (-not (Test-Path -LiteralPath $server -PathType Leaf)) { throw 'Web server is missing.' }
# This source-build launcher does not install tools, invoke WSL, modify PATH
# globally, or enable Developer Mode. Preserve the caller's native toolchain.
& $pythonCommand.Source $server --root $root --cppapplab $Binary --host 127.0.0.1 --port $Port
exit $LASTEXITCODE

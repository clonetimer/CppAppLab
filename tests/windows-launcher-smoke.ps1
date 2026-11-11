[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$launcherPath = Join-Path $repositoryRoot "scripts/Start-CppAppLab.ps1"
$stubProject = Join-Path $env:RUNNER_TEMP "CppAppLabWslStub"
$stubOutput = Join-Path $env:RUNNER_TEMP "CppAppLabWslStubOutput"
$capturedArgsPath = Join-Path $env:RUNNER_TEMP "CppAppLabWslStubArgs.txt"

if (Test-Path -LiteralPath $stubProject) {
    Remove-Item -LiteralPath $stubProject -Recurse -Force
}
if (Test-Path -LiteralPath $stubOutput) {
    Remove-Item -LiteralPath $stubOutput -Recurse -Force
}

dotnet new console --framework net8.0 --output $stubProject --no-restore --force | Out-Null

$stubSource = @'
using System;
using System.IO;

internal static class Program
{
    private static int Main(string[] args)
    {
        if (args.Length == 2 && args[0] == "--list" && args[1] == "--quiet")
        {
            Console.WriteLine("Ubuntu-24.04");
            return 0;
        }

        if (args.Length == 2 && args[0] == "--list" && args[1] == "--verbose")
        {
            Console.WriteLine("  NAME            STATE           VERSION");
            Console.WriteLine("* Ubuntu-24.04    Stopped         2");
            return 0;
        }

        string capturePath = Environment.GetEnvironmentVariable("WSL_STUB_ARGS_PATH");
        if (args.Length >= 1 && args[0] == "--distribution" && !String.IsNullOrEmpty(capturePath))
        {
            File.WriteAllLines(capturePath, args);
            return 0;
        }

        Console.Error.WriteLine("Unexpected wsl.exe invocation: " + String.Join(" ", args));
        return 90;
    }
}
'@

Set-Content -LiteralPath (Join-Path $stubProject "Program.cs") -Value $stubSource -Encoding utf8
dotnet publish $stubProject -c Release -p:AssemblyName=wsl -o $stubOutput --nologo

$env:PATH = "$stubOutput;$env:PATH"
$env:WSL_STUB_ARGS_PATH = $capturedArgsPath

& $launcherPath -Distro "Ubuntu-24.04" -RepoPath "~/work/Cpp App Lab" -Setup
$launcherExitCode = $LASTEXITCODE
if ($launcherExitCode -ne 0) {
    throw "The launcher did not propagate the successful WSL stub exit code. Actual code: $launcherExitCode"
}

if (-not (Test-Path -LiteralPath $capturedArgsPath -PathType Leaf)) {
    throw "The launcher did not invoke the WSL command stub."
}

$capturedArgs = @(Get-Content -LiteralPath $capturedArgsPath)
if ($capturedArgs.Count -ne 6) {
    throw "Expected six WSL command arguments, received $($capturedArgs.Count)."
}

$expectedPrefix = @("--distribution", "Ubuntu-24.04", "--exec", "bash", "-lc")
for ($index = 0; $index -lt $expectedPrefix.Count; $index++) {
    if ($capturedArgs[$index] -ne $expectedPrefix[$index]) {
        throw "Unexpected WSL argument at index $index. Expected '$($expectedPrefix[$index])', received '$($capturedArgs[$index])'."
    }
}

$command = $capturedArgs[5]
$match = [regex]::Match(
    $command,
    "^printf '%s' '([^']+)' \| base64 --decode \| bash -s -- '([^']+)' '1'$"
)
if (-not $match.Success) {
    throw "The WSL bootstrap command was not passed as one safely quoted command argument."
}

$bootstrap = [System.Text.Encoding]::UTF8.GetString(
    [Convert]::FromBase64String($match.Groups[1].Value)
)
$repoPath = [System.Text.Encoding]::UTF8.GetString(
    [Convert]::FromBase64String($match.Groups[2].Value)
)

if ($repoPath -ne "~/work/Cpp App Lab") {
    throw "The Linux checkout path was not preserved through the Windows-to-WSL handoff."
}
if ($bootstrap -notmatch "normalize_repo_path" -or
    $bootstrap -notmatch "git clone https://github.com/clonetimer/CppAppLab.git") {
    throw "The expected WSL bootstrap content was not transferred."
}

Write-Host "Windows-to-WSL command handoff: PASS"
Write-Host "Distribution detection: Ubuntu-24.04 / WSL2"
Write-Host "Linux path transport: ~/work/Cpp App Lab"
Write-Host "Setup mode transport: 1"

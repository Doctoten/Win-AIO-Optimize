#requires -Version 5.1
[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$Binary)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$binaryPath = (Resolve-Path -LiteralPath $Binary).Path
& (Join-Path $PSScriptRoot 'verify-icon.ps1') -Binary $binaryPath -RefreshExplorer
$stream = [IO.File]::OpenRead($binaryPath)
$sha256 = [Security.Cryptography.SHA256]::Create()
try {
    $hash = [BitConverter]::ToString($sha256.ComputeHash($stream)).Replace('-', '').ToLowerInvariant()
} finally {
    $stream.Dispose()
    $sha256.Dispose()
}
$name = [IO.Path]::GetFileName($binaryPath)
[IO.File]::WriteAllText("$binaryPath.sha256", "$hash  $name`r`n", [Text.Encoding]::ASCII)
Write-Host "SHA256: $binaryPath.sha256"

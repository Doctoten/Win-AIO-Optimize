#requires -Version 5.1
<#
Downloads the native C release, verifies its SHA256, and opens the application.
This bootstrapper never applies Windows optimizations. All changes require consent in the UI.
#>
[CmdletBinding()]
param(
    [ValidatePattern('^6\.\d+\.\d+$')]
    [string]$Version = '6.0.1',
    [switch]$DownloadOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ($env:OS -ne 'Windows_NT' -or -not [Environment]::Is64BitOperatingSystem) {
    throw 'This release requires 64-bit Windows 10 or Windows 11.'
}

[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
$releaseBase = "https://github.com/Doctoten/Windows-utility-tool/releases/download/v$Version"
$fileName = 'win_aio_optimize.exe'
$downloadDirectory = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'WinAIOOptimize\Downloads'
$stagingDirectory = Join-Path $downloadDirectory ([Guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $stagingDirectory -Force
$downloadPath = Join-Path $stagingDirectory $fileName
$checksumPath = "$downloadPath.sha256"

try {
    Write-Host "Downloading Win AIO Optimize $Version..."
    Invoke-WebRequest -UseBasicParsing -Uri "$releaseBase/$fileName" -OutFile $downloadPath
    Invoke-WebRequest -UseBasicParsing -Uri "$releaseBase/$fileName.sha256" -OutFile $checksumPath
    $checksumText = (Get-Content -LiteralPath $checksumPath -Raw).Trim()
    if ($checksumText -notmatch '^(?<hash>[0-9a-fA-F]{64})[ \t]+\*?win_aio_optimize\.exe$') {
        throw 'The release checksum file is invalid. The application will not run.'
    }
    $expectedHash = $Matches['hash']
    $actualHash = (Get-FileHash -LiteralPath $downloadPath -Algorithm SHA256).Hash
    if ($actualHash -ine $expectedHash) {
        throw 'SHA256 verification failed. The application will not run.'
    }
    $fileVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($downloadPath).ProductVersion
    if ($fileVersion -ne $Version) {
        throw "Unexpected executable version '$fileVersion'; expected '$Version'."
    }
    Write-Host "Verified: $downloadPath"
    if (-not $DownloadOnly) {
        Start-Process -FilePath $downloadPath
        Write-Host 'The application will ask for administrator access. Choose and confirm each optimization in its UI.'
    }
} catch {
    # Remove only our two explicitly named partial downloads, in this unique staging directory.
    if (Test-Path -LiteralPath $downloadPath) { Remove-Item -LiteralPath $downloadPath -Force }
    if (Test-Path -LiteralPath $checksumPath) { Remove-Item -LiteralPath $checksumPath -Force }
    throw "Download failed. Publish release v$Version with both $fileName and $fileName.sha256, or build from source. $($_.Exception.Message)"
}

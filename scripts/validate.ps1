#requires -Version 5.1
<# Read-only validation: never launches the normal app or applies system changes. #>
[CmdletBinding()]
param(
    [string]$Binary,
    [switch]$Inventory,
    [switch]$SourceOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $Binary) { $Binary = Join-Path $repoRoot 'dist\win_aio_optimize.exe' }
$utf8 = New-Object Text.UTF8Encoding($false, $true)
$sourceFiles = @(
    Get-ChildItem -LiteralPath (Join-Path $repoRoot 'src') -File |
        Where-Object { $_.Extension -in '.c', '.h', '.rc', '.manifest' }
    Get-ChildItem -LiteralPath (Join-Path $repoRoot 'scripts') -File -Filter '*.ps1'
    Get-ChildItem -LiteralPath (Join-Path $repoRoot 'docs') -File -Filter '*.md'
    Get-Item -LiteralPath (Join-Path $repoRoot 'README.md'), (Join-Path $repoRoot 'TESTING.md'),
        (Join-Path $repoRoot 'install.ps1'), (Join-Path $repoRoot 'CMakeLists.txt'),
        (Join-Path $repoRoot '.github\workflows\build.yml')
)
foreach ($file in $sourceFiles) {
    $null = $utf8.GetString([IO.File]::ReadAllBytes($file.FullName))
    if ($file.Extension -eq '.ps1') {
        $tokens = $null
        $parseErrors = $null
        $null = [Management.Automation.Language.Parser]::ParseFile($file.FullName, [ref]$tokens, [ref]$parseErrors)
        if ($parseErrors.Count) { throw "PowerShell syntax error in $($file.Name): $parseErrors" }
    }
}
$cmake = [IO.File]::ReadAllText((Join-Path $repoRoot 'CMakeLists.txt'), $utf8)
if ($cmake -notmatch 'project\(WinAIOOptimize VERSION (\d+\.\d+\.\d+)') { throw 'Missing project version.' }
$version = $Matches[1]
$versionEscaped = [regex]::Escape($version)
$header = [IO.File]::ReadAllText((Join-Path $repoRoot 'src\app.h'), $utf8)
if ($header -notmatch ('#define APP_VERSION L"' + $versionEscaped + '"')) { throw 'C/header version mismatch.' }
$resource = [IO.File]::ReadAllText((Join-Path $repoRoot 'src\app.rc'), $utf8)
$resourceVersion = $version.Replace('.', ',') + ',0'
foreach ($entry in 'FILEVERSION', 'PRODUCTVERSION') {
    if ($resource -notmatch ($entry + '\s+' + [regex]::Escape($resourceVersion) + '\b')) { throw "$entry mismatch." }
}
foreach ($entry in 'FileVersion', 'ProductVersion') {
    $values = [regex]::Matches($resource, ('VALUE "' + $entry + '", "([^"\\]+)\\0"'))
    if ($values.Count -ne 2) { throw "Missing bilingual $entry metadata." }
    foreach ($value in $values) { if ($value.Groups[1].Value -ne $version) { throw "$entry mismatch." } }
}
[xml]$manifest = [IO.File]::ReadAllText((Join-Path $repoRoot 'src\app.manifest'), $utf8)
if ($manifest.assembly.assemblyIdentity.version -ne "$version.0") { throw 'Manifest version mismatch.' }
$bootstrap = [IO.File]::ReadAllText((Join-Path $repoRoot 'install.ps1'), $utf8)
if ($bootstrap -notmatch ('\[string\]\$Version = ''' + $versionEscaped + '''')) { throw 'Download version mismatch.' }
Write-Host "Source encoding, PowerShell syntax and version $version consistency: PASS"
if ($SourceOnly) { return }

$binaryPath = (Resolve-Path -LiteralPath $Binary).Path
$metadata = [Diagnostics.FileVersionInfo]::GetVersionInfo($binaryPath)
if ($metadata.ProductName -ne 'Win AIO Optimize' -or $metadata.ProductVersion -ne $version) {
    throw 'Executable product/version metadata does not match source.'
}
$checksum = [IO.File]::ReadAllText("$binaryPath.sha256").Trim()
$stream = [IO.File]::OpenRead($binaryPath)
$sha256 = [Security.Cryptography.SHA256]::Create()
try {
    $hash = [BitConverter]::ToString($sha256.ComputeHash($stream)).Replace('-', '').ToLowerInvariant()
} finally {
    $stream.Dispose()
    $sha256.Dispose()
}
$expected = $hash + '  ' + [IO.Path]::GetFileName($binaryPath)
if ($checksum -cne $expected) { throw 'Executable checksum is missing or stale. Rebuild before validation.' }
Write-Host 'Executable metadata and SHA256: PASS'
& (Join-Path $PSScriptRoot 'verify-icon.ps1') -Binary $binaryPath

$checks = @(
    @{ Name = 'self-test'; Arguments = @('--self-test'); Timeout = 45 }
    @{ Name = 'english-light'; Arguments = @('--smoke-test', '--lang=en', '--theme=light'); Timeout = 45 }
    @{ Name = 'english-dark'; Arguments = @('--smoke-test', '--lang=en', '--theme=dark'); Timeout = 45 }
    @{ Name = 'vietnamese-light'; Arguments = @('--smoke-test', '--lang=vi', '--theme=light'); Timeout = 45 }
    @{ Name = 'vietnamese-dark'; Arguments = @('--smoke-test', '--lang=vi', '--theme=dark'); Timeout = 45 }
)
if ($Inventory) { $checks += @{ Name = 'inventory'; Arguments = @('--inventory-test'); Timeout = 120 } }
$logDirectory = Join-Path $repoRoot ('build-native\validation-' + [Guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $logDirectory
foreach ($check in $checks) {
    $outputPath = Join-Path $logDirectory ($check.Name + '.out.log')
    $errorPath = Join-Path $logDirectory ($check.Name + '.err.log')
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = $binaryPath
    $start.Arguments = $check.Arguments -join ' ' # Fixed diagnostic flags only, never user-supplied command text.
    $start.WorkingDirectory = $repoRoot
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $process = [Diagnostics.Process]::Start($start)
    $outputTask = $process.StandardOutput.ReadToEndAsync()
    $errorTask = $process.StandardError.ReadToEndAsync()
    try {
        $timedOut = -not $process.WaitForExit($check.Timeout * 1000)
        if ($timedOut) {
            $process.Kill()
            $process.WaitForExit()
        }
        $outputText = $outputTask.GetAwaiter().GetResult()
        $errorText = $errorTask.GetAwaiter().GetResult()
        [IO.File]::WriteAllText($outputPath, $outputText, $utf8)
        [IO.File]::WriteAllText($errorPath, $errorText, $utf8)
        if ($outputText) { Write-Host $outputText.TrimEnd() }
        if ($errorText) { Write-Host $errorText.TrimEnd() }
        if ($timedOut) { throw "$($check.Name) exceeded $($check.Timeout) seconds. Logs: $logDirectory" }
        if ($process.ExitCode -ne 0) { throw "$($check.Name) failed with exit code $($process.ExitCode). Logs: $logDirectory" }
        Write-Host "$($check.Name): PASS"
    } finally {
        $process.Dispose()
    }
}
Write-Host "All read-only checks passed. Logs: $logDirectory"

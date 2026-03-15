#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Unregisters the TSF IME from the system (calls regsvr32 /u).
.PARAMETER DllPath
    Full path to MyIME.dll. Defaults to ..\build\x64-release\src\Release\MyIME.dll.
#>
param(
    [string]$DllPath = (Join-Path $PSScriptRoot "..\build\x64-release\src\Release\MyIME.dll")
)

$resolved = Resolve-Path $DllPath -ErrorAction SilentlyContinue
if (-not $resolved) {
    Write-Error "DLL not found: $DllPath"
    exit 1
}

Write-Host "Unregistering IME: $resolved"
& regsvr32.exe /s /u "$resolved"

if ($LASTEXITCODE -eq 0) {
    Write-Host "Unregistration successful!" -ForegroundColor Green
} else {
    Write-Error "Unregistration failed. regsvr32 returned: $LASTEXITCODE"
    exit $LASTEXITCODE
}

#Requires -RunAsAdministrator
<#
.SYNOPSIS
    Registers the TSF IME with the system (calls regsvr32).
.PARAMETER DllPath
    Full path to MyIME.dll. Defaults to ..\build\x64-release\src\Release\MyIME.dll.
#>
param(
    [string]$DllPath = (Join-Path $PSScriptRoot "..\build\x64-release\src\Release\MyIME.dll")
)

$resolved = Resolve-Path $DllPath -ErrorAction SilentlyContinue
if (-not $resolved) {
    Write-Error "DLL not found: $DllPath`nPlease run cmake --build first."
    exit 1
}

Write-Host "Registering IME: $resolved"
& regsvr32.exe /s "$resolved"

if ($LASTEXITCODE -eq 0) {
    Write-Host "Registration successful!" -ForegroundColor Green
} else {
    Write-Error "Registration failed. regsvr32 returned: $LASTEXITCODE"
    exit $LASTEXITCODE
}

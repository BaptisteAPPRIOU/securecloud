#!/usr/bin/env pwsh
# SecureCloud Qt Client Development Launcher
# Loads environment variables and starts the Qt client
# Run from project root: .\client\qt-app\run-dev.ps1

param(
    [string]$Environment = "dev",
    [switch]$Help
)

$ErrorActionPreference = "Stop"
$ServiceName = "Qt Client"
$BinaryPath = "build\dev\client\qt-app\MSF_Login.exe"
$EnvFile = "config\env\$Environment\.env"
$WorkingDir = "client\qt-app"

Write-Host "=== SecureCloud $ServiceName Dev Launcher ===" -ForegroundColor Cyan
Write-Host "Environment: $Environment" -ForegroundColor Gray

if ($Help) {
    Write-Host "Usage: .\client\qt-app\run-dev.ps1 [-Environment <dev|prod>] [-Help]"
    Write-Host "  -Environment  Environment to use (default: dev)"
    Write-Host "  -Help         Show this help message"
    exit 0
}

# Detect project root (script is in client/qt-app/)
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $ScriptDir)

# Change to project root for consistent paths
Push-Location $ProjectRoot

try {
    # Check if .env file exists (optional for client, but load if present)
    if (Test-Path $EnvFile) {
        Write-Host "Loading environment from $EnvFile..." -ForegroundColor Green
        Get-Content $EnvFile | ForEach-Object {
            if ($_ -match '^([^#][^=]*)=(.*)$') {
                $name = $matches[1].Trim()
                $value = $matches[2].Trim()
                [Environment]::SetEnvironmentVariable($name, $value, "Process")
            }
        }
    }

    # TLS defaults: production-like verification (no implicit trust bypass).
    if (-not [Environment]::GetEnvironmentVariable("SECURECLOUD_DEV_ALLOW_SELF_SIGNED", "Process")) {
        [Environment]::SetEnvironmentVariable("SECURECLOUD_DEV_ALLOW_SELF_SIGNED", "false", "Process")
    }
    if (-not [Environment]::GetEnvironmentVariable("SECURECLOUD_TLS_CA_FILE", "Process")) {
        $DefaultCaPath = Join-Path $ProjectRoot "ops\certs\gateway\ca.crt"
        if (Test-Path $DefaultCaPath) {
            [Environment]::SetEnvironmentVariable("SECURECLOUD_TLS_CA_FILE", $DefaultCaPath, "Process")
            Write-Host "TLS CA configured: $DefaultCaPath" -ForegroundColor Green
        }
        else {
            Write-Host "TLS CA not set (SECURECLOUD_TLS_CA_FILE). Using system trust store only." -ForegroundColor Yellow
        }
    }

    # Check if binary exists
    if (!(Test-Path $BinaryPath)) {
        Write-Host "ERROR: Binary not found: $BinaryPath" -ForegroundColor Red
        Write-Host "Build first: cmake --build build/dev --target MSF_Login" -ForegroundColor Yellow
        exit 1
    }

    # Add MSYS2 to PATH (for Qt DLLs)
    $env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"

    Write-Host ""
    Write-Host "Starting $ServiceName..." -ForegroundColor Green
    Write-Host ""

    # Change to client directory and run
    Set-Location $WorkingDir
    & "..\..\$BinaryPath"
}
finally {
    Pop-Location
}

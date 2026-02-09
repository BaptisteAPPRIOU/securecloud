#!/usr/bin/env pwsh
# SecureCloud Gateway Development Launcher
# Loads environment variables and starts the gateway
# Run from project root: .\gateway\run-dev.ps1

param(
    [string]$Environment = "dev",
    [switch]$Help
)

$ErrorActionPreference = "Stop"
$ServiceName = "Gateway"
$BinaryPath = "build\dev\gateway\gateway.exe"
$EnvFile = "config\env\$Environment\.env"
$WorkingDir = "gateway"

Write-Host "=== SecureCloud $ServiceName Dev Launcher ===" -ForegroundColor Cyan
Write-Host "Environment: $Environment" -ForegroundColor Gray

if ($Help) {
    Write-Host "Usage: .\gateway\run-dev.ps1 [-Environment <dev|prod>] [-Help]"
    Write-Host "  -Environment  Environment to use (default: dev)"
    Write-Host "  -Help         Show this help message"
    exit 0
}

# Detect project root (script is in gateway/)
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir

# Change to project root for consistent paths
Push-Location $ProjectRoot

try {
    # Check if .env file exists
    if (!(Test-Path $EnvFile)) {
        Write-Host "ERROR: Environment file not found: $EnvFile" -ForegroundColor Red
        Write-Host "The central .env file should exist at config/env/dev/.env" -ForegroundColor Yellow
        Write-Host "Check that you're in the project root directory." -ForegroundColor Yellow
        exit 1
    }

    # Check if binary exists
    if (!(Test-Path $BinaryPath)) {
        Write-Host "ERROR: Binary not found: $BinaryPath" -ForegroundColor Red
        Write-Host "Build first: cmake --build build/dev --target gateway" -ForegroundColor Yellow
        exit 1
    }

    # Add MSYS2 to PATH
    $env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"

    # Load environment variables from .env file
    Write-Host "Loading environment from $EnvFile..." -ForegroundColor Green
    Get-Content $EnvFile | ForEach-Object {
        if ($_ -match '^([^#][^=]*)=(.*)$') {
            $name = $matches[1].Trim()
            $value = $matches[2].Trim()
            [Environment]::SetEnvironmentVariable($name, $value, "Process")
        }
    }

    Write-Host ""
    Write-Host "Starting $ServiceName..." -ForegroundColor Green
    Write-Host "Press Ctrl+C to stop" -ForegroundColor Yellow
    Write-Host ""

    # Change to service directory and run
    Set-Location $WorkingDir
    & "..\$BinaryPath"
}
finally {
    Pop-Location
}
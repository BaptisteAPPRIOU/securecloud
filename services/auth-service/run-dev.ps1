#!/usr/bin/env pwsh
# SecureCloud Auth Service Development Launcher
# Loads environment variables and starts the auth service
# Run from project root: .\services\auth-service\run-dev.ps1

param(
    [string]$Environment = "dev",
    [switch]$Help
)

$ErrorActionPreference = "Stop"
$ServiceName = "Auth Service"
$BinaryPath = "build\dev\services\auth-service\auth-service.exe"
$EnvFile = "config\env\$Environment\.env"
$WorkingDir = "services\auth-service"

Write-Host "=== SecureCloud $ServiceName Dev Launcher ===" -ForegroundColor Cyan
Write-Host "Environment: $Environment" -ForegroundColor Gray

if ($Help) {
    Write-Host "Usage: .\services\auth-service\run-dev.ps1 [-Environment <dev|prod>] [-Help]"
    Write-Host "  -Environment  Environment to use (default: dev)"
    Write-Host "  -Help         Show this help message"
    exit 0
}

# Detect project root (script is in services/auth-service/)
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $ScriptDir)

# Change to project root for consistent paths
Push-Location $ProjectRoot

try {
    # Check if .env file exists
    if (!(Test-Path $EnvFile)) {
        $fallbackEnv = ".env"
        if (Test-Path $fallbackEnv) {
            Write-Host "Env file not found at $EnvFile, using $fallbackEnv" -ForegroundColor Yellow
            $EnvFile = $fallbackEnv
        } else {
            Write-Host "ERROR: Environment file not found: $EnvFile" -ForegroundColor Red
            Write-Host "Expected config/env/dev/.env or .env at repo root." -ForegroundColor Yellow
            exit 1
        }
    }

    # Check if binary exists
    if (!(Test-Path $BinaryPath)) {
        Write-Host "ERROR: Binary not found: $BinaryPath" -ForegroundColor Red
        Write-Host "Build first: cmake --build build/dev --target auth-service" -ForegroundColor Yellow
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
    & "..\..\$BinaryPath"
}
finally {
    Pop-Location
}

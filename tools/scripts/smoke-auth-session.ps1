#!/usr/bin/env pwsh
# SecureCloud E2E smoke test: login -> refresh -> logout -> reuse old access token.

param(
    [string]$GatewayBaseUrl = "http://localhost:8443",
    [string]$Email = "",
    [string]$Password = "",
    [string]$ProtectedPath = "/api/me",
    [switch]$ShowBodies
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($Email)) {
    if (-not [string]::IsNullOrWhiteSpace($env:SMOKE_EMAIL)) {
        $Email = $env:SMOKE_EMAIL
    } else {
        $Email = "admin@demo.local"
    }
}

if ([string]::IsNullOrWhiteSpace($Password)) {
    if (-not [string]::IsNullOrWhiteSpace($env:SMOKE_PASSWORD)) {
        $Password = $env:SMOKE_PASSWORD
    } else {
        $Password = "admin1234"
    }
}

if (-not $ProtectedPath.StartsWith("/")) {
    $ProtectedPath = "/$ProtectedPath"
}

$gateway = $GatewayBaseUrl.TrimEnd("/")
$supportsSkipCertCheck = (Get-Command Invoke-WebRequest).Parameters.ContainsKey("SkipCertificateCheck")
$supportsSkipHttpErrorCheck = (Get-Command Invoke-WebRequest).Parameters.ContainsKey("SkipHttpErrorCheck")

if (-not $supportsSkipHttpErrorCheck) {
    Write-Error "PowerShell 7+ is required (Invoke-WebRequest -SkipHttpErrorCheck is missing)."
}

$results = [System.Collections.Generic.List[object]]::new()
$failures = [System.Collections.Generic.List[string]]::new()

function Invoke-JsonRequest {
    param(
        [Parameter(Mandatory = $true)]
        [ValidateSet("GET", "POST", "PUT", "PATCH", "DELETE")]
        [string]$Method,
        [Parameter(Mandatory = $true)]
        [string]$Url,
        [hashtable]$Headers = @{},
        [object]$Body = $null
    )

    $params = @{
        Method = $Method
        Uri = $Url
        Headers = $Headers
        SkipHttpErrorCheck = $true
    }

    if ($supportsSkipCertCheck) {
        $params.SkipCertificateCheck = $true
    }

    if ($null -ne $Body) {
        $params.ContentType = "application/json"
        $params.Body = ($Body | ConvertTo-Json -Depth 8 -Compress)
    }

    try {
        $resp = Invoke-WebRequest @params
    } catch {
        throw "HTTP request failed for $Method $Url : $($_.Exception.Message)"
    }
    $json = $null

    if (-not [string]::IsNullOrWhiteSpace($resp.Content)) {
        try {
            $json = $resp.Content | ConvertFrom-Json
        } catch {
            $json = $null
        }
    }

    return [pscustomobject]@{
        Status = [int]$resp.StatusCode
        Body = $resp.Content
        Json = $json
    }
}

function Add-Check {
    param(
        [string]$Step,
        [int]$Status,
        [string]$Expected,
        [bool]$Passed,
        [string]$Body = ""
    )

    $results.Add([pscustomobject]@{
        Step = $Step
        Status = $Status
        Expected = $Expected
        Result = $(if ($Passed) { "PASS" } else { "FAIL" })
    })

    if (-not $Passed) {
        if ([string]::IsNullOrWhiteSpace($Body)) {
            $failures.Add("$Step failed (status=$Status expected=$Expected).")
        } else {
            $failures.Add("$Step failed (status=$Status expected=$Expected body=$Body).")
        }
    }
}

Write-Host "=== SecureCloud Auth Session Smoke Test ===" -ForegroundColor Cyan
Write-Host "Gateway: $gateway"
Write-Host "Protected endpoint: $ProtectedPath"
Write-Host "User: $Email"
Write-Host ""

# 0) Health check
$healthResp = Invoke-JsonRequest -Method GET -Url "$gateway/health"
$healthOk = ($healthResp.Status -eq 200)
Add-Check -Step "0.health" -Status $healthResp.Status -Expected "200" -Passed $healthOk -Body $healthResp.Body

# 1) Login
$loginResp = Invoke-JsonRequest -Method POST -Url "$gateway/api/login" -Body @{
    email = $Email
    password = $Password
}
$loginOk = ($loginResp.Status -eq 200)
Add-Check -Step "1.login" -Status $loginResp.Status -Expected "200" -Passed $loginOk -Body $loginResp.Body

$accessToken = ""
$refreshToken = ""

if ($loginOk -and $null -ne $loginResp.Json) {
    $accessToken = [string]$loginResp.Json.access_token
    $refreshToken = [string]$loginResp.Json.refresh_token
}

$tokensOk = (-not [string]::IsNullOrWhiteSpace($accessToken)) -and (-not [string]::IsNullOrWhiteSpace($refreshToken))
Add-Check -Step "1b.tokens_present" -Status $(if ($tokensOk) { 200 } else { 0 }) -Expected "access+refresh token present" -Passed $tokensOk -Body $loginResp.Body

# 2) Use access token before logout (must not be rejected as unauthorized)
$preLogoutStatus = 0
$preLogoutBody = ""
if ($tokensOk) {
    $preLogoutResp = Invoke-JsonRequest -Method GET -Url "$gateway$ProtectedPath" -Headers @{
        Authorization = "Bearer $accessToken"
    }
    $preLogoutStatus = $preLogoutResp.Status
    $preLogoutBody = $preLogoutResp.Body
}
$preLogoutOk = ($preLogoutStatus -ne 401) -and ($preLogoutStatus -ne 0)
Add-Check -Step "2.pre_logout_access" -Status $preLogoutStatus -Expected "not 401" -Passed $preLogoutOk -Body $preLogoutBody

# 3) Refresh
$refreshStatus = 0
$refreshBody = ""
$refreshOk = $false
if ($tokensOk) {
    $refreshResp = Invoke-JsonRequest -Method POST -Url "$gateway/api/refresh" -Body @{
        refresh_token = $refreshToken
    }
    $refreshStatus = $refreshResp.Status
    $refreshBody = $refreshResp.Body
    $refreshOk = ($refreshStatus -eq 200)
}
Add-Check -Step "3.refresh" -Status $refreshStatus -Expected "200" -Passed $refreshOk -Body $refreshBody

# 4) Logout (send both access and refresh token)
$logoutStatus = 0
$logoutBody = ""
$logoutOk = $false
if ($tokensOk) {
    $logoutResp = Invoke-JsonRequest -Method POST -Url "$gateway/api/logout" -Headers @{
        Authorization = "Bearer $accessToken"
    } -Body @{
        refresh_token = $refreshToken
    }
    $logoutStatus = $logoutResp.Status
    $logoutBody = $logoutResp.Body
    $logoutOk = ($logoutStatus -eq 200)
}
Add-Check -Step "4.logout" -Status $logoutStatus -Expected "200" -Passed $logoutOk -Body $logoutBody

# 5) Reuse old access token (must be rejected)
$postLogoutStatus = 0
$postLogoutBody = ""
$postLogoutOk = $false
if ($tokensOk) {
    $postLogoutResp = Invoke-JsonRequest -Method GET -Url "$gateway$ProtectedPath" -Headers @{
        Authorization = "Bearer $accessToken"
    }
    $postLogoutStatus = $postLogoutResp.Status
    $postLogoutBody = $postLogoutResp.Body
    $postLogoutOk = ($postLogoutStatus -eq 401)
}
Add-Check -Step "5.reuse_old_access" -Status $postLogoutStatus -Expected "401" -Passed $postLogoutOk -Body $postLogoutBody

Write-Host ""
($results | Format-Table -AutoSize | Out-String).TrimEnd() | Write-Host
Write-Host ""

if ($ShowBodies) {
    Write-Host "Response bodies:" -ForegroundColor DarkCyan
    Write-Host "health: $($healthResp.Body)"
    Write-Host "login: $($loginResp.Body)"
    if ($tokensOk) {
        Write-Host "pre_logout_access: $preLogoutBody"
        Write-Host "refresh: $refreshBody"
        Write-Host "logout: $logoutBody"
        Write-Host "reuse_old_access: $postLogoutBody"
    }
    Write-Host ""
}

if ($failures.Count -gt 0) {
    Write-Host "Smoke test failed:" -ForegroundColor Red
    foreach ($failure in $failures) {
        Write-Host " - $failure" -ForegroundColor Red
    }
    exit 1
}

Write-Host "Smoke test passed: login -> refresh -> logout -> old access token rejected." -ForegroundColor Green

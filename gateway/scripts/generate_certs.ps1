# Generate self-signed certificates for development/testing
# DO NOT use these certificates in production!
# Requires OpenSSL to be installed and in PATH

$ErrorActionPreference = "Stop"

$CERT_DIR = "config\certs"
$DAYS_VALID = 365

Write-Host "Creating certificate directory: $CERT_DIR" -ForegroundColor Green
New-Item -ItemType Directory -Force -Path $CERT_DIR | Out-Null

# Generate development certificate
Write-Host "`nGenerating development certificate (dev-cert.pem)..." -ForegroundColor Cyan
& openssl req -x509 -newkey rsa:4096 `
    -keyout "$CERT_DIR\dev-key.pem" `
    -out "$CERT_DIR\dev-cert.pem" `
    -days $DAYS_VALID `
    -nodes `
    -subj "/C=FR/ST=IDF/L=Paris/O=SecureCloud/CN=localhost"

if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to generate development certificate"
    exit 1
}

Write-Host "Generated: $CERT_DIR\dev-cert.pem and $CERT_DIR\dev-key.pem" -ForegroundColor Green

# Generate test certificate
Write-Host "`nGenerating test certificate (test-cert.pem)..." -ForegroundColor Cyan
& openssl req -x509 -newkey rsa:4096 `
    -keyout "$CERT_DIR\test-key.pem" `
    -out "$CERT_DIR\test-cert.pem" `
    -days $DAYS_VALID `
    -nodes `
    -subj "/C=FR/ST=IDF/L=Paris/O=SecureCloud/CN=test.localhost"

if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to generate test certificate"
    exit 1
}

Write-Host "Generated: $CERT_DIR\test-cert.pem and $CERT_DIR\test-key.pem" -ForegroundColor Green

Write-Host "`n Development and test certificates generated successfully!" -ForegroundColor Green
Write-Host "`n  WARNING: These are self-signed certificates for DEVELOPMENT ONLY" -ForegroundColor Yellow
Write-Host "   For production, use certificates from a trusted CA (Let's Encrypt, etc.)" -ForegroundColor Yellow

Write-Host "`nCertificate details:" -ForegroundColor Cyan
& openssl x509 -in "$CERT_DIR\dev-cert.pem" -noout -subject -dates

Write-Host "`nTo verify certificates:" -ForegroundColor Cyan
Write-Host "  openssl x509 -in $CERT_DIR\dev-cert.pem -text -noout"

# Certificate Generation Scripts

This directory contains scripts to generate self-signed TLS certificates for development and testing.

## ⚠️ WARNING

**These certificates are for DEVELOPMENT and TESTING ONLY.**  
Never use self-signed certificates in production!

For production environments, use certificates from a trusted Certificate Authority:
- [Let's Encrypt](https://letsencrypt.org/) (free, automated)
- Corporate CA certificates
- Commercial CA (DigiCert, GlobalSign, etc.)

## Usage

### Windows (PowerShell)

```powershell
cd gateway
.\scripts\generate_certs.ps1
```

### Linux/macOS (Bash)

```bash
cd gateway
chmod +x scripts/generate_certs.sh
./scripts/generate_certs.sh
```

## Generated Files

The scripts create the following files in `config/certs/`:

- **dev-cert.pem** / **dev-key.pem** - Development certificates
- **test-cert.pem** / **test-key.pem** - Test certificates

These files are automatically excluded from Git (see `.gitignore`).

## Certificate Details

- **Algorithm**: RSA 4096-bit
- **Validity**: 365 days
- **Subject**: 
  - Country: FR
  - State: IDF
  - City: Paris
  - Organization: SecureCloud
  - Common Name: localhost (dev) or test.localhost (test)

## Verifying Certificates

To view certificate details:

```bash
openssl x509 -in config/certs/dev-cert.pem -text -noout
```

To check certificate dates:

```bash
openssl x509 -in config/certs/dev-cert.pem -noout -dates
```

## Production Certificates

For production deployment, configure certificates via environment variables:

```bash
export GATEWAY_CERT_FILE=/etc/securecloud/certs/prod-cert.pem
export GATEWAY_KEY_FILE=/etc/securecloud/certs/prod-key.pem
```

Or mount certificates in Docker:

```yaml
volumes:
  - /path/to/certs:/etc/securecloud/certs:ro
```

## Requirements

- OpenSSL must be installed and available in PATH
- On Windows: Install from https://slproweb.com/products/Win32OpenSSL.html
- On Linux: Usually pre-installed, or `apt install openssl`
- On macOS: Pre-installed via LibreSSL or Homebrew OpenSSL

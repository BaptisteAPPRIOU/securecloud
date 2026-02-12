# Gateway TLS Certs

Place trusted gateway certificate material in this directory.

Expected filenames:

- `tls.crt` (certificate chain)
- `tls.key` (private key)
- `ca.crt` (CA certificate used by clients to verify `tls.crt` if private PKI is used)

These are mounted into the gateway container at `/etc/securecloud/certs`.

## Local trusted cert (mkcert)

Example (PowerShell, local private CA):

```powershell
openssl genrsa -out ops/certs/gateway/ca.key 4096
openssl req -x509 -new -nodes -key ops/certs/gateway/ca.key -sha256 -days 3650 -out ops/certs/gateway/ca.crt -subj "/C=FR/ST=IDF/L=Paris/O=SecureCloud/CN=SecureCloud Local CA"
openssl genrsa -out ops/certs/gateway/tls.key 4096
openssl req -new -key ops/certs/gateway/tls.key -out ops/certs/gateway/tls.csr -subj "/C=FR/ST=IDF/L=Paris/O=SecureCloud/CN=localhost"
@"
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = digitalSignature, keyEncipherment
extendedKeyUsage = serverAuth
subjectAltName = @alt_names
[alt_names]
DNS.1 = localhost
DNS.2 = gateway
IP.1 = 127.0.0.1
"@ | Set-Content ops/certs/gateway/tls.ext
openssl x509 -req -in ops/certs/gateway/tls.csr -CA ops/certs/gateway/ca.crt -CAkey ops/certs/gateway/ca.key -CAcreateserial -out ops/certs/gateway/tls.crt -days 825 -sha256 -extfile ops/certs/gateway/tls.ext
```

Clients must trust `ca.crt` (or set `SECURECLOUD_TLS_CA_FILE` to this file).

## Production

Use CA-issued certs (Let's Encrypt / corporate PKI) and copy them to:

- `ops/certs/gateway/tls.crt`
- `ops/certs/gateway/tls.key`

Never commit private keys.

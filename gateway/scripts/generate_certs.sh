#!/bin/bash
# Generate self-signed certificates for development/testing
# DO NOT use these certificates in production!

set -e

CERT_DIR="config/certs"
DAYS_VALID=365

echo "Creating certificate directory: $CERT_DIR"
mkdir -p "$CERT_DIR"

# Generate development certificate
echo "Generating development certificate (dev-cert.pem)..."
openssl req -x509 -newkey rsa:4096 \
    -keyout "$CERT_DIR/dev-key.pem" \
    -out "$CERT_DIR/dev-cert.pem" \
    -days $DAYS_VALID \
    -nodes \
    -subj "/C=FR/ST=IDF/L=Paris/O=SecureCloud/CN=localhost"

echo "Generated: $CERT_DIR/dev-cert.pem and $CERT_DIR/dev-key.pem"

# Generate test certificate
echo "Generating test certificate (test-cert.pem)..."
openssl req -x509 -newkey rsa:4096 \
    -keyout "$CERT_DIR/test-key.pem" \
    -out "$CERT_DIR/test-cert.pem" \
    -days $DAYS_VALID \
    -nodes \
    -subj "/C=FR/ST=IDF/L=Paris/O=SecureCloud/CN=test.localhost"

echo "Generated: $CERT_DIR/test-cert.pem and $CERT_DIR/test-key.pem"

# Set proper permissions
chmod 600 "$CERT_DIR"/*.pem

echo ""
echo "✓ Development and test certificates generated successfully!"
echo ""
echo "⚠️  WARNING: These are self-signed certificates for DEVELOPMENT ONLY"
echo "   For production, use certificates from a trusted CA (Let's Encrypt, etc.)"
echo ""
echo "Certificate details:"
openssl x509 -in "$CERT_DIR/dev-cert.pem" -noout -subject -dates

echo ""
echo "To verify certificates:"
echo "  openssl x509 -in $CERT_DIR/dev-cert.pem -text -noout"

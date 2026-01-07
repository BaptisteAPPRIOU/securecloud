# SecureCloud Production Deployment Guide

**Version:** 1.0  
**Last Updated:** January 6, 2026  
**Target Environment:** Production

##  Table of Contents

1. [Pre-Deployment Checklist](#pre-deployment-checklist)
2. [Environment Configuration](#environment-configuration)
3. [Secret Management](#secret-management)
4. [TLS Certificate Setup](#tls-certificate-setup)
5. [Database Configuration](#database-configuration)
6. [Service Deployment](#service-deployment)
7. [Monitoring & Logging](#monitoring--logging)
8. [Backup & Recovery](#backup--recovery)
9. [Security Hardening](#security-hardening)
10. [Performance Tuning](#performance-tuning)
11. [Troubleshooting](#troubleshooting)

---

## Pre-Deployment Checklist

### Infrastructure Requirements

```markdown
 Server Specifications:
  - CPU: 4+ cores (8+ recommended)
  - RAM: 16GB minimum (32GB recommended)
  - Storage: 500GB SSD (1TB recommended)
  - Network: 1Gbps minimum

 Operating System:
  - Ubuntu 22.04 LTS (recommended)
  - Debian 12
  - RHEL 9 / Rocky Linux 9
  - Docker Engine 24.0+ installed

 Network Requirements:
  - Static IP address
  - Domain name (e.g., securecloud.example.com)
  - DNS records configured (A, AAAA, CNAME)
  - Ports open: 443 (HTTPS), 22 (SSH)

 Access Control:
  - SSH key-based authentication (no password)
  - Firewall configured (ufw, iptables)
  - Sudo access for deployment user
```

### Security Prerequisites

```markdown
 Credentials Ready:
  - Database passwords (strong, unique)
  - Redis password (strong, unique)
  - JWT secret (256-bit random)
  - Admin user credentials

 TLS Certificates:
  - SSL/TLS certificate from trusted CA
  - Private key secured
  - CA bundle (intermediate certificates)

 Secret Management:
  - HashiCorp Vault setup (recommended)
  - AWS Secrets Manager / Azure Key Vault (alternative)
  - Environment variables encrypted

 Backup Strategy:
  - S3 bucket / Azure Blob Storage configured
  - Backup encryption keys generated
  - Restore procedure documented
```

### Compliance Checks

```markdown
 Legal Documents:
  - Privacy Policy published
  - Terms of Service published
  - Cookie Policy published
  - GDPR consent forms ready

 Data Protection:
  - Data retention policies configured
  - GDPR compliance verified
  - Data processing agreements signed

 Security Policies:
  - Incident response plan documented
  - Security contact published
  - Vulnerability disclosure policy
```

---

## Environment Configuration

### Directory Structure

```bash
# Production directory layout

/opt/securecloud/
├── .env.prod                    # Environment variables (SECURED)
├── docker-compose.prod.yml      # Production compose file
├── certs/                       # TLS certificates
│   ├── server.crt
│   ├── server.key
│   ├── ca-bundle.crt
│   └── dhparam.pem
├── config/                      # Service configurations
│   ├── gateway.prod.yaml
│   ├── logging.yaml
│   └── rate-limits.yaml
├── scripts/                     # Deployment scripts
│   ├── deploy.sh
│   ├── backup.sh
│   └── restore.sh
├── logs/                        # Application logs (volume mount)
└── backups/                     # Local backup staging
```

### Environment Variables (.env.prod)

```bash
# DO NOT commit this file to version control!
# Permissions: chmod 600 .env.prod
# Owner: root or deployment user only

#########################################
# Database Configuration
#########################################

DB_HOST=postgres
DB_PORT=5432
DB_NAME=securecloud_prod
DB_USER=sc_prod_user
DB_PASSWORD=CHANGE_ME_STRONG_PASSWORD_HERE  # Use pwgen -s 32 1

# Database connection pool
DB_POOL_SIZE=20
DB_POOL_MAX_OVERFLOW=10
DB_POOL_TIMEOUT=30

# Database SSL/TLS
DB_SSL_MODE=require
DB_SSL_CERT=/certs/client.crt
DB_SSL_KEY=/certs/client.key
DB_SSL_ROOT_CERT=/certs/ca.crt

#########################################
# Redis Configuration
#########################################

REDIS_HOST=redis
REDIS_PORT=6379
REDIS_PASSWORD=CHANGE_ME_STRONG_REDIS_PASSWORD  # Use pwgen -s 32 1
REDIS_DB=0
REDIS_MAX_CONNECTIONS=50

# Redis SSL/TLS (optional)
REDIS_SSL=true
REDIS_SSL_CERT=/certs/client.crt
REDIS_SSL_KEY=/certs/client.key
REDIS_SSL_CA=/certs/ca.crt

#########################################
# JWT Configuration
#########################################

# Generate with: openssl rand -base64 32
JWT_SECRET=CHANGE_ME_GENERATE_RANDOM_256_BIT_SECRET
JWT_ALGORITHM=HS256  # Future: RS256 with JWKS
JWT_ISSUER=securecloud
JWT_AUDIENCE=securecloud-api
JWT_ACCESS_TOKEN_EXPIRY=3600       # 1 hour
JWT_REFRESH_TOKEN_EXPIRY=2592000  # 30 days

#########################################
# Encryption Configuration
#########################################

# Master encryption key (AES-256)
# Generate with: openssl rand -hex 32
MASTER_ENCRYPTION_KEY=CHANGE_ME_GENERATE_RANDOM_256_BIT_KEY

# File encryption settings
FILE_ENCRYPTION_ALGORITHM=AES-256-GCM
FILE_ENCRYPTION_KEY_ROTATION_DAYS=90

#########################################
# Service URLs (Internal)
#########################################

GATEWAY_URL=https://securecloud.example.com
AUTH_SERVICE_URL=http://auth-service:8001
FILES_SERVICE_URL=http://files-service:8003
MESSAGING_SERVICE_URL=http://messaging-service:8004
AUDIT_SERVICE_URL=http://audit-service:8002

#########################################
# Gateway Configuration
#########################################

GATEWAY_HOST=0.0.0.0
GATEWAY_HTTP_PORT=8080
GATEWAY_HTTPS_PORT=8443
GATEWAY_THREADS=8  # Number of CPU cores

# TLS Configuration
TLS_ENABLED=true
TLS_CERT_FILE=/certs/server.crt
TLS_KEY_FILE=/certs/server.key
TLS_CA_FILE=/certs/ca-bundle.crt
TLS_MIN_VERSION=TLSv1.2
TLS_VERIFY_CLIENT=false

#########################################
# Rate Limiting
#########################################

RATE_LIMIT_GLOBAL_RPS=1000      # Requests per second (total)
RATE_LIMIT_PER_IP_RPS=10        # Requests per second per IP
RATE_LIMIT_PER_USER_RPM=600     # Requests per minute per user
RATE_LIMIT_LOGIN_RPM=5          # Login attempts per minute

#########################################
# Observability
#########################################

LOG_LEVEL=info  # debug, info, warn, error
LOG_FORMAT=json
LOG_FILE=/app/logs/securecloud.log
LOG_MAX_SIZE_MB=100
LOG_MAX_FILES=10

METRICS_ENABLED=true
METRICS_PORT=9090
METRICS_PATH=/metrics

# Distributed tracing (optional)
JAEGER_ENABLED=false
JAEGER_AGENT_HOST=jaeger
JAEGER_AGENT_PORT=6831

#########################################
# Email Configuration (Future)
#########################################

SMTP_HOST=smtp.example.com
SMTP_PORT=587
SMTP_USER=noreply@securecloud.example.com
SMTP_PASSWORD=CHANGE_ME_SMTP_PASSWORD
SMTP_FROM=SecureCloud <noreply@securecloud.example.com>
SMTP_TLS=true

#########################################
# Storage Configuration
#########################################

FILE_STORAGE_PATH=/data/files
FILE_MAX_SIZE_MB=100
FILE_ALLOWED_MIME_TYPES=application/pdf,image/*,text/*,application/vnd.ms-*

# S3-compatible storage (optional)
S3_ENABLED=false
S3_ENDPOINT=https://s3.amazonaws.com
S3_BUCKET=securecloud-files
S3_ACCESS_KEY=CHANGE_ME
S3_SECRET_KEY=CHANGE_ME
S3_REGION=us-east-1

#########################################
# Backup Configuration
#########################################

BACKUP_ENABLED=true
BACKUP_SCHEDULE="0 2 * * *"  # Daily at 2 AM
BACKUP_RETENTION_DAYS=30
BACKUP_S3_BUCKET=securecloud-backups
BACKUP_ENCRYPTION_KEY=CHANGE_ME_BACKUP_GPG_KEY

#########################################
# Monitoring & Alerting
#########################################

# Prometheus
PROMETHEUS_URL=http://prometheus:9090

# Grafana
GRAFANA_URL=http://grafana:3000
GRAFANA_ADMIN_PASSWORD=CHANGE_ME_GRAFANA_PASSWORD

# Alertmanager
ALERTMANAGER_URL=http://alertmanager:9093
ALERT_EMAIL=alerts@securecloud.example.com
ALERT_SLACK_WEBHOOK=https://hooks.slack.com/services/YOUR/WEBHOOK/URL

#########################################
# Feature Flags
#########################################

FEATURE_MFA_ENABLED=false        # Enable after testing
FEATURE_E2EE_ENABLED=false       # End-to-end encryption
FEATURE_FILE_VERSIONING=true
FEATURE_MESSAGE_ENCRYPTION=true
FEATURE_AUDIT_LOGGING=true

#########################################
# Compliance
#########################################

GDPR_ENABLED=true
DATA_RETENTION_DAYS=365
AUDIT_LOG_RETENTION_YEARS=7
```

### Generate Secrets Script

```bash
#!/bin/bash
# scripts/generate_secrets.sh
# Generates secure random secrets for production

echo "Generating secure secrets for production..."

echo ""
echo "# Database Password"
echo "DB_PASSWORD=$(pwgen -s 32 1)"

echo ""
echo "# Redis Password"
echo "REDIS_PASSWORD=$(pwgen -s 32 1)"

echo ""
echo "# JWT Secret (256-bit)"
echo "JWT_SECRET=$(openssl rand -base64 32)"

echo ""
echo "# Master Encryption Key (256-bit hex)"
echo "MASTER_ENCRYPTION_KEY=$(openssl rand -hex 32)"

echo ""
echo "# Backup Encryption Key"
echo "BACKUP_ENCRYPTION_KEY=$(openssl rand -base64 32)"

echo ""
echo "IMPORTANT: Store these secrets securely!"
echo "- Add to .env.prod (chmod 600)"
echo "- Backup to password manager"
echo "- Use secret management (Vault, AWS Secrets Manager)"
```

### Validate Environment Script

```bash
#!/bin/bash
# scripts/validate_env.sh
# Validates production environment configuration

set -e

echo "Validating production environment..."

# Check required files
required_files=(
    ".env.prod"
    "docker-compose.prod.yml"
    "certs/server.crt"
    "certs/server.key"
)

for file in "${required_files[@]}"; do
    if [ ! -f "$file" ]; then
        echo " Missing required file: $file"
        exit 1
    fi
    echo " Found: $file"
done

# Check environment variables
required_vars=(
    "DB_PASSWORD"
    "REDIS_PASSWORD"
    "JWT_SECRET"
    "MASTER_ENCRYPTION_KEY"
)

source .env.prod

for var in "${required_vars[@]}"; do
    if [ -z "${!var}" ]; then
        echo " Missing environment variable: $var"
        exit 1
    fi
    
    # Check if default value (not changed)
    if [[ "${!var}" == *"CHANGE_ME"* ]]; then
        echo " Default value detected for: $var"
        echo "   Please generate secure secrets!"
        exit 1
    fi
    
    echo " Set: $var"
done

# Check certificate validity
echo ""
echo "Checking TLS certificate..."
openssl x509 -in certs/server.crt -noout -dates
openssl x509 -in certs/server.crt -noout -subject

# Verify certificate matches key
if openssl x509 -noout -modulus -in certs/server.crt | openssl md5 | \
   diff - <(openssl rsa -noout -modulus -in certs/server.key | openssl md5); then
    echo " Certificate and key match"
else
    echo " Certificate and key do not match!"
    exit 1
fi

echo ""
echo " Environment validation passed!"
```

---

## Secret Management

### Using HashiCorp Vault (Recommended)

#### 1. Install Vault

```bash
# Install Vault
wget https://releases.hashicorp.com/vault/1.15.0/vault_1.15.0_linux_amd64.zip
unzip vault_1.15.0_linux_amd64.zip
sudo mv vault /usr/local/bin/

# Verify installation
vault version

# Initialize Vault
vault operator init -key-shares=5 -key-threshold=3

# Save unseal keys and root token securely!
```

#### 2. Store Secrets in Vault

```bash
# Unseal Vault (repeat 3 times with different keys)
vault operator unseal <key1>
vault operator unseal <key2>
vault operator unseal <key3>

# Login with root token
vault login <root-token>

# Enable KV secrets engine
vault secrets enable -path=securecloud kv-v2

# Store secrets
vault kv put securecloud/prod/database \
  password="strong_db_password" \
  host="postgres" \
  port="5432"

vault kv put securecloud/prod/jwt \
  secret="256_bit_random_secret" \
  algorithm="HS256"

vault kv put securecloud/prod/encryption \
  master_key="256_bit_encryption_key"

# Verify secrets
vault kv get securecloud/prod/database
```

#### 3. Access Secrets from Services

```bash
# Set Vault environment variables
export VAULT_ADDR='http://127.0.0.1:8200'
export VAULT_TOKEN='<service-token>'

# Retrieve secrets in deployment script
DB_PASSWORD=$(vault kv get -field=password securecloud/prod/database)
JWT_SECRET=$(vault kv get -field=secret securecloud/prod/jwt)

# Export to environment
export DB_PASSWORD
export JWT_SECRET
```

### Alternative: AWS Secrets Manager

```bash
# Install AWS CLI
curl "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip" -o "awscliv2.zip"
unzip awscliv2.zip
sudo ./aws/install

# Configure AWS credentials
aws configure

# Create secrets
aws secretsmanager create-secret \
  --name securecloud/prod/database \
  --secret-string '{"password":"strong_password","host":"postgres"}'

aws secretsmanager create-secret \
  --name securecloud/prod/jwt \
  --secret-string '{"secret":"256_bit_random_secret"}'

# Retrieve secrets
aws secretsmanager get-secret-value \
  --secret-id securecloud/prod/database \
  --query SecretString \
  --output text | jq -r .password
```

### Secret Rotation

```bash
#!/bin/bash
# scripts/rotate_secrets.sh
# Rotate secrets every 90 days

echo "Rotating secrets..."

# 1. Generate new JWT secret
NEW_JWT_SECRET=$(openssl rand -base64 32)

# 2. Update Vault
vault kv put securecloud/prod/jwt \
  secret="$NEW_JWT_SECRET" \
  rotated_at="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

# 3. Rolling restart services (zero downtime)
docker service update --force gateway
docker service update --force auth-service

# 4. Verify new secret is active
# (Wait 5 minutes for all instances to reload)

# 5. Revoke old JWT tokens (optional)
# Add old JWT secret to blacklist in Redis

echo "Secret rotation completed!"
```

---

## TLS Certificate Setup

### Option 1: Let's Encrypt (Recommended)

```bash
# Install Certbot
sudo apt update
sudo apt install certbot

# Obtain certificate (standalone mode)
sudo certbot certonly --standalone \
  -d securecloud.example.com \
  -d www.securecloud.example.com \
  --email admin@example.com \
  --agree-tos \
  --non-interactive

# Certificates installed to:
# /etc/letsencrypt/live/securecloud.example.com/fullchain.pem
# /etc/letsencrypt/live/securecloud.example.com/privkey.pem

# Copy to application directory
sudo cp /etc/letsencrypt/live/securecloud.example.com/fullchain.pem \
  /opt/securecloud/certs/server.crt

sudo cp /etc/letsencrypt/live/securecloud.example.com/privkey.pem \
  /opt/securecloud/certs/server.key

sudo chmod 600 /opt/securecloud/certs/server.key

# Auto-renewal (cron job)
sudo crontab -e
# Add: 0 0 1 * * certbot renew --deploy-hook "/opt/securecloud/scripts/reload_certs.sh"
```

### Option 2: Commercial Certificate

```bash
# 1. Generate CSR (Certificate Signing Request)
openssl req -new -newkey rsa:2048 -nodes \
  -keyout /opt/securecloud/certs/server.key \
  -out /opt/securecloud/certs/server.csr \
  -subj "/C=US/ST=State/L=City/O=Organization/CN=securecloud.example.com"

# 2. Submit CSR to Certificate Authority (DigiCert, GlobalSign, etc.)

# 3. Receive certificate files:
#    - server.crt (certificate)
#    - ca-bundle.crt (intermediate certificates)

# 4. Copy to application directory
cp server.crt /opt/securecloud/certs/
cp ca-bundle.crt /opt/securecloud/certs/
chmod 600 /opt/securecloud/certs/server.key
```

### Generate DH Parameters

```bash
# Generate strong DH parameters (2048-bit, takes ~10 minutes)
openssl dhparam -out /opt/securecloud/certs/dhparam.pem 2048

# Or use 4096-bit for maximum security (takes ~1 hour)
openssl dhparam -out /opt/securecloud/certs/dhparam.pem 4096
```

### Verify Certificate

```bash
# Check certificate validity
openssl x509 -in /opt/securecloud/certs/server.crt -noout -text

# Check expiry date
openssl x509 -in /opt/securecloud/certs/server.crt -noout -dates

# Verify certificate chain
openssl verify -CAfile /opt/securecloud/certs/ca-bundle.crt \
  /opt/securecloud/certs/server.crt

# Test TLS connection
openssl s_client -connect securecloud.example.com:443 -showcerts
```

---

## Database Configuration

### PostgreSQL Production Setup

```bash
# 1. Create production database
docker exec -it sc_postgres psql -U postgres

CREATE DATABASE securecloud_prod OWNER sc_prod_user ENCODING 'UTF8';

# 2. Create separate users per service (least privilege)
CREATE USER auth_service_user WITH PASSWORD 'auth_service_password';
CREATE USER files_service_user WITH PASSWORD 'files_service_password';
CREATE USER messaging_service_user WITH PASSWORD 'messaging_service_password';
CREATE USER audit_service_user WITH PASSWORD 'audit_service_password';

# 3. Grant schema-specific permissions
GRANT USAGE ON SCHEMA auth TO auth_service_user;
GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES IN SCHEMA auth TO auth_service_user;

GRANT USAGE ON SCHEMA files TO files_service_user;
GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES IN SCHEMA files TO files_service_user;

# 4. Read-only user for reporting
CREATE USER readonly_user WITH PASSWORD 'readonly_password';
GRANT SELECT ON ALL TABLES IN SCHEMA auth TO readonly_user;
GRANT SELECT ON ALL TABLES IN SCHEMA files TO readonly_user;
```

### PostgreSQL Performance Tuning

```sql
-- /opt/securecloud/config/postgresql.conf

# Memory Configuration (for 16GB RAM server)
shared_buffers = 4GB                # 25% of total RAM
effective_cache_size = 12GB         # 75% of total RAM
maintenance_work_mem = 1GB
work_mem = 64MB

# Checkpoint Configuration
checkpoint_completion_target = 0.9
wal_buffers = 16MB
max_wal_size = 4GB
min_wal_size = 1GB

# Query Planner
random_page_cost = 1.1              # SSD optimized
effective_io_concurrency = 200      # SSD

# Connection Configuration
max_connections = 200
max_worker_processes = 8
max_parallel_workers_per_gather = 4
max_parallel_workers = 8

# Logging (production)
log_destination = 'stderr'
logging_collector = on
log_directory = '/var/log/postgresql'
log_filename = 'postgresql-%Y-%m-%d.log'
log_rotation_age = 1d
log_min_duration_statement = 1000   # Log slow queries (>1s)
log_line_prefix = '%t [%p]: user=%u,db=%d,app=%a,client=%h '

# SSL/TLS
ssl = on
ssl_cert_file = '/certs/server.crt'
ssl_key_file = '/certs/server.key'
ssl_ca_file = '/certs/ca.crt'
ssl_min_protocol_version = 'TLSv1.2'
```

### Database Migrations

```bash
# Run Flyway migrations
docker run --rm \
  --network securecloud-net \
  -v /opt/securecloud/db/migrations/auth:/flyway/sql:ro \
  flyway/flyway:10 \
  -url=jdbc:postgresql://postgres:5432/securecloud_prod \
  -user=sc_prod_user \
  -password=$DB_PASSWORD \
  -schemas=auth \
  migrate

# Verify migrations
docker run --rm \
  --network securecloud-net \
  flyway/flyway:10 \
  -url=jdbc:postgresql://postgres:5432/securecloud_prod \
  -user=sc_prod_user \
  -password=$DB_PASSWORD \
  info
```

---

## Service Deployment

### Production Docker Compose

```yaml
# docker-compose.prod.yml

version: '3.8'

services:
  gateway:
    image: securecloud/gateway:1.0.0
    container_name: sc_gateway
    restart: always
    ports:
      - "443:8443"
      - "127.0.0.1:9090:9090"  # Metrics (internal only)
    volumes:
      - ./certs:/certs:ro
      - ./config/gateway.prod.yaml:/app/config/gateway.yaml:ro
      - sc_logs:/app/logs
    environment:
      - LOG_LEVEL=info
    env_file:
      - .env.prod
    networks:
      - securecloud-net
    depends_on:
      - redis
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8080/health"]
      interval: 30s
      timeout: 10s
      retries: 3
      start_period: 40s
    deploy:
      resources:
        limits:
          cpus: '4'
          memory: 4G
        reservations:
          cpus: '2'
          memory: 2G

  auth-service:
    image: securecloud/auth-service:1.0.0
    container_name: sc_auth
    restart: always
    volumes:
      - sc_logs:/app/logs
    env_file:
      - .env.prod
    networks:
      - securecloud-net
    depends_on:
      - postgres
      - redis
    deploy:
      resources:
        limits:
          cpus: '2'
          memory: 2G

  # ... other services ...

  postgres:
    image: postgres:16-alpine
    container_name: sc_postgres
    restart: always
    environment:
      POSTGRES_DB: securecloud_prod
      POSTGRES_USER: ${DB_USER}
      POSTGRES_PASSWORD: ${DB_PASSWORD}
    volumes:
      - sc_pgdata:/var/lib/postgresql/data
      - ./config/postgresql.conf:/etc/postgresql/postgresql.conf:ro
    command: postgres -c config_file=/etc/postgresql/postgresql.conf
    networks:
      - securecloud-net
    shm_size: 1g  # Increase shared memory for PostgreSQL
    deploy:
      resources:
        limits:
          cpus: '4'
          memory: 8G

volumes:
  sc_pgdata:
    driver: local
  sc_redis:
    driver: local
  sc_files:
    driver: local
  sc_logs:
    driver: local

networks:
  securecloud-net:
    driver: bridge
    ipam:
      config:
        - subnet: 172.18.0.0/16
```

### Deployment Script

```bash
#!/bin/bash
# scripts/deploy.sh
# Production deployment script

set -e

echo "Starting SecureCloud production deployment..."

# 1. Validate environment
./scripts/validate_env.sh

# 2. Pull latest images
docker compose -f docker-compose.prod.yml pull

# 3. Run database migrations
echo "Running database migrations..."
docker compose -f docker-compose.prod.yml run --rm flyway-auth
docker compose -f docker-compose.prod.yml run --rm flyway-files
docker compose -f docker-compose.prod.yml run --rm flyway-messaging
docker compose -f docker-compose.prod.yml run --rm flyway-audit

# 4. Start services (rolling update, zero downtime)
echo "Starting services..."
docker compose -f docker-compose.prod.yml up -d

# 5. Wait for services to be healthy
echo "Waiting for services to be healthy..."
sleep 30

# 6. Health check
echo "Running health checks..."
curl -f https://securecloud.example.com/health || exit 1

# 7. Verify metrics endpoint
curl -f http://localhost:9090/metrics || exit 1

echo " Deployment completed successfully!"
```

---

## Monitoring & Logging

### Prometheus Configuration

```yaml
# config/prometheus.yml

global:
  scrape_interval: 15s
  evaluation_interval: 15s

scrape_configs:
  - job_name: 'gateway'
    static_configs:
      - targets: ['gateway:9090']
    
  - job_name: 'auth-service'
    static_configs:
      - targets: ['auth-service:9090']
    
  - job_name: 'postgres'
    static_configs:
      - targets: ['postgres-exporter:9187']

alerting:
  alertmanagers:
    - static_configs:
        - targets: ['alertmanager:9093']

rule_files:
  - '/etc/prometheus/rules/*.yml'
```

### Alert Rules

```yaml
# config/prometheus/rules/alerts.yml

groups:
  - name: securecloud_alerts
    interval: 30s
    rules:
      - alert: HighErrorRate
        expr: rate(gateway_requests_total{status=~"5.."}[5m]) > 0.05
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "High error rate detected"
          description: "Error rate is {{ $value }} errors/s"
      
      - alert: ServiceDown
        expr: up{job="gateway"} == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "Service {{ $labels.job }} is down"
      
      - alert: HighResponseTime
        expr: histogram_quantile(0.99, gateway_request_duration_seconds) > 1
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High response time (p99 > 1s)"
```

### Log Aggregation (ELK Stack)

```yaml
# docker-compose.logging.yml

services:
  elasticsearch:
    image: docker.elastic.co/elasticsearch/elasticsearch:8.11.0
    environment:
      - discovery.type=single-node
      - xpack.security.enabled=false
    volumes:
      - es_data:/usr/share/elasticsearch/data
    
  logstash:
    image: docker.elastic.co/logstash/logstash:8.11.0
    volumes:
      - ./config/logstash.conf:/usr/share/logstash/pipeline/logstash.conf:ro
      - sc_logs:/logs:ro
    depends_on:
      - elasticsearch
    
  kibana:
    image: docker.elastic.co/kibana/kibana:8.11.0
    ports:
      - "127.0.0.1:5601:5601"
    environment:
      ELASTICSEARCH_HOSTS: http://elasticsearch:9200
    depends_on:
      - elasticsearch

volumes:
  es_data:
```

---

## Backup & Recovery

### Automated Backup Script

```bash
#!/bin/bash
# scripts/backup.sh
# Automated backup script (run daily via cron)

set -e

DATE=$(date +%Y%m%d_%H%M%S)
BACKUP_DIR="/opt/securecloud/backups"
S3_BUCKET="s3://securecloud-backups"

echo "Starting backup at $(date)"

# 1. Backup PostgreSQL
echo "Backing up database..."
docker exec sc_postgres pg_dump -U $DB_USER -F c securecloud_prod > \
  $BACKUP_DIR/postgres_$DATE.dump

# 2. Backup files
echo "Backing up files..."
tar czf $BACKUP_DIR/files_$DATE.tar.gz \
  -C /var/lib/docker/volumes/securecloud_sc_files/_data .

# 3. Backup configuration
echo "Backing up configuration..."
tar czf $BACKUP_DIR/config_$DATE.tar.gz \
  -C /opt/securecloud config certs

# 4. Encrypt backups
echo "Encrypting backups..."
gpg --encrypt --recipient backup@securecloud.com \
  $BACKUP_DIR/postgres_$DATE.dump

gpg --encrypt --recipient backup@securecloud.com \
  $BACKUP_DIR/files_$DATE.tar.gz

# 5. Upload to S3
echo "Uploading to S3..."
aws s3 cp $BACKUP_DIR/postgres_$DATE.dump.gpg $S3_BUCKET/postgres/
aws s3 cp $BACKUP_DIR/files_$DATE.tar.gz.gpg $S3_BUCKET/files/
aws s3 cp $BACKUP_DIR/config_$DATE.tar.gz $S3_BUCKET/config/

# 6. Clean up local backups (keep 7 days)
find $BACKUP_DIR -name "*.dump" -mtime +7 -delete
find $BACKUP_DIR -name "*.tar.gz" -mtime +7 -delete
find $BACKUP_DIR -name "*.gpg" -mtime +7 -delete

# 7. Verify backup integrity
aws s3 ls $S3_BUCKET/postgres/postgres_$DATE.dump.gpg || exit 1

echo " Backup completed successfully at $(date)"
```

### Restore Procedure

```bash
#!/bin/bash
# scripts/restore.sh
# Database and file restore script

set -e

if [ -z "$1" ]; then
    echo "Usage: ./restore.sh BACKUP_DATE (e.g., 20260106_020000)"
    exit 1
fi

BACKUP_DATE=$1
BACKUP_DIR="/opt/securecloud/backups"
S3_BUCKET="s3://securecloud-backups"

echo "Starting restore from backup: $BACKUP_DATE"

# 1. Download backups from S3
echo "Downloading backups..."
aws s3 cp $S3_BUCKET/postgres/postgres_$BACKUP_DATE.dump.gpg $BACKUP_DIR/
aws s3 cp $S3_BUCKET/files/files_$BACKUP_DATE.tar.gz.gpg $BACKUP_DIR/

# 2. Decrypt backups
echo "Decrypting backups..."
gpg --decrypt $BACKUP_DIR/postgres_$BACKUP_DATE.dump.gpg > \
  $BACKUP_DIR/postgres_$BACKUP_DATE.dump

gpg --decrypt $BACKUP_DIR/files_$BACKUP_DATE.tar.gz.gpg > \
  $BACKUP_DIR/files_$BACKUP_DATE.tar.gz

# 3. Stop services
echo "Stopping services..."
docker compose -f docker-compose.prod.yml stop

# 4. Restore database
echo "Restoring database..."
docker exec -i sc_postgres pg_restore -U $DB_USER -d securecloud_prod -c \
  < $BACKUP_DIR/postgres_$BACKUP_DATE.dump

# 5. Restore files
echo "Restoring files..."
rm -rf /var/lib/docker/volumes/securecloud_sc_files/_data/*
tar xzf $BACKUP_DIR/files_$BACKUP_DATE.tar.gz \
  -C /var/lib/docker/volumes/securecloud_sc_files/_data

# 6. Start services
echo "Starting services..."
docker compose -f docker-compose.prod.yml start

# 7. Verify restoration
echo "Verifying restoration..."
sleep 30
curl -f https://securecloud.example.com/health || exit 1

echo " Restore completed successfully!"
```

---

## Security Hardening

### Firewall Configuration (ufw)

```bash
# Enable UFW
sudo ufw enable

# Default policies
sudo ufw default deny incoming
sudo ufw default allow outgoing

# Allow SSH (restrict to specific IPs in production)
sudo ufw allow from 203.0.113.0/24 to any port 22

# Allow HTTPS
sudo ufw allow 443/tcp

# Allow Prometheus (internal only)
sudo ufw allow from 172.18.0.0/16 to any port 9090

# Deny all other traffic
sudo ufw status verbose
```

### System Hardening

```bash
# Disable root login
sudo passwd -l root

# SSH hardening (/etc/ssh/sshd_config)
PermitRootLogin no
PasswordAuthentication no
PubkeyAuthentication yes
X11Forwarding no
AllowUsers deployuser

# Restart SSH
sudo systemctl restart sshd

# Install fail2ban (brute force protection)
sudo apt install fail2ban
sudo systemctl enable fail2ban

# Configure fail2ban
cat > /etc/fail2ban/jail.local <<EOF
[sshd]
enabled = true
port = ssh
logpath = /var/log/auth.log
maxretry = 3
bantime = 3600
EOF

sudo systemctl restart fail2ban
```

---

## Troubleshooting

### Common Issues

```markdown
Issue: Gateway not starting
- Check logs: docker logs sc_gateway
- Verify certificates: openssl verify certs/server.crt
- Check port binding: netstat -tlnp | grep 8443

Issue: Database connection failed
- Check PostgreSQL status: docker ps | grep postgres
- Test connection: docker exec sc_gateway psql -h postgres -U $DB_USER
- Verify credentials in .env.prod

Issue: High memory usage
- Check resource limits: docker stats
- Increase memory limits in docker-compose.prod.yml
- Review PostgreSQL shared_buffers configuration

Issue: Slow response times
- Check Prometheus metrics: http://localhost:9090
- Review slow query log: docker exec sc_postgres tail /var/log/postgresql/*.log
- Optimize database indexes
```

---

**Document Version:** 1.0  
**Deployment Contact:** ops@securecloud.com  
**Emergency Contact:** +1-555-0123 (24/7)

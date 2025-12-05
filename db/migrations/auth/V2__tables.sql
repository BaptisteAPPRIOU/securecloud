-- prerequisites
CREATE EXTENSION IF NOT EXISTS pgcrypto; -- gen_random_uuid()

SET search_path TO auth, public;

-- TENANTS
CREATE TABLE IF NOT EXISTS tenants (
  tenant_identifier   uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  tenant_name         text NOT NULL,
  creation_timestamp  timestamptz NOT NULL DEFAULT now(),
  deletion_timestamp  timestamptz,
  tenant_status       text NOT NULL DEFAULT 'active'
);

-- USERS
CREATE TABLE IF NOT EXISTS users (
  user_identifier       uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  tenant_identifier     uuid NOT NULL REFERENCES tenants(tenant_identifier) ON DELETE CASCADE,
  user_email            text NOT NULL,
  user_display_name     text NOT NULL,
  pass_hash             text NOT NULL,
  password_algo         text NOT NULL,
  mfa_required          boolean NOT NULL DEFAULT false,
  lockout_counter       int NOT NULL DEFAULT 0,
  last_failed_login_at  timestamptz,
  password_updated_at   timestamptz,
  idp_provider          text,
  idp_subject           text,
  user_status           text NOT NULL DEFAULT 'enabled',
  creation_timestamp    timestamptz NOT NULL DEFAULT now(),
  last_login_timestamp  timestamptz,
  CONSTRAINT uq_user_email_per_tenant UNIQUE (tenant_identifier, user_email)
);

-- ROLES
CREATE TABLE IF NOT EXISTS roles (
  role_identifier     uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  tenant_identifier   uuid NOT NULL REFERENCES tenants(tenant_identifier) ON DELETE CASCADE,
  role_name           text NOT NULL,
  role_description    text,
  CONSTRAINT uq_role_name_per_tenant UNIQUE (tenant_identifier, role_name)
);

-- PERMISSIONS (global catalogue)
CREATE TABLE IF NOT EXISTS permissions (
  permission_identifier uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  permission_name       text NOT NULL UNIQUE,
  permission_description text
);

-- USER_ROLE_ASSIGNMENTS
CREATE TABLE IF NOT EXISTS user_role_assignments (
  user_role_assignment_identifier uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  user_identifier  uuid NOT NULL REFERENCES users(user_identifier) ON DELETE CASCADE,
  role_identifier  uuid NOT NULL REFERENCES roles(role_identifier) ON DELETE CASCADE,
  assignment_timestamp timestamptz NOT NULL DEFAULT now(),
  CONSTRAINT uq_user_role UNIQUE (user_identifier, role_identifier)
);

-- ROLE_PERMISSION_ASSIGNMENTS
CREATE TABLE IF NOT EXISTS role_permission_assignments (
  role_permission_assignment_identifier uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  role_identifier        uuid NOT NULL REFERENCES roles(role_identifier) ON DELETE CASCADE,
  permission_identifier  uuid NOT NULL REFERENCES permissions(permission_identifier) ON DELETE CASCADE,
  assignment_timestamp   timestamptz NOT NULL DEFAULT now(),
  CONSTRAINT uq_role_permission UNIQUE (role_identifier, permission_identifier)
);

-- USER_RECOVERY_CODES
CREATE TABLE IF NOT EXISTS user_recovery_codes (
  id         uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  user_id    uuid NOT NULL REFERENCES users(user_identifier) ON DELETE CASCADE,
  code_hash  text NOT NULL,
  used_at    timestamptz,
  CONSTRAINT uq_recovery_per_user UNIQUE (user_id, code_hash)
);

-- REVOKED_JTI
CREATE TABLE IF NOT EXISTS revoked_jti (
  jti_hash   text PRIMARY KEY,
  user_id    uuid REFERENCES users(user_identifier) ON DELETE SET NULL,
  exp        timestamptz,
  revoked_at timestamptz NOT NULL DEFAULT now()
);

-- USER_MFA_FACTORS
CREATE TABLE IF NOT EXISTS user_mfa_factors (
  id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  user_id          uuid NOT NULL REFERENCES users(user_identifier) ON DELETE CASCADE,
  type             text NOT NULL,                 -- 'totp' | 'webauthn' | ...
  totp_secret_enc  bytea,                         -- chiffré app-side
  webauthn_cred_id bytea,
  webauthn_pubkey  bytea,
  sign_count       int DEFAULT 0,
  enabled          boolean NOT NULL DEFAULT true,
  created_at       timestamptz NOT NULL DEFAULT now(),
  last_used_at     timestamptz,
  CONSTRAINT uq_factor_type_per_user UNIQUE (user_id, type)
);

-- JWK store
CREATE TABLE IF NOT EXISTS jwk (
  kid         text PRIMARY KEY,
  kty         text NOT NULL,
  alg         text NOT NULL,
  use         text,
  public_jwk  jsonb NOT NULL,  -- clef publique au format JWK
  private_enc bytea,           -- privé chiffré (optionnel)
  active      boolean NOT NULL DEFAULT true,
  created_at  timestamptz NOT NULL DEFAULT now(),
  deactivated_at timestamptz
);

-- Helpful indexes
CREATE INDEX IF NOT EXISTS idx_users_tenant ON users(tenant_identifier);
CREATE INDEX IF NOT EXISTS idx_user_role_user ON user_role_assignments(user_identifier);
CREATE INDEX IF NOT EXISTS idx_role_perm_role ON role_permission_assignments(role_identifier);

CREATE EXTENSION IF NOT EXISTS pgcrypto;
SET search_path TO audit, public;

CREATE TABLE IF NOT EXISTS tenants (
  tenant_identifier uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  tenant_name       text NOT NULL
);

CREATE TABLE IF NOT EXISTS users (
  user_identifier   uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  tenant_identifier uuid NOT NULL REFERENCES tenants(tenant_identifier) ON DELETE CASCADE,
  user_email        text NOT NULL,
  user_display_name text NOT NULL,
  CONSTRAINT uq_audit_user_email_per_tenant UNIQUE (tenant_identifier, user_email)
);

CREATE TABLE IF NOT EXISTS audit_events (
  audit_event_identifier   uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  tenant_identifier        uuid NOT NULL REFERENCES tenants(tenant_identifier) ON DELETE CASCADE,
  event_timestamp          timestamptz NOT NULL DEFAULT now(),
  actor_user_identifier    uuid REFERENCES users(user_identifier) ON DELETE SET NULL,
  action_name              text NOT NULL,
  target_primary_identifier uuid,
  target_primary_type      text,
  encrypted_event_metadata bytea
);

CREATE INDEX IF NOT EXISTS idx_audit_tenant_ts ON audit_events(tenant_identifier, event_timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_audit_target ON audit_events(target_primary_identifier);

CREATE EXTENSION IF NOT EXISTS pgcrypto;
SET search_path TO files, public;

CREATE TABLE IF NOT EXISTS tenants (
  tenant_identifier uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  tenant_name       text NOT NULL
);

CREATE TABLE IF NOT EXISTS users (
  user_identifier   uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  tenant_identifier uuid NOT NULL REFERENCES tenants(tenant_identifier) ON DELETE CASCADE,
  user_email        text NOT NULL,
  user_display_name text NOT NULL,
  CONSTRAINT uq_files_user_email_per_tenant UNIQUE (tenant_identifier, user_email)
);

CREATE TABLE IF NOT EXISTS binary_objects (
  binary_object_key      text PRIMARY KEY,  -- ex: path/uuid
  storage_location_url   text NOT NULL,
  binary_object_size_in_bytes bigint NOT NULL,
  reference_count        bigint NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS files (
  file_identifier    uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  tenant_identifier  uuid NOT NULL REFERENCES tenants(tenant_identifier) ON DELETE CASCADE,
  owner_user_identifier uuid NOT NULL REFERENCES users(user_identifier) ON DELETE RESTRICT,
  file_size_in_bytes bigint NOT NULL,
  mime_type          text,
  file_state         text NOT NULL DEFAULT 'pending', -- 'pending' | 'committed'
  creation_timestamp timestamptz NOT NULL DEFAULT now(),
  commit_timestamp   timestamptz,
  binary_object_key  text REFERENCES binary_objects(binary_object_key) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS file_uploads (
  file_upload_identifier uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  file_identifier        uuid NOT NULL REFERENCES files(file_identifier) ON DELETE CASCADE,
  upload_part_size_in_bytes int NOT NULL,
  upload_status          text NOT NULL DEFAULT 'uploading',
  creation_timestamp     timestamptz NOT NULL DEFAULT now(),
  expiration_timestamp   timestamptz
);

CREATE TABLE IF NOT EXISTS file_chunks (
  file_identifier  uuid NOT NULL REFERENCES files(file_identifier) ON DELETE CASCADE,
  chunk_index      int NOT NULL,
  binary_object_key text NOT NULL REFERENCES binary_objects(binary_object_key) ON DELETE RESTRICT,
  chunk_size_in_bytes bigint NOT NULL,
  sha256_ciphertext_digest bytea NOT NULL,
  PRIMARY KEY (file_identifier, chunk_index)
);

-- Indexes
CREATE INDEX IF NOT EXISTS idx_files_tenant ON files(tenant_identifier, creation_timestamp);

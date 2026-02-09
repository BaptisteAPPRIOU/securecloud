CREATE EXTENSION IF NOT EXISTS pgcrypto;
SET search_path TO messaging, public;

CREATE TABLE IF NOT EXISTS tenants (
  tenant_identifier uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  tenant_name       text NOT NULL
);

CREATE TABLE IF NOT EXISTS users (
  user_identifier   uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  tenant_identifier uuid NOT NULL REFERENCES tenants(tenant_identifier) ON DELETE CASCADE,
  user_email        text NOT NULL,
  user_display_name text NOT NULL,
  CONSTRAINT uq_msg_user_email_per_tenant UNIQUE (tenant_identifier, user_email)
);

CREATE TABLE IF NOT EXISTS conversations (
  conversation_identifier uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  tenant_identifier       uuid NOT NULL REFERENCES tenants(tenant_identifier) ON DELETE CASCADE,
  conversation_type       text NOT NULL,   -- 'direct' | 'group' | ...
  conversation_title      text,
  creation_timestamp      timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS conversations_memberships (
  conversation_membership_identifier uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  conversation_identifier uuid NOT NULL REFERENCES conversations(conversation_identifier) ON DELETE CASCADE,
  user_identifier         uuid NOT NULL REFERENCES users(user_identifier) ON DELETE CASCADE,
  membership_role         text NOT NULL DEFAULT 'member', -- 'owner' | 'moderator' | 'member'
  join_timestamp          timestamptz NOT NULL DEFAULT now(),
  leave_timestamp         timestamptz,
  CONSTRAINT uq_member_once UNIQUE (conversation_identifier, user_identifier)
);

CREATE TABLE IF NOT EXISTS messages (
  message_identifier      uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  conversation_identifier uuid NOT NULL REFERENCES conversations(conversation_identifier) ON DELETE CASCADE,
  author_user_identifier  uuid NOT NULL REFERENCES users(user_identifier) ON DELETE CASCADE,
  message_ciphertext_payload bytea NOT NULL,
  message_ciphertext_headers bytea,
  creation_timestamp      timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS message_deliveries (
  message_delivery_identifier uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  message_identifier          uuid NOT NULL REFERENCES messages(message_identifier) ON DELETE CASCADE,
  recipient_user_identifier   uuid NOT NULL REFERENCES users(user_identifier) ON DELETE CASCADE,
  delivery_status             text NOT NULL DEFAULT 'queued', -- 'queued' | 'sent' | 'read'
  delivery_timestamp          timestamptz,
  read_timestamp              timestamptz,
  CONSTRAINT uq_delivery_once UNIQUE (message_identifier, recipient_user_identifier)
);

-- Indexes
CREATE INDEX IF NOT EXISTS idx_msg_conversation ON messages(conversation_identifier, creation_timestamp);
CREATE INDEX IF NOT EXISTS idx_delivery_recipient ON message_deliveries(recipient_user_identifier, delivery_status);

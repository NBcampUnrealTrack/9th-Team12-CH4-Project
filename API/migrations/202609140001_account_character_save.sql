CREATE TABLE td_accounts (
    id UUID PRIMARY KEY,
    login_id TEXT NOT NULL UNIQUE CHECK (length(login_id) BETWEEN 2 AND 32),
    password_hash TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE TABLE td_sessions (
    token_hash TEXT PRIMARY KEY,
    account_id UUID NOT NULL REFERENCES td_accounts(id) ON DELETE CASCADE,
    expires_at TIMESTAMPTZ NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX td_sessions_account_idx ON td_sessions(account_id);
CREATE INDEX td_sessions_expiry_idx ON td_sessions(expires_at);
CREATE TABLE td_characters (
    id UUID PRIMARY KEY,
    account_id UUID NOT NULL REFERENCES td_accounts(id) ON DELETE CASCADE,
    slot_index SMALLINT NOT NULL CHECK (slot_index BETWEEN 0 AND 5),
    name TEXT NOT NULL,
    name_key TEXT NOT NULL UNIQUE,
    class_id TEXT NOT NULL CHECK (class_id IN ('Warrior','Mage','Archer')),
    save_data JSONB NOT NULL CHECK (jsonb_typeof(save_data) = 'object'),
    revision BIGINT NOT NULL DEFAULT 0 CHECK (revision >= 0),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    UNIQUE(account_id, slot_index)
);
-- Retain request receipts independently of the current snapshot: retries of older
-- successful saves must not overwrite a newer snapshot.
CREATE TABLE td_save_receipts (
    character_id UUID NOT NULL REFERENCES td_characters(id) ON DELETE CASCADE,
    request_id UUID NOT NULL,
    payload_hash TEXT NOT NULL,
    revision BIGINT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY(character_id, request_id)
);

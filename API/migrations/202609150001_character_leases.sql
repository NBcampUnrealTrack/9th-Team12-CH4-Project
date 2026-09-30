CREATE TABLE td_character_leases (
    character_id UUID PRIMARY KEY REFERENCES td_characters(id) ON DELETE CASCADE,
    account_id UUID NOT NULL UNIQUE REFERENCES td_accounts(id) ON DELETE CASCADE,
    token_hash TEXT NOT NULL,
    expires_at TIMESTAMPTZ NOT NULL
);

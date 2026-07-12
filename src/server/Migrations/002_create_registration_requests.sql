CREATE TABLE IF NOT EXISTS registration_requests (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL,
    public_key BLOB NOT NULL,
    source_address TEXT NOT NULL,
    status TEXT NOT NULL DEFAULT 'pending'
        CHECK (status IN ('pending', 'approved', 'rejected')),
    created_at TEXT NOT NULL,
    reviewed_at TEXT,
    UNIQUE(username, public_key)
);

CREATE INDEX IF NOT EXISTS idx_registration_requests_status
ON registration_requests(status, created_at);

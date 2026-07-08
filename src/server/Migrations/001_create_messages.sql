CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    display_name TEXT NOT NULL,
    public_key TEXT NOT NULL,
    created_at TEXT NOT NULL
);

/* TODO Remove this later, when user exist */
INSERT OR IGNORE INTO users (id, username, display_name, public_key, created_at)
VALUES (1, 'anonymous', 'Anonymous', '', strftime('%Y-%m-%dT%H:%M:%fZ', 'now'));

CREATE TABLE IF NOT EXISTS messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,
    message_type INTEGER NOT NULL,
    body TEXT NOT NULL,
    client_timestamp TEXT NOT NULL,
    stored_at TEXT NOT NULL,
    FOREIGN KEY (user_id) REFERENCES users(id)
);

CREATE INDEX IF NOT EXISTS idx_messages_stored_at
ON messages(stored_at, id);

CREATE INDEX IF NOT EXISTS idx_messages_user_id
ON messages(user_id);

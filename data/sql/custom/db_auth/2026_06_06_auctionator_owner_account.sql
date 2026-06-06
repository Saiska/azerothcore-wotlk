-- Dedicated Auctionator auction-owner account (never logs in; zero verifier = un-loginable).
-- NOTE: username 'AUCTIONEER' is RESERVED for this owner. Do NOT hand-create an account with this
-- name at another id (account.username is UNIQUE) — it would make this INSERT collide on the name
-- key instead of the id PK and silently skip creating id=4000000.
INSERT INTO account (id, username, salt, verifier, email, reg_mail, expansion)
VALUES (4000000, 'AUCTIONEER', UNHEX(REPEAT('00',32)), UNHEX(REPEAT('00',32)), '', '', 2)
ON DUPLICATE KEY UPDATE username = VALUES(username);

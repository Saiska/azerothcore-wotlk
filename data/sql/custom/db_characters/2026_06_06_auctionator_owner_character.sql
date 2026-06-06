-- Dedicated Auctionator auction-owner character (DB-only; never logs in; NOT in the bot pool).
INSERT INTO characters
    (guid, account, name, race, class, gender, level, money, map,
     position_x, position_y, position_z, orientation, taximask, innTriggerId, online, at_login)
VALUES
    (4000000, 4000000, 'Auctioneer', 1, 1, 0, 80, 0, 0,
     -8949.95, -132.49, 83.53, 0, '0', 0, 0, 0)
ON DUPLICATE KEY UPDATE account = VALUES(account), name = VALUES(name);

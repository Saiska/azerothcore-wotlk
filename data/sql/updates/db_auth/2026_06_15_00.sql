-- DB update 2026_05_14_00 -> 2026_06_15_00
-- RBAC permission for .realmtime (read-only GM calendar status command).
-- Pairs with RBAC_PERM_COMMAND_REALMTIME = 1000 (RBAC.h) used by cs_realmtime.cpp.
-- Feature: game-time-speed-knob (accelerated persistent realm calendar).
-- Linked under role 197 (Gamemaster Commands) -> available to SEC_GAMEMASTER and,
-- via the 192->193->197 chain, SEC_ADMINISTRATOR.
DELETE FROM `rbac_permissions` WHERE `id` IN (1000);
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
(1000, 'Command: realmtime');

DELETE FROM `rbac_linked_permissions` WHERE `linkedId` IN (1000);
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
(197, 1000);

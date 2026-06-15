-- DB update 2026_04_12_00 -> 2026_06_15_00
-- Persisted accelerated realm calendar clock (feature: game-time-speed-knob).
-- Single guard row id=0, INSERTed by the worldserver on first boot from
-- RealmTime.Epoch (this file only guarantees the table exists). Idempotent.
--
-- NOTE: this lives in updates/ (not base/) so it applies to ALREADY-POPULATED
-- characters DBs on the next boot. base/ files only run during a fresh DB
-- population, so an existing realm would never get the table from base/.
CREATE TABLE IF NOT EXISTS `realm_time` (
  `id`            TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `calendar_time` BIGINT           NOT NULL,
  `real_anchor`   BIGINT           NOT NULL,
  `speed`         FLOAT            NOT NULL,
  `updated_at`    BIGINT           NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Accelerated realm calendar state';

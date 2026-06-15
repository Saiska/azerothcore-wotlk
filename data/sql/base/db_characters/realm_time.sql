-- Persisted accelerated realm calendar clock (feature: game-time-speed-knob).
-- Single guard row id=0. Seeded by the worldserver on first boot from RealmTime.Epoch
-- (the server INSERTs the row if absent), so this file only guarantees the table exists.
-- Idempotent / re-runnable.
CREATE TABLE IF NOT EXISTS `realm_time` (
  `id`            TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `calendar_time` BIGINT           NOT NULL,
  `real_anchor`   BIGINT           NOT NULL,
  `speed`         FLOAT            NOT NULL,
  `updated_at`    BIGINT           NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Accelerated realm calendar state';

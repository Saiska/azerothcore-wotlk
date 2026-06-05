-- ============================================================================
-- scale_creature_models.sql                              [REUSABLE / IDEMPOTENT]
-- ----------------------------------------------------------------------------
-- Enlarge EVERY creature's visual model by a uniform `@SCALE` multiplier
-- (currently 1.33), so NPCs match the scaled player/bot scale
-- (lua_scripts/player_bot_scale.lua) and the whole world reads at one consistent size.
--
--   Target DB : world DB (pbtest_world on the rebuild; acore_world on legacy)
--   Table     : creature_template_model (col `DisplayScale`)
--   Scope     : ALL model rows -- the ~40k normal NPCs at 1.0 become `@SCALE`x (1.0->1.33)
--               (matching scaled players); the handful of larger bosses and
--               smaller critters keep their proportions, scaled x`@SCALE`.
--   Effect    : VISUAL model size only. DisplayScale is the display scale; it
--               does NOT change combat reach / bounding radius (separate
--               columns) -> purely cosmetic, no hitbox / gameplay change.
--
-- WHY THE BACKUP TABLE (load-bearing here):
--   A `@SCALE` MULTIPLIER is NOT self-idempotent -- running it twice would compound
--   to @SCALE^2. So originals are snapshotted ONCE into `creature_model_scale_backup`
--   and DisplayScale is ALWAYS recomputed as orig * @SCALE from that snapshot ->
--   running this N times yields the same result as running it once.
--   >> Take the first snapshot on a VANILLA / freshly-restored world DB. <<
--
-- DEPLOYMENT (dual copy):
--   * canonical + rollback live in asp-server-config\custom_sql_script\
--   * THIS forward script is ALSO copied (as 2026-06-04_scale_creature_models.sql)
--     into the AC custom folder data/sql/custom/db_world/ so the dbupdater
--     auto-applies it at worldserver boot (survives DB restores). Bare table
--     names (no USE) -> the dbupdater targets db_world automatically.
--     The ROLLBACK is NEVER placed there.
--
-- AFTER RUNNING: worldserver caches creature templates at boot -> restart World.
--   The client also caches seen creatures in Cache\WDB\creaturecache.wdb; a
--   fresh client / unseen NPC shows the new size immediately.
--
-- ROLLBACK: run rollback_scale_creature_models.sql (restores exact originals).
-- ============================================================================

-- === WORLD-SCALE KNOB: edit this one value to retune creature model size ===
SET @SCALE = 1.33;

-- 1) Snapshot table of ORIGINAL model scales (created once, never dropped here).
CREATE TABLE IF NOT EXISTS creature_model_scale_backup (
    CreatureID        INT UNSIGNED      NOT NULL,
    Idx               SMALLINT UNSIGNED NOT NULL,
    orig_DisplayScale FLOAT             NOT NULL,
    PRIMARY KEY (CreatureID, Idx)
);

-- 2) Record originals the FIRST time only. INSERT IGNORE means re-runs never
--    overwrite a real original with an already-scaled value.
INSERT IGNORE INTO creature_model_scale_backup (CreatureID, Idx, orig_DisplayScale)
SELECT CreatureID, Idx, DisplayScale
FROM   creature_template_model;

-- 3) Apply the @SCALE multiplier from the SNAPSHOT (idempotent -- no compounding).
UPDATE creature_template_model m
JOIN   creature_model_scale_backup b
  ON   m.CreatureID = b.CreatureID AND m.Idx = b.Idx
SET    m.DisplayScale = b.orig_DisplayScale * @SCALE;

-- 4) Summary (informational; harmless when run by the dbupdater).
--    Float-safe: 1.33 is NOT exactly representable in FLOAT, so an `= 1.33`
--    literal would falsely report 0. Compare each row to its own orig*@SCALE.
SELECT 'rows_scaled'                    AS metric, COUNT(*) AS n
FROM   creature_model_scale_backup
UNION ALL
SELECT CONCAT('scaled_to_x', @SCALE),   COUNT(*)
FROM   creature_template_model m
JOIN   creature_model_scale_backup b
  ON   m.CreatureID = b.CreatureID AND m.Idx = b.Idx
WHERE  ABS(m.DisplayScale - b.orig_DisplayScale * @SCALE) < 0.001;
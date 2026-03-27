#ifndef GRUDA_THEME_H
#define GRUDA_THEME_H

/**
 * GRUDA WCS Theme — Color Palette
 * Matches warlord-crafting-suite branding + Grudge logo colors
 * Orange primary, dark backgrounds, green accents for chain/node
 */

/* ── Brand Colors (from Grudge logo) ──────────────── */
#define WCS_ORANGE 0xFF6600       /* primary brand orange */
#define WCS_ORANGE_DARK 0xCC5200  /* hover/pressed orange */
#define WCS_ORANGE_LIGHT 0xFF8833 /* highlights */
#define WCS_BLACK 0x000000        /* logo shadow/outline */

/* ── Backgrounds ──────────────────────────────────── */
#define WCS_BG_DARK 0x0A0A14    /* deepest background */
#define WCS_BG_CARD 0x12121F    /* card/panel background */
#define WCS_BG_SURFACE 0x1A1A2E /* elevated surface */
#define WCS_BG_INPUT 0x16213E   /* input fields, tab bar */

/* ── Text ─────────────────────────────────────────── */
#define WCS_TEXT_PRIMARY 0xFFFFFF   /* main text */
#define WCS_TEXT_SECONDARY 0xAAAABB /* secondary labels */
#define WCS_TEXT_MUTED 0x666677     /* disabled/hint text */

/* ── Accents ──────────────────────────────────────── */
#define WCS_GREEN 0x00FF88      /* success, node active, chain */
#define WCS_GREEN_DARK 0x00CC66 /* green pressed */
#define WCS_RED 0xFF4444        /* error, offline, reject */
#define WCS_BLUE 0x4488FF       /* info, Solana, links */
#define WCS_YELLOW 0xFFAA00     /* warning, syncing, gold */
#define WCS_PURPLE 0xAA44FF     /* validator, staking */

/* ── Gold Coin ────────────────────────────────────── */
#define WCS_GOLD 0xD4A843       /* GRUDA coin main */
#define WCS_GOLD_LIGHT 0xF0D060 /* coin highlight */
#define WCS_GOLD_DARK 0x9A7B30  /* coin shadow */
#define WCS_GOLD_EDGE 0xB8942E  /* coin rim */

/* ── Status Bar ───────────────────────────────────── */
#define WCS_STATUSBAR_BG 0x0A0A14
#define WCS_STATUSBAR_TEXT 0x888899

/* ── Buttons / Overlays ───────────────────────────── */
#define WCS_BUTTON_BG 0x1B2438
#define WCS_BUTTON_ACTIVE 0xFF6600
#define WCS_BUTTON_TEXT 0xF5F7FA
#define WCS_OVERLAY_BG 0x0E1422

#endif /* GRUDA_THEME_H */

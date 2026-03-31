/* ── Grudge theme — matches web/index.html :root vars ── */

export const colors = {
  orange: '#ff6600',
  orangeDk: '#cc5200',
  orangeLt: '#ff8833',
  gold: '#d4a843',
  goldLt: '#f0d060',
  green: '#00ff88',
  red: '#ff4444',
  blue: '#4488ff',
  yellow: '#ffaa00',
  purple: '#aa44ff',
  bg: '#0a0a14',
  bgCard: '#12121f',
  bgSurface: '#1a1a2e',
  bgInput: '#16213e',
  text: '#ffffff',
  textSec: '#aaaabb',
  textMuted: '#666677',
  border: '#1e1e35',
} as const;

export const fonts = {
  mono: 'monospace',
  default: 'System',
} as const;

export const spacing = {
  xs: 4,
  sm: 8,
  md: 16,
  lg: 24,
  xl: 32,
} as const;

export const radii = {
  sm: 6,
  md: 12,
  lg: 999,
} as const;

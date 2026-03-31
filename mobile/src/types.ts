/* ── Types mirroring ESP32 firmware structs ─────────── */

/** Matches GrudgeAccount in include/account.h */
export interface GrudgeAccount {
  grudgeId: string;       // e.g. "GID-a1b2c3d4"
  displayName: string;    // e.g. "RacAlvin"
  authToken: string;      // session bearer token
  expiresAt: number;      // unix timestamp
  loggedIn: boolean;
}

/** Mobile wallet — unlike ESP32 (UUID only), phone holds real keys */
export interface GrudaWallet {
  publicKey: string;      // Solana public key
  deviceId: string;       // random UUID for this install
  initialized: boolean;
}

/** Matches GRD17NodeState — chain status from master node */
export interface NodeState {
  networkId: string;
  chainId: string;
  consensus: string;
  firmware: string;
  blockHeight: number;
  peers: number;
  uptime: number;
  running: boolean;
}

/** Validator info displayed on dashboard */
export interface Validator {
  id: string;
  label: string;
  stake: number;
  color: string;
}

/** Token listing */
export interface SupportedToken {
  symbol: string;
  chain: string;
}

/** WebSocket message from GrudaChain */
export interface ChainMessage {
  type?: 'heartbeat' | 'node_online' | 'node_offline' | 'block' | 'tx';
  blockHeight?: number;
  peers?: number;
  uptime?: number;
  [key: string]: unknown;
}

/** Game title entry for the launcher */
export interface GameTitle {
  id: string;
  name: string;
  url: string;
  icon: string;
  description: string;
  playStoreUrl?: string;
}

/**
 * GrudaChain WebSocket client — mirrors _chain_ws_event() in src/main.cpp.
 * Sends the same auth JSON the ESP32 sends, with platform:"android".
 */
import { WS_BASE, WS_NS_CHAIN, PLATFORM, APP_VERSION, CHAIN_RECONNECT_MS } from '../config';
import type { ChainMessage } from '../types';

type ChainListener = (msg: ChainMessage) => void;
type StatusListener = (connected: boolean) => void;

let ws: WebSocket | null = null;
let reconnectTimer: ReturnType<typeof setTimeout> | null = null;
const listeners = new Set<ChainListener>();
const statusListeners = new Set<StatusListener>();

let currentAuth: { grudgeId: string; token: string; deviceId: string } | null = null;

/* ── Public API ──────────────────────────────────────── */

export function connectChain(auth: {
  grudgeId: string;
  token: string;
  deviceId: string;
}): void {
  currentAuth = auth;
  _connect();
}

export function disconnectChain(): void {
  currentAuth = null;
  if (reconnectTimer) clearTimeout(reconnectTimer);
  reconnectTimer = null;
  if (ws) {
    ws.onclose = null;
    ws.close();
    ws = null;
  }
  _notifyStatus(false);
}

export function onChainMessage(fn: ChainListener): () => void {
  listeners.add(fn);
  return () => listeners.delete(fn);
}

export function onChainStatus(fn: StatusListener): () => void {
  statusListeners.add(fn);
  return () => statusListeners.delete(fn);
}

/* ── Internal ────────────────────────────────────────── */

function _connect(): void {
  if (!currentAuth) return;
  if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) return;

  const url = `${WS_BASE}${WS_NS_CHAIN}`;

  try {
    ws = new WebSocket(url);
  } catch {
    _scheduleReconnect();
    return;
  }

  ws.onopen = () => {
    _notifyStatus(true);

    /* Send auth payload — same JSON shape as ESP32 _chain_ws_event() */
    const authMsg = JSON.stringify({
      event: 'auth',
      grudgeId: currentAuth!.grudgeId,
      token: currentAuth!.token,
      deviceId: currentAuth!.deviceId,
      ns: WS_NS_CHAIN,
      platform: PLATFORM,
      firmware: APP_VERSION,
    });
    ws!.send(authMsg);
  };

  ws.onmessage = (e) => {
    try {
      const msg: ChainMessage = JSON.parse(String(e.data));
      for (const fn of listeners) fn(msg);
    } catch {
      // ignore parse errors
    }
  };

  ws.onclose = () => {
    _notifyStatus(false);
    _scheduleReconnect();
  };

  ws.onerror = () => {
    ws?.close();
  };
}

function _scheduleReconnect(): void {
  if (!currentAuth) return;
  if (reconnectTimer) clearTimeout(reconnectTimer);
  reconnectTimer = setTimeout(_connect, CHAIN_RECONNECT_MS);
}

function _notifyStatus(connected: boolean): void {
  for (const fn of statusListeners) fn(connected);
}

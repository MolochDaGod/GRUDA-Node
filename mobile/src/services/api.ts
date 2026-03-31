/**
 * Grudge API client — mirrors the HTTP calls in src/account.cpp
 * All requests go through api.grudge-studio.com
 */
import { API_BASE, DEVICE_AUTH_POLL_PATH, DEVICE_REGISTER_PATH } from '../config';
import type { GrudgeAccount, NodeState, SupportedToken } from '../types';

/* ── Helpers ─────────────────────────────────────────── */

async function fetchJSON<T>(url: string, opts?: RequestInit): Promise<T> {
  const res = await fetch(url, {
    headers: { 'Accept': 'application/json', ...opts?.headers },
    ...opts,
  });
  if (!res.ok) throw new Error(`API ${res.status}: ${res.statusText}`);
  return res.json();
}

function authHeaders(token: string): HeadersInit {
  return { Authorization: `Bearer ${token}` };
}

/* ── Auth ─────────────────────────────────────────────── */

/** Login via id.grudge-studio.com — returns session after OAuth redirect */
export async function exchangeAuthCode(code: string): Promise<GrudgeAccount> {
  const data = await fetchJSON<{
    grudgeId: string;
    displayName: string;
    token: string;
    expiresAt: number;
  }>(`${API_BASE}/auth/exchange`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ code, platform: 'android' }),
  });

  return {
    grudgeId: data.grudgeId,
    displayName: data.displayName,
    authToken: data.token,
    expiresAt: data.expiresAt,
    loggedIn: true,
  };
}

/** Verify existing token is still valid */
export async function verifySession(token: string): Promise<boolean> {
  try {
    await fetchJSON(`${API_BASE}/auth/verify`, {
      headers: authHeaders(token),
    });
    return true;
  } catch {
    return false;
  }
}

/** Logout — notify backend (best-effort, same as account_logout in account.cpp) */
export async function logout(token: string): Promise<void> {
  try {
    await fetch(`${API_BASE}/device/auth/logout`, {
      method: 'POST',
      headers: authHeaders(token),
    });
  } catch {
    // best-effort, same as ESP32
  }
}

/* ── Device registration (mobile client node) ────────── */

export async function registerDevice(
  token: string,
  deviceId: string,
): Promise<{ deviceToken: string }> {
  return fetchJSON(`${API_BASE}${DEVICE_REGISTER_PATH}`, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      ...authHeaders(token),
    },
    body: JSON.stringify({
      deviceId,
      platform: 'android',
      type: 'client-node',
    }),
  });
}

/* ── Node status ─────────────────────────────────────── */

export async function fetchNodeStatus(): Promise<NodeState> {
  const data = await fetchJSON<NodeState & { ok?: boolean; fallback?: NodeState }>(
    `${API_BASE}/api/status`,
  );
  if (data.ok === false && data.fallback) return data.fallback;
  return data;
}

/* ── Account info ────────────────────────────────────── */

export async function fetchAccountInfo(
  token: string,
): Promise<{ grudgeId: string; displayName: string; email?: string }> {
  return fetchJSON(`${API_BASE}/auth/me`, {
    headers: authHeaders(token),
  });
}

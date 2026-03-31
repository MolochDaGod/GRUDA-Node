/**
 * Session Storage — mirrors NVS on the ESP32.
 * Uses expo-secure-store for auth tokens, AsyncStorage for non-sensitive data.
 */
import * as SecureStore from 'expo-secure-store';
import AsyncStorage from '@react-native-async-storage/async-storage';
import { GrudgeAccount } from '../types';

const KEY_GRUDGE_ID = 'gruda_grudge_id';
const KEY_DISPLAY_NAME = 'gruda_display_name';
const KEY_AUTH_TOKEN = 'gruda_auth_token';
const KEY_EXPIRES_AT = 'gruda_expires_at';
const KEY_DEVICE_ID = 'gruda_device_id';

/* ── Account session ─────────────────────────────────── */

export async function saveSession(acct: GrudgeAccount): Promise<void> {
  await SecureStore.setItemAsync(KEY_AUTH_TOKEN, acct.authToken);
  await AsyncStorage.multiSet([
    [KEY_GRUDGE_ID, acct.grudgeId],
    [KEY_DISPLAY_NAME, acct.displayName],
    [KEY_EXPIRES_AT, String(acct.expiresAt)],
  ]);
}

export async function resumeSession(): Promise<GrudgeAccount | null> {
  const token = await SecureStore.getItemAsync(KEY_AUTH_TOKEN);
  if (!token) return null;

  const pairs = await AsyncStorage.multiGet([
    KEY_GRUDGE_ID,
    KEY_DISPLAY_NAME,
    KEY_EXPIRES_AT,
  ]);
  const map = Object.fromEntries(pairs);

  const grudgeId = map[KEY_GRUDGE_ID];
  if (!grudgeId) return null;

  return {
    grudgeId,
    displayName: map[KEY_DISPLAY_NAME] ?? 'Guest',
    authToken: token,
    expiresAt: Number(map[KEY_EXPIRES_AT] ?? 0),
    loggedIn: true,
  };
}

export async function clearSession(): Promise<void> {
  await SecureStore.deleteItemAsync(KEY_AUTH_TOKEN);
  await AsyncStorage.multiRemove([
    KEY_GRUDGE_ID,
    KEY_DISPLAY_NAME,
    KEY_EXPIRES_AT,
  ]);
}

/* ── Device identity ─────────────────────────────────── */

export async function getDeviceId(): Promise<string> {
  let id = await AsyncStorage.getItem(KEY_DEVICE_ID);
  if (!id) {
    id = generateUUID();
    await AsyncStorage.setItem(KEY_DEVICE_ID, id);
  }
  return id;
}

function generateUUID(): string {
  return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, (c) => {
    const r = (Math.random() * 16) | 0;
    const v = c === 'x' ? r : (r & 0x3) | 0x8;
    return v.toString(16);
  });
}

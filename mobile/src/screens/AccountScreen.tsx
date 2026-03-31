import React, { useEffect, useState, useCallback } from 'react';
import {
  View,
  Text,
  StyleSheet,
  TouchableOpacity,
  ActivityIndicator,
  Alert,
} from 'react-native';
import * as WebBrowser from 'expo-web-browser';
import * as Linking from 'expo-linking';
import { colors, spacing, radii } from '../theme';
import { ID_BASE } from '../config';
import { exchangeAuthCode, logout as apiLogout, verifySession } from '../services/api';
import { saveSession, resumeSession, clearSession } from '../services/session';
import type { GrudgeAccount } from '../types';

export default function AccountScreen() {
  const [account, setAccount] = useState<GrudgeAccount | null>(null);
  const [loading, setLoading] = useState(true);

  /* ── Resume saved session on mount ──────────────── */
  useEffect(() => {
    (async () => {
      const saved = await resumeSession();
      if (saved) {
        const valid = await verifySession(saved.authToken);
        if (valid) {
          setAccount(saved);
        } else {
          await clearSession();
        }
      }
      setLoading(false);
    })();
  }, []);

  /* ── Handle deep link callback from id.grudge-studio.com ── */
  useEffect(() => {
    const sub = Linking.addEventListener('url', async ({ url }) => {
      const parsed = Linking.parse(url);
      const code = parsed.queryParams?.code as string | undefined;
      if (code) {
        setLoading(true);
        try {
          const acct = await exchangeAuthCode(code);
          await saveSession(acct);
          setAccount(acct);
        } catch (err: any) {
          Alert.alert('Login failed', err.message);
        }
        setLoading(false);
      }
    });
    return () => sub.remove();
  }, []);

  /* ── Login action ──────────────────────────────── */
  const handleLogin = useCallback(async () => {
    const redirectUri = Linking.createURL('auth');
    const loginUrl = `${ID_BASE}/auth?redirect_uri=${encodeURIComponent(redirectUri)}&platform=android`;
    await WebBrowser.openBrowserAsync(loginUrl);
  }, []);

  /* ── Logout action ─────────────────────────────── */
  const handleLogout = useCallback(async () => {
    if (account) {
      await apiLogout(account.authToken);
      await clearSession();
      setAccount(null);
    }
  }, [account]);

  if (loading) {
    return (
      <View style={styles.container}>
        <ActivityIndicator size="large" color={colors.orange} />
      </View>
    );
  }

  /* ── Logged out ─────────────────────────────────── */
  if (!account) {
    return (
      <View style={styles.container}>
        <Text style={styles.logo}>GRUDA NODE</Text>
        <Text style={styles.sub}>Sign in with your Grudge Account</Text>
        <TouchableOpacity style={styles.loginBtn} onPress={handleLogin}>
          <Text style={styles.loginBtnText}>Sign In</Text>
        </TouchableOpacity>
        <Text style={styles.hint}>
          Opens id.grudge-studio.com — supports Discord, Web3Auth, and email.
        </Text>
      </View>
    );
  }

  /* ── Logged in ──────────────────────────────────── */
  return (
    <View style={styles.container}>
      <View style={styles.card}>
        <Text style={styles.cardTitle}>GRUDGE ACCOUNT</Text>
        <Row label="Display Name" value={account.displayName} color={colors.orange} />
        <Row label="Grudge ID" value={account.grudgeId} color={colors.text} />
        <Row
          label="Session"
          value={account.loggedIn ? 'Active' : 'Expired'}
          color={account.loggedIn ? colors.green : colors.red}
        />
      </View>
      <TouchableOpacity style={styles.logoutBtn} onPress={handleLogout}>
        <Text style={styles.logoutBtnText}>Logout</Text>
      </TouchableOpacity>
    </View>
  );
}

function Row({ label, value, color }: { label: string; value: string; color: string }) {
  return (
    <View style={styles.row}>
      <Text style={styles.rowLabel}>{label}</Text>
      <Text style={[styles.rowValue, { color }]}>{value}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: colors.bg,
    justifyContent: 'center',
    alignItems: 'center',
    padding: spacing.lg,
  },
  logo: {
    fontSize: 28,
    fontWeight: '900',
    color: colors.orange,
    letterSpacing: 4,
    marginBottom: spacing.sm,
  },
  sub: {
    fontSize: 14,
    color: colors.textSec,
    marginBottom: spacing.xl,
  },
  hint: {
    fontSize: 12,
    color: colors.textMuted,
    textAlign: 'center',
    marginTop: spacing.md,
    paddingHorizontal: spacing.xl,
  },
  loginBtn: {
    backgroundColor: colors.orange,
    paddingHorizontal: 48,
    paddingVertical: 14,
    borderRadius: radii.md,
  },
  loginBtnText: {
    color: colors.text,
    fontWeight: '700',
    fontSize: 16,
  },
  card: {
    backgroundColor: colors.bgCard,
    borderWidth: 1,
    borderColor: colors.border,
    borderRadius: radii.md,
    padding: spacing.lg,
    width: '100%',
  },
  cardTitle: {
    fontSize: 11,
    letterSpacing: 2,
    color: colors.textMuted,
    textTransform: 'uppercase',
    marginBottom: spacing.md,
  },
  row: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    paddingVertical: spacing.sm,
    borderBottomWidth: 1,
    borderBottomColor: colors.bgSurface,
  },
  rowLabel: { color: colors.textSec, fontSize: 14 },
  rowValue: { fontWeight: '600', fontSize: 14, fontFamily: 'monospace' },
  logoutBtn: {
    marginTop: spacing.lg,
    paddingHorizontal: 32,
    paddingVertical: 12,
    borderRadius: radii.md,
    borderWidth: 1,
    borderColor: colors.red,
  },
  logoutBtnText: { color: colors.red, fontWeight: '600' },
});

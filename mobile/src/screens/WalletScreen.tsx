import React, { useState } from 'react';
import {
  View,
  Text,
  StyleSheet,
  TouchableOpacity,
  ScrollView,
  Alert,
  Clipboard,
} from 'react-native';
import { colors, spacing, radii } from '../theme';
import { GBUX_MINT, GRUDA_POLYGON } from '../config';

/**
 * Wallet Screen
 *
 * Web3Auth integration placeholder — when you add @web3auth/react-native-sdk,
 * the login flow will produce a Solana keypair stored in secure enclave.
 * For now this shows the token addresses and a connect button.
 */
export default function WalletScreen() {
  const [connected, setConnected] = useState(false);
  const [publicKey, setPublicKey] = useState<string | null>(null);

  const handleConnect = async () => {
    // TODO: Integrate @web3auth/react-native-sdk here
    // const web3auth = new Web3Auth({ clientId: WEB3_CLIENT_ID, ... });
    // await web3auth.login({ loginProvider: 'google' });
    // setPublicKey(web3auth.provider.publicKey);
    Alert.alert(
      'Web3Auth',
      'Add @web3auth/react-native-sdk to enable wallet connection.\nSee mobile/README.md for setup.',
    );
  };

  return (
    <ScrollView style={styles.scroll} contentContainerStyle={styles.container}>
      <Text style={styles.heading}>WALLET</Text>

      {/* Connection status */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>SOLANA WALLET</Text>
        {connected && publicKey ? (
          <>
            <Row label="Public Key" value={truncate(publicKey)} color={colors.green} />
            <TouchableOpacity
              onPress={() => {
                Clipboard.setString(publicKey);
                Alert.alert('Copied', 'Public key copied to clipboard');
              }}
            >
              <Text style={styles.copyHint}>Tap to copy full key</Text>
            </TouchableOpacity>
          </>
        ) : (
          <TouchableOpacity style={styles.connectBtn} onPress={handleConnect}>
            <Text style={styles.connectBtnText}>Connect Wallet</Text>
          </TouchableOpacity>
        )}
      </View>

      {/* Token addresses */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>TOKENS</Text>
        <TokenRow symbol="SOL" chain="solana" address="—" color={colors.purple} />
        <TokenRow symbol="GBUX" chain="solana" address={truncate(GBUX_MINT)} color={colors.gold} />
        <TokenRow symbol="GRUDA" chain="polygon" address={truncate(GRUDA_POLYGON)} color={colors.orange} />
        <TokenRow symbol="ETH" chain="ethereum" address="—" color={colors.blue} />
        <TokenRow symbol="POLY" chain="polygon" address="—" color={colors.purple} />
      </View>

      {/* Purchase link */}
      <View style={styles.card}>
        <Text style={styles.cardTitle}>BUY GBUX</Text>
        <Text style={styles.purchaseHint}>
          Purchase GBUX on Raydium Launchpad (Solana)
        </Text>
        <TouchableOpacity style={styles.buyBtn}>
          <Text style={styles.buyBtnText}>Open Raydium</Text>
        </TouchableOpacity>
      </View>
    </ScrollView>
  );
}

function TokenRow({
  symbol, chain, address, color,
}: {
  symbol: string; chain: string; address: string; color: string;
}) {
  return (
    <View style={styles.tokenRow}>
      <View style={styles.tokenLeft}>
        <Text style={[styles.tokenSymbol, { color }]}>{symbol}</Text>
        <View style={styles.chainBadge}>
          <Text style={styles.chainText}>{chain}</Text>
        </View>
      </View>
      <Text style={styles.tokenAddr}>{address}</Text>
    </View>
  );
}

function truncate(s: string, len = 12): string {
  if (s.length <= len) return s;
  return `${s.slice(0, 6)}…${s.slice(-4)}`;
}

const styles = StyleSheet.create({
  scroll: { flex: 1, backgroundColor: colors.bg },
  container: { padding: spacing.lg },
  heading: {
    fontSize: 11,
    letterSpacing: 4,
    color: colors.orange,
    fontWeight: '900',
    marginBottom: spacing.md,
  },
  card: {
    backgroundColor: colors.bgCard,
    borderWidth: 1,
    borderColor: colors.border,
    borderRadius: radii.md,
    padding: spacing.lg,
    marginBottom: spacing.md,
  },
  cardTitle: {
    fontSize: 11,
    letterSpacing: 2,
    color: colors.textMuted,
    textTransform: 'uppercase',
    marginBottom: spacing.md,
  },
  connectBtn: {
    backgroundColor: colors.orange,
    paddingVertical: 12,
    borderRadius: radii.md,
    alignItems: 'center',
  },
  connectBtnText: { color: colors.text, fontWeight: '700', fontSize: 15 },
  copyHint: { color: colors.textMuted, fontSize: 12, marginTop: spacing.sm },
  tokenRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingVertical: spacing.sm,
    borderBottomWidth: 1,
    borderBottomColor: colors.bgSurface,
  },
  tokenLeft: { flexDirection: 'row', alignItems: 'center', gap: 8 },
  tokenSymbol: { fontWeight: '700', fontSize: 14 },
  chainBadge: {
    backgroundColor: colors.bgInput,
    paddingHorizontal: 6,
    paddingVertical: 2,
    borderRadius: 4,
  },
  chainText: {
    fontSize: 10,
    color: colors.textMuted,
    textTransform: 'uppercase',
    letterSpacing: 1,
  },
  tokenAddr: { color: colors.textSec, fontSize: 12, fontFamily: 'monospace' },
  purchaseHint: { color: colors.textSec, fontSize: 13, marginBottom: spacing.md },
  buyBtn: {
    borderWidth: 1,
    borderColor: colors.gold,
    paddingVertical: 10,
    borderRadius: radii.md,
    alignItems: 'center',
  },
  buyBtnText: { color: colors.gold, fontWeight: '600' },
});

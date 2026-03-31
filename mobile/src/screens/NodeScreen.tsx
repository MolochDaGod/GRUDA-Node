import React, { useEffect, useState, useRef } from 'react';
import { View, Text, StyleSheet, ScrollView } from 'react-native';
import { colors, spacing, radii } from '../theme';
import { STATUS_POLL_MS } from '../config';
import { fetchNodeStatus } from '../services/api';
import { connectChain, disconnectChain, onChainMessage, onChainStatus } from '../services/chain-ws';
import { resumeSession, getDeviceId } from '../services/session';
import type { NodeState, ChainMessage, Validator } from '../types';

/* ── Static validators (matches web/index.html) ──────── */
const VALIDATORS: Validator[] = [
  { id: 'GRD-17-NODE-001', label: 'GRD-17', stake: 17000, color: colors.orange },
  { id: 'GRD-27-NODE-001', label: 'GRD-27', stake: 12000, color: colors.green },
  { id: 'ALE-VALIDATOR-001', label: 'ALE', stake: 8500, color: colors.blue },
  { id: 'DANGRD-VAL-001', label: 'DANGRD', stake: 5500, color: colors.purple },
];
const TOTAL_STAKE = VALIDATORS.reduce((s, v) => s + v.stake, 0);

export default function NodeScreen() {
  const [node, setNode] = useState<Partial<NodeState>>({});
  const [wsConnected, setWsConnected] = useState(false);
  const [events, setEvents] = useState<string[]>(['Waiting for node data…']);
  const pollRef = useRef<ReturnType<typeof setInterval>>();

  /* ── Fetch initial status via REST ─────────────── */
  useEffect(() => {
    const poll = async () => {
      try {
        const status = await fetchNodeStatus();
        setNode(status);
      } catch {
        setNode((prev) => ({ ...prev, running: false }));
      }
    };
    poll();
    pollRef.current = setInterval(poll, STATUS_POLL_MS);
    return () => clearInterval(pollRef.current);
  }, []);

  /* ── Connect WebSocket for live updates ────────── */
  useEffect(() => {
    let unsub1: (() => void) | undefined;
    let unsub2: (() => void) | undefined;

    (async () => {
      const acct = await resumeSession();
      const deviceId = await getDeviceId();
      if (!acct) return;

      connectChain({
        grudgeId: acct.grudgeId,
        token: acct.authToken,
        deviceId,
      });

      unsub1 = onChainMessage((msg: ChainMessage) => {
        if (msg.blockHeight !== undefined) {
          setNode((prev) => ({ ...prev, blockHeight: msg.blockHeight }));
          addEvent(`Block #${msg.blockHeight}`);
        }
        if (msg.peers !== undefined) {
          setNode((prev) => ({ ...prev, peers: msg.peers }));
        }
        if (msg.type === 'heartbeat') {
          setNode((prev) => ({ ...prev, running: true, uptime: msg.uptime ?? prev.uptime }));
          addEvent('♥ Heartbeat');
        }
        if (msg.type === 'node_online') {
          setNode((prev) => ({ ...prev, running: true }));
          addEvent('⚡ Node online');
        }
        if (msg.type === 'node_offline') {
          setNode((prev) => ({ ...prev, running: false }));
          addEvent('⛔ Node offline');
        }
      });

      unsub2 = onChainStatus(setWsConnected);
    })();

    return () => {
      unsub1?.();
      unsub2?.();
      disconnectChain();
    };
  }, []);

  const addEvent = (text: string) => {
    const ts = new Date().toLocaleTimeString('en-US', { hour12: false });
    setEvents((prev) => [`${ts}  ${text}`, ...prev.slice(0, 49)]);
  };

  const nodeColor = node.running ? colors.green : node.running === false ? colors.red : colors.orange;
  const nodeLabel = node.running ? 'Online' : node.running === false ? 'Offline' : 'Checking…';

  return (
    <ScrollView style={styles.scroll} contentContainerStyle={styles.container}>
      {/* Status pills */}
      <View style={styles.pillRow}>
        <Pill dotColor={nodeColor} label={nodeLabel} />
        <Pill dotColor={wsConnected ? colors.green : colors.red} label={wsConnected ? 'Live Feed' : 'Feed Off'} />
      </View>

      {/* Network */}
      <Card title="NETWORK">
        <Row label="Network" value={node.networkId ?? '—'} color={colors.orange} />
        <Row label="Chain ID" value={node.chainId ?? '—'} />
        <Row label="Consensus" value={node.consensus ?? '—'} />
        <Row label="Block Time" value="3s target" />
        <Row label="Block Reward" value="17 GRUDA" color={colors.goldLt} />
      </Card>

      {/* Node */}
      <Card title="NODE STATUS">
        <Row label="Block Height" value={node.blockHeight?.toLocaleString() ?? '—'} color={colors.green} />
        <Row label="Peers" value={node.peers?.toString() ?? '—'} />
        <Row label="Uptime" value={node.uptime ? formatUptime(node.uptime) : '—'} />
        <Row label="Firmware" value={node.firmware ? `v${node.firmware}` : '—'} color={colors.blue} />
        <Row label="Device" value="ESP32-GRD17" />
      </Card>

      {/* Validators */}
      <Card title="VALIDATORS">
        {VALIDATORS.map((v) => {
          const pct = ((v.stake / TOTAL_STAKE) * 100).toFixed(1);
          return (
            <View key={v.id} style={{ marginBottom: spacing.md }}>
              <Row label={v.label} value={`${v.stake.toLocaleString()} GRUDA · ${pct}%`} color={v.color} />
              <View style={styles.bar}>
                <View style={[styles.barFill, { width: `${pct}%`, backgroundColor: v.color }]} />
              </View>
            </View>
          );
        })}
      </Card>

      {/* Live events */}
      <Card title="LIVE EVENTS">
        {events.slice(0, 20).map((e, i) => (
          <Text key={i} style={styles.eventLine}>{e}</Text>
        ))}
      </Card>
    </ScrollView>
  );
}

/* ── Sub-components ──────────────────────────────────── */

function Card({ title, children }: { title: string; children: React.ReactNode }) {
  return (
    <View style={styles.card}>
      <Text style={styles.cardTitle}>{title}</Text>
      {children}
    </View>
  );
}

function Row({ label, value, color }: { label: string; value: string; color?: string }) {
  return (
    <View style={styles.row}>
      <Text style={styles.rowLabel}>{label}</Text>
      <Text style={[styles.rowValue, color ? { color } : null]}>{value}</Text>
    </View>
  );
}

function Pill({ dotColor, label }: { dotColor: string; label: string }) {
  return (
    <View style={styles.pill}>
      <View style={[styles.dot, { backgroundColor: dotColor }]} />
      <Text style={styles.pillText}>{label}</Text>
    </View>
  );
}

function formatUptime(seconds: number): string {
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const s = seconds % 60;
  if (h > 0) return `${h}h ${m}m`;
  if (m > 0) return `${m}m ${s}s`;
  return `${s}s`;
}

/* ── Styles ──────────────────────────────────────────── */

const styles = StyleSheet.create({
  scroll: { flex: 1, backgroundColor: colors.bg },
  container: { padding: spacing.lg },
  pillRow: { flexDirection: 'row', gap: spacing.sm, marginBottom: spacing.md },
  pill: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: spacing.sm,
    backgroundColor: colors.bgSurface,
    borderWidth: 1,
    borderColor: colors.border,
    borderRadius: radii.lg,
    paddingHorizontal: spacing.md,
    paddingVertical: 6,
  },
  pillText: { color: colors.textSec, fontSize: 13 },
  dot: { width: 10, height: 10, borderRadius: 5 },
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
  row: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    paddingVertical: 6,
    borderBottomWidth: 1,
    borderBottomColor: colors.bgSurface,
  },
  rowLabel: { color: colors.textSec, fontSize: 14 },
  rowValue: { color: colors.text, fontWeight: '600', fontSize: 14, fontFamily: 'monospace' },
  bar: {
    height: 6,
    backgroundColor: colors.bgSurface,
    borderRadius: 3,
    marginTop: 4,
    overflow: 'hidden',
  },
  barFill: { height: '100%', borderRadius: 3 },
  eventLine: {
    fontFamily: 'monospace',
    fontSize: 12,
    color: colors.textSec,
    lineHeight: 22,
    borderBottomWidth: 1,
    borderBottomColor: colors.bgCard,
    paddingVertical: 2,
  },
});

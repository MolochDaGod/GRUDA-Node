/**
 * VoiceScreen — Talk to your GRUDA Node via Bluetooth
 *
 * - Scans and connects to the node via BLE
 * - Hold-to-talk button captures speech → STT
 * - Sends transcribed text to the node
 * - Displays AI responses and speaks them via TTS
 *
 * Created by RacAlvin The Pirate King for GRUDGE STUDIO
 */

import React, { useCallback, useEffect, useRef, useState } from 'react';
import {
  View,
  Text,
  TouchableOpacity,
  ScrollView,
  StyleSheet,
  Platform,
  Alert,
  PermissionsAndroid,
} from 'react-native';
import Voice, { SpeechResultsEvent } from '@react-native-voice/voice';
import * as Speech from 'expo-speech';
import { bleVoice, BLEStatus, AIResponse } from '../services/ble-voice';

/* ── Theme (matches WCS branding) ───────────────────── */
const COLORS = {
  bg: '#0A0A14',
  card: '#12121F',
  surface: '#1A1A2E',
  orange: '#FF6600',
  orangeDark: '#CC5200',
  green: '#00FF88',
  blue: '#4488FF',
  red: '#FF4444',
  yellow: '#FFAA00',
  textPrimary: '#FFFFFF',
  textSecondary: '#AAAABB',
  textMuted: '#666677',
  buttonBg: '#1B2438',
};

interface TranscriptItem {
  id: string;
  userText: string;
  aiResponse: string;
  timestamp: number;
}

export default function VoiceScreen() {
  const [bleStatus, setBleStatus] = useState<BLEStatus>('disconnected');
  const [deviceName, setDeviceName] = useState<string | null>(null);
  const [isListening, setIsListening] = useState(false);
  const [partialText, setPartialText] = useState('');
  const [transcript, setTranscript] = useState<TranscriptItem[]>([]);
  const scrollRef = useRef<ScrollView>(null);

  /* ── BLE setup ─────────────────────────────────────── */
  useEffect(() => {
    const unsubStatus = bleVoice.onStatusChange((s) => {
      setBleStatus(s);
      setDeviceName(bleVoice.getDeviceName());
    });

    const unsubResponse = bleVoice.onResponse((resp: AIResponse) => {
      setTranscript((prev) => {
        const last = prev[prev.length - 1];
        if (last && !last.aiResponse) {
          return [
            ...prev.slice(0, -1),
            { ...last, aiResponse: resp.text },
          ];
        }
        return [
          ...prev,
          {
            id: Date.now().toString(),
            userText: '?',
            aiResponse: resp.text,
            timestamp: Date.now(),
          },
        ];
      });

      /* Speak the AI response */
      Speech.speak(resp.text, { language: 'en-US', rate: 0.9 });
    });

    return () => {
      unsubStatus();
      unsubResponse();
    };
  }, []);

  /* ── Voice recognition setup ───────────────────────── */
  useEffect(() => {
    Voice.onSpeechResults = (e: SpeechResultsEvent) => {
      const text = e.value?.[0] ?? '';
      setPartialText('');
      if (text.trim().length > 0) {
        handleVoiceResult(text.trim());
      }
    };

    Voice.onSpeechPartialResults = (e: SpeechResultsEvent) => {
      setPartialText(e.value?.[0] ?? '');
    };

    Voice.onSpeechEnd = () => {
      setIsListening(false);
    };

    Voice.onSpeechError = (e: any) => {
      console.error('[Voice] Error:', e.error);
      setIsListening(false);
      setPartialText('');
    };

    return () => {
      Voice.destroy().then(Voice.removeAllListeners);
    };
  }, []);

  /* ── Request Android permissions ───────────────────── */
  const requestPermissions = async () => {
    if (Platform.OS === 'android') {
      const granted = await PermissionsAndroid.requestMultiple([
        PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN,
        PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT,
        PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION,
        PermissionsAndroid.PERMISSIONS.RECORD_AUDIO,
      ]);

      const allGranted = Object.values(granted).every(
        (v) => v === PermissionsAndroid.RESULTS.GRANTED
      );
      if (!allGranted) {
        Alert.alert('Permissions Required', 'BLE and microphone access are needed.');
        return false;
      }
    }
    return true;
  };

  /* ── Connect to node ───────────────────────────────── */
  const handleConnect = useCallback(async () => {
    if (bleStatus === 'connected') {
      await bleVoice.disconnect();
      return;
    }

    const ok = await requestPermissions();
    if (!ok) return;

    const found = await bleVoice.scanAndConnect();
    if (!found) {
      Alert.alert('Not Found', 'No GRUDA Node found nearby. Make sure it is powered on.');
    }
  }, [bleStatus]);

  /* ── Handle transcribed voice ──────────────────────── */
  const handleVoiceResult = useCallback(
    async (text: string) => {
      /* Add to transcript immediately */
      const item: TranscriptItem = {
        id: Date.now().toString(),
        userText: text,
        aiResponse: '',
        timestamp: Date.now(),
      };
      setTranscript((prev) => [...prev, item]);

      /* Send to node via BLE */
      const sent = await bleVoice.sendText(text);
      if (!sent) {
        setTranscript((prev) => {
          const last = prev[prev.length - 1];
          if (last && last.id === item.id) {
            return [
              ...prev.slice(0, -1),
              { ...last, aiResponse: 'BLE not connected' },
            ];
          }
          return prev;
        });
      }
    },
    []
  );

  /* ── Start / stop listening ────────────────────────── */
  const startListening = useCallback(async () => {
    try {
      setIsListening(true);
      setPartialText('');
      await Voice.start('en-US');
    } catch (err) {
      console.error('[Voice] Start error:', err);
      setIsListening(false);
    }
  }, []);

  const stopListening = useCallback(async () => {
    try {
      await Voice.stop();
    } catch (err) {
      console.error('[Voice] Stop error:', err);
    }
    setIsListening(false);
  }, []);

  /* ── Scroll to bottom on new messages ──────────────── */
  useEffect(() => {
    setTimeout(() => scrollRef.current?.scrollToEnd({ animated: true }), 100);
  }, [transcript]);

  /* ── Status color ──────────────────────────────────── */
  const statusColor =
    bleStatus === 'connected'
      ? COLORS.green
      : bleStatus === 'scanning' || bleStatus === 'connecting'
      ? COLORS.yellow
      : COLORS.red;

  const statusLabel =
    bleStatus === 'connected'
      ? `Connected: ${deviceName ?? 'GRUDA Node'}`
      : bleStatus === 'scanning'
      ? 'Scanning...'
      : bleStatus === 'connecting'
      ? 'Connecting...'
      : 'Disconnected';

  return (
    <View style={styles.container}>
      {/* Header */}
      <View style={styles.header}>
        <Text style={styles.title}>Voice Admin</Text>
        <TouchableOpacity style={styles.connectBtn} onPress={handleConnect}>
          <View style={[styles.statusDot, { backgroundColor: statusColor }]} />
          <Text style={styles.statusText}>{statusLabel}</Text>
        </TouchableOpacity>
      </View>

      {/* Transcript */}
      <ScrollView ref={scrollRef} style={styles.transcript} contentContainerStyle={{ paddingBottom: 12 }}>
        {transcript.length === 0 && (
          <Text style={styles.emptyText}>
            Connect to your GRUDA Node and hold the mic button to talk.
          </Text>
        )}
        {transcript.map((item) => (
          <View key={item.id} style={styles.msgGroup}>
            <Text style={styles.userMsg}>{`> ${item.userText}`}</Text>
            {item.aiResponse ? (
              <Text style={styles.aiMsg}>{item.aiResponse}</Text>
            ) : (
              <Text style={styles.pendingMsg}>Waiting...</Text>
            )}
          </View>
        ))}
      </ScrollView>

      {/* Partial speech text */}
      {partialText.length > 0 && (
        <View style={styles.partialBar}>
          <Text style={styles.partialText}>{partialText}</Text>
        </View>
      )}

      {/* Talk button */}
      <View style={styles.footer}>
        <TouchableOpacity
          style={[
            styles.talkBtn,
            isListening && styles.talkBtnActive,
            bleStatus !== 'connected' && styles.talkBtnDisabled,
          ]}
          onPressIn={startListening}
          onPressOut={stopListening}
          disabled={bleStatus !== 'connected'}
          activeOpacity={0.7}
        >
          <Text style={styles.talkBtnText}>
            {isListening ? 'Listening...' : 'Hold to Talk'}
          </Text>
        </TouchableOpacity>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: COLORS.bg,
  },
  header: {
    paddingHorizontal: 16,
    paddingTop: 12,
    paddingBottom: 8,
    borderBottomWidth: 1,
    borderBottomColor: COLORS.orangeDark,
  },
  title: {
    color: COLORS.orange,
    fontSize: 20,
    fontWeight: '700',
    marginBottom: 6,
  },
  connectBtn: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: 6,
  },
  statusDot: {
    width: 10,
    height: 10,
    borderRadius: 5,
    marginRight: 8,
  },
  statusText: {
    color: COLORS.textSecondary,
    fontSize: 13,
  },
  transcript: {
    flex: 1,
    paddingHorizontal: 16,
    paddingTop: 8,
  },
  emptyText: {
    color: COLORS.textMuted,
    fontSize: 14,
    textAlign: 'center',
    marginTop: 40,
  },
  msgGroup: {
    marginBottom: 10,
  },
  userMsg: {
    color: COLORS.orange,
    fontSize: 14,
    fontWeight: '600',
  },
  aiMsg: {
    color: COLORS.green,
    fontSize: 13,
    marginTop: 2,
    paddingLeft: 8,
  },
  pendingMsg: {
    color: COLORS.yellow,
    fontSize: 13,
    fontStyle: 'italic',
    marginTop: 2,
    paddingLeft: 8,
  },
  partialBar: {
    paddingHorizontal: 16,
    paddingVertical: 6,
    backgroundColor: COLORS.surface,
  },
  partialText: {
    color: COLORS.textMuted,
    fontSize: 13,
    fontStyle: 'italic',
  },
  footer: {
    padding: 16,
    alignItems: 'center',
    borderTopWidth: 1,
    borderTopColor: COLORS.orangeDark,
  },
  talkBtn: {
    backgroundColor: COLORS.buttonBg,
    borderRadius: 40,
    paddingVertical: 18,
    paddingHorizontal: 48,
    borderWidth: 2,
    borderColor: COLORS.orange,
  },
  talkBtnActive: {
    backgroundColor: COLORS.orangeDark,
    borderColor: COLORS.green,
  },
  talkBtnDisabled: {
    opacity: 0.4,
    borderColor: COLORS.textMuted,
  },
  talkBtnText: {
    color: COLORS.textPrimary,
    fontSize: 16,
    fontWeight: '700',
    textAlign: 'center',
  },
});

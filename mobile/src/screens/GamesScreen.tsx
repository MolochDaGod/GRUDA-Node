import React from 'react';
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  TouchableOpacity,
  Linking,
} from 'react-native';
import { Ionicons } from '@expo/vector-icons';
import { colors, spacing, radii } from '../theme';
import { GRUDGE_WARLORDS_URL, GRUDGE_STUDIO_URL } from '../config';
import type { GameTitle } from '../types';

/* ── Game registry — add your Play Store titles here ── */
const GAMES: GameTitle[] = [
  {
    id: 'grudge-warlords',
    name: 'Grudge Warlords',
    url: GRUDGE_WARLORDS_URL,
    icon: 'game-controller',
    description: 'MMO souls-like PvP/PvE with faction warfare, crafting, and permadeath crews.',
    playStoreUrl: undefined, // Add Play Store URL when published
  },
  {
    id: 'gruda-wars',
    name: 'Gruda Wars',
    url: `${GRUDGE_STUDIO_URL}/games/gruda-wars`,
    icon: 'planet',
    description: 'MOBA-style arena combat with voxel characters and dynamic lanes.',
    playStoreUrl: undefined,
  },
  {
    id: 'dungeon-crawler',
    name: 'Dungeon Crawler Quest',
    url: 'https://dungeon-crawler-quest.vercel.app',
    icon: 'skull',
    description: 'Procedural dungeon crawling with voxel monsters and AI-generated content.',
    playStoreUrl: undefined,
  },
];

export default function GamesScreen() {
  const openGame = async (game: GameTitle) => {
    // Prefer Play Store app if available, otherwise open web URL
    const url = game.playStoreUrl ?? game.url;
    const canOpen = await Linking.canOpenURL(url);
    if (canOpen) {
      await Linking.openURL(url);
    }
  };

  return (
    <ScrollView style={styles.scroll} contentContainerStyle={styles.container}>
      <Text style={styles.heading}>GAMES</Text>
      <Text style={styles.sub}>Grudge Studio titles — by RacAlvin The Pirate King</Text>

      {GAMES.map((game) => (
        <TouchableOpacity key={game.id} style={styles.card} onPress={() => openGame(game)}>
          <View style={styles.cardHeader}>
            <Ionicons name={game.icon as any} size={24} color={colors.orange} />
            <Text style={styles.cardName}>{game.name}</Text>
            {game.playStoreUrl && (
              <View style={styles.storeBadge}>
                <Text style={styles.storeBadgeText}>PLAY STORE</Text>
              </View>
            )}
          </View>
          <Text style={styles.cardDesc}>{game.description}</Text>
          <View style={styles.cardFooter}>
            <Text style={styles.urlText}>{game.url}</Text>
            <Ionicons name="open-outline" size={16} color={colors.textMuted} />
          </View>
        </TouchableOpacity>
      ))}

      {/* Developer link */}
      <TouchableOpacity
        style={styles.devLink}
        onPress={() => Linking.openURL('https://g.dev/grudge')}
      >
        <Ionicons name="logo-google-playstore" size={18} color={colors.green} />
        <Text style={styles.devLinkText}>View all titles on Google Play</Text>
      </TouchableOpacity>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  scroll: { flex: 1, backgroundColor: colors.bg },
  container: { padding: spacing.lg },
  heading: {
    fontSize: 11,
    letterSpacing: 4,
    color: colors.orange,
    fontWeight: '900',
    marginBottom: spacing.xs,
  },
  sub: {
    fontSize: 13,
    color: colors.textMuted,
    marginBottom: spacing.lg,
  },
  card: {
    backgroundColor: colors.bgCard,
    borderWidth: 1,
    borderColor: colors.border,
    borderRadius: radii.md,
    padding: spacing.lg,
    marginBottom: spacing.md,
  },
  cardHeader: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: spacing.sm,
    marginBottom: spacing.sm,
  },
  cardName: {
    flex: 1,
    fontSize: 16,
    fontWeight: '700',
    color: colors.text,
  },
  storeBadge: {
    backgroundColor: colors.bgInput,
    paddingHorizontal: 8,
    paddingVertical: 2,
    borderRadius: 4,
  },
  storeBadgeText: {
    fontSize: 9,
    color: colors.green,
    letterSpacing: 1,
    fontWeight: '700',
  },
  cardDesc: {
    fontSize: 13,
    color: colors.textSec,
    lineHeight: 20,
    marginBottom: spacing.sm,
  },
  cardFooter: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
  },
  urlText: {
    fontSize: 12,
    color: colors.textMuted,
    fontFamily: 'monospace',
  },
  devLink: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    gap: spacing.sm,
    paddingVertical: spacing.lg,
    marginTop: spacing.md,
    borderTopWidth: 1,
    borderTopColor: colors.border,
  },
  devLinkText: {
    color: colors.green,
    fontSize: 14,
    fontWeight: '600',
  },
});

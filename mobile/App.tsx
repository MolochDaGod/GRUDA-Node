import React from 'react';
import { StatusBar } from 'expo-status-bar';
import { NavigationContainer, DefaultTheme } from '@react-navigation/native';
import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { Ionicons } from '@expo/vector-icons';
import { colors } from './src/theme';

import AccountScreen from './src/screens/AccountScreen';
import WalletScreen from './src/screens/WalletScreen';
import NodeScreen from './src/screens/NodeScreen';
import GamesScreen from './src/screens/GamesScreen';

const Tab = createBottomTabNavigator();

const GrudgeTheme = {
  ...DefaultTheme,
  dark: true,
  colors: {
    ...DefaultTheme.colors,
    primary: colors.orange,
    background: colors.bg,
    card: colors.bgCard,
    text: colors.text,
    border: colors.border,
    notification: colors.orange,
  },
};

const TABS: {
  name: string;
  component: React.ComponentType;
  icon: keyof typeof Ionicons.glyphMap;
}[] = [
  { name: 'Account', component: AccountScreen, icon: 'person' },
  { name: 'Wallet', component: WalletScreen, icon: 'wallet' },
  { name: 'Node', component: NodeScreen, icon: 'server' },
  { name: 'Games', component: GamesScreen, icon: 'game-controller' },
];

export default function App() {
  return (
    <NavigationContainer theme={GrudgeTheme}>
      <StatusBar style="light" />
      <Tab.Navigator
        screenOptions={{
          headerStyle: {
            backgroundColor: colors.bgCard,
            borderBottomWidth: 2,
            borderBottomColor: colors.orange,
          } as any,
          headerTintColor: colors.orange,
          headerTitleStyle: { fontWeight: '900', letterSpacing: 2 },
          tabBarStyle: {
            backgroundColor: colors.bgCard,
            borderTopColor: colors.border,
          },
          tabBarActiveTintColor: colors.orange,
          tabBarInactiveTintColor: colors.textMuted,
        }}
      >
        {TABS.map((tab) => (
          <Tab.Screen
            key={tab.name}
            name={tab.name}
            component={tab.component}
            options={{
              tabBarIcon: ({ color, size }) => (
                <Ionicons name={tab.icon} size={size} color={color} />
              ),
            }}
          />
        ))}
      </Tab.Navigator>
    </NavigationContainer>
  );
}

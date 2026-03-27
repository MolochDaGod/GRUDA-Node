"use strict";

const supportedTokens = [
  {
    symbol: "SOL",
    chain: "solana",
    network: "mainnet-beta",
    address: "native",
    decimals: 9,
    enabled: true,
    swapEnabled: true,
  },
  {
    symbol: "POLY",
    chain: "polygon",
    network: "mainnet",
    address: "native",
    decimals: 18,
    enabled: true,
    swapEnabled: true,
  },
  {
    symbol: "ETH",
    chain: "ethereum",
    network: "mainnet",
    address: "native",
    decimals: 18,
    enabled: true,
    swapEnabled: true,
  },
  {
    symbol: "GBUX",
    chain: "solana",
    network: "mainnet-beta",
    address: "55TpSoMNxbfsNJ9U1dQoo9H3dRtDmjBZVMcKqvU2nray",
    decimals: 9,
    enabled: true,
    swapEnabled: true,
    purchaseUrl: "https://raydium.io/launchpad/token/?mint=55TpSoMNxbfsNJ9U1dQoo9H3dRtDmjBZVMcKqvU2nray",
  },
  {
    symbol: "GRUDA",
    chain: "polygon",
    network: "mainnet",
    address: "0xa6fd32edc7c037b77537b673da9971df0f34a721",
    decimals: 18,
    enabled: true,
    swapEnabled: true,
  },
];

const swapPolicy = {
  mode: "server-routed-device-approved",
  bestPractices: [
    "Never keep production swap routing secrets or admin keys on the ESP32.",
    "Treat the device as a display and approval surface, not a hot backend wallet.",
    "Use allowlisted tokens and chains only.",
    "Require server-side quote generation and slippage checks before signing.",
    "Separate auth identity from on-device local wallet storage.",
    "Prefer session-based auth for mobile/web companions and short-lived device actions.",
  ],
  allowedChains: ["solana", "polygon", "ethereum"],
  allowedTokens: supportedTokens
    .filter((token) => token.enabled)
    .map((token) => token.symbol),
};

module.exports = {
  supportedTokens,
  swapPolicy,
};

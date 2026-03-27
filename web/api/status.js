export default async function handler(req, res) {
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.setHeader("Cache-Control", "s-maxage=5, stale-while-revalidate=10");

  try {
    const upstream = await fetch(
      "https://api.grudge-studio.com/devices/status?type=GRD17",
      {
        headers: { Accept: "application/json" },
        signal: AbortSignal.timeout(8000),
      }
    );

    if (!upstream.ok) {
      return res.status(502).json({
        ok: false,
        error: `Backend returned ${upstream.status}`,
        fallback: getStaticFallback(),
      });
    }

    const data = await upstream.json();
    return res.status(200).json({ ok: true, ...data });
  } catch (err) {
    return res.status(200).json({
      ok: false,
      error: "Backend unreachable",
      fallback: getStaticFallback(),
    });
  }
}

function getStaticFallback() {
  return {
    networkId: "GRUDACHAIN-MAINNET-17",
    chainId: "gruda-17",
    consensus: "Keweebec2-PoS",
    firmware: "1.0.0",
    tokens: [
      { symbol: "SOL", chain: "solana" },
      { symbol: "POLY", chain: "polygon" },
      { symbol: "ETH", chain: "ethereum" },
      { symbol: "GBUX", chain: "solana", address: "55TpSoMNxbfsNJ9U1dQoo9H3dRtDmjBZVMcKqvU2nray" },
      { symbol: "GRUDA", chain: "polygon", address: "0xa6fd32edc7c037b77537b673da9971df0f34a721" },
    ],
  };
}

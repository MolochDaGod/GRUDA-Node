/**
 * GET /api/treaty/history?guild=grudge-studio-default&channel=general&limit=50
 * Returns message history for a guild channel.
 * Proxies from api.grudge-studio.com, returns empty on failure.
 */
export default async function handler(req, res) {
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.setHeader("Cache-Control", "s-maxage=5, stale-while-revalidate=10");

  const guild   = req.query.guild   || "grudge-studio-default";
  const channel = req.query.channel || "general";
  const limit   = parseInt(req.query.limit, 10) || 50;

  try {
    const upstream = await fetch(
      `https://api.grudge-studio.com/treaty/history?guild=${encodeURIComponent(guild)}&channel=${encodeURIComponent(channel)}&limit=${limit}`,
      {
        headers: { Accept: "application/json" },
        signal: AbortSignal.timeout(5000),
      }
    );

    if (upstream.ok) {
      const data = await upstream.json();
      return res.status(200).json({ ok: true, ...data });
    }
  } catch (_) {
    /* Fall through */
  }

  /* No history available — client will rely on WS for live messages */
  return res.status(200).json({
    ok: true,
    guild,
    channel,
    messages: [],
  });
}

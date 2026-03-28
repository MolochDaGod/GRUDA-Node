/**
 * GET /api/treaty/members?guild=grudge-studio-default
 * Returns online members in a guild.
 * Proxies from api.grudge-studio.com, returns empty on failure.
 */
export default async function handler(req, res) {
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.setHeader("Cache-Control", "s-maxage=10, stale-while-revalidate=20");

  const guild = req.query.guild || "grudge-studio-default";

  try {
    const upstream = await fetch(
      `https://api.grudge-studio.com/treaty/members?guild=${encodeURIComponent(guild)}`,
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

  /* Empty member list — WS will populate when connected */
  return res.status(200).json({
    ok: true,
    guild,
    members: [],
  });
}

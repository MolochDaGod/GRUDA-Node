/**
 * GET /api/treaty/channels?guild=grudge-studio-default
 * Returns the guild channel list.
 * Proxies from api.grudge-studio.com, falls back to static default.
 */
export default async function handler(req, res) {
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.setHeader("Cache-Control", "s-maxage=30, stale-while-revalidate=60");

  const guild = req.query.guild || "grudge-studio-default";

  try {
    const upstream = await fetch(
      `https://api.grudge-studio.com/treaty/channels?guild=${encodeURIComponent(guild)}`,
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
    /* Fall through to static */
  }

  /* Static fallback — mirrors include/config.h channel defs */
  return res.status(200).json({
    ok: true,
    guild,
    channels: [
      { id: "node-chat",     name: "# node-chat",     type: "text",  readOnly: false },
      { id: "general",       name: "# general",       type: "text",  readOnly: false },
      { id: "announcements", name: "# announcements", type: "text",  readOnly: true  },
      { id: "treaty-deals",  name: "# treaty-deals",  type: "text",  readOnly: false },
      { id: "rules",         name: "# rules",         type: "info",  readOnly: true  },
      { id: "node-status",   name: "# node-status",   type: "info",  readOnly: true  },
      { id: "vc-lobby",      name: "♫ vc-lobby",       type: "voice", readOnly: false },
      { id: "vc-war-room",   name: "♫ vc-war-room",    type: "voice", readOnly: false },
    ],
  });
}

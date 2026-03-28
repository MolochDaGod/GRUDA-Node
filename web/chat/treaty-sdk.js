/**
 * ═══════════════════════════════════════════════════════════════
 *  Treaty Chat SDK — Grudge Studio
 *  WebSocket client that speaks the same protocol as ESP32 firmware.
 *  Usable standalone (headless) or with treaty-ui.js (full UI).
 * ═══════════════════════════════════════════════════════════════
 */

/* ── Default Guild Channels (mirrors include/config.h) ─────── */
export const GUILD_CHANNELS = [
  { id: "node-chat",     name: "# node-chat",     type: "text",  readOnly: false },
  { id: "general",       name: "# general",       type: "text",  readOnly: false },
  { id: "announcements", name: "# announcements", type: "text",  readOnly: true  },
  { id: "treaty-deals",  name: "# treaty-deals",  type: "text",  readOnly: false },
  { id: "rules",         name: "# rules",         type: "info",  readOnly: true  },
  { id: "node-status",   name: "# node-status",   type: "info",  readOnly: true  },
  { id: "vc-lobby",      name: "♫ vc-lobby",       type: "voice", readOnly: false },
  { id: "vc-war-room",   name: "♫ vc-war-room",    type: "voice", readOnly: false },
];

export const QUICK_REPLIES = [
  "GG",
  "On my way",
  "Need backup",
  "Treaty accepted",
  "Denied",
  "At base",
];

export const FACTIONS = {
  Crusade: "crusade",
  Legion:  "legion",
  Fabled:  "fabled",
};

/* ── Defaults ──────────────────────────────────────────────── */
const DEFAULT_WS_URL     = "wss://ws.grudge-studio.com/treaty";
const DEFAULT_GUILD_ID   = "grudge-studio-default";
const DEFAULT_CHANNEL    = "node-chat";
const MAX_MESSAGES       = 200;          /* per channel ring buffer */
const RECONNECT_BASE_MS  = 2000;
const RECONNECT_MAX_MS   = 30000;
const HEARTBEAT_MS       = 25000;

/* ── TreatyChat ────────────────────────────────────────────── */
export class TreatyChat {
  /**
   * @param {Object} opts
   * @param {string} opts.grudgeId      - Grudge Account ID
   * @param {string} opts.authToken     - Session auth token
   * @param {string} [opts.displayName] - Display name
   * @param {string} [opts.faction]     - Crusade | Legion | Fabled
   * @param {string} [opts.guild]       - Guild ID (default: grudge-studio-default)
   * @param {string} [opts.wsUrl]       - WebSocket URL override
   * @param {string} [opts.apiBase]     - REST API base URL
   */
  constructor(opts = {}) {
    this.grudgeId    = opts.grudgeId    || "";
    this.authToken   = opts.authToken   || "";
    this.displayName = opts.displayName || "";
    this.faction     = opts.faction     || "";
    this.guild       = opts.guild       || DEFAULT_GUILD_ID;
    this.wsUrl       = opts.wsUrl       || DEFAULT_WS_URL;
    this.apiBase     = opts.apiBase     || "";

    /* ── State ────────────────────────────── */
    this.connected      = false;
    this.activeChannel  = DEFAULT_CHANNEL;
    this.channels       = [...GUILD_CHANNELS];
    this.messages       = {};            /* channelId -> TreatyMessage[] */
    this.members        = [];            /* online members */
    this.unread         = {};            /* channelId -> count */

    /* Init message arrays */
    for (const ch of this.channels) {
      this.messages[ch.id] = [];
      this.unread[ch.id]   = 0;
    }

    /* ── Internals ────────────────────────── */
    this._ws             = null;
    this._listeners      = {};
    this._reconnectMs    = RECONNECT_BASE_MS;
    this._reconnectTimer = null;
    this._heartbeatTimer = null;
    this._destroyed      = false;
  }

  /* ── Event Emitter ──────────────────────────────────────── */

  on(event, fn)  { (this._listeners[event] ||= []).push(fn); return this; }
  off(event, fn) {
    const arr = this._listeners[event];
    if (arr) this._listeners[event] = arr.filter(f => f !== fn);
    return this;
  }
  _emit(event, ...args) {
    for (const fn of (this._listeners[event] || [])) {
      try { fn(...args); } catch (e) { console.error(`[Treaty] Event "${event}" handler error:`, e); }
    }
  }

  /* ── Connection ─────────────────────────────────────────── */

  connect() {
    if (this._destroyed) return;
    if (this._ws) this.disconnect();

    try {
      this._ws = new WebSocket(this.wsUrl);
    } catch (err) {
      console.error("[Treaty] WebSocket creation failed:", err);
      this._scheduleReconnect();
      return;
    }

    this._ws.onopen = () => {
      console.log("[Treaty] Connected");
      this.connected = true;
      this._reconnectMs = RECONNECT_BASE_MS;
      this._emit("connected");

      /* Authenticate — same payload as firmware */
      this._send({
        event:   "auth",
        grudgeId: this.grudgeId,
        token:    this.authToken,
        ns:       "/treaty",
        guild:    this.guild,
        name:     this.displayName,
        faction:  this.faction,
      });

      this._startHeartbeat();
    };

    this._ws.onclose = () => {
      console.log("[Treaty] Disconnected");
      this.connected = false;
      this._stopHeartbeat();
      this._emit("disconnected");
      this._scheduleReconnect();
    };

    this._ws.onerror = (err) => {
      console.error("[Treaty] WebSocket error:", err);
      this._ws.close();
    };

    this._ws.onmessage = (evt) => {
      try {
        const msg = JSON.parse(evt.data);
        this._handleMessage(msg);
      } catch (e) {
        console.warn("[Treaty] Bad message:", evt.data);
      }
    };
  }

  disconnect() {
    this._destroyed = false;  /* allow reconnect after explicit disconnect */
    clearTimeout(this._reconnectTimer);
    this._stopHeartbeat();
    if (this._ws) {
      this._ws.onclose = null;  /* prevent reconnect */
      this._ws.close();
      this._ws = null;
    }
    this.connected = false;
    this._emit("disconnected");
  }

  destroy() {
    this._destroyed = true;
    this.disconnect();
    this._listeners = {};
  }

  _send(obj) {
    if (this._ws && this._ws.readyState === WebSocket.OPEN) {
      this._ws.send(JSON.stringify(obj));
    }
  }

  _scheduleReconnect() {
    if (this._destroyed) return;
    clearTimeout(this._reconnectTimer);
    console.log(`[Treaty] Reconnecting in ${this._reconnectMs}ms...`);
    this._reconnectTimer = setTimeout(() => {
      this._reconnectMs = Math.min(this._reconnectMs * 1.5, RECONNECT_MAX_MS);
      this.connect();
    }, this._reconnectMs);
  }

  _startHeartbeat() {
    this._stopHeartbeat();
    this._heartbeatTimer = setInterval(() => {
      this._send({ event: "heartbeat", grudgeId: this.grudgeId });
    }, HEARTBEAT_MS);
  }

  _stopHeartbeat() {
    clearInterval(this._heartbeatTimer);
    this._heartbeatTimer = null;
  }

  /* ── Incoming Message Handler ───────────────────────────── */

  _handleMessage(msg) {
    const event = msg.event;

    if (event === "auth_ok") {
      this._emit("authenticated", msg);
      /* Request channel history if backend supports it */
      this._send({ event: "history", guild: this.guild, channel: this.activeChannel, limit: 50 });
      return;
    }

    if (event === "auth_error") {
      console.error("[Treaty] Auth failed:", msg.error);
      this._emit("auth_error", msg);
      return;
    }

    if (event === "channel_msg" || event === "dm") {
      const tm = {
        id:         msg.id || `${Date.now()}-${Math.random().toString(36).slice(2, 8)}`,
        senderId:   msg.from?.grudgeId || msg.from || "",
        senderName: msg.from?.name     || msg.fromName || "Unknown",
        faction:    msg.from?.faction   || msg.faction  || "",
        channelId:  msg.channel || "",
        text:       msg.text   || "",
        timestamp:  msg.ts     || Date.now(),
        read:       false,
        type:       event,
      };

      const chId = tm.channelId || "__dm";
      if (!this.messages[chId]) this.messages[chId] = [];
      this.messages[chId].push(tm);

      /* Ring buffer */
      if (this.messages[chId].length > MAX_MESSAGES) {
        this.messages[chId] = this.messages[chId].slice(-MAX_MESSAGES);
      }

      /* Unread tracking */
      if (chId !== this.activeChannel) {
        this.unread[chId] = (this.unread[chId] || 0) + 1;
      }

      this._emit("message", tm);
      this._emit(`message:${chId}`, tm);
      return;
    }

    if (event === "history") {
      /* Batch load of historical messages */
      const chId = msg.channel || this.activeChannel;
      const history = (msg.messages || []).map(m => ({
        id:         m.id || `hist-${Math.random().toString(36).slice(2, 8)}`,
        senderId:   m.from?.grudgeId || m.from || "",
        senderName: m.from?.name     || m.fromName || "Unknown",
        faction:    m.from?.faction   || m.faction  || "",
        channelId:  chId,
        text:       m.text || "",
        timestamp:  m.ts   || 0,
        read:       true,
        type:       "channel_msg",
      }));
      this.messages[chId] = [...history, ...(this.messages[chId] || [])];
      if (this.messages[chId].length > MAX_MESSAGES) {
        this.messages[chId] = this.messages[chId].slice(-MAX_MESSAGES);
      }
      this._emit("history", { channel: chId, messages: history });
      return;
    }

    if (event === "members") {
      this.members = msg.members || [];
      this._emit("members", this.members);
      return;
    }

    if (event === "presence") {
      this._emit("presence", msg);
      return;
    }

    if (event === "typing") {
      this._emit("typing", msg);
      return;
    }

    if (event === "error") {
      console.error("[Treaty] Server error:", msg.error);
      this._emit("error", msg);
      return;
    }

    /* Unknown event — pass through */
    this._emit("raw", msg);
  }

  /* ── Public API: Sending ────────────────────────────────── */

  /**
   * Send a message to a guild channel.
   * @param {string} channelId - Channel ID (e.g. "general")
   * @param {string} text      - Message text
   */
  sendChannel(channelId, text) {
    if (!text || !text.trim()) return false;
    const payload = {
      event:     "channel_msg",
      guild:     this.guild,
      channel:   channelId,
      text:      text.trim(),
      from:      this.grudgeId,
      authToken: this.authToken,
    };
    this._send(payload);

    /* Optimistic local insert */
    const tm = {
      id:         `local-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`,
      senderId:   this.grudgeId,
      senderName: this.displayName || this.grudgeId.substring(0, 8),
      faction:    this.faction,
      channelId:  channelId,
      text:       text.trim(),
      timestamp:  Date.now(),
      read:       true,
      type:       "channel_msg",
      local:      true,
    };
    if (!this.messages[channelId]) this.messages[channelId] = [];
    this.messages[channelId].push(tm);
    this._emit("message", tm);
    this._emit(`message:${channelId}`, tm);

    return true;
  }

  /**
   * Send a direct message.
   * @param {string} recipientId - Grudge ID of recipient
   * @param {string} text        - Message text
   */
  sendDM(recipientId, text) {
    if (!text || !text.trim()) return false;
    this._send({
      event:     "dm",
      to:        recipientId,
      text:      text.trim(),
      from:      this.grudgeId,
      authToken: this.authToken,
    });
    return true;
  }

  /**
   * Send a quick reply to the active channel.
   * @param {number} index - Quick reply index (0-5)
   */
  sendQuick(index) {
    if (index < 0 || index >= QUICK_REPLIES.length) return false;
    return this.sendChannel(this.activeChannel, QUICK_REPLIES[index]);
  }

  /* ── Public API: Channel Management ─────────────────────── */

  /**
   * Switch active channel.
   * @param {string} channelId
   */
  switchChannel(channelId) {
    const prev = this.activeChannel;
    this.activeChannel = channelId;
    this.unread[channelId] = 0;

    /* Mark all messages in this channel as read */
    for (const m of (this.messages[channelId] || [])) {
      m.read = true;
    }

    /* Request history for new channel */
    this._send({ event: "history", guild: this.guild, channel: channelId, limit: 50 });

    this._emit("channel_switch", { from: prev, to: channelId });
  }

  /**
   * Get channel config by ID.
   * @param {string} channelId
   */
  getChannel(channelId) {
    return this.channels.find(c => c.id === channelId) || null;
  }

  /**
   * Get total unread count across all channels.
   */
  getTotalUnread() {
    return Object.values(this.unread).reduce((sum, n) => sum + n, 0);
  }

  /**
   * Send typing indicator.
   */
  sendTyping() {
    this._send({
      event:   "typing",
      guild:   this.guild,
      channel: this.activeChannel,
      from:    this.grudgeId,
    });
  }

  /* ── Public API: REST Fallback ──────────────────────────── */

  /**
   * Fetch channel list from REST API.
   */
  async fetchChannels() {
    if (!this.apiBase) return this.channels;
    try {
      const res = await fetch(`${this.apiBase}/api/treaty/channels?guild=${this.guild}`);
      const data = await res.json();
      if (data.channels) {
        this.channels = data.channels;
        for (const ch of this.channels) {
          if (!this.messages[ch.id]) this.messages[ch.id] = [];
          if (this.unread[ch.id] === undefined) this.unread[ch.id] = 0;
        }
        this._emit("channels", this.channels);
      }
      return this.channels;
    } catch (e) {
      console.warn("[Treaty] Failed to fetch channels:", e);
      return this.channels;
    }
  }

  /**
   * Fetch message history from REST API.
   * @param {string} channelId
   * @param {number} [limit=50]
   */
  async fetchHistory(channelId, limit = 50) {
    if (!this.apiBase) return [];
    try {
      const res = await fetch(
        `${this.apiBase}/api/treaty/history?guild=${this.guild}&channel=${channelId}&limit=${limit}`
      );
      const data = await res.json();
      return data.messages || [];
    } catch (e) {
      console.warn("[Treaty] Failed to fetch history:", e);
      return [];
    }
  }

  /**
   * Fetch online members from REST API.
   */
  async fetchMembers() {
    if (!this.apiBase) return this.members;
    try {
      const res = await fetch(`${this.apiBase}/api/treaty/members?guild=${this.guild}`);
      const data = await res.json();
      if (data.members) {
        this.members = data.members;
        this._emit("members", this.members);
      }
      return this.members;
    } catch (e) {
      console.warn("[Treaty] Failed to fetch members:", e);
      return this.members;
    }
  }
}

/* ── UMD fallback (for <script> tag usage in GDevelop, etc.) ── */
if (typeof window !== "undefined") {
  window.TreatyChat    = TreatyChat;
  window.GUILD_CHANNELS = GUILD_CHANNELS;
  window.QUICK_REPLIES  = QUICK_REPLIES;
  window.FACTIONS       = FACTIONS;
}

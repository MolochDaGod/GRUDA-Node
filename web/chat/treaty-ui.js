/**
 * ═══════════════════════════════════════════════════════════════
 *  Treaty Chat UI — Grudge Studio
 *  Renders the Discord-like chat interface and binds to TreatyChat SDK.
 * ═══════════════════════════════════════════════════════════════
 */

import { GUILD_CHANNELS, QUICK_REPLIES, FACTIONS } from "./treaty-sdk.js";

/* ── Helpers ──────────────────────────────────────────────── */

function el(tag, cls, html) {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (html !== undefined) e.innerHTML = html;
  return e;
}

function factionClass(faction) {
  const f = (faction || "").toLowerCase();
  if (f === "crusade") return "crusade";
  if (f === "legion")  return "legion";
  if (f === "fabled")  return "fabled";
  return "default";
}

function channelIcon(type) {
  if (type === "voice") return "♫";
  if (type === "info")  return "ℹ";
  return "#";
}

function formatTime(ts) {
  const d = new Date(typeof ts === "number" && ts < 1e12 ? ts * 1000 : ts);
  if (isNaN(d.getTime())) return "";
  const now = new Date();
  const sameDay = d.toDateString() === now.toDateString();
  const time = d.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
  if (sameDay) return `Today at ${time}`;
  return `${d.toLocaleDateString([], { month: "short", day: "numeric" })} ${time}`;
}

function initials(name) {
  if (!name) return "?";
  const parts = name.split(/[\s-]+/);
  return parts.length > 1
    ? (parts[0][0] + parts[1][0]).toUpperCase()
    : name.substring(0, 2).toUpperCase();
}

/* ── TreatyUI ─────────────────────────────────────────────── */

export class TreatyUI {
  /**
   * @param {import('./treaty-sdk.js').TreatyChat} chat - SDK instance
   * @param {HTMLElement} root - Container element
   * @param {Object} [opts]
   * @param {boolean} [opts.embed]   - Embed mode (no sidebar/members)
   * @param {string}  [opts.channel] - Initial channel override
   */
  constructor(chat, root, opts = {}) {
    this.chat = chat;
    this.root = root;
    this.embed = opts.embed || false;
    this._membersVisible = !this.embed;
    this._typingTimeout = null;

    if (opts.channel) {
      this.chat.activeChannel = opts.channel;
    }

    if (this.embed) {
      document.body.classList.add("embed-mode");
    }

    this._build();
    this._bind();
    this._renderChannels();
    this._renderMessages();
    this._updateConnectionDot();
  }

  /* ── Build DOM ──────────────────────────────────────────── */

  _build() {
    this.root.innerHTML = "";
    this.root.className = "treaty-app";

    /* ── Sidebar ─────────────────────────── */
    this.$sidebar = el("aside", "treaty-sidebar");

    const sidebarHeader = el("div", "treaty-sidebar__header");
    sidebarHeader.appendChild(el("div", "treaty-sidebar__guild-icon", "⬡"));
    sidebarHeader.appendChild(el("div", "treaty-sidebar__guild-name", "Grudge Studio"));
    this.$sidebar.appendChild(sidebarHeader);

    this.$channelList = el("div", "treaty-sidebar__channels");
    this.$sidebar.appendChild(this.$channelList);

    /* User footer */
    this.$sidebarFooter = el("div", "treaty-sidebar__footer");
    this.$userAvatar = el("div", "treaty-sidebar__avatar");
    this.$userName = el("div", "treaty-sidebar__user-name", "Connecting…");
    this.$userId = el("div", "treaty-sidebar__user-id");
    const userInfo = el("div", "");
    userInfo.style.cssText = "flex:1;min-width:0;";
    userInfo.appendChild(this.$userName);
    userInfo.appendChild(this.$userId);
    this.$sidebarFooter.appendChild(this.$userAvatar);
    this.$sidebarFooter.appendChild(userInfo);
    this.$sidebar.appendChild(this.$sidebarFooter);

    this.root.appendChild(this.$sidebar);

    /* ── Main ────────────────────────────── */
    this.$main = el("div", "treaty-main");

    /* Header */
    this.$header = el("header", "treaty-header");
    this.$hamburger = el("button", "treaty-header__hamburger", "☰");
    this.$headerIcon = el("span", "treaty-header__channel-icon", "#");
    this.$headerName = el("span", "treaty-header__channel-name", "node-chat");
    this.$headerDivider = el("div", "treaty-header__divider");
    this.$headerTopic = el("span", "treaty-header__topic", "Welcome to Treaty Chat");

    this.$headerStatus = el("div", "treaty-header__status");
    this.$connDot = el("div", "treaty-header__dot");
    this.$connLabel = el("span", "", "Connecting…");
    this.$headerStatus.appendChild(this.$connDot);
    this.$headerStatus.appendChild(this.$connLabel);

    this.$membersToggle = el("button", "treaty-header__members-toggle", "👥");
    this.$membersToggle.title = "Toggle Members";

    this.$header.append(
      this.$hamburger, this.$headerIcon, this.$headerName,
      this.$headerDivider, this.$headerTopic,
      this.$headerStatus, this.$membersToggle
    );
    this.$main.appendChild(this.$header);

    /* Content (messages + members side-by-side) */
    this.$content = el("div", "treaty-content");

    /* Messages column */
    this.$messagesCol = el("div", "treaty-messages");
    this.$msgScroll = el("div", "treaty-messages__scroll");
    this.$messagesCol.appendChild(this.$msgScroll);

    /* Compose */
    this.$compose = el("div", "treaty-compose");
    this._buildCompose();
    this.$messagesCol.appendChild(this.$compose);

    this.$content.appendChild(this.$messagesCol);

    /* Members panel */
    this.$members = el("aside", "treaty-members");
    if (this._membersVisible) this.$members.classList.add("visible");
    this.$content.appendChild(this.$members);

    this.$main.appendChild(this.$content);
    this.root.appendChild(this.$main);
  }

  _buildCompose() {
    this.$compose.innerHTML = "";

    const ch = this.chat.getChannel(this.chat.activeChannel);
    if (ch && ch.readOnly) {
      this.$compose.className = "treaty-compose treaty-compose--readonly";
      this.$compose.textContent = "This channel is read-only";
      this.$composeInput = null;
      return;
    }

    if (ch && ch.type === "voice") {
      this.$compose.className = "treaty-compose treaty-compose--readonly";
      this.$compose.textContent = "🎙 Voice channels coming soon";
      this.$composeInput = null;
      return;
    }

    this.$compose.className = "treaty-compose";

    /* Quick replies */
    const quickRow = el("div", "treaty-compose__quick");
    for (let i = 0; i < QUICK_REPLIES.length; i++) {
      const btn = el("button", "treaty-compose__quick-btn", QUICK_REPLIES[i]);
      btn.addEventListener("click", () => this.chat.sendQuick(i));
      quickRow.appendChild(btn);
    }
    this.$compose.appendChild(quickRow);

    /* Input bar */
    const bar = el("div", "treaty-compose__bar");
    this.$composeInput = document.createElement("textarea");
    this.$composeInput.className = "treaty-compose__input";
    this.$composeInput.placeholder = `Message #${this.chat.activeChannel}`;
    this.$composeInput.rows = 1;

    this.$sendBtn = el("button", "treaty-compose__send", "➤");
    this.$sendBtn.title = "Send";

    bar.appendChild(this.$composeInput);
    bar.appendChild(this.$sendBtn);
    this.$compose.appendChild(bar);

    /* Events */
    this.$sendBtn.addEventListener("click", () => this._sendMessage());
    this.$composeInput.addEventListener("keydown", (e) => {
      if (e.key === "Enter" && !e.shiftKey) {
        e.preventDefault();
        this._sendMessage();
      }
    });

    /* Auto-resize textarea */
    this.$composeInput.addEventListener("input", () => {
      this.$composeInput.style.height = "auto";
      this.$composeInput.style.height = Math.min(this.$composeInput.scrollHeight, 120) + "px";

      /* Typing indicator (throttled) */
      clearTimeout(this._typingTimeout);
      this._typingTimeout = setTimeout(() => this.chat.sendTyping(), 2000);
    });
  }

  /* ── Bind SDK Events ───────────────────────────────────── */

  _bind() {
    this.chat.on("connected", () => this._updateConnectionDot());
    this.chat.on("disconnected", () => this._updateConnectionDot());

    this.chat.on("authenticated", (msg) => {
      this._updateUser();
      this._renderMessages();
    });

    this.chat.on("message", (msg) => {
      if (msg.channelId === this.chat.activeChannel) {
        this._appendMessage(msg);
        this._scrollToBottom();
      }
      this._updateUnreadBadges();
    });

    this.chat.on("history", ({ channel }) => {
      if (channel === this.chat.activeChannel) {
        this._renderMessages();
      }
    });

    this.chat.on("channel_switch", () => {
      this._renderChannels();
      this._renderMessages();
      this._buildCompose();
      this._updateHeader();
    });

    this.chat.on("members", () => this._renderMembers());

    /* UI controls */
    this.$membersToggle.addEventListener("click", () => {
      this._membersVisible = !this._membersVisible;
      this.$members.classList.toggle("visible", this._membersVisible);
      this.$membersToggle.classList.toggle("active", this._membersVisible);
    });

    this.$hamburger.addEventListener("click", () => {
      this.$sidebar.classList.toggle("open");
    });
  }

  /* ── Render: Channels ──────────────────────────────────── */

  _renderChannels() {
    this.$channelList.innerHTML = "";

    const textChannels = this.chat.channels.filter(c => c.type === "text" || c.type === "info");
    const voiceChannels = this.chat.channels.filter(c => c.type === "voice");

    if (textChannels.length) {
      this.$channelList.appendChild(el("div", "channel-category", "Text Channels"));
      for (const ch of textChannels) {
        this.$channelList.appendChild(this._channelBtn(ch));
      }
    }

    if (voiceChannels.length) {
      this.$channelList.appendChild(el("div", "channel-category", "Voice Channels"));
      for (const ch of voiceChannels) {
        this.$channelList.appendChild(this._channelBtn(ch));
      }
    }

    this._updateHeader();
  }

  _channelBtn(ch) {
    const active = ch.id === this.chat.activeChannel;
    const classes = [
      "channel-btn",
      active ? "active" : "",
      ch.readOnly ? "channel-btn--readonly" : "",
      ch.type === "voice" ? "channel-btn--voice" : "",
    ].filter(Boolean).join(" ");

    const btn = el("button", classes);
    btn.appendChild(el("span", "channel-btn__icon", channelIcon(ch.type)));
    btn.appendChild(el("span", "channel-btn__name", ch.id));

    const badge = el("span", "channel-btn__badge");
    const unread = this.chat.unread[ch.id] || 0;
    if (unread > 0) badge.textContent = unread > 99 ? "99+" : unread;
    btn.appendChild(badge);

    btn.addEventListener("click", () => {
      this.chat.switchChannel(ch.id);
      /* Close mobile sidebar */
      this.$sidebar.classList.remove("open");
    });

    return btn;
  }

  _updateUnreadBadges() {
    const badges = this.$channelList.querySelectorAll(".channel-btn");
    badges.forEach((btn) => {
      const nameEl = btn.querySelector(".channel-btn__name");
      const badgeEl = btn.querySelector(".channel-btn__badge");
      if (!nameEl || !badgeEl) return;
      const chId = nameEl.textContent;
      const unread = this.chat.unread[chId] || 0;
      badgeEl.textContent = unread > 0 ? (unread > 99 ? "99+" : unread) : "";
    });
  }

  /* ── Render: Header ────────────────────────────────────── */

  _updateHeader() {
    const ch = this.chat.getChannel(this.chat.activeChannel);
    if (ch) {
      this.$headerIcon.textContent = channelIcon(ch.type);
      this.$headerName.textContent = ch.id;
      this.$headerTopic.textContent = ch.readOnly
        ? "Read-only channel"
        : ch.type === "voice"
        ? "Voice channel — coming soon"
        : `Welcome to #${ch.id}`;
    }
  }

  _updateConnectionDot() {
    if (this.chat.connected) {
      this.$connDot.className = "treaty-header__dot connected";
      this.$connLabel.textContent = "Connected";
    } else {
      this.$connDot.className = "treaty-header__dot";
      this.$connLabel.textContent = "Connecting…";
    }
  }

  /* ── Render: User Info ─────────────────────────────────── */

  _updateUser() {
    const name = this.chat.displayName || this.chat.grudgeId.substring(0, 12) || "User";
    this.$userName.textContent = name;
    this.$userId.textContent = this.chat.grudgeId ? this.chat.grudgeId.substring(0, 16) : "";
    this.$userAvatar.textContent = initials(name);
    this.$userAvatar.style.color = `var(--faction-${factionClass(this.chat.faction)}, var(--text))`;
  }

  /* ── Render: Messages ──────────────────────────────────── */

  _renderMessages() {
    this.$msgScroll.innerHTML = "";

    const chId = this.chat.activeChannel;
    const ch = this.chat.getChannel(chId);
    const msgs = this.chat.messages[chId] || [];

    /* Welcome header */
    const welcome = el("div", "treaty-messages__welcome");
    welcome.innerHTML = `
      <h2>${channelIcon(ch?.type || "text")} ${chId}</h2>
      <p>This is the start of #${chId}. ${ch?.readOnly ? "This is a read-only channel." : "Send a treaty to begin."}</p>
    `;
    this.$msgScroll.appendChild(welcome);

    /* Messages */
    let lastSender = "";
    for (const msg of msgs) {
      const compact = msg.senderId === lastSender;
      this.$msgScroll.appendChild(this._msgEl(msg, compact));
      lastSender = msg.senderId;
    }

    this._scrollToBottom();
  }

  _appendMessage(msg) {
    const msgs = this.chat.messages[this.chat.activeChannel] || [];
    const prev = msgs.length > 1 ? msgs[msgs.length - 2] : null;
    const compact = prev && prev.senderId === msg.senderId;
    this.$msgScroll.appendChild(this._msgEl(msg, compact));
  }

  _msgEl(msg, compact) {
    const div = el("div", `treaty-msg${compact ? " compact" : ""}`);

    /* Avatar */
    const avatar = el("div", "treaty-msg__avatar");
    avatar.textContent = initials(msg.senderName);
    avatar.style.color = `var(--faction-${factionClass(msg.faction)}, var(--text-sec))`;
    div.appendChild(avatar);

    /* Body */
    const body = el("div", "treaty-msg__body");

    const header = el("div", "treaty-msg__header");
    const name = el("span", `treaty-msg__name ${factionClass(msg.faction)}`, msg.senderName);
    const time = el("span", "treaty-msg__time", formatTime(msg.timestamp));
    header.appendChild(name);
    header.appendChild(time);
    body.appendChild(header);

    const text = el("div", "treaty-msg__text", this._escapeHtml(msg.text));
    body.appendChild(text);

    div.appendChild(body);
    return div;
  }

  _escapeHtml(str) {
    const div = document.createElement("div");
    div.textContent = str;
    return div.innerHTML;
  }

  _scrollToBottom() {
    requestAnimationFrame(() => {
      this.$msgScroll.scrollTop = this.$msgScroll.scrollHeight;
    });
  }

  /* ── Render: Members ───────────────────────────────────── */

  _renderMembers() {
    this.$members.innerHTML = "";

    const byFaction = {};
    for (const m of this.chat.members) {
      const f = m.faction || "Other";
      (byFaction[f] ||= []).push(m);
    }

    for (const [faction, members] of Object.entries(byFaction)) {
      const group = el("div", "treaty-members__group");
      group.appendChild(
        el("div", "treaty-members__group-title", `${faction} — ${members.length}`)
      );
      for (const m of members) {
        const row = el("div", "treaty-member");
        const avatar = el("div", "treaty-member__avatar");
        avatar.textContent = initials(m.name || m.grudgeId);
        row.appendChild(avatar);
        row.appendChild(el("span", `treaty-member__name ${factionClass(m.faction)}`, m.name || m.grudgeId));
        row.appendChild(el("div", "treaty-member__status"));
        row.addEventListener("click", () => {
          /* Future: open DM panel */
          console.log("[Treaty UI] Clicked member:", m.grudgeId);
        });
        group.appendChild(row);
      }
      this.$members.appendChild(group);
    }

    /* If no members data yet, show placeholder */
    if (this.chat.members.length === 0) {
      const placeholder = el("div", "treaty-members__group");
      placeholder.appendChild(el("div", "treaty-members__group-title", "Online — 0"));
      const hint = el("div", "treaty-member");
      hint.innerHTML = `<span style="color:var(--text-muted);font-size:12px;padding:8px;">Members will appear when connected</span>`;
      placeholder.appendChild(hint);
      this.$members.appendChild(placeholder);
    }
  }

  /* ── Send ───────────────────────────────────────────────── */

  _sendMessage() {
    if (!this.$composeInput) return;
    const text = this.$composeInput.value.trim();
    if (!text) return;

    this.chat.sendChannel(this.chat.activeChannel, text);
    this.$composeInput.value = "";
    this.$composeInput.style.height = "auto";
    this.$composeInput.focus();
  }
}

/* ── UMD fallback ─────────────────────────────────────────── */
if (typeof window !== "undefined") {
  window.TreatyUI = TreatyUI;
}

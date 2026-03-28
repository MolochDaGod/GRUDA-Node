"use strict";

const http = require("http");
const fs = require("fs");
const path = require("path");
const { spawn, exec } = require("child_process");
require("dotenv").config({ path: path.resolve(__dirname, "..", ".env") });
const express = require("express");
const WebSocket = require("ws");
const { supportedTokens, swapPolicy } = require("./config/tokens");
const { createRemoteJWKSet, jwtVerify } = require("jose");

/* ── Config ────────────────────────────────────────── */
const PROJECT_DIR = path.resolve(__dirname, "..");
const PORT = 3000;
const PIO_SCRIPTS =
  "C:\\Users\\nugye\\AppData\\Local\\Programs\\Python\\Python313\\Scripts";

function getPublicRuntimeConfig() {
  return {
    grudgeIdUrl: process.env.GRUDGE_ID_URL || "",
    web3AuthClientId: process.env.WEB3_Client_ID || "",
    web3AuthJwksEndpoint: process.env.WEB3_JWKS_Endpoint || "",
    supportedTokens,
    swapPolicy,
  };
}

// Ensure pio is findable
process.env.PATH = `${process.env.PATH};${PIO_SCRIPTS}`;

/* ── Express ───────────────────────────────────────── */
const app = express();
const server = http.createServer(app);

app.use(express.json());
app.use(express.static(path.join(__dirname, "public")));

/* ── WebSocket ─────────────────────────────────────── */
const wss = new WebSocket.Server({ server });

function broadcast(obj) {
  const msg = JSON.stringify(obj);
  wss.clients.forEach((c) => {
    if (c.readyState === WebSocket.OPEN) c.send(msg);
  });
}

/* ── Process Management ────────────────────────────── */
let activeProc = null;
let activeName = null;

function killActive() {
  if (!activeProc) return;
  try {
    spawn("taskkill", ["/PID", String(activeProc.pid), "/T", "/F"], {
      shell: true,
    });
  } catch (_) {}
  activeProc = null;
  activeName = null;
}

function runCmd(cmd, args, name) {
  if (activeProc) {
    broadcast({
      type: "error",
      msg: `[BUSY] "${activeName}" is still running. Stop it first.`,
    });
    return false;
  }

  broadcast({ type: "start", name, cmd: `${cmd} ${args.join(" ")}` });

  const proc = spawn(cmd, args, {
    cwd: PROJECT_DIR,
    shell: true,
    env: process.env,
  });

  activeProc = proc;
  activeName = name;

  proc.stdout.on("data", (d) => {
    const text = d.toString();
    parseDeviceAccountText(text);
    broadcast({ type: "stdout", data: text });
  });
  proc.stderr.on("data", (d) =>
    broadcast({ type: "stderr", data: d.toString() }),
  );

  proc.on("close", (code) => {
    broadcast({ type: "done", name, code });
    if (activeProc === proc) {
      activeProc = null;
      activeName = null;
    }
  });

  proc.on("error", (err) => {
    broadcast({ type: "error", msg: `[ERROR] ${err.message}` });
    if (activeProc === proc) {
      activeProc = null;
      activeName = null;
    }
  });

  return true;
}

/* ── COM Port Detection ────────────────────────────── */
function getPorts(cb) {
  exec("pio device list", { env: process.env }, (err, stdout) => {
    const ports = [];
    if (!err && stdout) {
      stdout.split("\n").forEach((line) => {
        const m = line.match(/^(COM\d+)/i);
        if (m && !ports.includes(m[1])) ports.push(m[1]);
      });
    }
    // Always fallback-check mode command
    exec("mode", (_e, out) => {
      const fallback = (out || "").match(/COM\d+/gi) || [];
      fallback.forEach((p) => {
        const u = p.toUpperCase();
        if (!ports.includes(u)) ports.push(u);
      });
      cb(ports);
    });
  });
}

/* ── REST API ──────────────────────────────────────── */

app.get("/api/status", (_req, res) => {
  res.json({ running: !!activeProc, process: activeName });
});

app.get("/api/runtime-config", (_req, res) => {
  res.json(getPublicRuntimeConfig());
});

app.get("/api/tokens", (_req, res) => {
  res.json({ tokens: supportedTokens });
});

app.get("/api/swap-policy", (_req, res) => {
  res.json(swapPolicy);
});

app.get("/api/auth/providers", (_req, res) => {
  res.json({
    providers: [
      {
        id: "web3auth",
        enabled: Boolean(process.env.WEB3_Client_ID),
        clientIdConfigured: Boolean(process.env.WEB3_Client_ID),
        jwksConfigured: Boolean(process.env.WEB3_JWKS_Endpoint),
      },
    ],
  });
});

app.get("/api/ports", (_req, res) => {
  getPorts((ports) => res.json({ ports }));
});

app.get("/api/device-info", (_req, res) => {
  try {
    const iniPath = path.join(PROJECT_DIR, "platformio.ini");
    const raw = fs.readFileSync(iniPath, "utf8");
    const info = { board: "", framework: "", uploadPort: "", monitorSpeed: "" };
    for (const line of raw.split("\n")) {
      const t = line.trim();
      if (t.startsWith("board =")) info.board = t.split("=")[1].trim();
      if (t.startsWith("framework =")) info.framework = t.split("=")[1].trim();
      if (t.startsWith("upload_port =")) info.uploadPort = t.split("=")[1].trim();
      if (t.startsWith("monitor_speed =")) info.monitorSpeed = t.split("=")[1].trim();
    }
    res.json(info);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

/* ── Grudge Account (device state via serial cache) ── */
let deviceAccount = {
  loggedIn: false,
  grudgeId: "",
  displayName: "",
  lastEvent: "No device session observed yet",
};

function updateDeviceAccount(next) {
  deviceAccount = { ...deviceAccount, ...next };
  broadcast({ type: "account", account: deviceAccount });
}

function parseDeviceAccountText(text) {
  if (!text) return;

  const resumed = text.match(/\[ACCT\] Resumed session:\s+(.+?)\s+\((.+?)\)/);
  if (resumed) {
    updateDeviceAccount({
      loggedIn: true,
      displayName: resumed[1],
      grudgeId: resumed[2],
      lastEvent: "Session resumed from device storage",
    });
    return;
  }

  const loggedIn = text.match(/\[ACCT\] Logged in as\s+(.+?)\s+\((.+?)\)/);
  if (loggedIn) {
    updateDeviceAccount({
      loggedIn: true,
      displayName: loggedIn[1],
      grudgeId: loggedIn[2],
      lastEvent: "Device login completed",
    });
    return;
  }

  if (text.includes("[ACCT] Logged out")) {
    updateDeviceAccount({
      loggedIn: false,
      displayName: "",
      grudgeId: "",
      lastEvent: "Device logged out",
    });
  }
}

function sendSerialCommand(port, command, cb) {
  const script = [
    `$p = New-Object System.IO.Ports.SerialPort('{{PORT}}', 115200)`,
    "$p.NewLine = \"`r`n\"",
    "$p.ReadTimeout = 1000",
    "$p.WriteTimeout = 1000",
    "$p.Open()",
    `$p.WriteLine('{{COMMAND}}')`,
    "$p.Close()",
  ]
    .join("; ")
    .replace("{{PORT}}", port)
    .replace("{{COMMAND}}", command);

  exec(`pwsh -NoProfile -Command "${script}"`, (err, stdout, stderr) => {
    cb(err, stdout, stderr);
  });
}

app.get("/api/account/status", (_req, res) => {
  res.json(deviceAccount);
});

app.post("/api/account/update", (req, res) => {
  const { loggedIn, grudgeId, displayName } = req.body || {};
  updateDeviceAccount({
    loggedIn: Boolean(loggedIn),
    grudgeId: grudgeId || "",
    displayName: displayName || "",
    lastEvent: "Updated from DevApp API",
  });
  res.json({ ok: true });
});

app.post("/api/account/logout", (req, res) => {
  const port = req.body?.port || "COM6";
  sendSerialCommand(port, "LOGOUT", (err) => {
    if (err) {
      return res.status(500).json({ ok: false, error: err.message });
    }
    updateDeviceAccount({
      loggedIn: false,
      displayName: "",
      grudgeId: "",
      lastEvent: `Logout requested on ${port}`,
    });
    broadcast({
      type: "info",
      msg: `[ACCOUNT] Sent LOGOUT to device on ${port}`,
    });
    res.json({ ok: true });
  });
});

app.get("/api/env-check", (_req, res) => {
  const required = ["GRUDGE_ID_URL", "WEB3_Client_ID", "WEB3_JWKS_Endpoint"];
  const status = {};
  for (const key of required) {
    status[key] = Boolean(process.env[key]);
  }
  res.json({ ok: required.every((k) => process.env[k]), status });
});

/* ── Web3Auth JWT Verification ─────────────────────── */
const JWKS_URL = process.env.WEB3_JWKS_Endpoint || "https://api-auth.web3auth.io/.well-known/jwks.json";
let jwks = null;

function getJWKS() {
  if (!jwks) {
    jwks = createRemoteJWKSet(new URL(JWKS_URL));
  }
  return jwks;
}

/**
 * POST /api/auth/web3auth/verify
 * Verifies a Web3Auth idToken and returns a Grudge session.
 * Body: { idToken, verifier, verifierId }
 */
app.post("/api/auth/web3auth/verify", async (req, res) => {
  try {
    const { idToken } = req.body;
    if (!idToken) {
      return res.status(400).json({ ok: false, error: "Missing idToken" });
    }

    /* Verify JWT signature against Web3Auth JWKS */
    const { payload } = await jwtVerify(idToken, getJWKS(), {
      issuer: "https://api-auth.web3auth.io",
    });

    /* Extract user info from JWT claims */
    const grudgeId = payload.sub || payload.wallet_address || "";
    const email = payload.email || "";
    const name = payload.name || payload.nickname || email.split("@")[0] || "User";

    /* Create Grudge session (in production, call api.grudge-studio.com) */
    const session = {
      ok: true,
      grudgeId: `GID-${grudgeId.substring(0, 16)}`,
      displayName: name,
      email,
      token: idToken, /* Pass-through for device pairing */
      expiresAt: payload.exp || 0,
      provider: "web3auth",
      web3AuthSub: payload.sub,
    };

    console.log(`[AUTH] Web3Auth verified: ${name} (${email})`);
    broadcast({ type: "info", msg: `[AUTH] Web3Auth login: ${name}` });
    res.json(session);
  } catch (err) {
    console.error(`[AUTH] Web3Auth verify failed: ${err.message}`);
    res.status(401).json({ ok: false, error: err.message });
  }
});

/**
 * POST /api/auth/web3auth/pair
 * Links a Web3Auth session to a device pairing code.
 * Called by the web frontend after Web3Auth login.
 * Body: { code, idToken, grudgeId, displayName }
 */
const pendingPairings = new Map(); /* code -> session */

app.post("/api/auth/web3auth/pair", (req, res) => {
  const { code, idToken, grudgeId, displayName } = req.body;
  if (!code || !idToken) {
    return res.status(400).json({ ok: false, error: "Missing code or token" });
  }
  pendingPairings.set(code.toUpperCase(), {
    grudgeId,
    displayName,
    token: idToken,
    expiresAt: Math.floor(Date.now() / 1000) + 86400,
    createdAt: Date.now(),
  });
  /* Expire old pairings after 10 min */
  setTimeout(() => pendingPairings.delete(code.toUpperCase()), 600000);
  console.log(`[AUTH] Pairing code ${code} linked to ${displayName}`);
  res.json({ ok: true });
});

/**
 * GET /api/device/auth/poll?code=XXXXXX
 * Device polls this to check if pairing code has been claimed.
 */
app.get("/api/device/auth/poll", (req, res) => {
  const code = (req.query.code || "").toUpperCase();
  const session = pendingPairings.get(code);
  if (!session) {
    return res.json({ status: "pending" });
  }
  pendingPairings.delete(code);
  res.json({
    status: "authorized",
    grudgeId: session.grudgeId,
    displayName: session.displayName,
    token: session.token,
    expiresAt: session.expiresAt,
  });
});

app.post("/api/kill", (_req, res) => {
  if (activeProc) {
    killActive();
    broadcast({ type: "killed", msg: "[STOPPED] Process killed by user." });
    res.json({ ok: true });
  } else {
    res.json({ ok: false, msg: "Nothing running" });
  }
});

app.post("/api/build", (_req, res) => {
  const ok = runCmd("pio", ["run", "-d", PROJECT_DIR], "Build");
  res.json({ ok });
});

app.post("/api/upload", (req, res) => {
  const port = req.body?.port || "COM6";
  const ok = runCmd(
    "pio",
    ["run", "-d", PROJECT_DIR, "--target", "upload", "--upload-port", port],
    "Upload",
  );
  res.json({ ok });
});

app.post("/api/flash", (req, res) => {
  // Build + Upload + open Monitor on success
  const port = req.body?.port || "COM6";
  if (activeProc) {
    broadcast({ type: "error", msg: "[BUSY] Stop current process first." });
    return res.json({ ok: false });
  }

  broadcast({
    type: "start",
    name: "Flash",
    cmd: `pio run --target upload (then monitor)`,
  });

  const proc = spawn(
    "pio",
    ["run", "-d", PROJECT_DIR, "--target", "upload", "--upload-port", port],
    {
      cwd: PROJECT_DIR,
      shell: true,
      env: process.env,
    },
  );
  activeProc = proc;
  activeName = "Flash";

  proc.stdout.on("data", (d) =>
    broadcast({ type: "stdout", data: d.toString() }),
  );
  proc.stderr.on("data", (d) =>
    broadcast({ type: "stderr", data: d.toString() }),
  );
  proc.on("error", (err) => {
    broadcast({ type: "error", msg: err.message });
    activeProc = null;
    activeName = null;
  });
  proc.on("close", (code) => {
    broadcast({ type: "done", name: "Flash", code });
    activeProc = null;
    activeName = null;
    if (code === 0) {
      setTimeout(() => {
        broadcast({
          type: "info",
          msg: "[MONITOR] Starting serial monitor...",
        });
        runCmd(
          "pio",
          ["device", "monitor", "--port", port, "--baud", "115200"],
          "Monitor",
        );
      }, 1500);
    }
  });

  res.json({ ok: true });
});

app.post("/api/monitor", (req, res) => {
  const port = req.body?.port || "COM6";
  const ok = runCmd(
    "pio",
    ["device", "monitor", "--port", port, "--baud", "115200"],
    "Monitor",
  );
  res.json({ ok });
});

app.post("/api/clean", (_req, res) => {
  const ok = runCmd(
    "pio",
    ["run", "-d", PROJECT_DIR, "--target", "clean"],
    "Clean",
  );
  res.json({ ok });
});

app.post("/api/images", (_req, res) => {
  const script = path.join(PROJECT_DIR, "scripts", "convert_images.ps1");
  const ok = runCmd("pwsh", ["-File", script], "Convert Images");
  res.json({ ok });
});

/* ── Crossmint NFT Integration ─────────────────── */
const CROSSMINT_API = process.env.CROSSMINT_ENV === "production"
  ? "https://www.crossmint.com/api" : "https://staging.crossmint.com/api";
const CROSSMINT_COLLECTION = process.env.CROSSMINT_COLLECTION_ID || "2397b172-1803-403f-9d30-4dc553776c58";
const CROSSMINT_TEMPLATES = [
  "0100715c-1039-4a91-95c2-4ec9d6c53d76",
  "b7aa8645-f224-479e-abc9-b26bc3760fdf",
];

async function crossmintFetch(urlPath) {
  const apiKey = process.env.CROSSMINT_API_KEY;
  if (!apiKey) return { error: "CROSSMINT_API_KEY not configured" };
  const res = await fetch(`${CROSSMINT_API}${urlPath}`, {
    headers: { "X-API-KEY": apiKey, Accept: "application/json" },
  });
  if (!res.ok) return { error: `Crossmint ${res.status}` };
  return res.json();
}

/**
 * GET /api/nfts
 * Returns NFT list from Crossmint templates, normalized for device display.
 * The ESP32 fetches this to populate the NFT Gallery tab.
 */
app.get("/api/nfts", async (_req, res) => {
  try {
    const nfts = [];
    for (const tid of CROSSMINT_TEMPLATES) {
      const t = await crossmintFetch(
        `/2022-06-09/collections/${CROSSMINT_COLLECTION}/templates/${tid}`
      );
      if (t.error) continue;
      const meta = t.metadata || {};
      let imageUrl = meta.image || "";
      /* Resolve IPFS URLs to HTTP gateway for device fetch */
      if (imageUrl.startsWith("ipfs://")) {
        imageUrl = "https://ipfs.crossmint.com/" + imageUrl.slice(7);
      }
      nfts.push({
        templateId: t.templateId || tid,
        name: meta.name || "Untitled",
        description: meta.description || "",
        image: imageUrl,
        collection: CROSSMINT_COLLECTION,
        chain: "polygon",
        supply: t.supply || {},
      });
    }
    res.json({ ok: true, count: nfts.length, nfts });
  } catch (err) {
    console.error(`[NFT] Crossmint fetch error: ${err.message}`);
    res.status(500).json({ ok: false, error: err.message });
  }
});

/**
 * GET /api/nfts/collection
 * Returns all minted NFTs in the collection (paginated).
 */
app.get("/api/nfts/collection", async (req, res) => {
  const page = req.query.page || 1;
  const perPage = req.query.perPage || 20;
  const data = await crossmintFetch(
    `/2022-06-09/collections/${CROSSMINT_COLLECTION}/nfts?page=${page}&perPage=${perPage}`
  );
  res.json(data);
});

/* ── WebSocket Connection ────────────────────── */
wss.on("connection", (ws) => {
  console.log("[WS] Client connected");
  ws.send(
    JSON.stringify({
      type: "welcome",
      running: !!activeProc,
      process: activeName,
      project: PROJECT_DIR,
      version: "1.0.0",
    }),
  );
  ws.on("close", () => console.log("[WS] Client disconnected"));
});

/* ── Start ─────────────────────────────────────────── */
server.listen(PORT, "127.0.0.1", () => {
  console.log("\n╔══════════════════════════════════════╗");
  console.log("║   GRUDA Node DevApp  v1.0.0          ║");
  console.log("║   GRUDGE STUDIO — RacAlvin           ║");
  console.log("╠══════════════════════════════════════╣");
  console.log(`║   http://localhost:${PORT}               ║`);
  console.log(`║   Project: ${path.basename(PROJECT_DIR)}              ║`);
  console.log("╚══════════════════════════════════════╝\n");
});

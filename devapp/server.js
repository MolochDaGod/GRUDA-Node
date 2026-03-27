"use strict";

const http = require("http");
const fs = require("fs");
const path = require("path");
const { spawn, exec } = require("child_process");
require("dotenv").config({ path: path.resolve(__dirname, "..", ".env") });
const express = require("express");
const WebSocket = require("ws");
const { supportedTokens, swapPolicy } = require("./config/tokens");

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

/* ── WebSocket Connection ──────────────────────────── */
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

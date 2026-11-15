// Game Lab server: everything under /api/* runs here.
// All other URLs are served straight from the public/ folder.
//
//   GET  /api/scores?game=pixel-hop&limit=10   top scores for a game
//   POST /api/scores  {game, name, score}      save a score
//   GET  /api/save/<game>/<playerId>           load a player's saved progress
//   PUT  /api/save/<game>/<playerId>           save a player's progress (JSON body)
//   GET  /api/room/<game>/<CODE>               multiplayer room (WebSocket)
//   GET  /api/health                           quick check that the server runs

import { DurableObject } from "cloudflare:workers";

const GAME_RE = /^[a-z0-9-]{1,40}$/;
const PLAYER_RE = /^[A-Za-z0-9_-]{8,64}$/;
const ROOM_RE = /^[A-Za-z0-9]{3,8}$/;
const MAX_SAVE_BYTES = 64 * 1024;
const MAX_PLAYERS_PER_ROOM = 16;

function json(data, status = 200) {
  return new Response(JSON.stringify(data), {
    status,
    headers: { "content-type": "application/json; charset=utf-8", "cache-control": "no-store" },
  });
}

function cleanName(value) {
  return String(value ?? "")
    .replace(/[\u0000-\u001f\u007f<>]/g, "")
    .trim()
    .slice(0, 16);
}

// Create the tables the first time the database is used.
let schemaReady = null;
function ensureSchema(env) {
  if (!schemaReady) {
    schemaReady = env.DB.batch([
      env.DB.prepare(
        `CREATE TABLE IF NOT EXISTS scores (
           id INTEGER PRIMARY KEY AUTOINCREMENT,
           game TEXT NOT NULL,
           name TEXT NOT NULL,
           score INTEGER NOT NULL,
           created_at TEXT NOT NULL DEFAULT (datetime('now'))
         )`
      ),
      env.DB.prepare(`CREATE INDEX IF NOT EXISTS scores_by_game ON scores (game, score DESC)`),
      env.DB.prepare(
        `CREATE TABLE IF NOT EXISTS saves (
           game TEXT NOT NULL,
           player TEXT NOT NULL,
           data TEXT NOT NULL,
           updated_at TEXT NOT NULL DEFAULT (datetime('now')),
           PRIMARY KEY (game, player)
         )`
      ),
    ]).catch((err) => {
      schemaReady = null;
      throw err;
    });
  }
  return schemaReady;
}

async function handleScores(request, env, url) {
  await ensureSchema(env);

  if (request.method === "GET") {
    const game = url.searchParams.get("game") || "";
    if (!GAME_RE.test(game)) return json({ error: "unknown game" }, 400);
    const limit = Math.min(50, Math.max(1, parseInt(url.searchParams.get("limit") || "10", 10) || 10));
    const { results } = await env.DB.prepare(
      `SELECT name, score, created_at FROM scores WHERE game = ? ORDER BY score DESC, id ASC LIMIT ?`
    )
      .bind(game, limit)
      .all();
    return json({ game, scores: results });
  }

  if (request.method === "POST") {
    let body;
    try {
      body = await request.json();
    } catch {
      return json({ error: "send JSON" }, 400);
    }
    const game = String(body?.game || "");
    const name = cleanName(body?.name) || "Player";
    const score = Number(body?.score);
    if (!GAME_RE.test(game)) return json({ error: "unknown game" }, 400);
    if (!Number.isInteger(score) || score < 0 || score > 1_000_000_000) return json({ error: "bad score" }, 400);

    await env.DB.prepare(`INSERT INTO scores (game, name, score) VALUES (?, ?, ?)`).bind(game, name, score).run();
    const higher = await env.DB.prepare(`SELECT COUNT(*) AS n FROM scores WHERE game = ? AND score > ?`)
      .bind(game, score)
      .first("n");
    return json({ ok: true, rank: (higher ?? 0) + 1 }, 201);
  }

  return json({ error: "method not allowed" }, 405);
}

async function handleSave(request, env, game, player) {
  await ensureSchema(env);

  if (request.method === "GET") {
    const row = await env.DB.prepare(`SELECT data, updated_at FROM saves WHERE game = ? AND player = ?`)
      .bind(game, player)
      .first();
    if (!row) return json({ found: false, data: null });
    return json({ found: true, data: JSON.parse(row.data), updated_at: row.updated_at });
  }

  if (request.method === "PUT" || request.method === "POST") {
    const text = await request.text();
    if (text.length > MAX_SAVE_BYTES) return json({ error: "save is too big (max 64 KB)" }, 413);
    try {
      JSON.parse(text);
    } catch {
      return json({ error: "send JSON" }, 400);
    }
    await env.DB.prepare(
      `INSERT INTO saves (game, player, data, updated_at) VALUES (?, ?, ?, datetime('now'))
       ON CONFLICT (game, player) DO UPDATE SET data = excluded.data, updated_at = excluded.updated_at`
    )
      .bind(game, player, text)
      .run();
    return json({ ok: true });
  }

  if (request.method === "DELETE") {
    await env.DB.prepare(`DELETE FROM saves WHERE game = ? AND player = ?`).bind(game, player).run();
    return json({ ok: true });
  }

  return json({ error: "method not allowed" }, 405);
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    const path = url.pathname;

    try {
      if (path === "/api/health") return json({ ok: true });

      if (path === "/api/scores") return await handleScores(request, env, url);

      let m = path.match(/^\/api\/save\/([^/]+)\/([^/]+)$/);
      if (m) {
        const [, game, player] = m;
        if (!GAME_RE.test(game) || !PLAYER_RE.test(player)) return json({ error: "bad game or player id" }, 400);
        return await handleSave(request, env, game, player);
      }

      m = path.match(/^\/api\/room\/([^/]+)\/([^/]+)$/);
      if (m) {
        const [, game, code] = m;
        if (!GAME_RE.test(game) || !ROOM_RE.test(code)) return json({ error: "bad game or room code" }, 400);
        if (request.headers.get("Upgrade") !== "websocket") return json({ error: "connect with a WebSocket" }, 426);
        const room = env.ROOMS.get(env.ROOMS.idFromName(`${game}:${code.toUpperCase()}`));
        return room.fetch(request);
      }

      if (path.startsWith("/api/")) return json({ error: "not found" }, 404);

      return env.ASSETS.fetch(request);
    } catch (err) {
      console.error(err);
      return json({ error: "server error" }, 500);
    }
  },
};

// One GameRoom exists per game + room code. It relays messages between
// everyone in the room. Players are told who joins and leaves.
export class GameRoom extends DurableObject {
  constructor(ctx, env) {
    super(ctx, env);
    // Answer keep-alive pings without waking the room up.
    this.ctx.setWebSocketAutoResponse(new WebSocketRequestResponsePair('{"type":"ping"}', '{"type":"pong"}'));
  }

  async fetch(request) {
    const url = new URL(request.url);
    const name = cleanName(url.searchParams.get("name")) || "Player";

    const pair = new WebSocketPair();
    const [client, server] = Object.values(pair);
    this.ctx.acceptWebSocket(server);

    if (this.openSockets().length > MAX_PLAYERS_PER_ROOM) {
      server.send(JSON.stringify({ type: "error", message: "room is full" }));
      server.close(4000, "room is full");
      return new Response(null, { status: 101, webSocket: client });
    }

    const me = { id: crypto.randomUUID().slice(0, 8), name, joinedAt: Date.now() };
    server.serializeAttachment(me);

    server.send(JSON.stringify({ type: "welcome", id: me.id, players: this.players() }));
    this.broadcast({ type: "join", player: me }, server);

    return new Response(null, { status: 101, webSocket: client });
  }

  openSockets() {
    return this.ctx.getWebSockets().filter((ws) => ws.readyState === WebSocket.OPEN);
  }

  players() {
    return this.openSockets()
      .map((ws) => ws.deserializeAttachment())
      .filter(Boolean)
      .sort((a, b) => a.joinedAt - b.joinedAt);
  }

  broadcast(message, except) {
    const text = JSON.stringify(message);
    for (const ws of this.openSockets()) {
      if (ws === except) continue;
      try {
        ws.send(text);
      } catch {
        // that player is gone; their close event will clean up
      }
    }
  }

  async webSocketMessage(ws, message) {
    if (typeof message !== "string" || message.length > 16 * 1024) return;
    const me = ws.deserializeAttachment();
    if (!me) return;
    let data;
    try {
      data = JSON.parse(message);
    } catch {
      return;
    }
    // "to" sends a message to one player only; otherwise everyone else gets it.
    if (data && typeof data === "object" && typeof data.to === "string") {
      const target = this.openSockets().find((s) => s.deserializeAttachment()?.id === data.to);
      if (target) target.send(JSON.stringify({ type: "msg", from: me.id, data: data.data }));
      return;
    }
    this.broadcast({ type: "msg", from: me.id, data }, ws);
  }

  async webSocketClose(ws, code, reason) {
    this.leave(ws);
    try {
      ws.close(code, reason);
    } catch {
      // already closed
    }
  }

  async webSocketError(ws) {
    this.leave(ws);
  }

  leave(ws) {
    const me = ws.deserializeAttachment();
    if (!me || me.left) return;
    ws.serializeAttachment({ ...me, left: true });
    this.broadcast({ type: "leave", id: me.id }, ws);
  }
}

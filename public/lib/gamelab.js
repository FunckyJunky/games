/*
  Game Lab helper — add to any game with:
    <script src="/lib/gamelab.js"></script>

  SCORES
    await GameLab.submitScore('my-game', 'Alex', 1234)  -> { rank }
    await GameLab.topScores('my-game', 10)              -> [{ name, score, created_at }, ...]

  SAVED PROGRESS (per player, per game; any JSON up to 64 KB)
    await GameLab.save('my-game', { level: 3, coins: 50 })
    const data = await GameLab.load('my-game')          -> your object, or null

  PLAYER
    GameLab.playerName() / GameLab.setPlayerName('Alex')
    GameLab.playerId()   -> random id that stays the same in this browser

  MULTIPLAYER
    const room = GameLab.joinRoom('my-game', 'ABCD', {
      name: 'Alex',
      onOpen:    (me, players) => {},   // me = your id, players = everyone in the room
      onJoin:    (player) => {},        // { id, name }
      onLeave:   (id) => {},
      onMessage: (data, fromId) => {},  // whatever another player sent
      onClose:   () => {},
    });
    room.send({ x: 10, y: 20 });        // to everyone else in the room
    room.sendTo(playerId, { hi: 1 });   // to one player
    room.close();
    GameLab.newRoomCode()  -> e.g. "K7QD"

  MUSIC (starts after the first tap/click, as browsers require)
    const music = GameLab.music('music.mp3', { volume: 0.5 });
    music.play(); music.pause(); music.toggle(); music.muted
*/
(function () {
  const store = {
    get(key) { try { return localStorage.getItem(key); } catch (e) { return null; } },
    set(key, value) { try { localStorage.setItem(key, value); } catch (e) {} },
  };

  async function api(path, options) {
    const res = await fetch(path, options);
    const body = await res.json().catch(() => ({}));
    if (!res.ok) throw new Error(body.error || ('request failed: ' + res.status));
    return body;
  }

  function randomId(len) {
    const chars = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789';
    const bytes = new Uint8Array(len);
    crypto.getRandomValues(bytes);
    return Array.from(bytes, (b) => chars[b % chars.length]).join('');
  }

  const GameLab = {
    playerId() {
      let id = store.get('gamelab-player-id');
      if (!id) { id = randomId(20); store.set('gamelab-player-id', id); }
      return id;
    },
    playerName() { return store.get('gamelab-player-name') || ''; },
    setPlayerName(name) { store.set('gamelab-player-name', String(name || '').trim().slice(0, 16)); },

    async submitScore(game, name, score) {
      return api('/api/scores', {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify({ game, name, score: Math.floor(score) }),
      });
    },
    async topScores(game, limit) {
      const body = await api('/api/scores?game=' + encodeURIComponent(game) + '&limit=' + (limit || 10));
      return body.scores;
    },

    async save(game, data) {
      return api('/api/save/' + encodeURIComponent(game) + '/' + GameLab.playerId(), {
        method: 'PUT',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify(data),
      });
    },
    async load(game) {
      const body = await api('/api/save/' + encodeURIComponent(game) + '/' + GameLab.playerId());
      return body.found ? body.data : null;
    },

    newRoomCode() {
      const chars = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
      const bytes = new Uint8Array(4);
      crypto.getRandomValues(bytes);
      return Array.from(bytes, (b) => chars[b % chars.length]).join('');
    },

    joinRoom(game, code, handlers) {
      handlers = handlers || {};
      const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
      const url = proto + '//' + location.host + '/api/room/' + encodeURIComponent(game) + '/' +
        encodeURIComponent(String(code).toUpperCase()) + '?name=' + encodeURIComponent(handlers.name || GameLab.playerName() || 'Player');
      const ws = new WebSocket(url);
      const room = { id: null, players: [], code: String(code).toUpperCase(), socket: ws };
      let ping = null;

      ws.onmessage = (event) => {
        let msg;
        try { msg = JSON.parse(event.data); } catch (e) { return; }
        if (msg.type === 'welcome') {
          room.id = msg.id;
          room.players = msg.players;
          ping = setInterval(() => { if (ws.readyState === 1) ws.send('{"type":"ping"}'); }, 25000);
          handlers.onOpen && handlers.onOpen(msg.id, msg.players);
        } else if (msg.type === 'join') {
          room.players.push(msg.player);
          handlers.onJoin && handlers.onJoin(msg.player);
        } else if (msg.type === 'leave') {
          room.players = room.players.filter((p) => p.id !== msg.id);
          handlers.onLeave && handlers.onLeave(msg.id);
        } else if (msg.type === 'msg') {
          handlers.onMessage && handlers.onMessage(msg.data, msg.from);
        } else if (msg.type === 'error') {
          handlers.onError && handlers.onError(msg.message);
        }
      };
      ws.onclose = () => { clearInterval(ping); handlers.onClose && handlers.onClose(); };

      room.send = (data) => { if (ws.readyState === 1) ws.send(JSON.stringify(data)); };
      room.sendTo = (id, data) => { if (ws.readyState === 1) ws.send(JSON.stringify({ to: id, data })); };
      room.close = () => ws.close(1000, 'bye');
      return room;
    },

    music(src, options) {
      options = options || {};
      const audio = new Audio(src);
      audio.loop = options.loop !== false;
      audio.volume = options.volume != null ? options.volume : 0.5;
      audio.preload = 'auto';
      const key = 'gamelab-muted';
      const ctl = {
        audio,
        muted: store.get(key) === '1',
        play() { if (!ctl.muted) audio.play().catch(() => {}); },
        pause() { audio.pause(); },
        toggle() {
          ctl.muted = !ctl.muted;
          store.set(key, ctl.muted ? '1' : '0');
          if (ctl.muted) audio.pause(); else audio.play().catch(() => {});
          return ctl.muted;
        },
      };
      return ctl;
    },
  };

  window.GameLab = GameLab;
})();

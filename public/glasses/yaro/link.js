/* Yaro-Vission link: ties the glasses (/glasses/yaro/), the CON page (/glasses/con/) and an
   optional camera phone together in one GameLab room: /api/room/yaro-vission/<CODE>.

   Needs /lib/gamelab.js first.

     const link = YaroLink.join('K7QD', 'glasses' | 'cam' | 'con', {
       onPeers(peers)            // Map id -> { id, role, cam, ... } changed
       onStatus(s)               // 'open' | 'reconnecting' | 'full'
       onMsg(data, fromId, peer) // anything that isn't link plumbing (moves, actions, telemetry...)
       onVideo(stream, srcId)    // viewer: live WebRTC video (null = gone)
       onFrame(dataUrl, srcId)   // viewer: one low-res JPEG frame (relay mode)
       onRtc(state, srcId)       // viewer: 'connecting' | 'connected' | 'failed'
       onPong(ms)                // viewer: round trip to the source through the room
       onViewers(n)              // source: how many viewers are getting the picture
     });
     link.send(obj) / link.sendTo(id, obj) / link.hi(extra) / link.close()
     source:  link.setStream(mediaStream | null)
     viewer:  link.watch(srcId, 'rtc' | 'jpg', force) / link.unwatch() / link.ping()

   The picture goes peer to peer (WebRTC, STUN only). When that can't connect, the viewer
   asks for 'jpg': the source then sends ~5 small JPEG frames a second through the room. */
(function () {
  'use strict';
  const GAME = 'yaro-vission';
  const ICE = [{ urls: ['stun:stun.cloudflare.com:3478', 'stun:stun.l.google.com:19302'] }];
  const CHARS = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
  const ROLES = ['glasses', 'cam', 'con'];
  const MAX_MSG = 15800;                 // the room drops messages over 16 KB

  function newCode() {
    const b = new Uint8Array(4);
    crypto.getRandomValues(b);
    return Array.from(b, (x) => CHARS[x % CHARS.length]).join('');
  }
  const cleanCode = (s) => String(s || '').toUpperCase().replace(/[^A-Z0-9]/g, '').slice(0, 8);

  function join(code, role, h) {
    h = h || {};
    const L = { code: cleanCode(code), role, me: null, peers: new Map(), open: false, closed: false, extra: {} };
    let room = null, backoff = 1000, retryT = null;

    /* ---------------- room ---------------- */
    function connect() {
      clearTimeout(retryT);
      room = GameLab.joinRoom(GAME, L.code, {
        name: role.toUpperCase(),
        onOpen(me, players) {
          L.me = me; L.open = true; backoff = 1000;
          L.peers.clear();
          for (const p of players) if (p.id !== me) addPeer(p);
          L.hi();
          h.onStatus && h.onStatus('open');
          h.onPeers && h.onPeers(L.peers);
        },
        onJoin(p) {
          if (p.id === L.me) return;
          addPeer(p);
          L.hi(p.id);
          h.onPeers && h.onPeers(L.peers);
        },
        onLeave(id) { dropPeer(id); h.onPeers && h.onPeers(L.peers); },
        onMessage(data, from) { onData(data, from); },
        onError(m) { if (/full/.test(m)) { L.full = true; h.onStatus && h.onStatus('full'); } },
        onClose() {
          L.open = false;
          for (const id of [...L.peers.keys()]) dropPeer(id);
          h.onPeers && h.onPeers(L.peers);
          if (L.closed) return;
          h.onStatus && h.onStatus('reconnecting');
          retryT = setTimeout(connect, backoff);
          backoff = Math.min(8000, backoff * 1.6);
        },
      });
    }
    function addPeer(p) {
      const r = String(p.name || '').toLowerCase();
      const old = L.peers.get(p.id) || {};
      L.peers.set(p.id, Object.assign({ cam: false }, old, { id: p.id, role: ROLES.includes(r) ? r : (old.role || 'con'), joinedAt: p.joinedAt }));
    }
    function dropPeer(id) {
      L.peers.delete(id);
      srcDrop(id);
      if (watching === id) viewerReset(true);
    }

    L.send = (obj) => { if (room && L.open) room.send(obj); };
    L.sendTo = (id, obj) => { if (room && L.open && id) room.sendTo(id, obj); };
    L.hi = (to, extra) => {
      if (extra) Object.assign(L.extra, extra);
      const msg = Object.assign({ t: 'hi', role, cam: !!stream }, L.extra);
      if (typeof to === 'string') L.sendTo(to, msg); else L.send(msg);
    };
    L.close = () => {
      L.closed = true;
      clearTimeout(retryT);
      L.setStream && srcStopAll();
      viewerReset(false);
      try { room && room.close(); } catch (e) {}
    };
    L.byRole = (r) => [...L.peers.values()].filter((p) => p.role === r);

    function onData(d, from) {
      if (!d || typeof d !== 'object') return;
      const peer = L.peers.get(from);
      switch (d.t) {
        case 'hi':
          if (peer) Object.assign(peer, d, { id: from, role: ROLES.includes(d.role) ? d.role : peer.role });
          else L.peers.set(from, Object.assign({}, d, { id: from }));
          h.onPeers && h.onPeers(L.peers);
          return;
        case 'want': return srcWant(from, d.mode);
        case 'rtc': return from === watching ? viewerRtc(d.d) : srcRtc(from, d.d);
        case 'jpg': if (from === watching) { lastFrameAt = Date.now(); h.onFrame && h.onFrame(d.d, from); } return;
        case 'ping': return L.sendTo(from, { t: 'pong', ts: d.ts });
        case 'pong': if (typeof d.ts === 'number') h.onPong && h.onPong(Date.now() - d.ts); return;
      }
      h.onMsg && h.onMsg(d, from, peer);
    }

    /* ---------------- source: send the camera picture ---------------- */
    let stream = null, srcVideo = null, jpgT = null, jq = 0.6, frameN = 0, cv = null, cx = null;
    const pcs = new Map();                 // viewer id -> RTCPeerConnection
    const pend = new Map();                // viewer id -> ICE candidates waiting for the answer
    const jpgTo = new Set();

    L.setStream = (s) => {
      if (s === stream) return;
      srcStopAll();
      stream = s || null;
      if (stream) {
        srcVideo = document.createElement('video');
        srcVideo.muted = true; srcVideo.playsInline = true; srcVideo.setAttribute('playsinline', '');
        srcVideo.srcObject = stream;
        srcVideo.play().catch(() => {});
      } else srcVideo = null;
      L.extra.gen = (L.extra.gen || 0) + 1;      // viewers see a new picture and ask again
      L.hi();
    };
    L.viewers = () => new Set([...[...pcs.entries()].filter(([, pc]) => pc.connectionState === 'connected').map(([id]) => id), ...jpgTo]).size;
    const viewersChanged = () => h.onViewers && h.onViewers(L.viewers());

    function srcStopAll() {
      for (const id of [...pcs.keys()]) srcClose(id);
      jpgTo.clear();
      clearInterval(jpgT); jpgT = null;
      viewersChanged();
    }
    function srcClose(id) {
      const pc = pcs.get(id);
      pcs.delete(id); pend.delete(id);
      try { pc && pc.close(); } catch (e) {}
    }
    function srcDrop(id) {
      if (pcs.has(id) || jpgTo.has(id)) { srcClose(id); jpgTo.delete(id); viewersChanged(); }
    }
    function srcWant(from, mode) {
      if (!stream && mode !== 'off') { L.hi(from); return; }      // tells them there's no camera here
      srcClose(from); jpgTo.delete(from);
      if (mode === 'rtc') srcOffer(from);
      else if (mode === 'jpg') { jpgTo.add(from); if (!jpgT) jpgT = setInterval(srcFrame, 200); }
      if (!jpgTo.size) { clearInterval(jpgT); jpgT = null; }
      viewersChanged();
    }
    async function srcOffer(id) {
      if (!window.RTCPeerConnection) return;
      const pc = new RTCPeerConnection({ iceServers: ICE });
      pcs.set(id, pc); pend.set(id, []);
      stream.getVideoTracks().forEach((t) => pc.addTrack(t, stream));
      pc.onicecandidate = (e) => { if (e.candidate) L.sendTo(id, { t: 'rtc', d: { ice: e.candidate.toJSON ? e.candidate.toJSON() : e.candidate } }); };
      pc.onconnectionstatechange = () => {
        if (pcs.get(id) !== pc) return;
        if (pc.connectionState === 'failed' || pc.connectionState === 'closed') srcClose(id);
        viewersChanged();
      };
      try {
        await pc.setLocalDescription(await pc.createOffer());
        L.sendTo(id, { t: 'rtc', d: { sdp: pc.localDescription } });
        const s = pc.getSenders()[0];
        if (s && s.getParameters) {
          const p = s.getParameters();
          p.encodings = p.encodings && p.encodings.length ? p.encodings : [{}];
          p.encodings[0].maxBitrate = 1500000;
          s.setParameters(p).catch(() => {});
        }
      } catch (e) { srcClose(id); }
    }
    async function srcRtc(from, d) {
      const pc = pcs.get(from);
      if (!pc || !d) return;
      try {
        if (d.sdp && d.sdp.type === 'answer') {
          await pc.setRemoteDescription(d.sdp);
          for (const c of (pend.get(from) || []).splice(0)) await pc.addIceCandidate(c).catch(() => {});
        } else if (d.ice) {
          if (pc.remoteDescription) await pc.addIceCandidate(d.ice).catch(() => {});
          else (pend.get(from) || []).push(d.ice);
        }
      } catch (e) {}
    }
    function srcFrame() {
      if (!jpgTo.size || !srcVideo || !srcVideo.videoWidth || document.hidden) return;
      const vw = srcVideo.videoWidth, vh = srcVideo.videoHeight;
      const big = 320, w = vw >= vh ? big : Math.round(big * vw / vh), hgt = vw >= vh ? Math.round(big * vh / vw) : big;
      if (!cv) { cv = document.createElement('canvas'); cx = cv.getContext('2d'); }
      if (cv.width !== w || cv.height !== hgt) { cv.width = w; cv.height = hgt; }
      cx.drawImage(srcVideo, 0, 0, w, hgt);
      let url = cv.toDataURL('image/jpeg', jq);
      while (url.length > MAX_MSG - 200 && jq > 0.2) { jq = Math.max(0.2, jq - 0.1); url = cv.toDataURL('image/jpeg', jq); }
      if (url.length > MAX_MSG - 200) return;
      if (url.length < 9000 && jq < 0.75) jq += 0.03;
      frameN++;
      for (const id of jpgTo) L.sendTo(id, { t: 'jpg', d: url, n: frameN });
    }

    /* ---------------- viewer: get the picture ---------------- */
    let watching = null, vpc = null, vpend = [], vTimer = null, vMode = null, lastFrameAt = 0;
    L.watch = (srcId, mode, force) => {
      if (!force && srcId === watching && mode === vMode) return;
      viewerReset(true);
      watching = srcId; vMode = mode;
      if (!srcId) return;
      L.sendTo(srcId, { t: 'want', mode });
      if (mode === 'rtc') {
        h.onRtc && h.onRtc('connecting', srcId);
        vTimer = setTimeout(() => { if (watching === srcId && vMode === 'rtc' && (!vpc || vpc.connectionState !== 'connected')) h.onRtc && h.onRtc('failed', srcId); }, 12000);
      }
    };
    L.unwatch = () => viewerReset(true);
    L.watching = () => watching;
    L.mode = () => vMode;
    L.lastFrameAt = () => lastFrameAt;
    L.ping = () => { if (watching) L.sendTo(watching, { t: 'ping', ts: Date.now() }); };
    function viewerReset(tell) {
      clearTimeout(vTimer);
      if (watching && tell && L.peers.has(watching)) L.sendTo(watching, { t: 'want', mode: 'off' });
      try { vpc && vpc.close(); } catch (e) {}
      const had = !!vpc;
      vpc = null; vpend = []; watching = null; vMode = null;
      if (had) h.onVideo && h.onVideo(null, null);
    }
    async function viewerRtc(d) {
      if (!d) return;
      try {
        if (d.sdp && d.sdp.type === 'offer') {
          try { vpc && vpc.close(); } catch (e) {}
          const src = watching;
          const pc = vpc = new RTCPeerConnection({ iceServers: ICE });
          pc.onicecandidate = (e) => { if (e.candidate) L.sendTo(src, { t: 'rtc', d: { ice: e.candidate.toJSON ? e.candidate.toJSON() : e.candidate } }); };
          pc.ontrack = (e) => { if (vpc === pc) h.onVideo && h.onVideo(e.streams[0] || new MediaStream([e.track]), src); };
          pc.onconnectionstatechange = () => {
            if (vpc !== pc) return;
            const s = pc.connectionState;
            if (s === 'connected') { clearTimeout(vTimer); h.onRtc && h.onRtc('connected', src); }
            else if (s === 'failed') h.onRtc && h.onRtc('failed', src);
          };
          await pc.setRemoteDescription(d.sdp);
          await pc.setLocalDescription(await pc.createAnswer());
          L.sendTo(src, { t: 'rtc', d: { sdp: pc.localDescription } });
          for (const c of vpend.splice(0)) await pc.addIceCandidate(c).catch(() => {});
        } else if (d.ice) {
          if (vpc && vpc.remoteDescription) await vpc.addIceCandidate(d.ice).catch(() => {});
          else vpend.push(d.ice);
        }
      } catch (e) {}
    }

    connect();
    return L;
  }

  window.YaroLink = { join, newCode, cleanCode, GAME };
})();

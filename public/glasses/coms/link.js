/* COMS link: the phone page (error.blue/coms) sends its camera, its microphone and text to
   COMS.EXE on the glasses (/glasses/coms/). Both join the GameLab room coms/<CODE>; CODE is the
   glasses' 6-letter pairing code (the one SETUP.EXE shows and error.blue/hud uses).

   Needs /lib/gamelab.js first.

     const link = ComsLink.join('K7QDXA', 'glasses' | 'phone', {
       name: 'MAMA',             // shown on the glasses with that phone's messages
       onStatus(s)               // 'open' | 'reconnecting' | 'full'
       onPeers(peers)            // Map id -> { id, role, name, ...whatever that side put in hi() }
       onMsg(data, fromId, peer) // app messages (text, receipts, glasses state ...)
       // glasses (viewer)
       onMedia(stream, srcId)    // WebRTC stream with the phone's video + audio tracks (null = gone)
       onRtc(state, srcId)       // 'connecting' | 'connected' | 'failed'
       onFrame(dataUrl, srcId)   // relay mode: one JPEG frame
       onPcm(samples, rate)      // relay mode: Float32Array of the phone's sound (16 kHz)
       // phone (source)
       onViewers(list)           // [{ id, mode: 'rtc' | 'relay', state }]
     });
     link.send(obj) / link.sendTo(id, obj) / link.hi(extra) / link.close() / link.byRole(r)
     phone:   link.setTracks({ video, audio }) / link.setQuality('low' | 'mid' | 'high') / link.setAudioCtx(ctx)
     glasses: link.watch(srcId, 'rtc' | 'relay', force) / link.unwatch() / link.watching() / link.mode()

   Direct (WebRTC, STUN only): one connection per viewer with a video and an audio slot, so the
   phone can switch camera, turn the camera or mic off and on again without a new connection.
   Relay (when direct can't connect): ~6 JPEG frames a second, each cut into parts under the
   room's 16 KB message limit, and the sound as 16 kHz mu-law in 100 ms slices. */
(function () {
  'use strict';
  const GAME = 'coms';
  const ICE = [{ urls: ['stun:stun.cloudflare.com:3478', 'stun:stun.l.google.com:19302'] }];
  const ROLES = ['glasses', 'phone'];
  const PART = 15000;                    // the room drops messages over 16 KB
  const RATE = 16000;                    // relay sound
  const QUALITY = {
    low:  { v: 450000,  a: 32000, jw: 320, jfps: 4, jmax: 22000 },
    mid:  { v: 1200000, a: 64000, jw: 480, jfps: 6, jmax: 42000 },
    high: { v: 2500000, a: 96000, jw: 640, jfps: 8, jmax: 58000 },
  };
  const cleanCode = (s) => String(s || '').toUpperCase().replace(/[^A-Z0-9]/g, '').slice(0, 8);

  /* ---------- mu-law (G.711) for the relay sound ---------- */
  function mulaw(s) {
    let x = Math.round(Math.max(-1, Math.min(1, s)) * 32635);
    const sign = x < 0 ? 0x80 : 0;
    if (sign) x = -x;
    x += 0x84;
    let e = 7;
    for (let m = 0x4000; (x & m) === 0 && e > 0; m >>= 1) e--;
    return ~(sign | (e << 4) | ((x >> (e + 3)) & 0x0f)) & 0xff;
  }
  const UNMU = new Float32Array(256);
  for (let i = 0; i < 256; i++) {
    const u = ~i & 0xff, t = (((u & 0x0f) << 3) + 0x84) << ((u & 0x70) >> 4);
    UNMU[i] = ((u & 0x80) ? 0x84 - t : t - 0x84) / 32768;
  }
  function b64(bytes) {
    let s = '';
    for (let i = 0; i < bytes.length; i += 4096) s += String.fromCharCode.apply(null, bytes.subarray(i, i + 4096));
    return btoa(s);
  }
  function unb64(str) {
    const s = atob(str), out = new Float32Array(s.length);
    for (let i = 0; i < s.length; i++) out[i] = UNMU[s.charCodeAt(i)];
    return out;
  }

  function join(code, role, h) {
    h = h || {};
    const L = { code: cleanCode(code), role, me: null, peers: new Map(), open: false, closed: false, extra: {} };
    let room = null, backoff = 1000, retryT = null;

    /* ================= room ================= */
    function connect() {
      clearTimeout(retryT);
      room = GameLab.joinRoom(GAME, L.code, {
        name: role,
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
      L.peers.set(p.id, Object.assign({}, old, { id: p.id, role: ROLES.includes(r) ? r : (old.role || 'phone'), joinedAt: p.joinedAt }));
    }
    function dropPeer(id) {
      L.peers.delete(id);
      srcDrop(id);
      if (watching === id) viewerReset(false);
    }

    L.send = (obj) => { if (room && L.open) room.send(obj); };
    L.sendTo = (id, obj) => { if (room && L.open && id) room.sendTo(id, obj); };
    L.hi = (to, extra) => {
      if (extra) Object.assign(L.extra, extra);
      const msg = Object.assign({ t: 'hi', role, name: h.name || '', v: !!vTrack, a: !!aTrack }, L.extra);
      if (typeof to === 'string') L.sendTo(to, msg); else L.send(msg);
    };
    L.close = () => {
      L.closed = true;
      clearTimeout(retryT);
      srcStopAll();
      viewerReset(false);
      try { room && room.close(); } catch (e) {}
    };
    L.byRole = (r) => [...L.peers.values()].filter((p) => p.role === r);
    L.buffered = () => (room && room.socket ? room.socket.bufferedAmount : 0);

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
        case 'jpg': if (from === watching && vMode === 'relay') gotPart(d, from); return;
        case 'pcm': if (from === watching && vMode === 'relay' && typeof d.d === 'string') { lastPcmAt = Date.now(); try { h.onPcm && h.onPcm(unb64(d.d), RATE); } catch (e) {} } return;
      }
      h.onMsg && h.onMsg(d, from, peer);
    }

    /* ================= phone: send camera + mic ================= */
    let vTrack = null, aTrack = null, quality = 'mid', actx = null;
    const pcs = new Map();                 // viewer id -> { pc, vs, as, pend }
    const relayTo = new Set();

    L.setTracks = (o) => {
      if (o.video !== undefined) vTrack = o.video || null;
      if (o.audio !== undefined) aTrack = o.audio || null;
      for (const p of pcs.values()) {
        try { p.vs.sender.replaceTrack(vTrack).catch(() => {}); } catch (e) {}
        try { p.as.sender.replaceTrack(aTrack).catch(() => {}); } catch (e) {}
      }
      bitrates();
      relaySetup();
      L.hi();
    };
    L.setQuality = (q) => { if (QUALITY[q]) { quality = q; bitrates(); relaySetup(); } };
    L.setAudioCtx = (ctx) => { actx = ctx; relaySetup(); };
    L.viewers = () => {
      const out = [];
      for (const [id, p] of pcs) out.push({ id, mode: 'rtc', state: p.pc.connectionState });
      for (const id of relayTo) out.push({ id, mode: 'relay', state: 'connected' });
      return out;
    };
    const viewersChanged = () => h.onViewers && h.onViewers(L.viewers());

    function bitrates() {
      const Q = QUALITY[quality];
      for (const p of pcs.values()) {
        for (const [tr, max] of [[p.vs, Q.v], [p.as, Q.a]]) {
          const s = tr.sender;
          if (!s || !s.getParameters) continue;
          try {
            const pr = s.getParameters();
            if (!pr.encodings || !pr.encodings.length) continue;     // not negotiated yet; done after the answer
            pr.encodings[0].maxBitrate = max;
            if (tr === p.vs) pr.degradationPreference = 'balanced';
            s.setParameters(pr).catch(() => {});
          } catch (e) {}
        }
      }
    }
    function srcStopAll() {
      for (const id of [...pcs.keys()]) srcClose(id);
      relayTo.clear();
      relaySetup();
      viewersChanged();
    }
    function srcClose(id) {
      const p = pcs.get(id);
      pcs.delete(id);
      try { p && p.pc.close(); } catch (e) {}
    }
    function srcDrop(id) {
      if (pcs.has(id) || relayTo.has(id)) { srcClose(id); relayTo.delete(id); relaySetup(); viewersChanged(); }
    }
    function srcWant(from, mode) {
      if (role !== 'phone') return;
      srcClose(from); relayTo.delete(from);
      if (mode === 'rtc') srcOffer(from);
      else if (mode === 'relay') relayTo.add(from);
      relaySetup();
      viewersChanged();
    }
    async function srcOffer(id) {
      if (!window.RTCPeerConnection) { L.sendTo(id, { t: 'rtc', d: { none: true } }); return; }
      const pc = new RTCPeerConnection({ iceServers: ICE });
      const ms = new MediaStream();
      const p = { pc, pend: [] };
      p.vs = pc.addTransceiver('video', { direction: 'sendonly', streams: [ms] });
      p.as = pc.addTransceiver('audio', { direction: 'sendonly', streams: [ms] });
      pcs.set(id, p);
      if (vTrack) p.vs.sender.replaceTrack(vTrack).catch(() => {});
      if (aTrack) p.as.sender.replaceTrack(aTrack).catch(() => {});
      pc.onicecandidate = (e) => { if (e.candidate) L.sendTo(id, { t: 'rtc', d: { ice: e.candidate.toJSON ? e.candidate.toJSON() : e.candidate } }); };
      pc.onconnectionstatechange = () => {
        if (pcs.get(id) !== p) return;
        if (pc.connectionState === 'failed' || pc.connectionState === 'closed') srcClose(id);
        viewersChanged();
      };
      try {
        await pc.setLocalDescription(await pc.createOffer());
        L.sendTo(id, { t: 'rtc', d: { sdp: pc.localDescription } });
      } catch (e) { srcClose(id); viewersChanged(); }
    }
    async function srcRtc(from, d) {
      const p = pcs.get(from);
      if (!p || !d) return;
      try {
        if (d.sdp && d.sdp.type === 'answer') {
          await p.pc.setRemoteDescription(d.sdp);
          for (const c of p.pend.splice(0)) await p.pc.addIceCandidate(c).catch(() => {});
          bitrates();
        } else if (d.ice) {
          if (p.pc.remoteDescription) await p.pc.addIceCandidate(d.ice).catch(() => {});
          else p.pend.push(d.ice);
        }
      } catch (e) {}
    }

    /* ---------- relay: JPEG frames + mu-law sound through the room ---------- */
    let jVid = null, jCv = null, jCx = null, jT = null, jq = 0.7, jN = 0;
    let aSrc = null, aProc = null, aZero = null, aTrackIn = null, aPh = 0, aAcc = 0, aCnt = 0, aBuf = [], aN = 0;
    function relaySetup() {
      const on = role === 'phone' && relayTo.size > 0;
      // pictures
      if (on && vTrack) {
        if (!jVid) {
          jVid = document.createElement('video');
          jVid.muted = true; jVid.playsInline = true; jVid.setAttribute('playsinline', '');
        }
        if (!jVid.srcObject || jVid.srcObject.getVideoTracks()[0] !== vTrack) { jVid.srcObject = new MediaStream([vTrack]); jVid.play().catch(() => {}); }
        clearInterval(jT);
        jT = setInterval(jpgFrame, Math.round(1000 / QUALITY[quality].jfps));
      } else {
        clearInterval(jT); jT = null;
        if (jVid) { jVid.srcObject = null; }
      }
      // sound
      if (on && aTrack && actx) {
        if (aTrackIn !== aTrack) {
          audioStop();
          try {
            aTrackIn = aTrack;
            aSrc = actx.createMediaStreamSource(new MediaStream([aTrack]));
            aProc = actx.createScriptProcessor(2048, 1, 1);
            aZero = actx.createGain(); aZero.gain.value = 0;
            aProc.onaudioprocess = pcmIn;
            aSrc.connect(aProc); aProc.connect(aZero); aZero.connect(actx.destination);
            if (actx.state === 'suspended') actx.resume().catch(() => {});
          } catch (e) { audioStop(); }
        }
      } else audioStop();
    }
    function audioStop() {
      try { aSrc && aSrc.disconnect(); } catch (e) {}
      try { aProc && aProc.disconnect(); } catch (e) {}
      try { aZero && aZero.disconnect(); } catch (e) {}
      if (aProc) aProc.onaudioprocess = null;
      aSrc = aProc = aZero = aTrackIn = null; aBuf = []; aAcc = aCnt = 0; aPh = 0;
    }
    function pcmIn(e) {
      if (!relayTo.size) return;
      const inp = e.inputBuffer.getChannelData(0), ratio = actx.sampleRate / RATE;
      const live = aTrack && aTrack.enabled && aTrack.readyState === 'live';
      for (let i = 0; i < inp.length; i++) {               // box filter + decimate to 16 kHz
        aAcc += live ? inp[i] : 0; aCnt++;
        if (++aPh >= ratio) { aPh -= ratio; aBuf.push(aAcc / aCnt); aAcc = aCnt = 0; }
      }
      while (aBuf.length >= 1600) {                        // 100 ms
        const part = aBuf.splice(0, 1600), bytes = new Uint8Array(1600);
        for (let i = 0; i < 1600; i++) bytes[i] = mulaw(part[i]);
        if (L.buffered() > 400000) continue;               // the network is behind: drop sound, not latency
        const msg = { t: 'pcm', n: ++aN, d: b64(bytes) };
        for (const id of relayTo) L.sendTo(id, msg);
      }
    }
    function jpgFrame() {
      if (!relayTo.size || !jVid || !jVid.videoWidth || document.hidden) return;
      if (L.buffered() > 300000) return;                   // wait for the network
      const Q = QUALITY[quality], vw = jVid.videoWidth, vh = jVid.videoHeight;
      const w = vw >= vh ? Q.jw : Math.round(Q.jw * vw / vh), hh = vw >= vh ? Math.round(Q.jw * vh / vw) : Q.jw;
      if (!jCv) { jCv = document.createElement('canvas'); jCx = jCv.getContext('2d'); }
      if (jCv.width !== w || jCv.height !== hh) { jCv.width = w; jCv.height = hh; }
      jCx.drawImage(jVid, 0, 0, w, hh);
      let url = jCv.toDataURL('image/jpeg', jq);
      while (url.length > Q.jmax && jq > 0.3) { jq = Math.max(0.3, jq - 0.08); url = jCv.toDataURL('image/jpeg', jq); }
      if (url.length < Q.jmax * 0.6 && jq < 0.82) jq += 0.03;
      const k = Math.ceil(url.length / PART), n = ++jN;
      if (k > 5) return;
      for (const id of relayTo) for (let i = 0; i < k; i++) L.sendTo(id, { t: 'jpg', n, i, k, d: url.slice(i * PART, (i + 1) * PART) });
    }

    /* ================= glasses: get the phone's picture + sound ================= */
    let watching = null, vpc = null, vpend = [], vTimer = null, vMode = null, vStream = null, vTell = 0;
    let lastFrameAt = 0, lastPcmAt = 0, shownN = 0;
    const parts = new Map();
    L.watch = (srcId, mode, force) => {
      if (!force && srcId === watching && mode === vMode) return;
      viewerReset(true);
      watching = srcId; vMode = mode;
      if (!srcId) return;
      L.sendTo(srcId, { t: 'want', mode });
      if (mode === 'rtc') {
        h.onRtc && h.onRtc('connecting', srcId);
        vTimer = setTimeout(() => { if (watching === srcId && vMode === 'rtc' && (!vpc || vpc.connectionState !== 'connected')) h.onRtc && h.onRtc('failed', srcId); }, 10000);
      }
    };
    L.unwatch = () => viewerReset(true);
    L.watching = () => watching;
    L.mode = () => vMode;
    L.lastFrameAt = () => lastFrameAt;
    L.lastPcmAt = () => lastPcmAt;
    L.rtcState = () => (vpc ? vpc.connectionState : null);
    L.stats = async () => {                                 // { w, h, fps, kbps } of the incoming video, for the HUD
      if (!vpc || !vpc.getStats) return null;
      try {
        const st = await vpc.getStats();
        let out = null;
        st.forEach((r) => {
          if (r.type === 'inbound-rtp' && (r.kind === 'video' || r.mediaType === 'video')) {
            out = { w: r.frameWidth || 0, h: r.frameHeight || 0, fps: Math.round(r.framesPerSecond || 0), bytes: r.bytesReceived || 0, at: r.timestamp };
          }
        });
        return out;
      } catch (e) { return null; }
    };
    function viewerReset(tell) {
      clearTimeout(vTimer);
      if (watching && tell && L.peers.has(watching)) L.sendTo(watching, { t: 'want', mode: 'off' });
      try { vpc && vpc.close(); } catch (e) {}
      const had = !!vStream;
      vpc = null; vpend = []; watching = null; vMode = null; vStream = null;
      parts.clear(); shownN = 0;
      if (had) h.onMedia && h.onMedia(null, null);
    }
    async function viewerRtc(d) {
      if (!d) return;
      if (d.none) { h.onRtc && h.onRtc('failed', watching); return; }
      try {
        if (d.sdp && d.sdp.type === 'offer') {
          try { vpc && vpc.close(); } catch (e) {}
          const src = watching;
          const pc = vpc = new RTCPeerConnection({ iceServers: ICE });
          const ms = vStream = new MediaStream();
          pc.onicecandidate = (e) => { if (e.candidate) L.sendTo(src, { t: 'rtc', d: { ice: e.candidate.toJSON ? e.candidate.toJSON() : e.candidate } }); };
          pc.ontrack = (e) => {
            if (vpc !== pc) return;
            if (!ms.getTracks().includes(e.track)) ms.addTrack(e.track);
            clearTimeout(vTell);
            vTell = setTimeout(() => { if (vpc === pc) h.onMedia && h.onMedia(ms, src); }, 0);   // once, after both tracks
          };
          pc.onconnectionstatechange = () => {
            if (vpc !== pc) return;
            const s = pc.connectionState;
            if (s === 'connected') { clearTimeout(vTimer); h.onRtc && h.onRtc('connected', src); }
            else if (s === 'failed') h.onRtc && h.onRtc('failed', src);
            else if (s === 'disconnected') h.onRtc && h.onRtc('disconnected', src);
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
    function gotPart(d, from) {
      const n = d.n | 0, k = d.k | 0, i = d.i | 0;
      if (!n || k < 1 || k > 5 || i < 0 || i >= k || typeof d.d !== 'string' || n <= shownN) return;
      let f = parts.get(n);
      if (!f) { f = { k, p: new Array(k), got: 0 }; parts.set(n, f); }
      if (f.p[i] === undefined) { f.p[i] = d.d; f.got++; }
      if (f.got === f.k) {
        shownN = n;
        for (const key of [...parts.keys()]) if (key <= n) parts.delete(key);
        lastFrameAt = Date.now();
        h.onFrame && h.onFrame(f.p.join(''), from);
      }
      if (parts.size > 8) for (const key of [...parts.keys()].sort((a, b) => a - b).slice(0, parts.size - 8)) parts.delete(key);
    }

    connect();
    return L;
  }

  window.ComsLink = { join, cleanCode, GAME, QUALITY };
})();

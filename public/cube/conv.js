// conv.js: turns any video the browser can play (MP4, MOV, WebM...) into a video the ERROR-CUBE plays
// (MJPEG AVI + 16-bit mono PCM sound). Used by error.blue/cube (FILES uploads), /cube/video.html and the
// cube's own page http://<cube>/files. Everything happens in this browser.
//
//   CubeConv.isVideo(name)            -> true for a video the cube can't play as it is (mp4, mov, webm...)
//   CubeConv.aviName(name)            -> 'holiday.mp4' -> 'holiday.avi' (the cube looks for this twin)
//   CubeConv.convert(blob, opts, prog) -> Promise<{ blob, name, w, h, fps, len, sound }>
//        opts: size 320, fps 15, q 0.6, rate 22050 (0 = no sound), fill false (crop to a square),
//              from 0, len 0 (= all), speed 2 (plays the video this much faster while copying it),
//              maxSound 1200 (s: longer videos are converted without sound), view (element for the preview),
//              name (for the output name)
//        prog(fraction 0..1, text)
(function () {
  'use strict';
  const VID = /\.(mp4|m4v|mov|mkv|webm|3gp|3g2|ogv|mpg|mpeg|wmv|flv|ts)$/i;
  const isVideo = n => VID.test(n || '');
  const aviName = n => {
    let b = String(n || 'video').split('/').pop().replace(/\.[^.]+$/, '');
    b = b.replace(/[\\/:*?"<>|\x00-\x1f]/g, '_').trim().slice(0, 60) || 'video';
    return b + '.avi';
  };
  const tm = s => Math.floor(s / 60) + ':' + String(Math.floor(s % 60)).padStart(2, '0');

  // ------------------------------------------------------------------ the AVI file (what the cube's VIDEO.EXE reads)
  function le32(a, o, v) { a[o] = v & 255; a[o + 1] = v >> 8 & 255; a[o + 2] = v >> 16 & 255; a[o + 3] = v >>> 24 & 255; }
  function le16(a, o, v) { a[o] = v & 255; a[o + 1] = v >> 8 & 255; }
  function cc(a, o, s) { for (let i = 0; i < 4; i++) a[o + i] = s.charCodeAt(i); }
  function chunk(id, data) {
    const h = new Uint8Array(8); cc(h, 0, id); le32(h, 4, data.length);
    return data.length & 1 ? [h, data, new Uint8Array(1)] : [h, data];
  }
  function buildAvi(frames, audio, w, h, fps, rate) {
    const hasA = !!(audio && audio.length);
    const movi = [], idx = [];
    let off = 4, maxF = 0;
    const spf = hasA ? rate / fps : 0;
    for (let i = 0; i < frames.length; i++) {
      const f = frames[i]; maxF = Math.max(maxF, f.length);
      movi.push(...chunk('00dc', f)); idx.push(['00dc', 0x10, off, f.length]); off += 8 + f.length + (f.length & 1);
      if (hasA) {
        const a0 = Math.round(i * spf), a1 = i === frames.length - 1 ? audio.length : Math.min(audio.length, Math.round((i + 1) * spf));
        if (a1 > a0) {
          const pcm = new Uint8Array(audio.buffer, audio.byteOffset + a0 * 2, (a1 - a0) * 2).slice();
          movi.push(...chunk('01wb', pcm)); idx.push(['01wb', 0, off, pcm.length]); off += 8 + pcm.length + (pcm.length & 1);
        }
      }
    }
    const moviSize = 4 + movi.reduce((s, p) => s + p.length, 0);
    const avih = new Uint8Array(56);
    le32(avih, 0, Math.round(1e6 / fps)); le32(avih, 4, maxF * fps); le32(avih, 12, 0x10); le32(avih, 16, frames.length);
    le32(avih, 24, hasA ? 2 : 1); le32(avih, 28, maxF + 8); le32(avih, 32, w); le32(avih, 36, h);
    const vstrh = new Uint8Array(56);
    cc(vstrh, 0, 'vids'); cc(vstrh, 4, 'MJPG'); le32(vstrh, 20, 1); le32(vstrh, 24, fps); le32(vstrh, 32, frames.length);
    le32(vstrh, 36, maxF + 8); le32(vstrh, 40, -1); le16(vstrh, 52, w); le16(vstrh, 54, h);
    const vstrf = new Uint8Array(40);
    le32(vstrf, 0, 40); le32(vstrf, 4, w); le32(vstrf, 8, h); le16(vstrf, 12, 1); le16(vstrf, 14, 24); cc(vstrf, 16, 'MJPG'); le32(vstrf, 20, w * h * 3);
    const list = (type, parts) => { const body = parts.flat(); const n = 4 + body.reduce((s, p) => s + p.length, 0); const h8 = new Uint8Array(12); cc(h8, 0, 'LIST'); le32(h8, 4, n); cc(h8, 8, type); return [h8, ...body]; };
    const strls = [list('strl', [chunk('strh', vstrh), chunk('strf', vstrf)])];
    if (hasA) {
      const astrh = new Uint8Array(56);
      cc(astrh, 0, 'auds'); le32(astrh, 20, 1); le32(astrh, 24, rate); le32(astrh, 32, audio.length); le32(astrh, 36, rate); le32(astrh, 40, -1); le32(astrh, 44, 2);
      const astrf = new Uint8Array(16);
      le16(astrf, 0, 1); le16(astrf, 2, 1); le32(astrf, 4, rate); le32(astrf, 8, rate * 2); le16(astrf, 12, 2); le16(astrf, 14, 16);
      strls.push(list('strl', [chunk('strh', astrh), chunk('strf', astrf)]));
    }
    const hdrl = list('hdrl', [chunk('avih', avih), ...strls]);
    const moviH = new Uint8Array(12); cc(moviH, 0, 'LIST'); le32(moviH, 4, moviSize); cc(moviH, 8, 'movi');
    const ix = new Uint8Array(idx.length * 16);
    idx.forEach((e, i) => { cc(ix, i * 16, e[0]); le32(ix, i * 16 + 4, e[1]); le32(ix, i * 16 + 8, e[2]); le32(ix, i * 16 + 12, e[3]); });
    const parts = [...hdrl, moviH, ...movi, ...chunk('idx1', ix)];
    const total = parts.reduce((s, p) => s + p.length, 0);
    const riff = new Uint8Array(12); cc(riff, 0, 'RIFF'); le32(riff, 4, total + 4); cc(riff, 8, 'AVI ');
    return new Blob([riff, ...parts], { type: 'video/avi' });
  }

  // ------------------------------------------------------------------ the sound: decoded once, mono, the cube's rate
  async function getAudio(src, rate, from, len) {
    const ab = await src.arrayBuffer();
    let buf = null;
    try { const oc = new OfflineAudioContext(1, 1, rate); buf = await oc.decodeAudioData(ab.slice(0)); } catch (e) { buf = null; }
    if (!buf) {
      const AC = window.AudioContext || window.webkitAudioContext;
      const ac = new AC();
      try { buf = await new Promise((res, rej) => ac.decodeAudioData(ab, res, rej)); } finally { try { ac.close(); } catch (e) {} }
    }
    const sr = buf.sampleRate, nch = buf.numberOfChannels, ch = [];
    for (let i = 0; i < nch; i++) ch.push(buf.getChannelData(i));
    const n = Math.max(1, Math.floor(len * rate)), out = new Int16Array(n), k = sr / rate, s0 = from * sr, end = ch[0].length - 1;
    for (let i = 0; i < n; i++) {
      const p = s0 + i * k, a = Math.min(end, p | 0), b = Math.min(end, a + 1), f = p - a;
      let s = 0;
      for (let c = 0; c < nch; c++) s += ch[c][a] + (ch[c][b] - ch[c][a]) * f;
      s = s / nch * 32767;
      out[i] = s > 32767 ? 32767 : s < -32768 ? -32768 : s;
    }
    return out;
  }

  // ------------------------------------------------------------------ the pictures
  const wait = ms => new Promise(r => setTimeout(r, ms));
  function seekTo(vid, t) {
    return new Promise(res => {
      const d = () => { vid.removeEventListener('seeked', d); res(); };
      vid.addEventListener('seeked', d);
      vid.currentTime = t;
      setTimeout(d, 4000);
    });
  }
  async function convert(src, opts, prog) {
    const o = Object.assign({ size: 320, fps: 15, q: 0.6, rate: 22050, fill: false, from: 0, len: 0, speed: 2, maxSound: 1200 }, opts || {});
    prog = prog || (() => {});
    const url = URL.createObjectURL(src);
    const vid = document.createElement('video');
    vid.muted = true; vid.defaultMuted = true; vid.playsInline = true; vid.preload = 'auto';
    vid.setAttribute('playsinline', ''); vid.setAttribute('muted', '');
    vid.style.cssText = 'display:block;width:100%;max-width:260px;margin:8px auto;background:#000;border:2px solid #1b7a3c';
    let box = null;
    if (o.view) o.view.appendChild(vid);
    else {
      box = document.createElement('div');
      box.style.cssText = 'position:fixed;right:8px;bottom:8px;width:180px;z-index:99;background:#031208;border:2px solid #39ff7a;padding:4px;color:#39ff7a;font:14px monospace;text-align:center';
      box.textContent = 'CONVERTING FOR THE CUBE'; box.appendChild(vid); document.body.appendChild(box);
    }
    try {
      vid.src = url;
      await new Promise((res, rej) => {
        vid.onloadedmetadata = res;
        vid.onerror = () => rej(new Error('THIS BROWSER CAN\'T OPEN THAT VIDEO'));
        setTimeout(() => rej(new Error('THE VIDEO DOESN\'T LOAD')), 30000);
      });
      let dur = vid.duration;
      if (!isFinite(dur)) {               // some WebM files: find the length by seeking far
        await seekTo(vid, 1e7); dur = isFinite(vid.duration) ? vid.duration : vid.currentTime; await seekTo(vid, 0);
      }
      const vw = vid.videoWidth, vh = vid.videoHeight;
      if (!vw || !vh) throw new Error('NO PICTURE IN THIS FILE');
      const from = Math.max(0, Math.min(o.from || 0, dur - 0.1));
      const len = Math.max(0.1, Math.min(dur - from, o.len || 1e9));
      const fps = o.fps, n = Math.max(1, Math.floor(len * fps));
      let w, h, sx = 0, sy = 0, sw = vw, sh = vh;
      if (o.fill) { w = h = o.size; const s = Math.min(vw, vh); sw = sh = s; sx = (vw - s) / 2; sy = (vh - s) / 2; }
      else { const k = Math.min(1, o.size / Math.max(vw, vh)); w = Math.max(16, Math.round(vw * k / 2) * 2); h = Math.max(16, Math.round(vh * k / 2) * 2); }
      const held = document.createElement('canvas'); held.width = w; held.height = h;
      const hc = held.getContext('2d');
      let audio = null;
      if (o.rate && len <= o.maxSound) {
        prog(0, 'THE SOUND...');
        try { audio = await getAudio(src, o.rate, from, len); } catch (e) { console.log('no sound:', e); audio = null; }
      }
      const frames = new Array(n), jobs = [];
      const encode = () => {               // the picture now in `held` -> a JPEG (one job, shared by every slot it fills)
        const p = new Promise(r => held.toBlob(r, 'image/jpeg', o.q)).then(b => b.arrayBuffer()).then(a => new Uint8Array(a));
        jobs.push(p); return p;
      };
      const t0 = Date.now();
      const say = done => {
        const el = (Date.now() - t0) / 1000, left = done > 0.02 ? el / done * (1 - done) : 0;
        prog(Math.min(1, done), 'CONVERTING ' + Math.round(done * 100) + '%' + (left ? ' · ' + Math.round(left) + ' S LEFT' : '') + ' (' + tm(len) + ' of video)');
      };
      let next = 0;
      if ('requestVideoFrameCallback' in HTMLVideoElement.prototype) {
        // play it (faster than real time, silently) and copy the frames as they show
        if (from > 0) await seekTo(vid, from);
        let have = false, lastCb = Date.now(), rate = Math.max(1, o.speed || 1), q0 = null, wS = 0, wE = 0;
        await new Promise((res, rej) => {
          let over = false;
          const finish = () => { if (!over) { over = true; clearInterval(watch); vid.pause(); res(); } };
          const onFrame = (now, md) => {
            if (over) return;
            lastCb = Date.now();
            const t = md.mediaTime - from;
            if (t >= 0) {
              // the picture held so far stands for every slot before this frame's time
              if (!have) { hc.drawImage(vid, sx, sy, sw, sh, 0, 0, w, h); have = true; }
              const s = next;
              while (next < n && next / fps < t - 1e-4) next++;
              if (next > s) { const p = encode(); for (let i = s; i < next; i++) frames[i] = p; wS += next - s; wE++; }
              hc.drawImage(vid, sx, sy, sw, sh, 0, 0, w, h);
              say(next / n);
            }
            if (next >= n || t >= len) finish(); else vid.requestVideoFrameCallback(onFrame);
          };
          const watch = setInterval(() => {
            if (over) return;
            // too fast for this phone (dropping frames)? play it slower
            const pq = vid.getVideoPlaybackQuality ? vid.getVideoPlaybackQuality() : null;
            if (pq && q0 && rate > 1) {
              const tot = pq.totalVideoFrames - q0.totalVideoFrames, dr = pq.droppedVideoFrames - q0.droppedVideoFrames;
              if (tot > 20 && dr / tot > 0.12) { rate = 1; vid.playbackRate = 1; }
            }
            if (pq) q0 = { totalVideoFrames: pq.totalVideoFrames, droppedVideoFrames: pq.droppedVideoFrames };
            // or: many slots got the same picture (frames skipped): slower
            if (rate > 1 && wS >= 20 && wE / wS < 0.8) { rate = Math.max(1, rate / 2); vid.playbackRate = rate; }
            wS = wE = 0;
            if (document.hidden) { lastCb = Date.now(); prog(next / n, 'PAUSED: KEEP THIS PAGE OPEN ON THE SCREEN'); return; }
            if (vid.ended) { finish(); return; }
            if (Date.now() - lastCb > 6000) { vid.play().catch(() => {}); }
            if (Date.now() - lastCb > 25000) { over = true; clearInterval(watch); rej(new Error('THE VIDEO STOPPED PLAYING')); }
          }, 1000);
          vid.onended = () => setTimeout(finish, 300);
          vid.requestVideoFrameCallback(onFrame);
          vid.playbackRate = rate;
          try { vid.preservesPitch = false; } catch (e) {}
          vid.play().catch(e => { over = true; clearInterval(watch); rej(new Error('THE BROWSER WON\'T PLAY IT: ' + e.message)); });
        });
        if (next < n) {                       // the end: the last picture fills what's left
          if (!have) { await seekTo(vid, from + Math.min(len, next / fps)); hc.drawImage(vid, sx, sy, sw, sh, 0, 0, w, h); }
          const p = encode(); for (let i = next; i < n; i++) frames[i] = p;
        }
      } else {
        // older browsers: seek to every frame
        for (let i = 0; i < n; i++) {
          await seekTo(vid, from + i / fps + 0.001);
          hc.drawImage(vid, sx, sy, sw, sh, 0, 0, w, h);
          frames[i] = encode();
          if (i % 3 === 0) say((i + 1) / n);
          if (i % 8 === 7) await Promise.all(jobs.splice(0));
        }
      }
      prog(1, 'PACKING...');
      for (let i = 0; i < n; i++) frames[i] = await frames[i];
      await wait(0);
      const blob = buildAvi(frames, audio, w, h, fps, o.rate);
      return { blob, name: aviName(o.name || src.name || 'video'), w, h, fps, len, sound: !!audio, secs: Math.round((Date.now() - t0) / 1000) };
    } finally {
      try { vid.pause(); vid.removeAttribute('src'); vid.load(); } catch (e) {}
      vid.remove(); if (box) box.remove();
      URL.revokeObjectURL(url);
    }
  }

  window.CubeConv = { isVideo, aviName, convert, buildAvi, getAudio };
})();

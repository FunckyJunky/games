// Funda EYE · CYBERPUNK: the 150 m around the house as a neon city. Fly over it, or walk through the streets.
// A three.js scene built from the BAG building outlines the 3D map already has: windows lit in the colour the
// map gives each house (difference, WOZ value, score ...), neon edges, a glowing 150 m ring, rain and bloom.
//   createCyber(host) -> { set(data), resize(), dispose() }
//   data: { center: [lon, lat], radius, buildings: [{ ring: [[lon, lat]...], h, color, me, known, info }],
//           labels: [{ lon, lat, h, text, me }] }
import * as THREE from "three";
import { OrbitControls } from "three/addons/controls/OrbitControls.js";
import { EffectComposer } from "three/addons/postprocessing/EffectComposer.js";
import { RenderPass } from "three/addons/postprocessing/RenderPass.js";
import { UnrealBloomPass } from "three/addons/postprocessing/UnrealBloomPass.js";
import { OutputPass } from "three/addons/postprocessing/OutputPass.js";

const DEG = Math.PI / 180;
const EYE_H = 1.7, WALK = 4.2, RUN = 10, BODY = 0.45;

export function createCyber(host) {
  const touch = matchMedia("(pointer: coarse)").matches;
  const reduce = matchMedia("(prefers-reduced-motion: reduce)").matches;

  // ---------- renderer, scene, camera, glow ----------
  const renderer = new THREE.WebGLRenderer({ antialias: !touch, powerPreference: "high-performance" });
  renderer.setPixelRatio(Math.min(devicePixelRatio || 1, touch ? 1.5 : 2));
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.toneMappingExposure = 0.95;
  renderer.domElement.className = "cy-canvas";
  host.appendChild(renderer.domElement);

  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0x04020b);
  scene.fog = new THREE.FogExp2(0x07030f, 0.0042);
  const camera = new THREE.PerspectiveCamera(62, 1, 0.1, 2500);
  camera.position.set(150, 120, 170);

  const composer = new EffectComposer(renderer);
  composer.addPass(new RenderPass(scene, camera));
  const bloom = new UnrealBloomPass(new THREE.Vector2(256, 256), 0.62, 0.35, 0.42);
  composer.addPass(bloom);
  composer.addPass(new OutputPass());

  // sky: dark at the top, a purple-pink glow along the horizon
  const sky = new THREE.Mesh(new THREE.SphereGeometry(1800, 32, 16), new THREE.ShaderMaterial({
    side: THREE.BackSide, depthWrite: false, fog: false,
    vertexShader: "varying vec3 vD; void main(){ vD = normalize(position); gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }",
    fragmentShader: `varying vec3 vD;
      void main(){
        float y = vD.y;
        vec3 top = vec3(0.005, 0.003, 0.016), mid = vec3(0.03, 0.008, 0.06), hor = vec3(0.2, 0.025, 0.16);
        vec3 c = mix(mid, top, smoothstep(0.02, 0.55, y));
        c = mix(c, hor, exp(-pow(max(y, -0.2) / 0.06, 2.0)) * 0.8);
        gl_FragColor = vec4(c, 1.0);
      }`,
  }));
  scene.add(sky);

  // ground: a neon grid that fades out, and the 150 m ring
  const groundMat = new THREE.ShaderMaterial({
    uniforms: { uR: { value: 150 }, uT: { value: 0 } },
    vertexShader: "varying vec2 vP; void main(){ vec4 w = modelMatrix * vec4(position, 1.0); vP = w.xz; gl_Position = projectionMatrix * viewMatrix * w; }",
    fragmentShader: `varying vec2 vP; uniform float uR; uniform float uT;
      float grid(vec2 p, float s, float w){ vec2 g = abs(fract(p / s - 0.5) - 0.5) * s; return 1.0 - smoothstep(0.0, w, min(g.x, g.y)); }
      void main(){
        float r = length(vP);
        float fade = 1.0 - smoothstep(uR * 0.85, uR * 2.4, r);
        vec3 c = vec3(0.012, 0.008, 0.028);
        c += vec3(0.0, 0.8, 1.0) * grid(vP, 6.0, 0.06) * 0.07 * fade;
        c += vec3(0.55, 0.05, 1.0) * grid(vP, 30.0, 0.12) * 0.12 * fade;
        float ring = 1.0 - smoothstep(0.0, 0.55, abs(r - uR));
        c += vec3(1.0, 0.1, 0.72) * ring * (0.75 + 0.15 * sin(uT * 2.2));
        c += vec3(1.0, 0.1, 0.72) * exp(-pow((r - uR) / 7.0, 2.0)) * 0.05;
        c += vec3(0.0, 0.5, 0.7) * exp(-r / 26.0) * 0.04;
        gl_FragColor = vec4(c, 1.0);
      }`,
  });
  const ground = new THREE.Mesh(new THREE.PlaneGeometry(1400, 1400), groundMat);
  ground.rotation.x = -Math.PI / 2;
  scene.add(ground);

  // windows: one 12 x 12 m tile, repeated along every wall (uv in metres)
  const winTex = (() => {
    const c = document.createElement("canvas"); c.width = c.height = 256;
    const g = c.getContext("2d");
    g.fillStyle = "#0a0b14"; g.fillRect(0, 0, 256, 256);
    let seed = 11; const rnd = () => (seed = (seed * 16807) % 2147483647) / 2147483647;
    for (let i = 0; i < 4; i++) for (let j = 0; j < 4; j++) {
      const r = rnd(), b = r < 0.22 ? 0.06 : r < 0.5 ? 0.45 : r < 0.85 ? 0.85 : 1;
      g.fillStyle = `rgba(255,255,255,${b})`;
      g.fillRect(i * 64 + 16, j * 64 + 12, 32, 30);
      g.fillStyle = "rgba(10,11,20,.55)"; g.fillRect(i * 64 + 31, j * 64 + 12, 2, 30); // window frame
    }
    g.fillStyle = "rgba(255,255,255,.08)"; for (let j = 0; j < 4; j++) g.fillRect(0, j * 64 + 60, 256, 3); // floors
    const t = new THREE.CanvasTexture(c);
    t.wrapS = t.wrapT = THREE.RepeatWrapping; t.colorSpace = THREE.SRGBColorSpace; t.anisotropy = 4;
    t.repeat.set(1 / 12, 1 / 12);
    return t;
  })();
  const wallMat = new THREE.MeshBasicMaterial({ map: winTex, vertexColors: true, side: THREE.DoubleSide });
  const roofMat = new THREE.MeshBasicMaterial({ vertexColors: true, side: THREE.DoubleSide });
  const edgeMat = new THREE.LineBasicMaterial({ vertexColors: true, transparent: true, opacity: 0.95 });

  let city = null, polys = [], buildings = [], triWall = [], triRoof = [], R = 150, self = null, labels = [];
  const labelGroup = new THREE.Group(); scene.add(labelGroup);

  // the colour a house has on the 3D map, turned to neon
  function neon(css, me, known) {
    const c = new THREE.Color(); try { c.setStyle(css || "#888"); } catch { c.set("#888"); }
    const hsl = {}; c.getHSL(hsl);
    if (me) return { win: new THREE.Color(0x00f6ff).multiplyScalar(1.05), edge: new THREE.Color(0x40fbff).multiplyScalar(1.35), roof: new THREE.Color(0x002a33) };
    if (!known || hsl.s < 0.12) return { win: new THREE.Color().setHSL(0.62, 0.5, 0.4), edge: new THREE.Color().setHSL(0.74, 0.85, 0.38), roof: new THREE.Color(0x0a0816) };
    const win = new THREE.Color().setHSL(hsl.h, 1, 0.55).multiplyScalar(0.85);
    return { win, edge: new THREE.Color().setHSL(hsl.h, 1, 0.6).multiplyScalar(1.15), roof: new THREE.Color().setHSL(hsl.h, 0.7, 0.05) };
  }

  function textSprite(text, me) {
    const c = document.createElement("canvas"), g = c.getContext("2d");
    const fs = 44; g.font = `800 ${fs}px ui-monospace, Menlo, Consolas, monospace`;
    const w = Math.ceil(g.measureText(text).width) + 40; c.width = w; c.height = 72;
    g.font = `800 ${fs}px ui-monospace, Menlo, Consolas, monospace`;
    g.fillStyle = me ? "rgba(0,40,52,.72)" : "rgba(20,4,30,.62)"; g.strokeStyle = me ? "#40fbff" : "#ff3fd0"; g.lineWidth = 3;
    g.beginPath(); g.roundRect(3, 3, w - 6, 66, 12); g.fill(); g.stroke();
    g.fillStyle = "#ffffff"; g.shadowColor = me ? "#40fbff" : "#ff3fd0"; g.shadowBlur = 14; g.textBaseline = "middle";
    g.fillText(text, 20, 38);
    const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace;
    const m = new THREE.SpriteMaterial({ map: t, transparent: true, depthWrite: false, fog: false, depthTest: !me });
    const s = new THREE.Sprite(m);
    s.userData = { aspect: w / 72, me };
    s.renderOrder = me ? 10 : 5;
    return s;
  }
  function sizeLabels() {
    for (const s of labelGroup.children) {
      const a = s.userData.aspect;
      if (mode === "walk") { s.material.sizeAttenuation = true; s.scale.set(1.3 * a, 1.3, 1); }
      else { s.material.sizeAttenuation = false; const k = s.userData.me ? 0.05 : 0.036; s.scale.set(k * a, k, 1); }
      s.material.needsUpdate = true;
    }
  }

  let beam = null;
  function set(data) {
    const lon0 = data.center[0], lat0 = data.center[1];
    const kx = 111320 * Math.cos(lat0 * DEG), kz = 111320;
    const P = (lon, lat) => [(lon - lon0) * kx, -(lat - lat0) * kz];
    R = data.radius || 150; groundMat.uniforms.uR.value = R;
    const wp = [], wu = [], wc = [], rp = [], rc = [], ep = [], ec = [];
    polys = []; buildings = data.buildings; triWall = []; triRoof = []; self = null;
    data.buildings.forEach((b, bi) => {
      let pts = b.ring.map(([lo, la]) => P(lo, la));
      if (pts.length > 1 && pts[0][0] === pts[pts.length - 1][0] && pts[0][1] === pts[pts.length - 1][1]) pts = pts.slice(0, -1);
      if (pts.length < 3) return;
      const h = Math.max(0.6, b.h || 6);
      const col = neon(b.color, b.me, b.known);
      let u = 0;
      for (let i = 0; i < pts.length; i++) {
        const a = pts[i], q = pts[(i + 1) % pts.length], len = Math.hypot(q[0] - a[0], q[1] - a[1]);
        wp.push(a[0], 0, a[1], q[0], 0, q[1], q[0], h, q[1], a[0], 0, a[1], q[0], h, q[1], a[0], h, a[1]);
        wu.push(u, 0, u + len, 0, u + len, h, u, 0, u + len, h, u, h);
        for (let k = 0; k < 6; k++) wc.push(col.win.r, col.win.g, col.win.b);
        triWall.push(bi, bi);
        u += len;
        // neon edges: roof line bright, corner lines and the foot dimmer
        ep.push(a[0], h, a[1], q[0], h, q[1], a[0], 0.05, a[1], q[0], 0.05, q[1], a[0], 0, a[1], a[0], h, a[1]);
        const e = col.edge, d = 0.45;
        ec.push(e.r, e.g, e.b, e.r, e.g, e.b, e.r * d, e.g * d, e.b * d, e.r * d, e.g * d, e.b * d, e.r * d, e.g * d, e.b * d, e.r, e.g, e.b);
      }
      const tris = THREE.ShapeUtils.triangulateShape(pts.map((p) => new THREE.Vector2(p[0], p[1])), []);
      for (const t of tris) { for (const k of t) { rp.push(pts[k][0], h, pts[k][1]); rc.push(col.roof.r, col.roof.g, col.roof.b); } triRoof.push(bi); }
      let minx = Infinity, maxx = -Infinity, minz = Infinity, maxz = -Infinity, cx = 0, cz = 0;
      for (const p of pts) { minx = Math.min(minx, p[0]); maxx = Math.max(maxx, p[0]); minz = Math.min(minz, p[1]); maxz = Math.max(maxz, p[1]); cx += p[0]; cz += p[1]; }
      const poly = { pts, minx, maxx, minz, maxz, cx: cx / pts.length, cz: cz / pts.length, h, bi };
      polys.push(poly);
      if (b.me) self = poly;
    });
    if (city) { scene.remove(city); city.traverse((o) => o.geometry && o.geometry.dispose()); }
    city = new THREE.Group();
    const g1 = new THREE.BufferGeometry();
    g1.setAttribute("position", new THREE.Float32BufferAttribute(wp, 3));
    g1.setAttribute("uv", new THREE.Float32BufferAttribute(wu, 2));
    g1.setAttribute("color", new THREE.Float32BufferAttribute(wc, 3));
    const walls = new THREE.Mesh(g1, wallMat); walls.name = "walls";
    const g2 = new THREE.BufferGeometry();
    g2.setAttribute("position", new THREE.Float32BufferAttribute(rp, 3));
    g2.setAttribute("color", new THREE.Float32BufferAttribute(rc, 3));
    const roofs = new THREE.Mesh(g2, roofMat); roofs.name = "roofs";
    const g3 = new THREE.BufferGeometry();
    g3.setAttribute("position", new THREE.Float32BufferAttribute(ep, 3));
    g3.setAttribute("color", new THREE.Float32BufferAttribute(ec, 3));
    city.add(walls, roofs, new THREE.LineSegments(g3, edgeMat));
    scene.add(city);
    pickables = [walls, roofs];

    // a beam of light over this house
    if (beam) { scene.remove(beam); beam.geometry.dispose(); beam = null; }
    if (self) {
      beam = new THREE.Mesh(new THREE.CylinderGeometry(0.7, 0.7, 120, 16, 1, true),
        new THREE.MeshBasicMaterial({ color: new THREE.Color(0x30f4ff), transparent: true, opacity: 0.16, blending: THREE.AdditiveBlending, depthWrite: false, fog: false }));
      beam.position.set(self.cx, self.h + 60, self.cz);
      scene.add(beam);
    }
    // floating tags
    for (const s of labelGroup.children) { s.material.map.dispose(); s.material.dispose(); }
    labelGroup.clear();
    for (const l of data.labels || []) {
      const [x, z] = P(l.lon, l.lat);
      const s = textSprite(l.text, l.me);
      s.position.set(x, (l.h || 6) + (l.me ? 7 : 3.2), z);
      labelGroup.add(s);
    }
    sizeLabels();
    if (!placed) { placed = true; flyHome(); }
  }

  // ---------- fly over (orbit) ----------
  const orbit = new OrbitControls(camera, renderer.domElement);
  orbit.enableDamping = true; orbit.dampingFactor = 0.08;
  orbit.maxPolarAngle = 1.42; orbit.minDistance = 20; orbit.maxDistance = 520;
  orbit.autoRotate = !reduce; orbit.autoRotateSpeed = 0.55;
  orbit.addEventListener("start", () => { orbit.autoRotate = false; });
  let placed = false, pickables = [];
  function flyHome() {
    const t = self ? new THREE.Vector3(self.cx, 0, self.cz) : new THREE.Vector3();
    orbit.target.copy(t);
    camera.position.set(t.x + R * 0.55, R * 0.42, t.z + R * 0.62);
    orbit.update();
  }

  // ---------- walk ----------
  let mode = "fly";
  const me = { x: 0, z: 0, yaw: 0, pitch: 0, bob: 0 };
  const keys = new Set();
  const joy = { id: null, ox: 0, oy: 0, x: 0, y: 0 }, look = { id: null, x: 0, y: 0 };
  function inside(x, z, pts) {
    let c = false;
    for (let i = 0, j = pts.length - 1; i < pts.length; j = i++) {
      const [xi, zi] = pts[i], [xj, zj] = pts[j];
      if ((zi > z) !== (zj > z) && x < ((xj - xi) * (z - zi)) / (zj - zi) + xi) c = !c;
    }
    return c;
  }
  function nearEdge(x, z, pts, r) {
    for (let i = 0, j = pts.length - 1; i < pts.length; j = i++) {
      const [ax, az] = pts[j], [bx, bz] = pts[i], dx = bx - ax, dz = bz - az;
      const t = Math.max(0, Math.min(1, ((x - ax) * dx + (z - az) * dz) / (dx * dx + dz * dz || 1)));
      if (Math.hypot(x - (ax + t * dx), z - (az + t * dz)) < r) return true;
    }
    return false;
  }
  function blocked(x, z) {
    if (Math.hypot(x, z) > R + 60) return true;
    for (const p of polys) {
      if (x < p.minx - 1 || x > p.maxx + 1 || z < p.minz - 1 || z > p.maxz + 1) continue;
      if (inside(x, z, p.pts) || nearEdge(x, z, p.pts, BODY)) return true;
    }
    return false;
  }
  function startWalk() {
    mode = "walk"; orbit.enabled = false; orbit.autoRotate = false;
    const c = self ? [self.cx, self.cz] : [0, 0];
    let spot = null;
    for (const r of [13, 16, 10, 20, 25, 8]) {
      for (let k = 0; k < 24 && !spot; k++) { const a = (k / 24) * Math.PI * 2, x = c[0] + Math.cos(a) * r, z = c[1] + Math.sin(a) * r; if (!blocked(x, z)) spot = [x, z]; }
      if (spot) break;
    }
    spot = spot || [c[0] + 20, c[1]];
    me.x = spot[0]; me.z = spot[1];
    me.yaw = Math.atan2(-(c[0] - me.x), -(c[1] - me.z)); me.pitch = 0.08;
    sizeLabels(); hud();
  }
  function stopWalk() { mode = "fly"; orbit.enabled = true; flyHome(); sizeLabels(); hud(); }

  const onKey = (e) => {
    if (mode !== "walk" || !host.isConnected || host.offsetParent === null) return;
    const k = e.code;
    if (["KeyW", "KeyA", "KeyS", "KeyD", "ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", "ShiftLeft", "ShiftRight", "KeyQ", "KeyE"].includes(k)) {
      e.preventDefault();
      if (e.type === "keydown") keys.add(k); else keys.delete(k);
    }
  };
  addEventListener("keydown", onKey); addEventListener("keyup", onKey);
  addEventListener("blur", () => keys.clear());

  const cv = renderer.domElement;
  cv.addEventListener("pointerdown", (e) => {
    if (mode !== "walk") { tapPick(e); return; }
    const r = cv.getBoundingClientRect(), x = e.clientX - r.left;
    if (e.pointerType === "touch" && x < r.width * 0.45 && joy.id === null) {
      joy.id = e.pointerId; joy.ox = e.clientX; joy.oy = e.clientY; joy.x = joy.y = 0; showJoy(true, e.clientX - r.left, e.clientY - r.top);
    } else if (look.id === null) { look.id = e.pointerId; look.x = e.clientX; look.y = e.clientY; }
    cv.setPointerCapture(e.pointerId);
  });
  cv.addEventListener("pointermove", (e) => {
    if (e.pointerId === joy.id) {
      const dx = e.clientX - joy.ox, dy = e.clientY - joy.oy, m = Math.min(1, Math.hypot(dx, dy) / 55), a = Math.atan2(dy, dx);
      joy.x = Math.cos(a) * m; joy.y = Math.sin(a) * m; moveKnob(joy.x, joy.y);
    } else if (e.pointerId === look.id) {
      const k = e.pointerType === "touch" ? 0.006 : 0.004;
      me.yaw -= (e.clientX - look.x) * k; me.pitch = Math.max(-1.2, Math.min(1.2, me.pitch - (e.clientY - look.y) * k));
      look.x = e.clientX; look.y = e.clientY;
    }
  });
  const up = (e) => {
    if (e.pointerId === joy.id) { joy.id = null; joy.x = joy.y = 0; showJoy(false); }
    if (e.pointerId === look.id) look.id = null;
  };
  cv.addEventListener("pointerup", up); cv.addEventListener("pointercancel", up);

  // ---------- HUD ----------
  host.insertAdjacentHTML("beforeend", `
    <div class="cy-hud">
      <div class="cy-top"><span class="cy-tag">CYBER // 150 M</span><span class="cy-read" id="cyRead"></span></div>
      <div class="cy-cross" id="cyCross"></div>
      <div class="cy-info" id="cyInfo"></div>
      <div class="cy-joy" id="cyJoy"><i></i></div>
      <div class="cy-help" id="cyHelp"></div>
      <div class="cy-btns"><button type="button" id="cyWalk">▶ WALK</button><button type="button" id="cyRain" aria-pressed="true">RAIN</button></div>
    </div>`);
  const $c = (id) => host.querySelector("#" + id);
  $c("cyWalk").addEventListener("click", () => (mode === "walk" ? stopWalk() : startWalk()));
  $c("cyRain").addEventListener("click", () => { rain.visible = !rain.visible; $c("cyRain").setAttribute("aria-pressed", rain.visible); });
  function hud() {
    $c("cyWalk").textContent = mode === "walk" ? "▲ FLY" : "▶ WALK";
    $c("cyCross").style.display = mode === "walk" ? "block" : "none";
    $c("cyHelp").textContent = mode === "walk"
      ? (touch ? "LEFT THUMB: WALK · RIGHT THUMB: LOOK" : "WASD / ARROWS: WALK · DRAG: LOOK · SHIFT: RUN")
      : (touch ? "DRAG TO TURN · PINCH TO ZOOM · TAP A HOUSE" : "DRAG TO TURN · SCROLL TO ZOOM · CLICK A HOUSE");
    if (mode !== "walk") $c("cyInfo").innerHTML = "";
  }
  function showJoy(on, x, y) { const j = $c("cyJoy"); j.style.display = on ? "block" : "none"; if (on) { j.style.left = x + "px"; j.style.top = y + "px"; moveKnob(0, 0); } }
  function moveKnob(x, y) { $c("cyJoy").firstElementChild.style.transform = `translate(${x * 32}px, ${y * 32}px)`; }
  hud();

  // what you are looking at (walk) or tapped (fly)
  const ray = new THREE.Raycaster(); ray.far = 400;
  function pick(ndcX, ndcY) {
    ray.setFromCamera(new THREE.Vector2(ndcX, ndcY), camera);
    const hit = ray.intersectObjects(pickables, false)[0];
    if (!hit) return null;
    const bi = hit.object.name === "walls" ? triWall[hit.faceIndex] : triRoof[hit.faceIndex];
    return bi == null ? null : { b: buildings[bi], d: hit.distance };
  }
  let down = null;
  function tapPick(e) {
    down = { x: e.clientX, y: e.clientY };
    const done = (ev) => {
      cv.removeEventListener("pointerup", done);
      if (!down || Math.hypot(ev.clientX - down.x, ev.clientY - down.y) > 6) return;
      const r = cv.getBoundingClientRect();
      const p = pick(((ev.clientX - r.left) / r.width) * 2 - 1, -((ev.clientY - r.top) / r.height) * 2 + 1);
      $c("cyInfo").innerHTML = p && p.b.info ? p.b.info : "";
    };
    cv.addEventListener("pointerup", done);
  }

  // ---------- rain ----------
  const RAIN_N = touch ? 900 : 1800;
  const rainPos = new Float32Array(RAIN_N * 6);
  const rainGeo = new THREE.BufferGeometry(); rainGeo.setAttribute("position", new THREE.BufferAttribute(rainPos, 3));
  const rain = new THREE.LineSegments(rainGeo, new THREE.LineBasicMaterial({ color: 0x9fdcff, transparent: true, opacity: 0.22, fog: true }));
  rain.frustumCulled = false; scene.add(rain);
  const drops = new Float32Array(RAIN_N * 3);
  const resetDrop = (i, cx, cz, top) => { drops[i * 3] = cx + (Math.random() - 0.5) * 160; drops[i * 3 + 1] = top ? 30 + Math.random() * 40 : Math.random() * 70; drops[i * 3 + 2] = cz + (Math.random() - 0.5) * 160; };
  for (let i = 0; i < RAIN_N; i++) resetDrop(i, 0, 0, false);

  // ---------- size, loop ----------
  function resize() {
    const w = host.clientWidth || 300, h = host.clientHeight || 300;
    renderer.setSize(w, h, false); composer.setSize(w, h);
    bloom.resolution.set(w / 2, h / 2);
    camera.aspect = w / h; camera.updateProjectionMatrix();
  }
  const ro = new ResizeObserver(resize); ro.observe(host); resize();
  let visible = true;
  const io = new IntersectionObserver((es) => { visible = es[0].isIntersecting; }); io.observe(host);

  let last = performance.now(), raf = 0, lastPick = 0, alive = true;
  const fwd = new THREE.Vector3();
  function frame(now) {
    if (!alive) return;
    raf = requestAnimationFrame(frame);
    const dt = Math.min(0.05, (now - last) / 1000); last = now;
    if (!visible || document.hidden) return;
    groundMat.uniforms.uT.value = now / 1000;
    if (mode === "fly") orbit.update();
    else {
      // keys + joystick -> movement on the ground, sliding along walls
      let f = 0, s = 0, turn = 0;
      if (keys.has("KeyW") || keys.has("ArrowUp")) f += 1;
      if (keys.has("KeyS") || keys.has("ArrowDown")) f -= 1;
      if (keys.has("KeyA")) s -= 1;
      if (keys.has("KeyD")) s += 1;
      if (keys.has("ArrowLeft") || keys.has("KeyQ")) turn += 1;
      if (keys.has("ArrowRight") || keys.has("KeyE")) turn -= 1;
      f += -joy.y; s += joy.x;
      me.yaw += turn * 1.9 * dt;
      const mag = Math.min(1, Math.hypot(f, s));
      const speed = keys.has("ShiftLeft") || keys.has("ShiftRight") || Math.hypot(joy.x, joy.y) > 0.92 ? RUN : WALK;
      if (mag > 0.01) {
        const sy = Math.sin(me.yaw), cy = Math.cos(me.yaw);
        const vx = (-sy * f + cy * s) / Math.max(1, Math.hypot(f, s)), vz = (-cy * f - sy * s) / Math.max(1, Math.hypot(f, s));
        const step = speed * mag * dt;
        const stuck = blocked(me.x, me.z);
        const nx = me.x + vx * step, nz = me.z + vz * step;
        if (stuck || !blocked(nx, me.z)) me.x = nx;
        if (stuck || !blocked(me.x, nz)) me.z = nz;
        me.bob += step * 2.2;
      }
      camera.position.set(me.x, EYE_H + Math.sin(me.bob) * 0.045, me.z);
      camera.rotation.set(me.pitch, me.yaw, 0, "YXZ");
      if (now - lastPick > 160) {
        lastPick = now;
        const p = pick(0, 0);
        $c("cyInfo").innerHTML = p && p.d < 80 && p.b.info ? p.b.info : "";
        $c("cyCross").classList.toggle("on", !!(p && p.d < 80));
      }
    }
    // rain falls around the camera
    if (rain.visible) {
      const cx = camera.position.x, cz = camera.position.z;
      for (let i = 0; i < RAIN_N; i++) {
        let y = drops[i * 3 + 1] - 26 * dt;
        if (y < 0 || Math.abs(drops[i * 3] - cx) > 85 || Math.abs(drops[i * 3 + 2] - cz) > 85) { resetDrop(i, cx, cz, y < 0); y = drops[i * 3 + 1]; }
        drops[i * 3 + 1] = y;
        const x = drops[i * 3], z = drops[i * 3 + 2];
        rainPos.set([x, y, z, x - 0.05, y + 0.75, z], i * 6);
      }
      rainGeo.attributes.position.needsUpdate = true;
    }
    if (beam) beam.material.opacity = 0.12 + 0.04 * Math.sin(now / 420);
    camera.getWorldDirection(fwd);
    const deg = ((Math.atan2(fwd.x, -fwd.z) / DEG) + 360) % 360;
    const home = self ? Math.hypot(camera.position.x - self.cx, camera.position.z - self.cz) : 0;
    $c("cyRead").textContent = `${["N", "NE", "E", "SE", "S", "SW", "W", "NW"][Math.round(deg / 45) % 8]} ${String(Math.round(deg)).padStart(3, "0")}° · HOME ${Math.round(home)} M`;
    composer.render();
  }
  raf = requestAnimationFrame(frame);

  function dispose() {
    alive = false; cancelAnimationFrame(raf);
    removeEventListener("keydown", onKey); removeEventListener("keyup", onKey);
    ro.disconnect(); io.disconnect(); orbit.dispose();
    scene.traverse((o) => { if (o.geometry) o.geometry.dispose(); if (o.material) { (Array.isArray(o.material) ? o.material : [o.material]).forEach((m) => { if (m.map) m.map.dispose(); m.dispose(); }); } });
    composer.dispose && composer.dispose(); renderer.dispose();
    host.innerHTML = "";
  }
  return { set, resize, dispose };
}

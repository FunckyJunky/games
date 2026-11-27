// dream.js: DREAM-CUBE (esp32/DreamCube) on error.blue. The cube hears speech (its pieces go to /api/stt, Whisper),
// sends the words here, and gets back a small 3D scene to show: what the talk is about, as a little animated world.
//
//   POST /api/dream            {text, lang, id, code?, mind: "quick"|"smart", prev?}  -> {ok, scene, n?, via}
//        the words -> a scene (an LLM on Workers AI, JSON), checked against what the cube can draw. When the AI isn't
//        there or today's limit is reached, a word list makes the scene instead (free, always works).
//   The cube and the control page at error.blue/dream (linked by the cube's 8-letter code):
//   The cube and the control page at error.blue/dream (linked by the cube's 8-letter code):
//   POST /api/dream/cube   {code, st, last, lines: [{t, ago}], pair: [{id, ok}]} -> {ok, cmds: [{n, c, a}], top}
//        the cube: how it is, what it heard (the live transcript), its answers to "link a phone?" -> what to do
//   GET  /api/dream/near?hint=IPv4      the page, no code yet: cubes seen from this address (or the hinted IPv4: phones
//        often come over IPv6, the cube over IPv4) are asked on their screen "LINK A PHONE?" -> {pair, num}
//   GET  /api/dream/pair?id=            -> {code} once someone pressed YES on the cube, {wait} / {no}
//   GET  /api/dream/app?code=&since=N&lsince=L   -> {ok, cube, scenes, lines, mark, tx, interp}
//   POST /api/dream/app?code=  {c, a}   the page: listen pause wake sleep prev next cfg(a = {...}) unlink show(n) say(words)
//        rm(n) clear | save(a = {title}) the live transcript -> a saved one, clearlive, txget(id), txrm(id) |
//        interp(a = {...}) how speech becomes 3D (kept here, used by /api/dream for this cube)
// Kept in D1: dream_cube (how each cube is, its interpretation settings), dream_cmd (what it should do), dream_scene
// (its scenes, 300 at most), dream_line (the live transcript, 3000 lines), dream_tx (saved transcripts), dream_pair
// (phones asking to link), dream_use (scenes made today: all together, per address).
// The scene format (scene.cpp on the cube reads exactly this):
//   {title, said, place, time, weather, mood, fx, cam, things: [{m, n, a, at, c, c2, s} | {m: "custom", name, parts: [...]}]}

const QUICK = "@cf/meta/llama-3.1-8b-instruct-fp8-fast";
const SMART = "@cf/meta/llama-3.3-70b-instruct-fp8-fast";
const DAY_SCENES = 1500;          // all cubes together, per day (env.DREAM_DAY_SCENES)
const CODE_RE = /^[A-HJ-NP-Z2-9]{8}$/;
const ID_RE = /^[A-Za-z0-9-]{1,24}$/;

export const MODELS = ("person child robot ghost alien monster dog cat bird fish whale horse cow pig sheep rabbit bear panda elephant " +
  "snake butterfly spider dinosaur dragon snowman tree pine palm flower bush rock grass mountain cloud sun moon planet star fire volcano " +
  "rainbow mushroom cactus house tower castle tent lighthouse windmill factory bridge car bus truck bike train plane boat ship rocket " +
  "helicopter ufo ball book phone computer tv chair table bed cup bottle gift clock bulb guitar drum piano umbrella key money crown heart " +
  "letter pencil camera hat trophy flag bag drink cake pizza apple burger icecream bread banana carrot egg question exclaim note bolt " +
  "check cross smiley sadface speech up down globe lock balloon drop cube sphere pyramid cylinder ring").split(" ");
const PLACES = ["grass", "forest", "jungle", "desert", "beach", "sea", "snow", "mountains", "city", "room", "space", "moon", "underwater", "clouds", "lava", "stage", "void"];
const TIMES = ["day", "sunset", "night", "dawn"];
const WEATHERS = ["clear", "cloudy", "rain", "snow", "storm", "fog", "wind"];
const MOODS = ["neutral", "happy", "calm", "sad", "scary", "exciting", "love", "mystery", "angry", "funny"];
const FXS = ["none", "hearts", "sparkles", "fire", "bubbles", "music", "confetti", "leaves", "zzz", "questions", "money", "stars", "smoke"];
const CAMS = ["orbit", "close", "wide", "top", "low", "fly"];
const ACTS = ["idle", "walk", "run", "jump", "fly", "swim", "spin", "dance", "wave", "talk", "sleep", "shake", "float", "drive", "grow", "fall"];
const ATS = ["center", "left", "right", "back", "front", "around", "sky", "far"];
const PRIMS = ["box", "ball", "cyl", "cone", "pyr", "wedge", "torus", "disc", "dome", "star", "heart", "rock", "arc", "ring", "tube"];

// other words for the models (the AI sometimes says these; the word list uses them too). English, Dutch, German.
const SYN = {
  person: "man woman people human guy girl boy lady mister mom mum mother dad father friend teacher worker doctor nurse police farmer king queen " +
    "mens man vrouw jongen meisje mensen moeder vader vriend vriendin leraar juf meester dokter politie boer koning koningin " +
    "mann frau junge mädchen mutter vater freund lehrer arzt polizei bauer könig",
  child: "kid kids baby children son daughter toddler kind kinderen kindje zoon dochter peuter kinder sohn tochter",
  robot: "robot robots machine ai android cyborg computerprogram roboter",
  ghost: "ghost spirit spook geest spoken gespenst geist",
  alien: "alien martian aliens buitenaards marsmannetje außerirdischer",
  monster: "monster beast creature troll zombie demon beest wezen ungeheuer",
  dog: "dog dogs puppy hond honden puppy hondje hund welpe",
  cat: "cat cats kitten kitty lion tiger kat katten poes poesje kitten leeuw tijger katze löwe",
  bird: "bird birds chicken duck parrot owl eagle pigeon seagull vogel vogels kip eend papegaai uil adelaar duif meeuw huhn ente eule",
  fish: "fish fishes goldfish shark vis vissen goudvis haai fisch hai",
  whale: "whale dolphin walvis dolfijn wal delfin",
  horse: "horse pony donkey paard pony ezel pferd esel",
  cow: "cow cows bull calf koe koeien stier kalf kuh",
  pig: "pig pigs piglet varken varkens big schwein",
  sheep: "sheep lamb goat schaap schapen lam geit schaf ziege",
  rabbit: "rabbit bunny hare konijn haas hase kaninchen",
  bear: "bear teddy beer teddybeer bär",
  elephant: "elephant olifant elefant",
  snake: "snake worm slang worm schlange",
  butterfly: "butterfly bee insect vlinder bij insect schmetterling biene",
  spider: "spider ant bug spin mier kever spinne ameise",
  dinosaur: "dinosaur dino trex dinosaurus dinosaurier",
  dragon: "dragon draak drache",
  snowman: "snowman sneeuwpop schneemann",
  tree: "tree trees oak wood boom bomen eik baum bäume",
  pine: "pine christmas fir dennenboom kerstboom den tanne weihnachtsbaum",
  palm: "palm palmtree palmboom palme",
  flower: "flower flowers rose tulip plant bloem bloemen roos tulp plant blume rose",
  bush: "bush hedge garden struik heg tuin busch garten",
  rock: "rock stone stones steen stenen rots stein felsen",
  mountain: "mountain mountains hill hills alps berg bergen heuvel alpen",
  cloud: "cloud clouds wolk wolken wolke",
  sun: "sun sunny sunshine zon zonnig sonne",
  moon: "moon maan mond",
  planet: "planet mars saturn jupiter planeet",
  star: "star stars ster sterren stern sterne",
  fire: "fire campfire flames burning vuur kampvuur vlammen brand feuer",
  volcano: "volcano vulkaan vulkan",
  rainbow: "rainbow regenboog regenbogen",
  mushroom: "mushroom paddenstoel champignon pilz",
  cactus: "cactus cactussen kaktus",
  house: "house home building school shop store hospital church office hotel restaurant cafe huis thuis gebouw school winkel ziekenhuis kerk kantoor haus zuhause gebäude schule laden krankenhaus kirche büro",
  tower: "tower skyscraper flat apartment toren wolkenkrabber flat turm hochhaus",
  castle: "castle palace kingdom kasteel paleis burcht schloss burg palast",
  tent: "tent camping kamperen zelt",
  lighthouse: "lighthouse vuurtoren leuchtturm",
  windmill: "windmill mill molen windmolen windmühle",
  factory: "factory work job company fabriek werk bedrijf fabrik arbeit firma",
  bridge: "bridge brug brücke",
  car: "car cars taxi auto autos wagen",
  bus: "bus buses bussen",
  truck: "truck lorry vrachtwagen lastwagen lkw",
  bike: "bike bicycle cycling fiets fietsen rad fahrrad",
  train: "train trains railway trein treinen zug bahn",
  plane: "plane airplane flight airport vliegtuig vlucht vliegveld flugzeug flug",
  boat: "boat sailing sail boot zeilen zeilboot boot segeln",
  ship: "ship cruise ferry schip cruise veerboot schiff fähre",
  rocket: "rocket launch raket rakete",
  helicopter: "helicopter heli helikopter hubschrauber",
  ufo: "ufo spaceship flyingsaucer ruimteschip raumschiff",
  ball: "ball football soccer basketball game sport bal voetbal spel sport fußball spiel",
  book: "book books story read reading library homework boek boeken verhaal lezen huiswerk buch bücher geschichte lesen",
  phone: "phone mobile smartphone call app telefoon mobiel bellen handy telefon anrufen",
  computer: "computer laptop internet email website code coding programming pc computer laptop programmeren",
  tv: "tv television movie film series netflix televisie tv film serie fernseher fernsehen",
  chair: "chair seat stoel stuhl",
  table: "table desk dinner tafel bureau diner tisch schreibtisch",
  bed: "bed sleep sleeping tired bedtime bed slapen moe schlafen müde bett",
  cup: "cup coffee tea mug koffie thee kopje kaffee tee tasse",
  bottle: "bottle wine beer fles wijn bier flasche wein",
  gift: "gift present presents birthday surprise cadeau cadeautje verjaardag verrassing geschenk geburtstag",
  clock: "clock time late hour minute watch klok tijd laat uur minuut horloge uhr zeit spät stunde",
  bulb: "idea lamp light bulb think thinking plan idee lamp licht denken plan idee licht denken",
  guitar: "guitar gitaar gitarre",
  drum: "drum drums trommel drumstel schlagzeug",
  piano: "piano keyboard klavier",
  umbrella: "umbrella paraplu regenschirm",
  key: "key keys sleutel schlüssel",
  money: "money cash coins euro euros dollar pay paid salary price bank rich geld munten betalen salaris prijs rijk geld münzen bezahlen preis reich",
  crown: "crown royal kroon krone",
  heart: "heart love loving hart liefde lief verliefd herz liebe",
  letter: "letter mail message envelope post brief bericht post nachricht",
  pencil: "pencil pen write writing potlood pen schrijven bleistift stift schreiben",
  camera: "camera photo photos picture foto fotos kamera",
  hat: "hat cap hoed pet hut mütze",
  trophy: "trophy prize win winner champion beker prijs winnen winnaar kampioen pokal preis gewinnen",
  flag: "flag country vlag land fahne flagge",
  bag: "bag shopping suitcase tas boodschappen koffer tasche einkaufen",
  drink: "drink juice water lemonade glass drinken sap limonade glas getränk saft",
  cake: "cake party pie taart feest gebak kuchen torte fest",
  pizza: "pizza",
  apple: "apple fruit appel fruit apfel obst",
  burger: "burger hamburger fastfood food eat eating hungry dinner lunch eten honger avondeten lunch essen hunger",
  icecream: "icecream ice cream ijs ijsje eis",
  bread: "bread sandwich breakfast brood boterham ontbijt brot frühstück",
  banana: "banana banaan",
  carrot: "carrot vegetables wortel groente karotte gemüse",
  egg: "egg eggs ei eieren eier",
  question: "question why what how maybe wonder unsure vraag waarom wat hoe misschien frage warum",
  exclaim: "warning danger attention important alarm waarschuwing gevaar let belangrijk achtung gefahr wichtig",
  note: "music song sing singing melody muziek lied zingen liedje musik singen",
  bolt: "electricity power energy lightning stroom energie bliksem strom energie blitz",
  check: "yes ok okay done good right correct ja klaar goed juist richtig",
  cross: "no wrong mistake error nee fout verkeerd nein falsch fehler",
  smiley: "happy laugh smile fun joke funny blij lachen grap leuk grappig froh lachen witz lustig",
  sadface: "sad cry crying unhappy verdrietig huilen zielig traurig weinen",
  speech: "talk talking say said conversation chat meeting praten zeggen gesprek vergadering reden gespräch",
  up: "more rise growth higher increase meer stijgen groei hoger mehr steigen wachstum",
  down: "less fall lower decrease drop minder dalen lager weniger fallen",
  globe: "world earth travel holiday vacation country wereld aarde reizen vakantie welt erde reise urlaub",
  lock: "safe secure secret password security veilig geheim wachtwoord sicher geheimnis passwort",
  balloon: "balloon balloons ballon ballonnen",
  drop: "water drop tear raindrop druppel traan tropfen",
};
const SYN_MAP = (() => { const m = {}; for (const [k, v] of Object.entries(SYN)) for (const w of v.split(" ")) if (w && !m[w]) m[w] = k; for (const k of MODELS) m[k] = k; return m; })();
// places, times, weather, moods from words (the word list)
const WORDS = {
  place: { beach: "beach strand zee holiday vakantie strand", sea: "sea ocean sailing zee oceaan meer see ozean", forest: "forest woods bos woud wald",
    jungle: "jungle rainforest oerwoud dschungel", desert: "desert sand woestijn wüste", snow: "snow winter ski cold sneeuw koud schnee kalt",
    mountains: "mountain mountains hiking berg bergen wandern", city: "city town street traffic stad straat verkeer stadt straße",
    room: "room home kitchen house inside bedroom kamer thuis keuken binnen zimmer küche", space: "space universe galaxy ruimte heelal weltraum",
    moon: "moon maan mond", underwater: "underwater diving duiken tauchen", clouds: "sky heaven flying lucht hemel himmel", stage: "concert party stage festival podium feest bühne",
    lava: "hell volcano lava hel vulkaan hölle" },
  time: { night: "night dark evening sleep nacht donker avond slapen abend", sunset: "sunset evening zonsondergang avond sonnenuntergang", dawn: "morning sunrise ochtend zonsopgang sonnenaufgang" },
  weather: { rain: "rain raining rainy wet regen regent regenen regende nat regnet", snow: "snow snowing sneeuw sneeuwt schnee", storm: "storm thunder lightning onweer bliksem gewitter", fog: "fog mist foggy nebel", wind: "wind windy waait", cloudy: "cloudy grey bewolkt grijs" },
  mood: { happy: "happy great nice fun yay blij leuk fijn gezellig toll schön", sad: "sad sorry miss verdrietig jammer mis traurig", scary: "scary afraid fear spooky eng bang angst gruselig",
    love: "love darling sweet liefde lief schat liebe", angry: "angry mad furious boos kwaad wütend", exciting: "wow amazing awesome exciting gaaf vet spannend geil super", funny: "funny haha lol joke grappig grap lustig witz", calm: "calm quiet relax peaceful rustig stil ontspannen ruhig" },
};
const WORD_MAP = (() => { const m = {}; for (const [kind, o] of Object.entries(WORDS)) for (const [k, v] of Object.entries(o)) for (const w of v.split(" ")) if (w) (m[w] ||= {})[kind] = k; return m; })();

function json(data, status = 200) {
  return new Response(JSON.stringify(data), { status, headers: { "content-type": "application/json; charset=utf-8", "cache-control": "no-store", "access-control-allow-origin": "*" } });
}
const pickOne = (v, list, def) => { v = String(v || "").toLowerCase().trim(); return list.includes(v) ? v : def; };
const COLOURS = { red: "#d32f2f", rood: "#d32f2f", rot: "#d32f2f", blue: "#1e6fd9", blauw: "#1e6fd9", blau: "#1e6fd9", green: "#43a047", groen: "#43a047", grün: "#43a047",
  yellow: "#f6cf3a", geel: "#f6cf3a", gelb: "#f6cf3a", black: "#1a1a1a", zwart: "#1a1a1a", schwarz: "#1a1a1a", white: "#f2f2f2", wit: "#f2f2f2", weiß: "#f2f2f2",
  pink: "#f48fb1", roze: "#f48fb1", rosa: "#f48fb1", purple: "#8e44ad", paars: "#8e44ad", lila: "#8e44ad", orange: "#f08a24", oranje: "#f08a24",
  brown: "#8d5a2b", bruin: "#8d5a2b", braun: "#8d5a2b", gray: "#8a8a8a", grey: "#8a8a8a", grijs: "#8a8a8a", grau: "#8a8a8a", gold: "#e0b33c", goud: "#e0b33c", silver: "#c0c8d0", zilver: "#c0c8d0" };
const hex = (v) => { const s = String(v || "").trim(); if (COLOURS[s.toLowerCase()]) return COLOURS[s.toLowerCase()]; return /^#?[0-9a-fA-F]{6}$/.test(s) ? "#" + s.replace("#", "").toLowerCase() : /^#?[0-9a-fA-F]{3}$/.test(s) ? "#" + s.replace("#", "").toLowerCase() : undefined; };
const num = (v, lo, hi, def) => { const n = Number(v); return Number.isFinite(n) ? Math.min(hi, Math.max(lo, n)) : def; };
function modelOf(name) {
  let n = String(name || "").toLowerCase().trim().replace(/[^a-zà-ÿß ]/g, "");
  if (!n) return null;
  if (SYN_MAP[n]) return SYN_MAP[n];
  for (const w of n.split(" ").reverse()) {            // "red sports car" -> car
    if (SYN_MAP[w]) return SYN_MAP[w];
    if (w.endsWith("s") && SYN_MAP[w.slice(0, -1)]) return SYN_MAP[w.slice(0, -1)];
    if (w.endsWith("en") && SYN_MAP[w.slice(0, -2)]) return SYN_MAP[w.slice(0, -2)];
  }
  return null;
}

// ------------------------------------------------------------------ checking a scene
export function cleanScene(raw, text) {
  const r = raw && typeof raw === "object" ? (raw.scene && typeof raw.scene === "object" ? raw.scene : raw) : {};
  const s = {
    title: String(r.title || "").replace(/[\u0000-\u001f"\\]/g, " ").replace(/\s+/g, " ").trim().slice(0, 36),
    said: String(text || "").replace(/[\u0000-\u001f]/g, " ").replace(/\s+/g, " ").trim().slice(0, 150),
    place: pickOne(r.place, PLACES, "grass"), time: pickOne(r.time, TIMES, "day"), weather: pickOne(r.weather, WEATHERS, "clear"),
    mood: pickOne(r.mood, MOODS, "neutral"), fx: pickOne(r.fx, FXS, "none"), cam: pickOne(r.cam, CAMS, "orbit"), things: [],
  };
  if (!PLACES.includes(String(r.place || "").toLowerCase())) { const w = WORD_MAP[String(r.place || "").toLowerCase()]; if (w && w.place) s.place = w.place; }
  let parts = 0;
  for (const t of Array.isArray(r.things) ? r.things.slice(0, 10) : []) {
    if (!t || typeof t !== "object" || s.things.length >= 6) continue;
    const o = {};
    const m = modelOf(t.m || t.model || t.name || t.type);
    if (m) o.m = m;
    else if (Array.isArray(t.parts) && t.parts.length && parts < 24) {
      o.m = "custom"; o.name = String(t.name || t.m || "").slice(0, 24); o.parts = [];
      for (const p of t.parts.slice(0, 10)) {
        if (!p || typeof p !== "object" || parts >= 24) continue;
        const prim = String(p.p || p.shape || "box").toLowerCase();
        o.parts.push({ p: PRIMS.includes(prim) ? prim : prim === "sphere" ? "ball" : prim.startsWith("cyl") ? "cyl" : prim.startsWith("pyr") ? "pyr" : "box",
          x: num(p.x, -6, 6, 0), y: num(p.y, -1, 10, 0.5), z: num(p.z, -6, 6, 0), w: num(p.w, 0.05, 8, 1), h: num(p.h, 0.05, 8, 1), d: num(p.d, 0.05, 8, 1),
          c: hex(p.c) || "#b0b0b0", r: num(p.r, -360, 360, 0) });
        parts++;
      }
      if (!o.parts.length) continue;
    } else continue;
    o.n = Math.round(num(t.n ?? t.count, 1, 8, 1));
    o.a = pickOne(t.a ?? t.action, ACTS, "idle");
    const at = String(t.at || t.where || "").toLowerCase();
    if (ATS.includes(at)) o.at = at;
    const c = hex(t.c ?? t.color), c2 = hex(t.c2);
    if (c) o.c = c;
    if (c2) o.c2 = c2;
    const sz = num(t.s ?? t.size, 0.3, 3, 1);
    if (sz !== 1) o.s = Math.round(sz * 100) / 100;
    s.things.push(o);
  }
  if (!s.things.length) s.things.push({ m: "question", n: 1, a: "spin", at: "center" });
  if (!s.title) s.title = s.things.map((t) => t.name || t.m).slice(0, 2).join(" & ").toUpperCase();
  return s;
}

// ------------------------------------------------------------------ the word list (no AI)
export function wordScene(text) {
  const words = String(text || "").toLowerCase().normalize("NFC").split(/[^a-zà-ÿß]+/).filter(Boolean);
  const count = {}, order = [], set = {};
  for (const w of words) {
    const m = SYN_MAP[w] || (w.endsWith("s") && SYN_MAP[w.slice(0, -1)]) || (w.endsWith("en") && SYN_MAP[w.slice(0, -2)]);
    if (m && !["bed", "room"].includes(w)) { if (!count[m]) order.push(m); count[m] = (count[m] || 0) + 1; }
    const k = WORD_MAP[w];
    if (k) for (const [kind, v] of Object.entries(k)) set[kind] = set[kind] || v;
  }
  const things = order.slice(0, 4).map((m, i) => ({ m, n: 1, a: ["person", "child", "dog", "cat", "horse"].includes(m) && i === 0 ? "walk" : "idle" }));
  const fx = set.mood === "love" ? "hearts" : order.includes("note") ? "music" : order.includes("money") ? "money" : order.includes("question") ? "questions" : "none";
  const raw = { title: order.slice(0, 2).join(" & ").toUpperCase() || "...", place: set.place || (things.length ? "grass" : "void"),
    time: set.time || "day", weather: set.weather || "clear", mood: set.mood || "neutral", fx, cam: "orbit", things };
  if (!things.length) raw.things = [{ m: "speech", a: "float" }, { m: "question", a: "spin", at: "right" }];
  return cleanScene(raw, text);
}

// ------------------------------------------------------------------ the AI
const SYSTEM = `You turn overheard speech into a tiny animated 3D scene for a toy screen: a little low-poly world that shows what the talk is about.
Answer with ONE JSON object and nothing else:
{"title": 2-4 words in the language of the speech,
 "place": ${PLACES.join("|")},
 "time": ${TIMES.join("|")}, "weather": ${WEATHERS.join("|")}, "mood": ${MOODS.join("|")},
 "fx": ${FXS.join("|")}, "cam": ${CAMS.join("|")},
 "things": [1 to 5 of {"m": model, "n": copies 1-8, "a": action, "at": spot, "c": "#rrggbb" (optional), "s": size 0.5-2 (optional)}]}
models: ${MODELS.join(" ")}
actions: ${ACTS.join(" ")}
spots: ${ATS.join(" ")}
How:
- Show the concrete things that are talked about, the most important first (it goes in the center). Several of something: n.
- Abstract ideas become symbols: love=heart, money/price=money, doubt/questions=question, idea/plan=bulb, time/late=clock, more/growth=up, less=down, yes=check, no/wrong=cross, talking/meeting=speech, world/travel=globe, safety/secret=lock, happy=smiley, sad=sadface, music=note, energy=bolt, winning=trophy.
- People: person (adult), child; give them actions that fit (talk, walk, dance, wave, sleep, run).
- Pick a place, time, weather and mood that fit the words; "void" is a dark neon grid for abstract talk, "room" is indoors.
- fx adds particles: hearts for love, music for songs, confetti for parties, money for money, zzz for sleep, questions for confusion.
- Colours: give real colours when said or obvious (a red car, a black cat).
- Only if nothing in the list fits an important concrete object: {"m":"custom","name":"...","parts":[2-8 of {"p":"box|ball|cyl|cone|pyr|dome|torus","x":0,"y":0.5,"z":0,"w":1,"h":1,"d":1,"c":"#rrggbb"}]} (metres, y = height of the part's centre).
- Unclear or empty speech: place "void", things [{"m":"question","a":"spin"}].`;
const EXAMPLES = [
  ["We took the dog to the beach yesterday and he kept chasing the seagulls.", { title: "Dog at the beach", place: "beach", time: "day", weather: "clear", mood: "happy", fx: "none", cam: "orbit", things: [{ m: "dog", n: 1, a: "run", at: "center", c: "#b07840" }, { m: "bird", n: 3, a: "fly", at: "around", c: "#ffffff" }, { m: "person", n: 2, a: "walk", at: "left" }] }],
  ["Ik moet morgen vroeg op want de trein naar Amsterdam gaat om zeven uur.", { title: "Vroege trein", place: "city", time: "dawn", weather: "clear", mood: "calm", fx: "none", cam: "wide", things: [{ m: "train", n: 1, a: "drive", at: "center" }, { m: "clock", n: 1, a: "idle", at: "right" }, { m: "person", n: 1, a: "walk", at: "left" }] }],
];
function firstJson(s) {
  s = String(s || "");
  const a = s.indexOf("{"), b = s.lastIndexOf("}");
  if (a < 0 || b <= a) return null;
  const t = s.slice(a, b + 1);
  try { return JSON.parse(t); } catch (e) {}
  try { return JSON.parse(t.replace(/,\s*([}\]])/g, "$1").replace(/[\u201c\u201d]/g, '"')); } catch (e) {}
  return null;
}
// how speech becomes 3D, per cube (the page's 3D tab)
const INTERP = { style: "playful", things: "normal", place: "auto", time: "auto", weather: "auto", mood: "auto", fx: "auto", cam: "auto",
  titleLang: "speech", story: 0, wish: "" };
const STYLES = ["playful", "literal", "dreamy", "symbolic"];
const TITLE_LANGS = { speech: "", en: "English", nl: "Dutch", de: "German", fr: "French", es: "Spanish", uk: "Ukrainian", ru: "Russian" };
export function cleanInterp(o) {
  const r = { ...INTERP };
  if (!o || typeof o !== "object") return r;
  r.style = pickOne(o.style, STYLES, "playful");
  r.things = pickOne(o.things, ["few", "normal", "many"], "normal");
  r.place = pickOne(o.place, ["auto", ...PLACES], "auto");
  r.time = pickOne(o.time, ["auto", "clock", ...TIMES], "auto");
  r.weather = pickOne(o.weather, ["auto", ...WEATHERS], "auto");
  r.mood = pickOne(o.mood, ["auto", ...MOODS], "auto");
  r.fx = pickOne(o.fx, ["auto", "more", ...FXS], "auto");
  r.cam = pickOne(o.cam, ["auto", ...CAMS], "auto");
  r.titleLang = pickOne(o.titleLang, Object.keys(TITLE_LANGS), "speech");
  r.story = o.story ? 1 : 0;
  r.wish = String(o.wish || "").replace(/[\u0000-\u001f]/g, " ").slice(0, 200).trim();
  return r;
}
function interpPrompt(ip) {
  const t = [];
  if (ip.style === "literal") t.push("Style: literal. Show exactly the things that are said, natural colours, no extra symbols, fx none unless it fits.");
  if (ip.style === "dreamy") t.push("Style: dreamy and surreal. Exaggerate, mix things in unexpected ways, prefer magical places (clouds, space, void, underwater, night), glowing colours and effects.");
  if (ip.style === "symbolic") t.push("Style: symbolic. Show the feelings and ideas with symbols (heart, bulb, question, money, up, down, clock, speech, globe, check, cross, smiley, sadface...) more than objects.");
  if (ip.things === "few") t.push("Use only 1 or 2 things: the most important.");
  if (ip.things === "many") t.push("Use 4 to 6 things; groups (n up to 8) are welcome.");
  if (ip.titleLang !== "speech") t.push(`Write the title in ${TITLE_LANGS[ip.titleLang]}.`);
  if (ip.wish) t.push(`The owner's wish (follow it when it doesn't break the JSON rules): ${ip.wish}`);
  return t.join("\n");
}
function timeFromHour(h) { return h >= 6 && h < 8 ? "dawn" : h >= 8 && h < 18 ? "day" : h >= 18 && h < 21 ? "sunset" : "night"; }
const MOOD_FX = { love: "hearts", happy: "sparkles", exciting: "confetti", funny: "confetti", calm: "leaves", sad: "leaves", scary: "smoke", mystery: "stars", angry: "fire" };
function applyInterp(sc, ip, hour) {
  if (!ip) return sc;
  if (ip.place !== "auto") sc.place = ip.place;
  if (ip.time === "clock" && hour >= 0 && hour < 24) sc.time = timeFromHour(hour);
  else if (ip.time !== "auto" && ip.time !== "clock") sc.time = ip.time;
  if (ip.weather !== "auto") sc.weather = ip.weather;
  if (ip.mood !== "auto") sc.mood = ip.mood;
  if (ip.fx === "more") { if (sc.fx === "none") sc.fx = MOOD_FX[sc.mood] || "sparkles"; }
  else if (ip.fx !== "auto") sc.fx = ip.fx;
  if (ip.cam !== "auto") sc.cam = ip.cam;
  const max = ip.things === "few" ? 2 : ip.things === "many" ? 6 : 5;
  sc.things = sc.things.slice(0, max);
  return sc;
}
async function aiScene(env, text, lang, prev, smart, ip) {
  const model = smart ? (env.DREAM_SMART || SMART) : (env.DREAM_QUICK || QUICK);
  const extra = ip ? interpPrompt(ip) : "";
  const messages = [{ role: "system", content: extra ? SYSTEM + "\nFor this screen:\n" + extra : SYSTEM }];
  for (const [u, a] of EXAMPLES) { messages.push({ role: "user", content: u }); messages.push({ role: "assistant", content: JSON.stringify(a) }); }
  let ask = text;
  if (prev) ask = `(the scene before: ${String(prev).slice(0, 60)})\n${text}`;
  if (lang) ask += `\n(language: ${lang})`;
  if (ip && ip.wish) ask += `\n(the owner's wish, follow it: ${ip.wish})`;
  messages.push({ role: "user", content: ask });
  const r = await env.AI.run(model, { messages, max_tokens: 700, temperature: 0.5 });
  const out = typeof r?.response === "string" ? r.response : r?.response ? JSON.stringify(r.response) : "";
  const parsed = typeof r?.response === "object" && r.response ? r.response : firstJson(out);
  if (!parsed || !Array.isArray(parsed.things)) throw new Error("no scene in the answer");
  return { scene: cleanScene(parsed, text), model, usage: r?.usage };
}

// ------------------------------------------------------------------ D1
let ready = null;
function tables(env) {
  if (!ready) {
    ready = (async () => {
      await env.DB.batch([
        env.DB.prepare(`CREATE TABLE IF NOT EXISTS dream_cube (code TEXT PRIMARY KEY, at INTEGER NOT NULL, st TEXT NOT NULL DEFAULT '{}', cmdn INTEGER NOT NULL DEFAULT 0, scenen INTEGER NOT NULL DEFAULT 0)`),
        env.DB.prepare(`CREATE TABLE IF NOT EXISTS dream_cmd (code TEXT NOT NULL, n INTEGER NOT NULL, at INTEGER NOT NULL, c TEXT NOT NULL, a TEXT NOT NULL DEFAULT '', PRIMARY KEY (code, n))`),
        env.DB.prepare(`CREATE TABLE IF NOT EXISTS dream_scene (code TEXT NOT NULL, n INTEGER NOT NULL, at INTEGER NOT NULL, title TEXT NOT NULL, said TEXT NOT NULL, json TEXT NOT NULL, PRIMARY KEY (code, n))`),
        env.DB.prepare(`CREATE TABLE IF NOT EXISTS dream_use (day TEXT NOT NULL, k TEXT NOT NULL, n INTEGER NOT NULL DEFAULT 0, PRIMARY KEY (day, k))`),
        env.DB.prepare(`CREATE TABLE IF NOT EXISTS dream_line (code TEXT NOT NULL, n INTEGER NOT NULL, at INTEGER NOT NULL, text TEXT NOT NULL, PRIMARY KEY (code, n))`),
        env.DB.prepare(`CREATE TABLE IF NOT EXISTS dream_tx (code TEXT NOT NULL, id INTEGER NOT NULL, at INTEGER NOT NULL, title TEXT NOT NULL, lines INTEGER NOT NULL, json TEXT NOT NULL, PRIMARY KEY (code, id))`),
        env.DB.prepare(`CREATE TABLE IF NOT EXISTS dream_pair (id TEXT NOT NULL, code TEXT NOT NULL, at INTEGER NOT NULL, num TEXT NOT NULL, ok INTEGER NOT NULL DEFAULT 0, PRIMARY KEY (id, code))`),
      ]);
      // 1.2: new columns (an error = already there)
      for (const col of ["ipk TEXT NOT NULL DEFAULT ''", "interp TEXT NOT NULL DEFAULT '{}'", "linen INTEGER NOT NULL DEFAULT 0", "mark INTEGER NOT NULL DEFAULT 0",
                         "heard TEXT NOT NULL DEFAULT ''", "hn INTEGER NOT NULL DEFAULT -1"])
        try { await env.DB.prepare(`ALTER TABLE dream_cube ADD COLUMN ${col}`).run(); } catch (e) {}
    })().catch((e) => { ready = null; throw e; });
  }
  return ready;
}
// where a request comes from, hashed: IPv4 as it is, IPv6 by its /64 (one home)
function ipPrefix(ip) {
  ip = String(ip || "");
  if (!ip.includes(":")) return ip;
  let [h, t] = ip.split("::");
  const a = h ? h.split(":") : [], b = t !== undefined ? (t ? t.split(":") : []) : [];
  const full = t !== undefined ? [...a, ...Array(8 - a.length - b.length).fill("0"), ...b] : a;
  return full.slice(0, 4).map((x) => parseInt(x || "0", 16).toString(16)).join(":") + "::/64";
}
async function ipKey(ip) { return (await sha("dream-ip:" + ipPrefix(ip))).slice(0, 16); }
async function cube(env, code) {
  await tables(env);
  let row = await env.DB.prepare(`SELECT * FROM dream_cube WHERE code = ?`).bind(code).first();
  if (!row) {
    await env.DB.prepare(`INSERT OR IGNORE INTO dream_cube (code, at) VALUES (?, 0)`).bind(code).run();
    row = { code, at: 0, st: "{}", cmdn: 0, scenen: 0, ipk: "", interp: "{}", linen: 0, mark: 0, heard: "", hn: -1 };
  }
  return row;
}
async function addCmd(env, code, c, a) {
  const row = await cube(env, code);
  const n = row.cmdn + 1, now = Date.now();
  await env.DB.batch([
    env.DB.prepare(`UPDATE dream_cube SET cmdn = ? WHERE code = ?`).bind(n, code),
    env.DB.prepare(`INSERT OR REPLACE INTO dream_cmd (code, n, at, c, a) VALUES (?, ?, ?, ?, ?)`).bind(code, n, now, c, typeof a === "string" ? a : JSON.stringify(a ?? "")),
    env.DB.prepare(`DELETE FROM dream_cmd WHERE code = ? AND at < ?`).bind(code, now - 86400000),
  ]);
  return n;
}
async function addScene(env, code, scene) {
  const row = await cube(env, code);
  const n = row.scenen + 1, now = Date.now();
  await env.DB.batch([
    env.DB.prepare(`UPDATE dream_cube SET scenen = ? WHERE code = ?`).bind(n, code),
    env.DB.prepare(`INSERT OR REPLACE INTO dream_scene (code, n, at, title, said, json) VALUES (?, ?, ?, ?, ?, ?)`).bind(code, n, now, scene.title, scene.said, JSON.stringify(scene)),
    env.DB.prepare(`DELETE FROM dream_scene WHERE code = ? AND n <= ?`).bind(code, n - 300),
  ]);
  return n;
}
async function sha(text) {
  const b = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(text));
  return [...new Uint8Array(b)].slice(0, 10).map((x) => x.toString(16).padStart(2, "0")).join("");
}

// words -> scene, with today's limits (the word list when the AI can't)
async function makeScene(request, env, ctx, text, lang, prev, smart, ip = null, hour = -1) {
  text = String(text || "").replace(/\s+/g, " ").trim().slice(0, 600);
  if (!text) return { scene: applyInterp(cleanScene({ title: "...", place: "void", time: "night", mood: "mystery", things: [{ m: "question", a: "spin" }] }, ""), ip, hour), via: "empty" };
  const day = new Date().toISOString().slice(0, 10);
  const ipKey = "ip:" + (await sha("dream:" + (request.headers.get("CF-Connecting-IP") || "local")));
  const cap = Math.max(10, Number(env.DREAM_DAY_SCENES) || DAY_SCENES);
  let all = 0, mine = 0;
  if (env.DB) {
    try {
      await tables(env);
      const rows = await env.DB.prepare(`SELECT k, n FROM dream_use WHERE day = ? AND k IN ('all', ?)`).bind(day, ipKey).all();
      for (const r of rows.results || []) { if (r.k === "all") all = r.n; else mine = r.n; }
    } catch (e) { /* no counting: still answer */ }
  }
  let res = null, why = "";
  if (env.AI && all < cap && mine < cap * 0.6) {
    try { res = await aiScene(env, text, lang, prev, smart, ip); } catch (e) { why = String(e?.message || e).slice(0, 80); }
    if (!res && smart) { try { res = await aiScene(env, text, lang, prev, false, ip); } catch (e) {} }
    if (res && env.DB) {
      const add = env.DB.prepare(`INSERT INTO dream_use (day, k, n) VALUES (?, ?, 1) ON CONFLICT(day, k) DO UPDATE SET n = n + 1`);
      const p = env.DB.batch([add.bind(day, "all"), add.bind(day, ipKey)]).catch(() => {});
      if (ctx?.waitUntil) ctx.waitUntil(p); else await p;
    }
  } else why = env.AI ? "daily limit" : "no ai";
  if (res) return { scene: applyInterp(res.scene, ip, hour), via: res.model.split("/").pop() };
  return { scene: applyInterp(wordScene(text), ip, hour), via: "words", why };
}

async function interpOf(env, code) {
  if (!code || !env.DB) return null;
  try { const row = await cube(env, code); return cleanInterp(JSON.parse(row.interp || "{}")); } catch (e) { return null; }
}
const clean = (t, n) => String(t || "").replace(/[\u0000-\u001f]/g, " ").replace(/\s+/g, " ").trim().slice(0, n).trim();
const rnd = (n, chars) => { let s = ""; const b = crypto.getRandomValues(new Uint8Array(n)); for (const x of b) s += chars[x % chars.length]; return s; };

// ------------------------------------------------------------------ the routes
export async function handleDream(request, env, ctx, path) {
  const url = new URL(request.url);
  if (request.method === "OPTIONS") return new Response(null, { headers: { "access-control-allow-origin": "*", "access-control-allow-methods": "GET, POST", "access-control-allow-headers": "content-type" } });
  let body = {};
  if (request.method === "POST") { try { body = await request.json(); } catch (e) { body = {}; } }

  if (path === "/api/dream") {
    if (request.method !== "POST") return json({ error: "POST {text}" }, 405);
    const lang = /^[a-z]{2}$/.test(String(body.lang || "")) ? body.lang : "";
    const id = ID_RE.test(String(body.id || "")) ? body.id : "";
    const code = CODE_RE.test(String(body.code || "")) ? body.code : "";
    const hour = Number.isInteger(body.hour) ? body.hour : -1;
    const t0 = Date.now();
    const ip = await interpOf(env, code);
    let prev = String(body.prev || "").slice(0, 80);
    if (ip && ip.story && code) {          // story: the scene before, and what was said then
      try { const x = await env.DB.prepare(`SELECT title, said FROM dream_scene WHERE code = ? ORDER BY n DESC LIMIT 1`).bind(code).first(); if (x) prev = `${x.title}: ${x.said}`.slice(0, 200); } catch (e) {}
    }
    const r = await makeScene(request, env, ctx, body.text, lang, prev, body.mind === "smart", ip, hour);
    let n = 0;
    if (code && env.DB && r.via !== "empty") { try { n = await addScene(env, code, r.scene); } catch (e) {} }
    return json({ ok: true, scene: r.scene, n, via: r.via, ms: Date.now() - t0, id });
  }
  if (!env.DB) return json({ error: "no database" }, 503);
  await tables(env);
  const now = Date.now();

  if (path === "/api/dream/cube") {
    const code = String(body.code || "");
    if (!CODE_RE.test(code)) return json({ error: "bad code" }, 400);
    const row = await cube(env, code);
    const so = body.st && typeof body.st === "object" ? body.st : null;
    const st = so ? JSON.stringify(so).slice(0, 2400) : row.st;
    const ipk = await ipKey(request.headers.get("CF-Connecting-IP") || "");
    // what it heard: the live transcript. A cube that sends no line for what it just heard
    // (firmware before 1.2, or a line that got lost) still streams: its "heard" becomes the line
    let lines = Array.isArray(body.lines) ? body.lines.slice(0, 20) : [];
    const heard = so ? clean(so.heard, 500) : row.heard || "";
    const hn = so && Number.isInteger(so.hn) ? so.hn : -1;
    const fresh = so && heard && (hn >= 0 ? hn !== row.hn : heard !== (row.heard || ""));
    if (fresh && !lines.length) lines = [{ t: heard, ago: 0 }];
    // written only when it changed, or once a minute (the page sees "online" from it)
    if (st !== row.st || ipk !== row.ipk || heard !== row.heard || hn !== row.hn || now - row.at > 60000)
      try { await env.DB.prepare(`UPDATE dream_cube SET at = ?, st = ?, ipk = ?, heard = ?, hn = ? WHERE code = ?`).bind(now, st, ipk, heard, hn, code).run(); }
      catch (e) { await env.DB.prepare(`UPDATE dream_cube SET at = ?, st = ?, ipk = ? WHERE code = ?`).bind(now, st, ipk, code).run(); }
    if (lines.length) try {
      let n = row.linen;
      const ins = env.DB.prepare(`INSERT OR REPLACE INTO dream_line (code, n, at, text) VALUES (?, ?, ?, ?)`);
      const batch = [];
      for (const l of lines) {
        const t = clean(l && l.t, 500);
        if (!t) continue;
        n++;
        batch.push(ins.bind(code, n, now - Math.min(Math.max(Number(l.ago) || 0, 0), 3600000), t));
      }
      if (batch.length) {
        batch.push(env.DB.prepare(`UPDATE dream_cube SET linen = ? WHERE code = ?`).bind(n, code));
        if (Math.floor(n / 50) !== Math.floor(row.linen / 50)) batch.push(env.DB.prepare(`DELETE FROM dream_line WHERE code = ? AND n <= ?`).bind(code, n - 3000));
        await env.DB.batch(batch);
      }
    } catch (e) { console.log("dream lines", code, String(e?.message || e)); }   // the commands still go back
    // its answers to "link a phone?"
    for (const p of Array.isArray(body.pair) ? body.pair.slice(0, 4) : [])
      if (p && typeof p.id === "string") await env.DB.prepare(`UPDATE dream_pair SET ok = ? WHERE id = ? AND code = ?`).bind(p.ok ? 1 : -1, p.id.slice(0, 16), code).run();
    const last = Math.max(0, Number(body.last) || 0);
    let cmds = [];
    if (row.cmdn > last) {
      const r = await env.DB.prepare(`SELECT n, c, a FROM dream_cmd WHERE code = ? AND n > ? ORDER BY n LIMIT 8`).bind(code, last).all();
      cmds = (r.results || []).map((x) => ({ n: x.n, c: x.c, a: x.a }));
    }
    return json({ ok: true, cmds, top: row.cmdn });
  }

  // a phone without a code: the cubes on its network (or the IPv4 it says it has) are asked on their screen
  if (path === "/api/dream/near") {
    const keys = [await ipKey(request.headers.get("CF-Connecting-IP") || "")];
    const hint = String(url.searchParams.get("hint") || "");
    if (/^\d{1,3}(\.\d{1,3}){3}$/.test(hint)) keys.push(await ipKey(hint));
    const r = await env.DB.prepare(`SELECT code FROM dream_cube WHERE ipk IN (?, ?) AND at > ? ORDER BY at DESC LIMIT 3`).bind(keys[0], keys[1] || keys[0], now - 10 * 60000).all();
    const codes = (r.results || []).map((x) => x.code);
    if (!codes.length) return json({ ok: true, found: 0 });
    const id = rnd(12, "abcdefghijkmnpqrstuvwxyz23456789"), num = String(1000 + (crypto.getRandomValues(new Uint16Array(1))[0] % 9000));
    await env.DB.prepare(`DELETE FROM dream_pair WHERE at < ?`).bind(now - 15 * 60000).run();
    for (const code of codes) {
      await env.DB.prepare(`INSERT INTO dream_pair (id, code, at, num) VALUES (?, ?, ?, ?)`).bind(id, code, now, num).run();
      await addCmd(env, code, "pair", id + "|" + num);
    }
    return json({ ok: true, found: codes.length, pair: id, num });
  }
  if (path === "/api/dream/pair") {
    const id = String(url.searchParams.get("id") || "").slice(0, 16);
    const r = await env.DB.prepare(`SELECT code, ok, at FROM dream_pair WHERE id = ?`).bind(id).all();
    const rows = r.results || [];
    const yes = rows.find((x) => x.ok === 1);
    if (yes) return json({ ok: true, code: yes.code });
    if (!rows.length || rows.every((x) => x.ok === -1)) return json({ ok: true, no: true });
    if (rows.every((x) => now - x.at > 5 * 60000)) return json({ ok: true, no: true, late: true });
    return json({ ok: true, wait: true });
  }

  if (path === "/api/dream/app") {
    const code = String(url.searchParams.get("code") || body.code || "").toUpperCase();
    if (!CODE_RE.test(code)) return json({ error: "bad code" }, 400);
    const row = await cube(env, code);
    if (request.method === "GET") {
      if (url.searchParams.get("tx")) {          // one saved transcript, whole
        const x = await env.DB.prepare(`SELECT id, at, title, lines, json FROM dream_tx WHERE code = ? AND id = ?`).bind(code, Number(url.searchParams.get("tx")) || 0).first();
        if (!x) return json({ error: "no such transcript" }, 404);
        return json({ ok: true, tx: { id: x.id, at: x.at, title: x.title, lines: JSON.parse(x.json) } });
      }
      const since = Math.max(0, Number(url.searchParams.get("since")) || 0);
      const lsince = Math.max(row.mark, Number(url.searchParams.get("lsince")) || 0);
      const [sc, ln, tx] = await Promise.all([
        env.DB.prepare(`SELECT n, at, title, said, json FROM dream_scene WHERE code = ? AND n > ? ORDER BY n DESC LIMIT 60`).bind(code, since).all(),
        env.DB.prepare(`SELECT n, at, text FROM dream_line WHERE code = ? AND n > ? ORDER BY n DESC LIMIT 400`).bind(code, lsince).all(),
        env.DB.prepare(`SELECT id, at, title, lines FROM dream_tx WHERE code = ? ORDER BY id DESC LIMIT 200`).bind(code).all(),
      ]);
      let st = {}; try { st = JSON.parse(row.st || "{}"); } catch (e) {}
      let ip = INTERP; try { ip = cleanInterp(JSON.parse(row.interp || "{}")); } catch (e) {}
      return json({ ok: true, cube: { seen: row.at, ago: row.at ? now - row.at : -1, online: row.at > 0 && now - row.at < 90000, st },
        top: row.scenen, scenes: (sc.results || []).map((x) => ({ n: x.n, at: x.at, title: x.title, said: x.said, scene: JSON.parse(x.json) })),
        linen: row.linen, mark: row.mark, lines: (ln.results || []).reverse(), tx: tx.results || [], interp: ip });
    }
    const c = String(body.c || "");
    const OK = ["listen", "pause", "wake", "sleep", "prev", "next", "cfg", "unlink"];
    if (OK.includes(c)) {
      const a = c === "cfg" ? JSON.stringify(body.a && typeof body.a === "object" ? body.a : {}).slice(0, 800) : String(body.a ?? "").slice(0, 40);
      return json({ ok: true, n: await addCmd(env, code, c, a) });
    }
    if (c === "interp") {
      const ip = cleanInterp(body.a);
      await env.DB.prepare(`UPDATE dream_cube SET interp = ? WHERE code = ?`).bind(JSON.stringify(ip), code).run();
      return json({ ok: true, interp: ip });
    }
    if (c === "say") {
      let st = {}; try { st = JSON.parse(row.st || "{}"); } catch (e) {}
      let ip = INTERP; try { ip = cleanInterp(JSON.parse(row.interp || "{}")); } catch (e) {}
      const r = await makeScene(request, env, ctx, body.a, st.lang || "", "", !!(st.cfg && st.cfg.mind === 1), ip, Number.isInteger(body.hour) ? body.hour : -1);
      const n = await addScene(env, code, r.scene);
      await addCmd(env, code, "scene", JSON.stringify({ n, ...r.scene }));
      return json({ ok: true, n, scene: r.scene, via: r.via });
    }
    if (c === "show") {
      const n = Number(body.a) || 0;
      const x = await env.DB.prepare(`SELECT json FROM dream_scene WHERE code = ? AND n = ?`).bind(code, n).first();
      if (!x) return json({ error: "no such scene" }, 404);
      await addCmd(env, code, "scene", JSON.stringify({ n, ...JSON.parse(x.json) }));
      return json({ ok: true });
    }
    if (c === "rm") { await env.DB.prepare(`DELETE FROM dream_scene WHERE code = ? AND n = ?`).bind(code, Number(body.a) || 0).run(); return json({ ok: true }); }
    if (c === "clear") { await env.DB.prepare(`DELETE FROM dream_scene WHERE code = ?`).bind(code).run(); return json({ ok: true }); }
    // the live transcript -> a saved one (and the live view starts again)
    if (c === "save") {
      const r = await env.DB.prepare(`SELECT at, text FROM dream_line WHERE code = ? AND n > ? AND n <= ? ORDER BY n`).bind(code, row.mark, row.linen).all();
      const lines = (r.results || []).map((x) => [x.at, x.text]);
      if (!lines.length) return json({ error: "nothing to save" }, 400);
      const a = body.a && typeof body.a === "object" ? body.a : {};
      const title = clean(a.title, 60) || clean(lines.map((l) => l[1]).join(" "), 40);
      const top = await env.DB.prepare(`SELECT MAX(id) AS m FROM dream_tx WHERE code = ?`).bind(code).first();
      const id = ((top && top.m) || 0) + 1;
      await env.DB.batch([
        env.DB.prepare(`INSERT INTO dream_tx (code, id, at, title, lines, json) VALUES (?, ?, ?, ?, ?, ?)`).bind(code, id, now, title, lines.length, JSON.stringify(lines)),
        env.DB.prepare(`UPDATE dream_cube SET mark = ? WHERE code = ?`).bind(row.linen, code),
      ]);
      return json({ ok: true, id, title, lines: lines.length });
    }
    if (c === "clearlive") { await env.DB.prepare(`UPDATE dream_cube SET mark = ? WHERE code = ?`).bind(row.linen, code).run(); return json({ ok: true }); }
    if (c === "txrm") { await env.DB.prepare(`DELETE FROM dream_tx WHERE code = ? AND id = ?`).bind(code, Number(body.a) || 0).run(); return json({ ok: true }); }
    if (c === "txren") {
      const a = body.a && typeof body.a === "object" ? body.a : {};
      await env.DB.prepare(`UPDATE dream_tx SET title = ? WHERE code = ? AND id = ?`).bind(clean(a.title, 60) || "TRANSCRIPT", code, Number(a.id) || 0).run();
      return json({ ok: true });
    }
    return json({ error: "unknown command" }, 400);
  }
  return json({ error: "not found" }, 404);
}

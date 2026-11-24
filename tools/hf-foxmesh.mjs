// hf-foxmesh.mjs: the FOX's 3D shape from public/thuisfront/dev/fox.js -> esp32/HomeFront/fox_mesh.h (the board draws the same fox)
//   node tools/hf-foxmesh.mjs
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
const here = path.dirname(fileURLToPath(import.meta.url));
const { FOX_MESH: M, FOX_CAM: C } = await import(path.join(here, '../public/thuisfront/dev/fox.js'));
const f = (x) => { let t = (Math.round(x * 1e6) / 1e6 + 0).toString(); if (!/[.e]/.test(t)) t += '.0'; return t + 'f'; };
const vecs = (a) => a.map((v) => '{ ' + v.map(f).join(', ') + ' }').join(', ');
let out = '// made by tools/hf-foxmesh.mjs from public/thuisfront/dev/fox.js: do not edit, run that again\n#pragma once\n#include <stdint.h>\n\n';
out += 'struct FoxTri { uint8_t a, b, c, e; int8_t s; };   // corners, outlined edges (1 a-b, 2 b-c, 4 c-a), shade\n';
for (const k of ['BODY', 'HEAD', 'EAR', 'LEG']) {
  const m = M[k];
  out += `static const float FOX_${k}_V[${m.v.length}][3] = { ${vecs(m.v)} };\n`;
  out += `static const FoxTri FOX_${k}_T[${m.t.length}] = { ${m.t.map((t) => `{ ${t.join(', ')} }`).join(', ')} };\n`;
  out += `#define FOX_${k}_NV ${m.v.length}\n#define FOX_${k}_NT ${m.t.length}\n`;
}
out += `static const float FOX_HIPS[4][3] = { ${vecs(M.HIPS)} };\n`;
out += `static const float FOX_LEG_PH[4] = { ${M.LEG_PH.map(f).join(', ')} };\n`;
out += `static const float FOX_HEAD_PIVOT[3] = { ${M.HEAD_PIVOT.map(f).join(', ')} };\n`;
out += `static const float FOX_EAR_PIVOT[3] = { ${M.EAR_PIVOT.map(f).join(', ')} };\n`;
out += `static const float FOX_TAIL_PIVOT[3] = { ${M.TAIL_PIVOT.map(f).join(', ')} };\n`;
out += `static const float FOX_TAIL_R[${M.TAIL_R.length}][2] = { ${vecs(M.TAIL_R)} };\n`;
out += `static const float FOX_TAIL_L[${M.TAIL_L.length}] = { ${M.TAIL_L.map(f).join(', ')} };\n`;
out += `static const int8_t FOX_TAIL_SHADE[${M.TAIL_SHADE.length}] = { ${M.TAIL_SHADE.join(', ')} };\n`;
out += `#define FOX_HS ${f(M.HS)}\n`;
out += `static const float FOX_CAM[3] = { ${C.CAM.map(f).join(', ')} }, FOX_CF[3] = { ${C.CF.map(f).join(', ')} }, FOX_CR[3] = { ${C.CR.map(f).join(', ')} }, FOX_CU[3] = { ${C.CU.map(f).join(', ')} };\n`;
out += `static const float FOX_LIGHT[3] = { ${C.LIGHT.map(f).join(', ')} };\n`;
out += `#define FOX_FOC ${f(C.FOC)}\n#define FOX_CX ${f(C.CX)}\n#define FOX_CY ${f(C.CY)}\n`;
out += `#define FOX_BOX_X ${f(C.BOX.x)}\n#define FOX_BOX_ZB ${f(C.BOX.zb)}\n#define FOX_BOX_ZF ${f(C.BOX.zf)}\n#define FOX_BOX_H ${f(C.BOX.h)}\n`;
const dst = path.join(here, '../esp32/HomeFront/fox_mesh.h');
fs.writeFileSync(dst, out);
console.log('wrote', dst, out.length, 'bytes');

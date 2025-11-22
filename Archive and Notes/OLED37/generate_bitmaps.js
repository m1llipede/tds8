#!/usr/bin/env node
/**
 * Generate Arduino PROGMEM headers from 128x64 PNGs.
 * Usage: node generate_bitmaps.js assets
 */
const fs = require('fs');
const path = require('path');
const { PNG } = require('pngjs');
const jpeg = require('jpeg-js');

const IN_DIR = process.argv[2] ? path.resolve(process.cwd(), process.argv[2]) : path.resolve(process.cwd(), 'assets');
const OUT_DIR = process.cwd();

const FILES = [
  { names: ['T.png'], out: 't_bitmap.h', sym: { W: 'T_BITMAP_W', H: 'T_BITMAP_H', DATA: 'T_BITMAP_DATA' } },
  { names: ['D.png'], out: 'd_bitmap.h', sym: { W: 'D_BITMAP_W', H: 'D_BITMAP_H', DATA: 'D_BITMAP_DATA' } },
  { names: ['S.png'], out: 's_bitmap.h', sym: { W: 'S_BITMAP_W', H: 'S_BITMAP_H', DATA: 'S_BITMAP_DATA' } },
  { names: ['dash.png', '-.png', 'dash.jpg', '-.jpg'], out: 'dash_bitmap.h', sym: { W: 'DASH_BITMAP_W', H: 'DASH_BITMAP_H', DATA: 'DASH_BITMAP_DATA' } },
  { names: ['eight.png', '8.png', 'eight.jpg', '8.jpg'], out: 'eight_bitmap.h', sym: { W: 'EIGHT_BITMAP_W', H: 'EIGHT_BITMAP_H', DATA: 'EIGHT_BITMAP_DATA' } },
  // Allow either scribble.png or "digital scribble strip.jpg"
  { names: ['scribble.png', 'digital scribble strip.jpg'], out: 'scribble_bitmap.h', sym: { W: 'SCRIBBLE_BITMAP_W', H: 'SCRIBBLE_BITMAP_H', DATA: 'SCRIBBLE_BITMAP_DATA' } },
];

function to1BPP(png) {
  const { width: W, height: H, data } = png;
  if (W !== 128 || H !== 64) {
    throw new Error(`Image must be 128x64, got ${W}x${H}`);
  }
  // Adafruit GFX drawBitmap expects packed rows L->R, top->bottom, 1 bit per pixel (MSB first per byte)
  const bytesPerRow = Math.ceil(W / 8);
  const out = Buffer.alloc(bytesPerRow * H);
  const threshold = 128; // binary threshold on luminance
  for (let y = 0; y < H; y++) {
    for (let x = 0; x < W; x++) {
      const idx = (W * y + x) << 2;
      const r = data[idx + 0];
      const g = data[idx + 1];
      const b = data[idx + 2];
      const a = data[idx + 3];
      // Pretreat alpha as white background: if alpha < 128, treat pixel as black (background) = 0
      const lum = a < 128 ? 0 : (0.299 * r + 0.587 * g + 0.114 * b);
      const bit = lum >= threshold ? 1 : 0; // white = 1, black = 0
      const byteIndex = y * bytesPerRow + (x >> 3);
      const bitPos = 7 - (x & 7); // MSB first
      if (bit) out[byteIndex] |= (1 << bitPos);
    }
  }
  return out;
}

function toHeaderBytes(buf) {
  const parts = [];
  for (let i = 0; i < buf.length; i++) {
    const v = buf[i];
    parts.push(`0x${v.toString(16).padStart(2, '0')}`);
  }
  return parts;
}

function emitHeader(dstPath, sym, width, height, bytes) {
  const arr = toHeaderBytes(bytes);
  const content = `#pragma once\n#include <Arduino.h>\n#define ${sym.W} ${width}\n#define ${sym.H} ${height}\nconst uint8_t ${sym.DATA}[] PROGMEM = {\n${arr.map((x, i) => (i % 16 === 0 ? '  ' : '') + x + (i < arr.length - 1 ? ',' : '') + ((i % 16 === 15 || i === arr.length - 1) ? '\n' : ' ')).join('')}\n};\n`;
  fs.writeFileSync(dstPath, content, 'utf8');
}

function exists(p) {
  try { fs.accessSync(p, fs.constants.R_OK); return true; } catch { return false; }
}

function decodeImage(srcPath) {
  const ext = path.extname(srcPath).toLowerCase();
  const buf = fs.readFileSync(srcPath);
  if (ext === '.png') {
    return PNG.sync.read(buf);
  }
  if (ext === '.jpg' || ext === '.jpeg') {
    const decoded = jpeg.decode(buf, { useTArray: true, formatAsRGBA: true });
    // Normalize into a PNG-like object with width, height, data (RGBA)
    return { width: decoded.width, height: decoded.height, data: Buffer.from(decoded.data) };
  }
  throw new Error(`Unsupported format: ${ext}`);
}

(async function main() {
  if (!exists(IN_DIR)) {
    console.error(`[ERROR] Input folder not found: ${IN_DIR}`);
    process.exit(1);
  }
  console.log(`[INFO] Generating headers from: ${IN_DIR}`);
  for (const f of FILES) {
    // Find first available candidate name
    let src = null;
    for (const cand of f.names) {
      const p = path.join(IN_DIR, cand);
      if (exists(p)) { src = p; break; }
    }
    if (!src) {
      console.warn(`[WARN] Missing ${f.names.join(' or ')}, skipping`);
      continue;
    }
    try {
      const img = decodeImage(src);
      const packed = to1BPP(img);
      const outPath = path.join(OUT_DIR, f.out);
      emitHeader(outPath, f.sym, img.width, img.height, packed);
      console.log(`[OK] Wrote ${path.basename(outPath)} (${packed.length} bytes)`);
    } catch (e) {
      console.error(`[ERROR] Failed to process ${path.basename(src)}: ${e.message}`);
    }
  }
  console.log('[DONE] Header generation complete.');
})();

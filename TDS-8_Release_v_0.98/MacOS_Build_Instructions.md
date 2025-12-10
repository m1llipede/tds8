# TDS-8 Bridge – macOS Build Instructions

These instructions are for rebuilding the **TDS-8 Bridge** Electron app for macOS so that it matches the behavior of the current Windows `TDS-8-Bridge.exe` in this release.

> Note: This document is intended to be used on a Mac with access to the same project files (via Git, SMB share, or copy).

---

## 1. Project Structure (Reference)

On Windows, the release you are looking at has this structure:

- `TDS-8_Release_v_0.98/`
  - `TDS-8-Bridge.exe`  
    Electron desktop app (Windows build)
  - `resources/tds8-bridge/`  
    Node/Express/OSC backend and web UI
    - `index.js` – main backend entry
    - `web/` – Bridge dashboard UI
    - `assets/` – logos/icons
    - `python/` – embedded Python for firmware flashing on Windows

The macOS build should:

- Run the **same `index.js` backend**.
- Serve the **same UI** on `http://localhost:8088`.
- Use the **same OSC ports and API routes** so the external TDS-8 Control app works unchanged.

---

## 2. Runtime Behavior to Preserve

- **HTTP server**: `http://localhost:8088` (Express serves `web/` and `assets/`).
- **OSC sender** (Bridge → Ableton M4L):
  - UDP **port 9001**, sends to **127.0.0.1:8001**.
- **OSC listener** (M4L → Bridge):
  - UDP **port 8003** (via `OSC_LISTEN_PORT`, default 8003).
- **Handshake**:
  - Bridge sends `/hello` to M4L.
  - M4L replies with `/hi` to port 8003.
  - `/trackname` and `/activetrack` updates also count as a heartbeat.
- **Track-name sweep** (`requestNextTrackName` in `index.js`):
  - Requests indices 0–31.
  - Uses **25 ms delay** between requests:
    ```js
    // Wait 25ms before requesting next track (2x faster refresh)
    trackRequestTimer = setTimeout(requestNextTrackName, 25);
    ```
- **API endpoints** the Control app depends on:
  - `GET /api/ports`
  - `GET /api/device-status`
  - `POST /api/firmware/flash`
  - `GET /api/firmware/releases`
  - Static UI + root:
    - `GET /` → `web/index.html`
    - `/assets` → `assets/`
- **Device routing**:
  - Up to 4 devices, 8 tracks per block, global track indices 0–31.
  - Keep the multi-device logic and legacy single-device behavior exactly as in `index.js`.
- **CORS allowlist**:
  - Keep the default `ALLOWLIST` behavior (localhost + optional remote origins).

---

## 3. macOS Prerequisites

On your Mac, install:

1. **Node.js** (LTS recommended, e.g. 18.x or 20.x)  
   - From https://nodejs.org or via Homebrew: `brew install node`
2. **Python 3 + esptool** (for firmware flashing):
   ```bash
   brew install python
   python3 -m pip install --upgrade pip
   python3 -m pip install esptool
   ```

Clone or copy the shared codebase to your Mac so you have the same `resources/tds8-bridge` folder.

---

## 4. Install Node Dependencies (macOS)

From the **`resources/tds8-bridge`** directory on your Mac:

```bash
cd resources/tds8-bridge
npm ci    # or: npm install
```

This should install all backend and frontend dependencies (Express, OSC, WebSocket, etc.).

---

## 5. Running the Bridge Backend Directly (Dev Mode)

Before packaging as an app, verify that the backend and UI run correctly on macOS.

1. From `resources/tds8-bridge` on your Mac:
   ```bash
   node index.js
   ```
2. In a browser on the Mac, open:
   ```
   http://localhost:8088
   ```
3. Confirm:
   - You see the Bridge dashboard.
   - The console logs show OSC sender (9001) and listener (8003) initializing.
   - TDS-8 Control (on the same machine) can connect to the bridge and devices.

If this works, the backend is macOS-compatible.

---

## 6. Electron Packaging for macOS (Concept)

The goal is to produce a macOS `.app` that:

- Bundles `resources/tds8-bridge` and its `node_modules`.
- Starts `index.js` (Node/Express/OSC) when the app launches.
- Opens a window that points to `http://localhost:8088` once the server is up.

A typical setup (to be created on macOS):

1. **Electron app folder** (e.g. `TDS-8_Bridge/` or similar) with a `package.json` defining:
   - `main`: Electron main process file (e.g. `main.js`).
   - `scripts`: `"start"`, `"dist"`, etc. using `electron` / `electron-builder`.
2. **Electron main process** (e.g. `main.js`) should:
   - Spawn the Node backend (`index.js`) as a child process.
   - Wait for `http://localhost:8088` to respond.
   - Create `BrowserWindow` pointed at `http://localhost:8088`.
3. Use a tool like **electron-builder** to generate a macOS `.app` / `.dmg`.

> These Electron wiring details depend on how you want to organize the project on your Mac. The important part is to keep ports, routes, and behavior identical to `index.js` as shipped here.

---

## 7. Python / esptool on macOS

In `index.js`, firmware flashing currently prefers an embedded Windows Python and falls back to `python` on PATH. On macOS:

- Ensure `getPythonCommand()` resolves to **`python3`** (or a Python with `esptool` installed).
- You may adjust `getPythonCommand()` on your Mac branch to:
  - Check `process.platform === 'darwin'` and return `'python3'`.
- Keep the actual flashing logic (offset detection, commands, logging) unchanged.

---

## 8. Minimal macOS Build Checklist

When building on macOS, verify:

- [ ] `node index.js` runs without errors on macOS.
- [ ] `http://localhost:8088` serves the Bridge web UI.
- [ ] OSC sender on port 9001 and listener on 8003 initialize correctly.
- [ ] TDS-8 Control can connect and no longer shows 404s.
- [ ] Track-name refresh from Ableton completes quickly (32 tracks at 25 ms per track).
- [ ] Firmware flashing works via Python 3 / esptool.
- [ ] The Electron `.app` launches and behaves like the Windows `TDS-8-Bridge.exe`.

---

## 9. Notes

- Keep any changes for macOS **minimal and platform-specific**, avoiding refactors that could change behavior on Windows.
- Preserve:
  - API routes and JSON shapes.
  - OSC addresses and ports.
  - Timing constants and dedup logic.
- If you change anything that affects the Control app, document it clearly in a separate section.

You can export this Markdown file to PDF from your editor (or using a Markdown tool) if you need a PDF version of these instructions.

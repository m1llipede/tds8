// TDS-8 Bridge Electron App (separate)
// Embeds the existing bridge at ../tds8-bridge/index.js in the main process and shows the dashboard

const { app, BrowserWindow, Tray, Menu, shell } = require('electron');
const path = require('path');
const { exec } = require('child_process');

let mainWindow = null;
let tray = null;
const BRIDGE_PORT = process.env.TDS8_PORT ? Number(process.env.TDS8_PORT) : 8088;

function killExistingProcesses() {
  return new Promise((resolve) => {
    // Only do aggressive cleanup on Windows; on macOS/Linux this is a no-op.
    if (process.platform !== 'win32') {
      return resolve();
    }

    console.log(`[Launcher] Checking for existing TDS-8 Bridge processes on ports 8088 and ${BRIDGE_PORT}...`);

    // Kill any process currently LISTENING on the old port 8088 and the active bridge port
    const portKillCmd = [
      'for /f "tokens=5" %a in (\'netstat -aon ^| findstr :8088\') do @taskkill /F /PID %a',
      `for /f "tokens=5" %b in (\'netstat -aon ^| findstr :${BRIDGE_PORT}\') do @taskkill /F /PID %b`
    ].join(' & ');

    exec(portKillCmd, () => {
      // 2) Kill any stray packaged bridge executables except the current process
      const exeNames = ['TDS-8 Bridge.exe', 'TDS-8-Bridge.exe'];
      const killExeCmds = exeNames.map(name => `taskkill /F /FI "IMAGENAME eq ${name}" /FI "PID ne ${process.pid}" 2>nul`);

      const runKillExe = (idx = 0) => {
        if (idx >= killExeCmds.length) {
          // 3) As a fallback, kill old Node-based bridge console windows started by the batch launcher
          exec('taskkill /F /IM node.exe /FI "WINDOWTITLE eq TDS-8*" 2>nul', () => {
            console.log('[Launcher] Cleanup complete');
            resolve();
          });
          return;
        }
        exec(killExeCmds[idx], () => runKillExe(idx + 1));
      };

      runKillExe();
    });
  });
}

function startBridge() {
  if (global.__tds8BridgeStarted) return;
  global.__tds8BridgeStarted = true;
  try {
    // Ensure PORT is set for the bridge
    process.env.PORT = String(BRIDGE_PORT);
    // In development, use the relative path. In production, use the path to the packaged extraResource.
    const bridgePath = app.isPackaged
      ? path.join(process.resourcesPath, 'tds8-bridge', 'index.js')
      : path.join(__dirname, '..', 'tds8-bridge', 'index.js');

    console.log(`[Launcher] Starting bridge from: ${bridgePath}`);
    require(bridgePath);
  } catch (e) {
    console.error('Failed to start embedded TDS-8 bridge:', e);
  }
}

function createWindow() {
  // Match the .bat behavior: just start the bridge and open the
  // GitHub-hosted dashboard in the user's default browser.
  // We do not show our own BrowserWindow UI.
  shell.openExternal(`http://localhost:${BRIDGE_PORT}`);
}

function createTray() {
  const iconPath = path.join(__dirname, '..', 'tds8-bridge', 'assets', 'icon-white.png');
  tray = new Tray(iconPath);
  const getLoginMenuItem = () => {
    const current = app.getLoginItemSettings();
    return {
      label: 'Start on login',
      type: 'checkbox',
      checked: !!current.openAtLogin,
      click: (item) => {
        app.setLoginItemSettings({
          openAtLogin: item.checked,
          openAsHidden: true,
          args: []
        });
      }
    };
  };

  const menu = Menu.buildFromTemplate([
    { label: 'Open Dashboard', click: () => shell.openExternal(`http://localhost:${BRIDGE_PORT}`) },
    { type: 'separator' },
    getLoginMenuItem(),
    { type: 'separator' },
    { label: 'Show Window', click: () => { if (mainWindow) { mainWindow.show(); mainWindow.focus(); } } },
    { type: 'separator' },
    { label: 'Quit', click: () => { app.isQuitting = true; app.quit(); } }
  ]);
  tray.setToolTip('TDS-8 Bridge');
  tray.setContextMenu(menu);
  tray.on('double-click', () => { if (mainWindow) { mainWindow.show(); mainWindow.focus(); } });
}

// Single instance lock to avoid multiple copies
const gotLock = app.requestSingleInstanceLock();
if (!gotLock) {
  app.quit();
} else {
  app.on('second-instance', () => {
    if (mainWindow) {
      if (!mainWindow.isVisible()) mainWindow.show();
      mainWindow.focus();
    }
  });

  app.on('ready', async () => {
    await killExistingProcesses();
    startBridge();
    createWindow();
    createTray();
  });
}

app.on('before-quit', () => { app.isQuitting = true; });
app.on('window-all-closed', () => { if (process.platform !== 'darwin') app.quit(); });
app.on('activate', () => { if (mainWindow === null) createWindow(); });

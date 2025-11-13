// TDS-8 Bridge Electron App (separate)
// Embeds the existing bridge at ../tds8-bridge/index.js in the main process and shows the dashboard

const { app, BrowserWindow, Tray, Menu, shell } = require('electron');
const path = require('path');

let mainWindow = null;
let tray = null;
const BRIDGE_PORT = process.env.TDS8_PORT ? Number(process.env.TDS8_PORT) : 8088;

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
  mainWindow = new BrowserWindow({
    width: 1400,
    height: 1000,
    minWidth: 1200,
    minHeight: 900,
    title: 'TDS-8 Bridge',
    icon: path.join(__dirname, '..', 'tds8-bridge', 'assets', 'icon-white.png'),
    backgroundColor: '#0f1115',
    titleBarStyle: 'default',
    show: false,
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      preload: path.join(__dirname, 'preload.js')
    }
  });

  let attempts = 0;
  const maxAttempts = 12;
  const tryLoad = () => {
    attempts++;
    mainWindow.loadURL(`http://127.0.0.1:${BRIDGE_PORT}`).catch(() => {
      if (attempts < maxAttempts) setTimeout(tryLoad, 1000);
    });
  };
  setTimeout(tryLoad, 1200);

  mainWindow.on('page-title-updated', (event) => {
    event.preventDefault();
    mainWindow.setTitle('TDS-8 Bridge');
  });

  mainWindow.once('ready-to-show', () => {
    mainWindow.setTitle('TDS-8 Bridge');
    mainWindow.show();
  });
  mainWindow.on('closed', () => { mainWindow = null; });
  mainWindow.webContents.setWindowOpenHandler(({ url }) => { shell.openExternal(url); return { action: 'deny' }; });
  mainWindow.on('minimize', (e) => { e.preventDefault(); mainWindow.hide(); });
  mainWindow.on('close', (e) => {
    if (!app.isQuitting) { e.preventDefault(); mainWindow.hide(); }
  });
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
    { label: 'Open Dashboard', click: () => shell.openExternal(`http://127.0.0.1:${BRIDGE_PORT}`) },
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

  app.on('ready', () => { startBridge(); createWindow(); createTray(); });
}

app.on('before-quit', () => { app.isQuitting = true; });
app.on('window-all-closed', () => { if (process.platform !== 'darwin') app.quit(); });
app.on('activate', () => { if (mainWindow === null) createWindow(); });

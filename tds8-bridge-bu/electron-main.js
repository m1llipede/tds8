// TDS-8 Electron Main Process
// Wraps the bridge in a native desktop app

const { app, BrowserWindow, Tray, Menu, ipcMain, shell } = require('electron');
const path = require('path');
const { spawn } = require('child_process');

let mainWindow = null;
let tray = null;
let bridgeProcess = null;
const BRIDGE_PORT = 8088;

// Start the bridge server
function startBridge() {
  console.log('Starting TDS-8 bridge server...');
  
  bridgeProcess = spawn('node', [path.join(__dirname, 'index.js')], {
    env: { ...process.env, PORT: BRIDGE_PORT },
    stdio: 'inherit'
  });

  bridgeProcess.on('error', (err) => {
    console.error('Bridge process error:', err);
  });

  bridgeProcess.on('exit', (code) => {
    console.log(`Bridge process exited with code ${code}`);
    if (code !== 0 && !app.isQuitting) {
      // Restart if crashed
      setTimeout(startBridge, 2000);
    }
  });
}

// Create the main window
function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1400,
    height: 900,
    minWidth: 1200,
    minHeight: 800,
    title: 'TDS-8 Control',
    icon: path.join(__dirname, 'web', 'icon.png'),
    webPreferences: {
      nodeIntegration: false,
      contextIsolation: true,
      preload: path.join(__dirname, 'electron-preload.js')
    },
    backgroundColor: '#f6f7fb',
    titleBarStyle: 'default', // Standard title bar with drag handle
    show: false // Show after ready
  });

  // Wait for bridge to start, then load
  setTimeout(() => {
    mainWindow.loadURL(`http://localhost:${BRIDGE_PORT}`);
  }, 1500);

  mainWindow.once('ready-to-show', () => {
    mainWindow.show();
  });

  mainWindow.on('closed', () => {
    mainWindow = null;
  });

  // Open external links in browser
  mainWindow.webContents.setWindowOpenHandler(({ url }) => {
    shell.openExternal(url);
    return { action: 'deny' };
  });

  // Handle minimize to tray
  mainWindow.on('minimize', (event) => {
    if (process.platform === 'darwin') {
      // Mac: minimize normally
      return;
    }
    // Windows/Linux: minimize to tray
    event.preventDefault();
    mainWindow.hide();
  });

  mainWindow.on('close', (event) => {
    if (!app.isQuitting && process.platform !== 'darwin') {
      event.preventDefault();
      mainWindow.hide();
    }
  });
}

// Create system tray
function createTray() {
  // Use a simple icon (you can replace with custom icon)
  const iconPath = path.join(__dirname, 'web', 'tray-icon.png');
  
  tray = new Tray(iconPath);
  
  const contextMenu = Menu.buildFromTemplate([
    {
      label: 'Show TDS-8',
      click: () => {
        if (mainWindow) {
          mainWindow.show();
          mainWindow.focus();
        } else {
          createWindow();
        }
      }
    },
    { type: 'separator' },
    {
      label: 'Open in Browser',
      click: () => {
        shell.openExternal(`http://localhost:${BRIDGE_PORT}`);
      }
    },
    { type: 'separator' },
    {
      label: 'Quit TDS-8',
      click: () => {
        app.isQuitting = true;
        app.quit();
      }
    }
  ]);

  tray.setToolTip('TDS-8 Control');
  tray.setContextMenu(contextMenu);

  // Double-click to show window
  tray.on('double-click', () => {
    if (mainWindow) {
      mainWindow.show();
      mainWindow.focus();
    } else {
      createWindow();
    }
  });
}

// App lifecycle
app.on('ready', () => {
  startBridge();
  createWindow();
  createTray();
});

app.on('window-all-closed', () => {
  // Keep app running on Mac
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

app.on('activate', () => {
  if (mainWindow === null) {
    createWindow();
  }
});

app.on('before-quit', () => {
  app.isQuitting = true;
});

app.on('will-quit', () => {
  // Stop bridge process
  if (bridgeProcess) {
    bridgeProcess.kill();
  }
});

// IPC handlers
ipcMain.handle('get-version', () => {
  return app.getVersion();
});

ipcMain.handle('minimize-to-tray', () => {
  if (mainWindow) {
    mainWindow.hide();
  }
});

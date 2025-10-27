// TDS-8 Electron Preload Script
// Exposes safe APIs to the renderer process

const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('electronAPI', {
  getVersion: () => ipcRenderer.invoke('get-version'),
  minimizeToTray: () => ipcRenderer.invoke('minimize-to-tray'),
  platform: process.platform
});

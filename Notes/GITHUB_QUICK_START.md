# GitHub OTA - Quick Visual Guide

## 🎯 Your Firmware Setup

### ✅ Already Configured!

**Your firmware is in your main repository:**

```
https://github.com/m1llipede/tds8
```

**Structure:**
```
tds8-firmware/
├── README.md
├── manifest.json          ← Points to latest version
├── firmware-0.30.bin      ← Your compiled firmware
├── firmware-0.31.bin      ← Future versions
└── firmware-0.32.bin
```

**Pros:**
- ✅ Clean and organized
- ✅ Easy to manage versions
- ✅ Can make public without exposing main code
- ✅ Simple URLs

---

### Option 2: Releases in Main Repository

**Use GitHub Releases in your main TDS-8 repo:**

1. Go to your main repository
2. Click "Releases" (right sidebar)
3. Click "Create a new release"
4. Tag: `v0.30`
5. Title: `TDS-8 v0.30 - Wired First`
6. Upload `firmware-0.30.bin`
7. Upload `manifest.json`
8. Click "Publish release"

**URL format:**
```
https://github.com/YOUR_USERNAME/TDS-8/releases/download/v0.30/firmware-0.30.bin
```

**Pros:**
- ✅ Everything in one repo
- ✅ Release notes built-in
- ✅ Version history automatic

**Cons:**
- ❌ Longer URLs
- ❌ Must update manifest URL for each release

---

## 🚀 Recommended Setup (Step by Step)

### Step 1: Create Firmware Repository

1. **Go to:** https://github.com/new
2. **Repository name:** `tds8-firmware`
3. **Description:** `TDS-8 OLED firmware releases and OTA updates`
4. **Public** ← Important!
5. **Add README** ← Check this
6. **Click:** "Create repository"

---

### Step 2: Add manifest.json

1. **Click:** "Add file" → "Create new file"
2. **Filename:** `manifest.json`
3. **Paste this:**

```json
{
  "version": "0.30",
  "url": "https://raw.githubusercontent.com/YOUR_USERNAME/tds8-firmware/main/firmware-0.30.bin",
  "notes": "Wired-first release with serial commands"
}
```

4. **Replace** `YOUR_USERNAME` with your GitHub username
5. **Click:** "Commit new file"

---

### Step 3: Export Firmware from Arduino

**In Arduino IDE:**

1. Open `OLED30.ino`
2. **Menu:** Sketch → Export Compiled Binary
3. Wait for "Done compiling"
4. **Find file:** 
   - Mac: `/Users/test/Documents/TDS-8/OLED30/`
   - Look for: `OLED30.ino.bin` or `OLED30.ino.esp32c3.bin`

**Rename file to:** `firmware-0.30.bin`

---

### Step 4: Upload Binary to GitHub

1. **Go to:** Your `tds8-firmware` repository
2. **Click:** "Add file" → "Upload files"
3. **Drag:** `firmware-0.30.bin` into the upload area
4. **Commit message:** "Add v0.30 firmware binary"
5. **Click:** "Commit changes"

**Your repo now looks like:**
```
📁 tds8-firmware
  📄 README.md
  📄 manifest.json
  📄 firmware-0.30.bin
```

---

### Step 5: Get Your URLs

**Your manifest URL (for Arduino code):**
```
https://raw.githubusercontent.com/YOUR_USERNAME/tds8-firmware/main/manifest.json
```

**Example** (if username is "johndoe"):
```
https://raw.githubusercontent.com/johndoe/tds8-firmware/main/manifest.json
```

**Test it:** Paste URL in browser - should show JSON

---

### Step 6: Update Arduino Code

**Open:** `OLED30.ino`

**Find line 40:**
```cpp
#define GITHUB_MANIFEST_URL "https://raw.githubusercontent.com/YOUR_USERNAME/tds8-firmware/main/manifest.json"
```

**Change to YOUR username:**
```cpp
#define GITHUB_MANIFEST_URL "https://raw.githubusercontent.com/johndoe/tds8-firmware/main/manifest.json"
```

**Save and re-upload to device!**

---

## 🔍 How to Find Your GitHub Username

### Method 1: GitHub Profile
1. Go to GitHub.com
2. Click your profile picture (top right)
3. Your username is shown below your name

### Method 2: Any Repository URL
Your repos are at:
```
https://github.com/YOUR_USERNAME
```

The part after `github.com/` is your username!

---

## 📝 Your Complete Setup

**Username:** `m1llipede`

### Your Repository URL:
```
https://github.com/m1llipede/tds8
```

### Your manifest.json:
```json
{
  "version": "0.30",
  "url": "https://github.com/m1llipede/tds8/releases/download/v0.30/OLED30.ino.bin"
}
```

### Your Arduino Code (line 40):
```cpp
#define GITHUB_MANIFEST_URL "https://raw.githubusercontent.com/m1llipede/tds8/main/manifest.json"
```

### Test URLs in Browser:

**Manifest (should show JSON):**
```
https://raw.githubusercontent.com/m1llipede/tds8/main/manifest.json
```

**Binary (should download file):**
```
https://github.com/m1llipede/tds8/releases/download/v0.30/OLED30.ino.bin
```

---

## 🎯 Quick Verification

### ✅ Repository is Public
- Go to your repo
- Look for "Public" badge near repo name
- If it says "Private", go to Settings → Change visibility

### ✅ Files Uploaded Correctly
Your repo should show:
```
📁 tds8-firmware
  📄 README.md
  📄 manifest.json (should be ~200 bytes)
  📄 firmware-0.30.bin (should be ~1.2 MB)
```

### ✅ Raw URLs Work
Test in browser:
1. Manifest URL → Shows JSON text
2. Binary URL → Downloads .bin file

### ✅ Arduino Code Updated
Line 40 has YOUR username, not "YOUR_USERNAME"

---

## 🔄 Releasing New Versions

**When you make v0.31:**

1. **Export** new binary → `firmware-0.31.bin`
2. **Upload** to GitHub (same repo)
3. **Edit** `manifest.json`:
   ```json
   {
     "version": "0.31",
     "url": "https://raw.githubusercontent.com/YOUR_USERNAME/tds8-firmware/main/firmware-0.31.bin",
     "notes": "Bug fixes and new features"
   }
   ```
4. **Users** see update notification in web dashboard
5. **Click** to install → Device updates automatically!

**Keep old versions** in repo for rollback:
```
📁 tds8-firmware
  📄 manifest.json (points to latest)
  📄 firmware-0.30.bin
  📄 firmware-0.31.bin
  📄 firmware-0.32.bin
```

---

## 🆘 Common Issues

### "404 Not Found" when checking updates
**Fix:**
1. Verify repository is **public**
2. Check username in Arduino code matches GitHub
3. Wait 1-2 minutes after uploading (GitHub caching)

### Binary file won't upload
**Fix:**
1. Make sure file is under 100MB (should be ~1-2MB)
2. Try uploading via GitHub Desktop app
3. Use Git LFS for large files (not usually needed)

### Manifest shows old version
**Fix:**
1. Edit `manifest.json` on GitHub
2. Update version number
3. Update URL to new .bin file
4. Commit changes
5. Wait 1-2 minutes for cache to clear

---

## 📱 Mobile Access

You can manage your firmware repo from GitHub mobile app:
1. Download "GitHub" app
2. Sign in
3. Navigate to `tds8-firmware`
4. View files, edit manifest, check downloads

---

## 🎉 You're Done!

Once set up, updating firmware is easy:
1. Make changes in Arduino
2. Export binary
3. Upload to GitHub
4. Update manifest.json
5. Users get notified!

**No need to physically access devices for updates!** 🚀

# M4L Track Name Handshake Protocol

## Problem
When M4L sends track names with a fixed delay (e.g., 200ms), the timing doesn't account for different computer speeds. Slower machines may skip track names or receive duplicates.

## Solution: Bridge-Controlled Request Protocol

The Bridge now **requests** track names from M4L one at a time using `/request-trackname`. M4L simply **responds** with the track name when requested. This gives the Bridge full control over pacing.

## Implementation

### M4L Patch Changes

M4L should:
1. Listen for `/request-trackname` messages from the Bridge (port 8001)
2. Extract the track index from the message
3. Send back `/trackname index "TrackName" index` to the Bridge (port 8003)
4. Wait for the next request

### Example Max/MSP Flow

```
[udpreceive 8001]
  ↓
[route /request-trackname]
  ↓
[unpack i]  ← Extract track index
  ↓
[lookup_track_name]  ← Get name from your track list
  ↓
[send /trackname $1 "name" $1]  ← Send back to Bridge
```

This ensures:
- ✅ No skipped tracks (Bridge controls the sequence)
- ✅ No duplicate tracks (Bridge requests each once)
- ✅ Works on all computer speeds (Bridge paces requests)
- ✅ Simple M4L logic (just respond to requests)

## Backend Details

**Request Message**: OSC `/request-trackname`
**Argument**: Integer (track index 0-31)
**Timing**: Bridge sends requests 500ms apart

**Response Expected**: OSC `/trackname`
**Arguments**: 
- Index (0-31)
- Track name (string)
- Global index (0-31)

### Example Flow

```
Bridge → M4L: /request-trackname 0
M4L → Bridge: /trackname 0 "Drums" 0
Bridge → M4L: /request-trackname 1
M4L → Bridge: /trackname 1 "Bass" 1
... (repeat for all 32 tracks)
```

## Configuration

- **OSC Listen Port (M4L → Bridge)**: 8003
- **OSC Send Port (Bridge → M4L)**: 8001
- **M4L Receive Port**: 8001
- **M4L Send Port**: 8003

## Testing

1. Open M4L patch with request-response logic
2. Press "Refresh from Ableton" in Bridge UI
3. Check Serial Monitor for `/request-trackname` messages
4. Verify M4L responds with `/trackname` for each request
5. Verify all 32 tracks appear correctly (no gaps, no duplicates)

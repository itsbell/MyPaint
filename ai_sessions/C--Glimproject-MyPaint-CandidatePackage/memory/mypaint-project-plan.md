---
name: mypaint-project-plan
description: MyPaint MFC assignment — agreed architecture (Command-based Core/Tools/UI) and the planned real-time sharing idea
metadata:
  node_type: memory
  type: project
  originSessionId: d992d2c6-0359-4aa2-a125-2e962b39c39d
  modified: 2026-11-25T08:11:12.570Z
---

MFC dialog-based paint app assignment (Windows Paint clone + one original idea). User handles git init and `ai_sessions/` themselves.

Original idea (decided 2026-11-25): host shares the drawing scene to other users in real time. To be built LAST, after basic paint features; detailed spec to be discussed with the user at that point.

Agreed architecture: every edit is a serializable `Command` applied only via `Document::Apply` (undo = pixel backup of Bounds, redo = re-Execute); Tools emit commands + preview command; CanvasWnd composes preview on a doc copy; Net (Winsock, separate thread, PostMessage to UI) comes last. Non-deterministic ops (text, paste) should become pixel-patch commands.

Sharing spec agreed 2026-11-25: viewers are view-only (max 8); LAN TCP + UDP broadcast room discovery, default port 47800 (changeable); no password, no approve/kick; live previews + host cursor shared; undo/redo/new/open/resize all mirrored; late joiner gets full snapshot then command stream; simple text chat + nickname participant list; viewers cannot save (this phase); host ends → viewer notified, last image kept; viewer disconnect → notice only (no auto-reconnect); README demo = two instances via 127.0.0.1. UI design to be agreed before coding.

**Why:** command stream doubles as the network protocol for the sharing feature.
**How to apply:** never let tools/UI write pixels directly; add new edits as Command subclasses with Write/Read. See [[mypaint-build-env]].

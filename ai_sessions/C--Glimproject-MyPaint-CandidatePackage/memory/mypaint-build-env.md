---
name: mypaint-build-env
description: "How to build/verify MyPaint on this machine — MSBuild path, no python, min/max macro gotcha, headless UI test approach"
metadata:
  node_type: memory
  type: reference
  originSessionId: d992d2c6-0359-4aa2-a125-2e962b39c39d
  modified: 2026-11-25T23:30:59.056Z
---

- MSBuild: `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe MyPaint.sln -p:Configuration=Release -p:Platform=x64` (toolset v143 works). "pwsh.exe not found" line in output is harmless (vcpkg applocal).
- `python` is only the Windows Store stub — use sed/PowerShell for scripted edits.
- Don't define NOMINMAX (GDI+/CImage need the macros); write `(std::min)(...)`; CPoint/CSize fields are LONG so use `0L` literals with std::clamp.
- Source files: UTF-8 with BOM + CRLF; Resource.h and .rc without BOM (.rc has `#pragma code_page(65001)`).
- PowerShell 5.1 test scripts with Korean text MUST be saved with a UTF-8 BOM — without it the file is read as CP949 and the line after each Korean comment is silently swallowed (looked like app bugs).
- Never SendMessage text-returning messages (SB_GETTEXT etc.) cross-process with a local buffer — it crashes MyPaint; read state from PrintWindow screenshots instead.
- Sharing tests: the user often keeps their own Debug MyPaint hosting on default port 47800 — automated share tests must use another port (e.g. 47900) or test clients will join the user's real room. Don't rebuild x64\Debug while their instances run (build Debug to scratchpad OutDir instead).
- Headless UI check: PowerShell Add-Type script that PostMessages mouse events to canvas (GetDlgItem id 1000), WM_COMMAND ids, and PrintWindow screenshots — doesn't touch the user's real mouse.

Related: [[mypaint-project-plan]]

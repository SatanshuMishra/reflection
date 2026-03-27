@echo off
:: SPDX-License-Identifier: GPL-3.0-or-later
:: Copyright (C) 2026 Satanshu Mishra
::
:: Reflection -- Portable Cleanup Script
:: Removes all Reflection settings, cached data, and firewall rules.
:: Run this before deleting the portable folder for a clean removal.

echo.
echo  Reflection -- Cleanup
echo  =====================
echo.
echo  This will remove all Reflection settings and cached data:
echo    - Registry settings (HKCU\Software\Reflection)
echo    - Auto-start entry
echo    - WebView2 browser cache
echo    - Windows Firewall rule (requires admin)
echo.
echo  Press any key to continue, or Ctrl+C to cancel.
pause >nul

"%~dp0Reflection.exe" --uninstall-cleanup

echo.
echo  Cleanup complete. You can now delete this folder.
echo.
pause

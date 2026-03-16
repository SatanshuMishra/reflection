#!/bin/bash
set -euo pipefail

# Usage: ./scripts/notarize.sh path/to/Reflection.dmg
# Requires: APPLE_ID, APPLE_TEAM_ID, APPLE_APP_SPECIFIC_PASSWORD env vars

if [ $# -ne 1 ]; then
    echo "Usage: $0 <path-to-dmg>"
    exit 1
fi

DMG_PATH="$1"

if [ ! -f "$DMG_PATH" ]; then
    echo "Error: File not found: $DMG_PATH"
    exit 1
fi

echo "Submitting $DMG_PATH for notarization..."
xcrun notarytool submit "$DMG_PATH" \
    --apple-id "${APPLE_ID:?APPLE_ID not set}" \
    --team-id "${APPLE_TEAM_ID:?APPLE_TEAM_ID not set}" \
    --password "${APPLE_APP_SPECIFIC_PASSWORD:?APPLE_APP_SPECIFIC_PASSWORD not set}" \
    --wait

echo "Stapling notarization ticket..."
xcrun stapler staple "$DMG_PATH"

echo "Done. $DMG_PATH is notarized and stapled."

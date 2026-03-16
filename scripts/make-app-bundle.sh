#!/bin/bash
set -euo pipefail

CONFIG="${1:-release}"

if [[ "$CONFIG" != "debug" && "$CONFIG" != "release" ]]; then
    echo "Usage: $0 [debug|release]"
    exit 1
fi

echo "Building ($CONFIG)..."
swift build -c "$CONFIG"

# Create .app bundle structure
APP_DIR="Reflection.app/Contents"
rm -rf Reflection.app
mkdir -p "$APP_DIR/MacOS"
mkdir -p "$APP_DIR/Resources"

# Copy binary
cp ".build/$CONFIG/Reflection" "$APP_DIR/MacOS/Reflection"

# Copy Info.plist
cp SupportingFiles/Info.plist "$APP_DIR/Info.plist"

# Ad-hoc sign with entitlements and hardened runtime
codesign --force --options runtime \
    --entitlements SupportingFiles/Reflection.entitlements \
    --sign - \
    Reflection.app

echo "Created Reflection.app ($CONFIG)"
echo "Run with: open Reflection.app"

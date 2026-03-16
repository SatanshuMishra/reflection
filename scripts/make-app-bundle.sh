#!/bin/bash
set -euo pipefail

# Build release
DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer swift build

# Create .app bundle structure
APP_DIR="Reflection.app/Contents"
rm -rf Reflection.app
mkdir -p "$APP_DIR/MacOS"
mkdir -p "$APP_DIR/Resources"

# Copy binary
cp .build/debug/Reflection "$APP_DIR/MacOS/Reflection"

# Copy Info.plist
cp SupportingFiles/Info.plist "$APP_DIR/Info.plist"

# Ad-hoc sign with entitlements and hardened runtime
codesign --force --options runtime \
    --entitlements SupportingFiles/Reflection.entitlements \
    --sign - \
    Reflection.app

echo "Created Reflection.app"
echo "Run with: open Reflection.app"

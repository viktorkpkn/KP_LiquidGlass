#!/bin/zsh
# KP_LiquidGlass — Copyright (c) 2026 Viktor Kopeikin.
# Licensed under the PolyForm Noncommercial License 1.0.0.
# Build KP_LiquidGlass and install into the user-level MediaCore folder.
# AE does not load symlinked plugins, so this copies the bundle.
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
DEST="$HOME/Library/Application Support/Adobe/Common/Plug-ins/7.0/MediaCore"

xcodebuild -project "$DIR/Mac/KP_LiquidGlass.xcodeproj" -configuration Debug \
    ARCHS=arm64 ONLY_ACTIVE_ARCH=NO build | grep -E "error:|BUILD (SUCCEEDED|FAILED)"

mkdir -p "$DEST"
rm -rf "$DEST/KP_LiquidGlass.plugin"
cp -R "$DIR/Mac/build/Debug/KP_LiquidGlass.plugin" "$DEST/"
echo "Installed to $DEST/KP_LiquidGlass.plugin (restart AE to pick it up)"

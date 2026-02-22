#!/bin/bash
#
# SmacAccess Release Packager
# Creates a ZIP file with all files needed to install SmacAccess.
#
# Usage: ./tools/package_release.sh [output_dir]
#   output_dir: where to place the ZIP (default: ./rel)
#
# Run from the project root directory.
#

set -e

# Config
OUTPUT_DIR="${1:-rel}"
BUILD_DIR="build_cmake"
TOLK_DIR="tolk/x86"
LANG_DIR="sr_lang"

# Check required build outputs exist
if [ ! -f "$BUILD_DIR/thinker.dll" ] || [ ! -f "$BUILD_DIR/thinker.exe" ]; then
    echo "ERROR: Build outputs not found in $BUILD_DIR/"
    echo "Run the build first (see README.md for build instructions)."
    exit 1
fi

# Create temp staging directory
STAGING=$(mktemp -d)
trap "rm -rf '$STAGING'" EXIT

echo "Packaging SmacAccess release..."

# Core mod files
cp "$BUILD_DIR/thinker.dll" "$STAGING/"
cp "$BUILD_DIR/thinker.exe" "$STAGING/"
echo "  + thinker.dll, thinker.exe"

# Configuration
cp docs/thinker.ini "$STAGING/"
echo "  + thinker.ini"

# Menu definitions (required by Thinker at startup)
if [ -f docs/modmenu.txt ]; then
    cp docs/modmenu.txt "$STAGING/"
    echo "  + modmenu.txt"
fi

# Tolk screen reader DLLs (only runtime files, not .lib/.exp)
cp "$TOLK_DIR/Tolk.dll" "$STAGING/"
cp "$TOLK_DIR/nvdaControllerClient32.dll" "$STAGING/"
cp "$TOLK_DIR/SAAPI32.dll" "$STAGING/"
cp "$TOLK_DIR/dolapi32.dll" "$STAGING/"
echo "  + Tolk DLLs (4 files)"

# Screen reader language files
mkdir -p "$STAGING/sr_lang"
cp "$LANG_DIR/en.txt" "$STAGING/sr_lang/"
cp "$LANG_DIR/de.txt" "$STAGING/sr_lang/"
echo "  + sr_lang/en.txt, sr_lang/de.txt"

# Documentation
cp README.md "$STAGING/"
cp Details.md "$STAGING/"
if [ -f Changelog.md ]; then
    cp Changelog.md "$STAGING/"
fi
cp License.txt "$STAGING/"
echo "  + Documentation (README, Details, License)"

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Generate ZIP filename
ZIP_NAME="SmacAccess.zip"
ZIP_PATH="$OUTPUT_DIR/$ZIP_NAME"

# Remove old ZIP if exists
rm -f "$ZIP_PATH"

# Create ZIP
cd "$STAGING"
if command -v 7z &> /dev/null; then
    7z a -mx9 "$OLDPWD/$ZIP_PATH" . > /dev/null
elif command -v zip &> /dev/null; then
    zip -r -9 "$OLDPWD/$ZIP_PATH" . > /dev/null
else
    echo "ERROR: Neither 7z nor zip found. Install one of them."
    exit 1
fi
cd "$OLDPWD"

# Summary
ZIP_SIZE=$(du -h "$ZIP_PATH" | cut -f1)
echo ""
echo "Done! Created: $ZIP_PATH ($ZIP_SIZE)"
echo ""
echo "Contents:"
echo "  thinker.dll          - SmacAccess mod DLL"
echo "  thinker.exe          - Launcher (start game with this)"
echo "  thinker.ini          - Configuration file"
echo "  modmenu.txt          - Menu definitions"
echo "  Tolk.dll             - Screen reader library"
echo "  nvdaControllerClient32.dll - NVDA support"
echo "  SAAPI32.dll          - SAPI support"
echo "  dolapi32.dll         - JAWS support"
echo "  sr_lang/en.txt       - English screen reader text"
echo "  sr_lang/de.txt       - German screen reader text"
echo "  README.md            - Installation instructions"
echo "  Details.md           - Thinker feature details"
echo "  License.txt          - GPL license"

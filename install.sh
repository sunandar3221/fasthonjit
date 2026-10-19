#!/usr/bin/env bash
set -e

REPO="sunandar3221/fasthonjit"
OS="${OS:-$(uname -s | tr '[:upper:]' '[:lower:]')}"
ARCH="${ARCH:-$(uname -m)}"

case "$ARCH" in
  x86_64|amd64)
    ARCH_TAG="x64"
    ;;
  aarch64|arm64)
    ARCH_TAG="arm64"
    ;;
  armv7*|armv8l|armv6*|armhf|arm)
    ARCH_TAG="armv7"
    ;;
  *)
    echo "Unsupported architecture: $ARCH"
    exit 1
    ;;
esac

IS_TERMUX=0
if [ -n "$PREFIX" ] && [ -d "$PREFIX/bin" ]; then
  IS_TERMUX=1
fi

if [ "$IS_TERMUX" -eq 1 ]; then
  BIN_DIR="$PREFIX/bin"
  if [ "$ARCH_TAG" = "arm64" ]; then
    ASSET_NAME="fasthonjit-android-arm64"
    FALLBACK_ASSET="fasthon-android-arm64"
  elif [ "$ARCH_TAG" = "armv7" ]; then
    ASSET_NAME="fasthonjit-android-armv7"
    FALLBACK_ASSET="fasthon-android-armv7"
  else
    ASSET_NAME="fasthonjit-linux-x64-static"
    FALLBACK_ASSET="fasthon-linux-x64-static"
  fi
else
  BIN_DIR="/usr/local/bin"
  if [ "$OS" = "linux" ]; then
    if [ "$ARCH_TAG" = "x64" ]; then
      ASSET_NAME="fasthonjit-linux-x64"
      FALLBACK_ASSET="fasthon-linux-x64"
    elif [ "$ARCH_TAG" = "arm64" ]; then
      ASSET_NAME="fasthonjit-linux-arm64-static"
      FALLBACK_ASSET="fasthon-linux-arm64-static"
    elif [ "$ARCH_TAG" = "armv7" ]; then
      ASSET_NAME="fasthonjit-linux-armv7-static"
      FALLBACK_ASSET="fasthon-linux-armv7-static"
    fi
  else
    echo "Unsupported operating system: $OS"
    exit 1
  fi
fi

SELECTED_VER=""
if [ -n "$1" ]; then
  SELECTED_VER="$1"
elif [ -n "$FASTHON_VERSION" ]; then
  SELECTED_VER="$FASTHON_VERSION"
elif [ -n "$VERSION" ]; then
  SELECTED_VER="$VERSION"
elif (exec </dev/tty) 2>/dev/null || [ -t 0 ]; then
  echo "=========================================="
  echo "          Pilih Versi FasthonJIT          "
  echo "=========================================="
  echo "  [1] FasthonJIT v2.0.0 (Super Duper Cepat JIT Engine) [Default]"
  echo "  [2] Fasthon v1.0.0 (Versi Lama Tanpa JIT)"
  echo "=========================================="
  USER_CHOICE=""
  if (exec </dev/tty) 2>/dev/null; then
    printf "Masukkan pilihan versi [1/2] (default: 1): " >/dev/tty 2>/dev/null || true
    read -r USER_CHOICE </dev/tty 2>/dev/null || USER_CHOICE="1"
  elif [ -t 0 ]; then
    read -r -p "Masukkan pilihan versi [1/2] (default: 1): " USER_CHOICE 2>/dev/null || USER_CHOICE="1"
  else
    USER_CHOICE="1"
  fi
  case "$USER_CHOICE" in
    2|v1|v1.0.0|"1.0.0")
      SELECTED_VER="v1.0.0"
      ;;
    *)
      SELECTED_VER="v2.0.0"
      ;;
  esac
else
  SELECTED_VER="v2.0.0"
fi

case "$SELECTED_VER" in
  1|v1|"1.0.0"|v1.0.0)
    TAG_NAME="v1.0.0"
    VERSION_DISPLAY="v1.0.0"
    ;;
  2|v2|"2.0.0"|v2.0.0|latest|"")
    TAG_NAME="v2.0.0"
    VERSION_DISPLAY="v2.0.0 (FasthonJIT Ultra Cepat)"
    ;;
  *)
    TAG_NAME="$SELECTED_VER"
    VERSION_DISPLAY="$SELECTED_VER"
    ;;
esac

DOWNLOAD_URL="https://github.com/$REPO/releases/download/$TAG_NAME/$ASSET_NAME"
FALLBACK_URL="https://github.com/$REPO/releases/latest/download/$ASSET_NAME"
OLD_URL="https://github.com/$REPO/releases/download/$TAG_NAME/$FALLBACK_ASSET"

echo "=========================================="
echo "          FasthonJIT Installer            "
echo "=========================================="
echo "Selected Version: $VERSION_DISPLAY"
echo "Detected OS: $OS"
echo "Detected Arch: $ARCH"
echo "Install Target: $BIN_DIR/fasthonjit (with 'fasthon' and 'eas' aliases)"
echo "Downloading $ASSET_NAME ($TAG_NAME)..."

TEMP_BIN="$(mktemp)"
HTTP_OK=0

if command -v curl >/dev/null 2>&1; then
  if curl -sSL --fail "$DOWNLOAD_URL" -o "$TEMP_BIN"; then
    HTTP_OK=1
  elif curl -sSL --fail "$FALLBACK_URL" -o "$TEMP_BIN"; then
    HTTP_OK=1
  elif curl -sSL --fail "$OLD_URL" -o "$TEMP_BIN"; then
    HTTP_OK=1
  fi
elif command -v wget >/dev/null 2>&1; then
  if wget -q "$DOWNLOAD_URL" -O "$TEMP_BIN"; then
    HTTP_OK=1
  elif wget -q "$FALLBACK_URL" -O "$TEMP_BIN"; then
    HTTP_OK=1
  elif wget -q "$OLD_URL" -O "$TEMP_BIN"; then
    HTTP_OK=1
  fi
else
  echo "Error: curl or wget is required to download FasthonJIT."
  exit 1
fi

if [ "$HTTP_OK" -ne 1 ]; then
  echo "Error: Failed to download $ASSET_NAME from GitHub Releases."
  rm -f "$TEMP_BIN"
  exit 1
fi

chmod +x "$TEMP_BIN"

if [ -w "$BIN_DIR" ]; then
  cp "$TEMP_BIN" "$BIN_DIR/fasthonjit"
  cp "$TEMP_BIN" "$BIN_DIR/fasthon"
  mv "$TEMP_BIN" "$BIN_DIR/eas"
  chmod +x "$BIN_DIR/fasthonjit" "$BIN_DIR/fasthon" "$BIN_DIR/eas"
else
  if command -v sudo >/dev/null 2>&1; then
    sudo cp "$TEMP_BIN" "$BIN_DIR/fasthonjit"
    sudo cp "$TEMP_BIN" "$BIN_DIR/fasthon"
    sudo mv "$TEMP_BIN" "$BIN_DIR/eas"
    sudo chmod +x "$BIN_DIR/fasthonjit" "$BIN_DIR/fasthon" "$BIN_DIR/eas"
  else
    mkdir -p "$HOME/.local/bin"
    cp "$TEMP_BIN" "$HOME/.local/bin/fasthonjit"
    cp "$TEMP_BIN" "$HOME/.local/bin/fasthon"
    mv "$TEMP_BIN" "$HOME/.local/bin/eas"
    chmod +x "$HOME/.local/bin/fasthonjit" "$HOME/.local/bin/fasthon" "$HOME/.local/bin/eas"
    BIN_DIR="$HOME/.local/bin"
    echo "Installed to $BIN_DIR/fasthonjit. Make sure $BIN_DIR is in your PATH."
  fi
fi

echo "=========================================="
echo " FasthonJIT successfully installed to $BIN_DIR/fasthonjit"
echo " Run 'fasthonjit' to enter interactive JIT REPL"
echo " Run 'fasthonjit <file.fsn>' to run with JIT speed"
echo " Aliases: 'fasthon' and 'eas' commands are fully supported."
echo "=========================================="

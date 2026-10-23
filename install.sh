#!/usr/bin/env bash
set -e

REPO="sunandar3221/fasthonjit"

# Detect OS and Arch robustly
RAW_OS="$(uname -s 2>/dev/null || echo "$OS")"
RAW_OS="$(echo "$RAW_OS" | tr '[:upper:]' '[:lower:]')"

case "$RAW_OS" in
  linux*)
    OS="linux"
    ;;
  darwin*)
    OS="darwin"
    ;;
  mingw*|msys*|cygwin*|windows*)
    OS="windows"
    ;;
  *)
    OS="$RAW_OS"
    ;;
esac

ARCH="${ARCH:-$(uname -m 2>/dev/null || echo "x86_64")}"

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

# Operating system detection
IS_TERMUX=0
IS_WINDOWS=0

if [ -n "$PREFIX" ] && [ -d "$PREFIX/bin" ]; then
  IS_TERMUX=1
  BIN_DIR="$PREFIX/bin"
elif [ "$OS" = "windows" ]; then
  IS_WINDOWS=1
  BIN_DIR="$HOME/AppData/Local/Microsoft/WindowsApps"
  if [ ! -d "$BIN_DIR" ]; then
    BIN_DIR="$HOME/bin"
    mkdir -p "$BIN_DIR"
  fi
else
  BIN_DIR="/usr/local/bin"
fi

if [ "$IS_TERMUX" -eq 1 ]; then
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
elif [ "$IS_WINDOWS" -eq 1 ]; then
  ASSET_NAME="fasthonjit-windows-x64.exe"
  FALLBACK_ASSET="fasthon-windows-x64.exe"
else
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

# Version selection
SELECTED_VER=""
if [ -n "$1" ]; then
  case "$1" in
    1|v1|v1.0.0|"1.0.0"|legacy)
      SELECTED_VER="v1.0.0"
      ;;
    2|v2|v2.0.0|"2.0.0")
      SELECTED_VER="v2.0.0"
      ;;
    3|v3|v3.0.0|"3.0.0"|latest)
      SELECTED_VER="v3.0.0"
      ;;
    *)
      SELECTED_VER="$1"
      ;;
  esac
elif [ -n "$FASTHON_VERSION" ]; then
  SELECTED_VER="$FASTHON_VERSION"
elif [ -n "$VERSION" ]; then
  SELECTED_VER="$VERSION"
elif (exec </dev/tty) 2>/dev/null || [ -t 0 ]; then
  echo "=========================================="
  echo "          Pilih Versi FasthonJIT          "
  echo "=========================================="
  echo "  [3] FasthonJIT v3.0.0 (Prerelease terbaru) [Default]"
  echo "  [2] FasthonJIT v2.0.0 (Native JIT Engine)"
  echo "  [1] Fasthon v1.0.0 (Versi lama tanpa JIT)"
  echo "=========================================="
  USER_CHOICE=""
  if (exec </dev/tty) 2>/dev/null; then
    printf "Masukkan pilihan versi [3/2/1] (default: 3): " >/dev/tty 2>/dev/null || true
    read -r USER_CHOICE </dev/tty 2>/dev/null || USER_CHOICE="3"
  elif [ -t 0 ]; then
    read -r -p "Masukkan pilihan versi [3/2/1] (default: 3): " USER_CHOICE 2>/dev/null || USER_CHOICE="3"
  else
    USER_CHOICE="3"
  fi
  case "$USER_CHOICE" in
    1|v1|v1.0.0|"1.0.0"|legacy)
      SELECTED_VER="v1.0.0"
      ;;
    2|v2|v2.0.0|"2.0.0")
      SELECTED_VER="v2.0.0"
      ;;
    *)
      SELECTED_VER="v3.0.0"
      ;;
  esac
else
  SELECTED_VER="v3.0.0"
fi

case "$SELECTED_VER" in
  v1|v1.0.0|"1.0.0"|legacy)
    TAG_NAME="v1.0.0"
    VERSION_DISPLAY="v1.0.0 (Fasthon Legacy Tanpa JIT)"
    ;;
  v2|v2.0.0|"2.0.0")
    TAG_NAME="v2.0.0"
    VERSION_DISPLAY="v2.0.0 (FasthonJIT Native Engine)"
    ;;
  v3|v3.0.0|"3.0.0"|latest|"")
    TAG_NAME="v3.0.0"
    VERSION_DISPLAY="v3.0.0 (FasthonJIT prerelease terbaru)"
    ;;
  *)
    TAG_NAME="$SELECTED_VER"
    VERSION_DISPLAY="$SELECTED_VER"
    ;;
esac

echo "=========================================="
echo "          FasthonJIT Installer            "
echo "=========================================="
echo "Versi Dipilih  : $VERSION_DISPLAY"
echo "Sistem Operasi : $OS"
echo "Arsitektur CPU : $ARCH ($ARCH_TAG)"
echo "Target Lokasi  : $BIN_DIR"
echo "Mengunduh berkas binary..."

TEMP_BIN="$(mktemp 2>/dev/null || echo "/tmp/fasthonjit-$$")"
HTTP_OK=0

try_fetch() {
  local url="$1"
  if command -v curl >/dev/null 2>&1; then
    curl -fL --progress-bar "$url" -o "$TEMP_BIN"
    return $?
  elif command -v wget >/dev/null 2>&1; then
    wget --show-progress --progress=bar:force:noscroll "$url" -O "$TEMP_BIN"
    return $?
  fi
  return 1
}

# Candidate download URLs in priority order
URLS=(
  "https://github.com/$REPO/releases/download/$TAG_NAME/$ASSET_NAME"
  "https://github.com/$REPO/releases/download/$TAG_NAME/$FALLBACK_ASSET"
  "https://github.com/sunandar3221/Fasthon/releases/download/$TAG_NAME/$FALLBACK_ASSET"
)

for target_url in "${URLS[@]}"; do
  if try_fetch "$target_url"; then
    if [ -s "$TEMP_BIN" ]; then
      HTTP_OK=1
      break
    fi
  fi
done

if [ "$HTTP_OK" -ne 1 ]; then
  echo "Error: Gagal mengunduh binary dari GitHub Releases (HTTP 404 atau koneksi terputus)."
  echo "Silakan unduh manual dari:"
  echo "https://github.com/$REPO/releases/tag/$TAG_NAME"
  rm -f "$TEMP_BIN"
  exit 1
fi

chmod +x "$TEMP_BIN"

TARGET_MAIN="fasthonjit"
TARGET_ALIAS1="fasthon"
TARGET_ALIAS2="eas"

if [ "$IS_WINDOWS" -eq 1 ]; then
  TARGET_MAIN="fasthonjit.exe"
  TARGET_ALIAS1="fasthon.exe"
  TARGET_ALIAS2="eas.exe"
fi

if [ -w "$BIN_DIR" ]; then
  cp "$TEMP_BIN" "$BIN_DIR/$TARGET_MAIN"
  cp "$TEMP_BIN" "$BIN_DIR/$TARGET_ALIAS1"
  mv "$TEMP_BIN" "$BIN_DIR/$TARGET_ALIAS2"
  chmod +x "$BIN_DIR/$TARGET_MAIN" "$BIN_DIR/$TARGET_ALIAS1" "$BIN_DIR/$TARGET_ALIAS2" 2>/dev/null || true
else
  if command -v sudo >/dev/null 2>&1; then
    sudo cp "$TEMP_BIN" "$BIN_DIR/$TARGET_MAIN"
    sudo cp "$TEMP_BIN" "$BIN_DIR/$TARGET_ALIAS1"
    sudo mv "$TEMP_BIN" "$BIN_DIR/$TARGET_ALIAS2"
    sudo chmod +x "$BIN_DIR/$TARGET_MAIN" "$BIN_DIR/$TARGET_ALIAS1" "$BIN_DIR/$TARGET_ALIAS2"
  else
    mkdir -p "$HOME/.local/bin"
    cp "$TEMP_BIN" "$HOME/.local/bin/$TARGET_MAIN"
    cp "$TEMP_BIN" "$HOME/.local/bin/$TARGET_ALIAS1"
    mv "$TEMP_BIN" "$HOME/.local/bin/$TARGET_ALIAS2"
    chmod +x "$HOME/.local/bin/$TARGET_MAIN" "$HOME/.local/bin/$TARGET_ALIAS1" "$HOME/.local/bin/$TARGET_ALIAS2"
    BIN_DIR="$HOME/.local/bin"
    echo "Dipasang di $BIN_DIR. Pastikan $BIN_DIR ada di PATH Anda."
  fi
fi

echo "=========================================="
echo " FasthonJIT berhasil terpasang di: $BIN_DIR/$TARGET_MAIN"
echo " Perintah untuk REPL interaktif : $TARGET_MAIN"
echo " Perintah untuk eksekusi skrip  : $TARGET_MAIN <file.fsn>"
echo " Alias resmi                    : '$TARGET_ALIAS1' dan '$TARGET_ALIAS2'"
echo " Cek status JIT Engine          : $TARGET_MAIN --jit-info"
echo "=========================================="

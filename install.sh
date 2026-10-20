#!/usr/bin/env bash
# Lorenz installer for Linux distributions.
#
# Builds Lorenz from this source tree and installs it for the current user in
# ~/.local. Administrator access is used only to install distribution packages
# and, on KDE Plasma, the KWin extension. Run ./install.sh --help for options.
set -euo pipefail
cd "$(dirname "$0")"

usage() {
    cat <<'EOF'
Usage: ./install.sh [options]

Without options, the installer asks before each optional step.

  -y, --yes             Accept the recommended answer to every question
      --no-deps         Do not install packages; use what is already installed
      --engines         Install conversion engines (FFmpeg, ImageMagick, …)
      --no-engines      Skip conversion engines
      --office          Install LibreOffice for office, slide and sheet routes
      --no-office       Skip LibreOffice
      --kwin            Install the KDE Plasma drag gesture (KWin extension)
      --no-kwin         Skip the KDE Plasma drag gesture
      --autostart       Start the Lorenz companion at login
      --no-autostart    Do not start Lorenz at login
  -h, --help            Show this help

Remove Lorenz later with ./uninstall.sh.
EOF
}

assume_yes=false deps=ask engines=ask office=ask kwin=ask autostart=ask
while (($#)); do
    case "$1" in
        -y|--yes) assume_yes=true ;;
        --no-deps) deps=no ;;
        --engines) engines=yes ;;
        --no-engines) engines=no ;;
        --office) office=yes ;;
        --no-office) office=no ;;
        --kwin) kwin=yes ;;
        --no-kwin) kwin=no ;;
        --autostart) autostart=yes ;;
        --no-autostart) autostart=no ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

bold=$'\e[1m' green=$'\e[32m' yellow=$'\e[33m' reset=$'\e[0m'
[[ -t 1 ]] || bold='' green='' yellow='' reset=''
step() { printf '\n%s==>%s %s%s%s\n' "$green" "$reset" "$bold" "$*" "$reset"; }
note() { printf '    %s\n' "$*"; }
warn() { printf '%s    ! %s%s\n' "$yellow" "$*" "$reset" >&2; }

# ask VARIABLE_VALUE "Question" DEFAULT(y|n) -> returns 0 for yes.
ask() {
    local preset=$1 question=$2 default=$3 reply
    [[ $preset == yes ]] && return 0
    [[ $preset == no ]] && return 1
    if $assume_yes || [[ ! -t 0 ]]; then [[ $default == y ]]; return; fi
    local hint='[y/N]'; [[ $default == y ]] && hint='[Y/n]'
    while true; do
        read -r -p "    $question $hint " reply || reply=''
        case "${reply,,}" in
            '') [[ $default == y ]]; return ;;
            y|yes) return 0 ;;
            n|no) return 1 ;;
        esac
    done
}

as_root() {
    if [[ $EUID -eq 0 ]]; then "$@"; else sudo "$@"; fi
}

# ---------------------------------------------------------------------------
# Distribution detection and package names
# ---------------------------------------------------------------------------
family=unknown
if command -v pacman >/dev/null; then family=arch
elif command -v apt-get >/dev/null; then family=debian
elif command -v dnf >/dev/null; then family=fedora
elif command -v zypper >/dev/null; then family=suse
elif command -v xbps-install >/dev/null; then family=void
elif command -v apk >/dev/null; then family=alpine
fi

# Each family lists: build tools, conversion engines, LibreOffice, KWin build files.
case $family in
    arch)
        build=(base-devel cmake rust qt6-base pkgconf libnotify)
        engine_pkgs=(ffmpeg imagemagick pandoc-cli poppler qpdf ghostscript libarchive wl-clipboard xclip)
        office_pkgs=(libreoffice-fresh)
        kwin_pkgs=(kwin extra-cmake-modules vulkan-headers qt6-declarative)
        install_cmd=(pacman -S --needed --noconfirm) ;;
    debian)
        build=(build-essential cmake cargo rustc qt6-base-dev pkg-config libnotify-bin curl)
        engine_pkgs=(ffmpeg imagemagick pandoc poppler-utils qpdf ghostscript libarchive-tools wl-clipboard xclip)
        office_pkgs=(libreoffice-writer-nogui libreoffice-calc-nogui libreoffice-impress-nogui)
        kwin_pkgs=(kwin-dev extra-cmake-modules libvulkan-dev qt6-declarative-dev)
        install_cmd=(apt-get install -y) ;;
    fedora)
        build=(gcc-c++ cmake cargo rust qt6-qtbase-devel pkgconf-pkg-config libnotify)
        engine_pkgs=(ffmpeg-free ImageMagick pandoc poppler-utils qpdf ghostscript bsdtar wl-clipboard xclip)
        office_pkgs=(libreoffice-writer libreoffice-calc libreoffice-impress)
        kwin_pkgs=(kwin-devel extra-cmake-modules vulkan-headers qt6-qtdeclarative-devel)
        install_cmd=(dnf install -y) ;;
    suse)
        build=(gcc-c++ cmake cargo rust qt6-widgets-devel pkgconf libnotify-tools)
        engine_pkgs=(ffmpeg-7 ImageMagick pandoc poppler-tools qpdf ghostscript bsdtar wl-clipboard xclip)
        office_pkgs=(libreoffice-writer libreoffice-calc libreoffice-impress)
        kwin_pkgs=(kwin6-devel extra-cmake-modules vulkan-devel qt6-declarative-devel)
        install_cmd=(zypper --non-interactive install) ;;
    void)
        build=(base-devel cmake rust cargo qt6-base-devel pkg-config libnotify)
        engine_pkgs=(ffmpeg ImageMagick pandoc poppler-utils qpdf ghostscript bsdtar wl-clipboard xclip)
        office_pkgs=(libreoffice)
        kwin_pkgs=()
        install_cmd=(xbps-install -Sy) ;;
    alpine)
        build=(build-base cmake cargo rust qt6-qtbase-dev pkgconf libnotify)
        engine_pkgs=(ffmpeg imagemagick pandoc poppler-utils qpdf ghostscript libarchive-tools wl-clipboard xclip)
        office_pkgs=(libreoffice)
        kwin_pkgs=()
        install_cmd=(apk add) ;;
    *)
        build=() engine_pkgs=() office_pkgs=() kwin_pkgs=() install_cmd=() ;;
esac

updated=false
install_packages() {
    ((${#install_cmd[@]})) || return 1
    if [[ $family == debian ]] && ! $updated; then as_root apt-get update -qq; updated=true; fi
    as_root "${install_cmd[@]}" "$@"
}
# Optional packages are installed one by one: a name missing from a release
# (or from enabled repositories) must not block the others.
install_optional() {
    local package failed=()
    for package in "$@"; do install_packages "$package" >/dev/null 2>&1 || failed+=("$package"); done
    ((${#failed[@]} == 0)) || warn "Not installed (unavailable here): ${failed[*]}"
}

step "Lorenz installer"
. /etc/os-release 2>/dev/null || true
note "System: ${PRETTY_NAME:-unknown Linux} · package family: $family"
note "Session: ${XDG_SESSION_TYPE:-unknown} · desktop: ${XDG_CURRENT_DESKTOP:-unknown}"

# ---------------------------------------------------------------------------
# 1. Dependencies
# ---------------------------------------------------------------------------
rust_ok() {
    command -v cargo >/dev/null && command -v rustc >/dev/null || return 1
    local version; version=$(rustc --version | awk '{print $2}')
    # Edition 2024 requires Rust 1.85 or newer.
    [[ "$(printf '%s\n1.85.0\n' "$version" | sort -V | head -n1)" == 1.85.0 ]]
}
qt_ok() { command -v qmake6 >/dev/null || [[ -e /usr/lib/qt6/bin/qmake6 || -e /usr/lib64/qt6/bin/qmake6 ]] || pkg-config --exists Qt6Widgets 2>/dev/null; }

step "Build tools"
if [[ $deps != no ]] && ((${#build[@]})); then
    missing=false
    for tool in cmake c++ pkg-config; do command -v "$tool" >/dev/null || missing=true; done
    rust_ok && qt_ok || missing=true
    if $missing; then
        if ask ask "Install the build tools with your package manager (${build[*]})?" y; then
            install_packages "${build[@]}"
        fi
    else
        note "Already installed."
    fi
elif [[ $family == unknown ]]; then
    warn "Unknown package manager. Install manually: Rust ≥ 1.85, a C++17 compiler, CMake ≥ 3.21, Qt 6 Widgets ≥ 6.4."
fi
if ! rust_ok; then
    warn "Rust 1.85 or newer is required (found: $(rustc --version 2>/dev/null || echo none))."
    if ask ask "Install the current Rust toolchain for your user with rustup (downloads from rust-lang.org)?" y; then
        curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal
        # shellcheck disable=SC1091
        . "$HOME/.cargo/env"
    fi
    rust_ok || { echo "Rust 1.85+ is still missing; aborting." >&2; exit 1; }
fi
for tool in cmake c++; do
    command -v "$tool" >/dev/null || { echo "Missing $tool; install it and run the installer again." >&2; exit 1; }
done

step "Conversion engines"
note "FFmpeg, ImageMagick, Pandoc, Poppler, qpdf, Ghostscript, libarchive, clipboard tools."
note "Missing engines only hide the conversions that need them."
if [[ $deps != no ]] && ((${#engine_pkgs[@]})) && ask "$engines" "Install the conversion engines?" y; then
    install_optional "${engine_pkgs[@]}"
fi
if [[ $deps != no ]] && ((${#office_pkgs[@]})) && ! command -v soffice >/dev/null; then
    note "LibreOffice adds DOCX/ODT → PDF, PowerPoint and spreadsheet conversions (large download)."
    if ask "$office" "Install LibreOffice?" n; then install_optional "${office_pkgs[@]}"; fi
fi
if [[ $family == fedora ]]; then
    note "Fedora's ffmpeg-free lacks some codecs (H.264, AAC). Enable RPM Fusion for full FFmpeg."
fi

# ---------------------------------------------------------------------------
# 2. Build and install for this user
# ---------------------------------------------------------------------------
step "Building Lorenz"
cargo build --release --locked
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build --parallel "$(nproc 2>/dev/null || echo 2)"

step "Installing in ~/.local"
# Stop the tray companion (not running conversions) so the new version replaces it.
companion='.*/lorenz-desktop( --background)?'
pkill -f -x "$companion" 2>/dev/null && was_running=true || was_running=false
cmake --install build --prefix "$HOME/.local" >/dev/null
command -v update-desktop-database >/dev/null && update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
case ":$PATH:" in
    *":$HOME/.local/bin:"*) ;;
    *) warn "$HOME/.local/bin is not in PATH. Add it to use the lorenz command in a terminal." ;;
esac

# ---------------------------------------------------------------------------
# 3. Global drag gesture (KDE Plasma 6.8 on Wayland)
# ---------------------------------------------------------------------------
step "Drag gesture"
kwin_header=/usr/include/kwin/config-kwin.h
kwin_version=$(sed -n 's/^#define KWIN_PLUGIN_VERSION_STRING "\([0-9.]*\)"/\1/p' "$kwin_header" 2>/dev/null || true)
if [[ -z $kwin_version ]] && command -v kwin_wayland >/dev/null; then
    kwin_version=$(kwin_wayland --version 2>/dev/null | awk '{print $2}')
fi
gesture=false
if [[ ${XDG_CURRENT_DESKTOP:-} == *KDE* && $kwin_version == 6.8.* ]]; then
    note "KDE Plasma with KWin $kwin_version: drag a file, then tap Shift, Ctrl or Alt."
    if ask "$kwin" "Install the KWin extension for the drag gesture (needs administrator access)?" y; then
        if [[ $deps != no && ! -f $kwin_header ]] && ((${#kwin_pkgs[@]})); then
            install_packages "${kwin_pkgs[@]}"
        fi
        if [[ ${XDG_SESSION_TYPE:-} == wayland ]]; then
            ./scripts/install-kwin.sh && gesture=true
        else
            warn "Log into a Plasma Wayland session and run ./scripts/install-kwin.sh to enable it."
        fi
    fi
elif [[ ${XDG_CURRENT_DESKTOP:-} == *KDE* ]]; then
    warn "The drag gesture needs KWin 6.8 (found ${kwin_version:-none}). Other features work."
else
    note "The global drag gesture is available on KDE Plasma (KWin 6.8, Wayland) only."
    note "Here, use the file manager context menu (Lorenz → Convert, Tools, Actions),"
    note "or bind a shortcut to: lorenz pick FILE --menu fuzzel|wofi|rofi"
fi

# ---------------------------------------------------------------------------
# 4. Start at login
# ---------------------------------------------------------------------------
step "Start at login"
note "The tray companion gives access to settings and the batch manager."
if ask "$autostart" "Start Lorenz automatically when you log in?" y; then
    "$HOME/.local/bin/lorenz" autostart enable | sed 's/^/    /'
    if [[ ${XDG_CURRENT_DESKTOP:-} =~ (Hyprland|sway|niri|river) ]]; then
        note "Tiling compositors may ignore XDG autostart. Add to your compositor config:"
        note "  exec-once = $HOME/.local/bin/lorenz-desktop --background   (Hyprland)"
        note "  exec $HOME/.local/bin/lorenz-desktop --background          (Sway)"
    fi
    start_now=true
else
    "$HOME/.local/bin/lorenz" autostart disable >/dev/null
    start_now=$was_running
fi
if $start_now && [[ -n ${WAYLAND_DISPLAY:-}${DISPLAY:-} ]]; then
    setsid -f "$HOME/.local/bin/lorenz-desktop" --background >/dev/null 2>&1 < /dev/null || true
fi

step "Done"
"$HOME/.local/bin/lorenz" doctor | sed 's/^/    /'
if $gesture; then
    note "${bold}Drag a file, then tap Shift (convert), Ctrl (tools) or Alt (actions).${reset}"
fi
note "Keys and startup can be changed later: Lorenz tray icon → Settings…"
note "Uninstall: ./uninstall.sh"

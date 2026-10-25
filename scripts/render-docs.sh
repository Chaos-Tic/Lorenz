#!/usr/bin/env bash
# Regenerates docs/media from canned demo data: no real file, window or path is
# ever shown. The wheels, the batch window and the settings dialog are rendered
# by the built desktop (cmake --build build) running head-less
# (QT_QPA_PLATFORM=offscreen), exactly as the CI "wheel-frames" job does, so the
# images are reproducible and do not need a graphical session.
#
# Needs: a built desktop (build/lorenz-wheel-test, build/lorenz-desktop), ffmpeg,
# ImageMagick (magick), Inkscape and librsvg (rsvg-convert).
#
# Art direction (Chaos-Tic Liquid Glass): the same accents everywhere, only the
# surface flips. Each hero visual ships light AND dark. Colours come from
# Chaostic-Skins skins/liquid-glass/tokens.css:
#   ink   #151820 (light) / #f3f4f8 (dark)   muted #4a5060 / #bcc2d0
#   blue  #40baf0 (Convert/Shift)            amber #ffa840 (Tools/Ctrl)
#   violet #ce6cf0 (Actions/Alt)
# Banner text is set in Geist and flattened to vector paths, so it looks the same
# on GitHub without shipping or fetching the font at view time.
set -euo pipefail
cd "$(dirname "$0")/.."
out=docs/media
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

have() { command -v "$1" >/dev/null 2>&1; }
for tool in ffmpeg magick inkscape rsvg-convert; do
    have "$tool" || { echo "render-docs: missing required tool: $tool" >&2; exit 1; }
done

# The desktop must be built first; this script renders, it does not compile.
if [[ ! -x build/lorenz-wheel-test || ! -x build/lorenz-desktop ]]; then
    echo "render-docs: build the desktop first:" >&2
    echo "  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build" >&2
    exit 1
fi

# Geist for the banners, vendored in the repository (OFL-1.1, scripts/fonts/,
# with OFL.txt alongside). It is used only to flatten the banner text to paths:
# nothing is installed on the host, nothing is fetched at render time, and the
# README ships no font. The render is fully offline and reproducible. Set
# GEIST_DIR to render from a different local copy of the same weights.
geist_src=${GEIST_DIR:-scripts/fonts}
fontdir="$work/fonts"
mkdir -p "$fontdir"
for f in Geist-Regular.ttf Geist-SemiBold.ttf GeistMono-Regular.ttf; do
    [[ -f "$geist_src/$f" ]] || { echo "render-docs: missing vendored font $geist_src/$f" >&2; exit 1; }
    cp "$geist_src/$f" "$fontdir/"
done

export QT_QPA_PLATFORM=offscreen

# --- 1. Render the wheels, both themes -------------------------------------
# build/lorenz-wheel-test cycles the three wheels (Convert, then Tools, then
# Actions) into frame-000..frame-119 and also drops clean stills tools.png /
# actions.png. Its backdrop is a demo wallpaper fixture.
for look in light dark; do
    LORENZ_THEME=$look LORENZ_WHEEL_BACKDROP=tests/fixtures/wallpaper-$look.jpg \
        LORENZ_WHEEL_FRAMES="$work/wheels-$look" build/lorenz-wheel-test >/dev/null
done

# --- 2. Render the batch window and settings dialog, both themes -----------
magick -size 1200x800 gradient:'#9ec9f0-#f0c69e' -blur 0x2 "$work/Mountain light.png"
for look in light dark; do
    LORENZ_THEME=$look LORENZ_SCREENSHOT="$work/desktop-ready-$look.png" \
        build/lorenz-desktop --smoke-test >/dev/null 2>&1
    LORENZ_THEME=$look LORENZ_SCREENSHOT="$work/desktop-converted-$look.png" \
        build/lorenz-desktop --test-convert "$work/Mountain light.png" >/dev/null 2>&1
    # The settings dialog stays modal; the screenshot is grabbed before it is
    # dismissed, so a non-zero exit here is expected and ignored.
    LORENZ_THEME=$look LORENZ_SCREENSHOT="$work/settings-$look.png" \
        build/lorenz-desktop --settings >/dev/null 2>&1 || true
done

# --- 3. Hero: the three wheels, light and dark -----------------------------
# The whole function in one loop: open Convert (blue), magnify WebP, switch to
# Tools (amber), then Actions (violet), then dismiss. Every second frame, 600 px
# wide, 16 fps, kept under ~2.5 MB.
build_hero_gif() {
    local look=$1 dest=$2 seq i=0 n
    seq="$work/hero-$look"; mkdir -p "$seq"
    for n in $(seq -w 0 2 119); do
        cp "$work/wheels-$look/frame-${n}.png" "$seq/$(printf '%03d' $i).png"
        i=$((i + 1))
    done
    ffmpeg -loglevel error -y -framerate 16 -i "$seq/%03d.png" \
        -vf "scale=600:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=128:stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=4:diff_mode=rectangle" \
        -loop 0 "$dest"
}
build_hero_gif light "$out/three-wheels.gif"
build_hero_gif dark  "$out/three-wheels-dark.gif"

# --- 4. Reinforcement still: the three wheels side by side -----------------
# Convert (WebP magnified) · Tools (Rotate right) · Actions (Terminal), the three
# real wheels captured frame by frame, composited at 1200 px wide.
for look in light dark; do
    dest="$out/three-wheels.png"; [[ $look == dark ]] && dest="$out/three-wheels-dark.png"
    magick montage "$work/wheels-$look/frame-038.png" "$work/wheels-$look/tools.png" \
        "$work/wheels-$look/actions.png" -tile 3x1 -geometry +8+0 -background none "$work/montage-$look.png"
    magick "$work/montage-$look.png" -resize 1200x -strip -define png:compression-level=9 "$dest"
done

# --- 5. Batch manager slideshow and settings, both themes ------------------
# A two-state slideshow (empty list, then a finished conversion), not a
# real-time recording; each state is held ~2.6 s.
for look in light dark; do
    gif="$out/workflow.gif"; [[ $look == dark ]] && gif="$out/workflow-dark.gif"
    magick -delay 260 -loop 0 "$work/desktop-ready-$look.png" "$work/desktop-converted-$look.png" \
        -resize 760x -layers optimize "$gif"
    png="$out/settings.png"; [[ $look == dark ]] && png="$out/settings-dark.png"
    magick "$work/settings-$look.png" -strip -define png:compression-level=9 "$png"
done

# --- 5b. Per-wheel stills and batch capture for the interaction guide ------
# docs/USER_GUIDE.md shows each wheel on its own and one finished conversion,
# in the light theme. Regenerated here from the same frames so nothing drifts.
magick "$work/wheels-light/frame-038.png" -resize 420x "$out/shift-drag-wheel.png"
magick "$work/wheels-light/tools.png"     -resize 420x "$out/ctrl-drag-tools.png"
magick "$work/wheels-light/actions.png"   -resize 420x "$out/alt-drag-actions.png"
cp "$work/desktop-converted-light.png" "$out/desktop-converted.png"

# --- 6. Banners: vector header, three wheels, both themes ------------------
# Authored with Geist text, then flattened to paths with Inkscape so GitHub
# needs no font. Light = dawn backdrop, dark = dusk; same accents.
emit_banner_src() {
    local look=$1 bg ink ink2 muted sub wheelbody rim sheentop hubgrad \
          capfill capink_b capink_a capink_v orb1 orb2 orb3 orb4 orb5 orbop shadow
    if [[ $look == light ]]; then
        bg='#eef0ff'; ink='#151820'; ink2='#3a4050'; muted='#4a5060'; sub='#3a4050'
        wheelbody='#ffffff'; local bodyop='.32'; rim='#ffffff'; local rimop='.9'
        sheentop='.55'; local dotop='.5'
        hubgrad='<radialGradient id="hub" cx=".4" cy=".35" r=".8"><stop offset="0" stop-color="#fff" stop-opacity=".92"/><stop offset="1" stop-color="#fff" stop-opacity=".55"/></radialGradient>'
        capfill='#ffffff'; local capop='.55'
        capink_b='#0b6690'; capink_a='#9a5a00'; capink_v='#7a2fae'
        orb1='#ffb48f'; orb2='#8fcaff'; orb3='#d6b0ff'; orb4='#e6dcff'; orb5='#fff1c9'; orbop='1'
        shadow='#2a1e5c'; local shadowop='.22'; local hubstroke='#fff'; local hubstrokeop='1'
    else
        bg='#0e1018'; ink='#f3f4f8'; ink2='#d6dae4'; muted='#bcc2d0'; sub='#d6dae4'
        wheelbody='#16162a'; local bodyop='.52'; rim='#ffffff'; local rimop='.42'
        sheentop='.28'; local dotop='.14'
        hubgrad='<radialGradient id="hub" cx=".4" cy=".35" r=".85"><stop offset="0" stop-color="#2b2c44" stop-opacity=".95"/><stop offset="1" stop-color="#1a1b2e" stop-opacity=".8"/></radialGradient>'
        capfill='#1a1b2e'; local capop='.62'
        capink_b='#7cc9f2'; capink_a='#ffc47a'; capink_v='#dfa0f5'
        orb1='#c76a3e'; orb2='#3f7fb8'; orb3='#7e5aa6'; orb4='#4a447a'; orb5='#b89a52'; orbop='.55'
        shadow='#000000'; local shadowop='.42'; local hubstroke='#fff'; local hubstrokeop='.3'
    fi
    local hubb=$capink_b huba=$capink_a hubv=$capink_v
    cat <<SVG
<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="300" viewBox="0 0 1200 300" role="img" aria-labelledby="t d">
<title id="t">Lorenz — A lighter way to convert.</title>
<desc id="d">Three frosted-glass Lorenz wheels: Convert in blue, Tools in amber and Actions in violet.</desc>
<defs>
<clipPath id="card"><rect width="1200" height="300" rx="24"/></clipPath>
<filter id="soft" x="-30%" y="-30%" width="160%" height="160%"><feGaussianBlur stdDeviation="60"/></filter>
<filter id="lift" x="-40%" y="-40%" width="180%" height="180%"><feDropShadow dx="0" dy="14" stdDeviation="18" flood-color="$shadow" flood-opacity="$shadowop"/></filter>
<linearGradient id="sheen" x1="0" y1="0" x2=".6" y2="1"><stop offset="0" stop-color="#fff" stop-opacity="$sheentop"/><stop offset=".5" stop-color="#fff" stop-opacity="0"/></linearGradient>
$hubgrad
</defs>
<g clip-path="url(#card)">
<rect width="1200" height="300" fill="$bg"/>
<g filter="url(#soft)" opacity="$orbop">
<circle cx="700" cy="30" r="150" fill="$orb1"/><circle cx="1090" cy="70" r="180" fill="$orb2"/>
<circle cx="980" cy="330" r="170" fill="$orb3"/><circle cx="560" cy="300" r="170" fill="$orb4"/><circle cx="60" cy="10" r="150" fill="$orb5"/>
</g>
</g>
<g font-family="Geist">
<text x="56" y="66" fill="$muted" font-family="Geist Mono" font-size="12" font-weight="500" letter-spacing="3">AT YOUR CURSOR · IN YOUR FLOW</text>
<text x="52" y="150" fill="$ink" font-family="Geist SemiBold" font-size="66" font-weight="600" letter-spacing="-2">Lorenz</text>
<text x="56" y="194" fill="$sub" font-size="23">A lighter way to convert.</text>
<text x="56" y="250" fill="$muted" font-size="14">Drag a file. Tap Shift, Ctrl or Alt. Release on a choice.</text>
</g>
<g font-family="Geist">
$(emit_wheel 686  '#40baf0' "$hubb" WEBP   15 "$wheelbody" "$bodyop" "$rim" "$rimop" "$dotop" "$hubstroke" "$hubstrokeop")
$(emit_cap   686  '#40baf0' "$capink_b" "$capfill" "$capop" "$ink" Shift Convert)
$(emit_wheel 900  '#ffa840' "$huba" Rotate 13 "$wheelbody" "$bodyop" "$rim" "$rimop" "$dotop" "$hubstroke" "$hubstrokeop")
$(emit_cap   900  '#ffa840' "$capink_a" "$capfill" "$capop" "$ink" Ctrl Tools)
$(emit_wheel 1114 '#ce6cf0' "$hubv" Copy   15 "$wheelbody" "$bodyop" "$rim" "$rimop" "$dotop" "$hubstroke" "$hubstrokeop")
$(emit_cap   1114 '#ce6cf0' "$capink_v" "$capfill" "$capop" "$ink" Alt Actions)
</g>
</svg>
SVG
}
emit_wheel() {
    local cx=$1 accent=$2 hubink=$3 label=$4 fs=$5 body=$6 bodyop=$7 rim=$8 rimop=$9 dotop=${10} hs=${11} hso=${12}
    cat <<WHEEL
<g transform="translate($cx,132)">
<g filter="url(#lift)"><circle r="74" fill="$body" fill-opacity="$bodyop"/><circle r="74" fill="url(#sheen)"/><circle r="73" fill="none" stroke="$rim" stroke-opacity="$rimop" stroke-width="1.2"/><circle r="74" fill="none" stroke="$accent" stroke-opacity=".9" stroke-width="2"/></g>
<circle cx="0" cy="-48" r="13" fill="#fff" fill-opacity="$dotop"/><circle cx="44" cy="-18" r="13" fill="#fff" fill-opacity="$dotop"/><circle cx="44" cy="30" r="13" fill="#fff" fill-opacity="$dotop"/><circle cx="-44" cy="30" r="13" fill="#fff" fill-opacity="$dotop"/><circle cx="-44" cy="-18" r="13" fill="#fff" fill-opacity="$dotop"/>
<circle r="30" fill="url(#hub)" stroke="$hs" stroke-opacity="$hso" stroke-width="1"/>
<text x="0" y="5" text-anchor="middle" fill="$hubink" font-family="Geist SemiBold" font-size="$fs" font-weight="700">$label</text>
</g>
WHEEL
}
emit_cap() {
    local cx=$1 accent=$2 keyink=$3 capfill=$4 capop=$5 ink=$6 key=$7 name=$8
    cat <<CAP
<rect x="$((cx - 58))" y="222" width="116" height="30" rx="15" fill="$capfill" fill-opacity="$capop" stroke="$accent" stroke-opacity=".5"/>
<text x="$cx" y="242" text-anchor="middle" fill="$ink" font-size="13"><tspan font-family="Geist Mono" fill="$keyink" font-weight="600">$key</tspan>  $name</text>
CAP
}
for look in light dark; do
    dest="$out/banner.svg"; [[ $look == dark ]] && dest="$out/banner-dark.svg"
    emit_banner_src "$look" > "$work/banner-src-$look.svg"
    XDG_DATA_HOME="$work" inkscape "$work/banner-src-$look.svg" \
        --export-type=svg --export-plain-svg --export-text-to-path -o "$dest" >/dev/null 2>&1
done

# --- 7. Trim the PNGs ------------------------------------------------------
for f in "$out"/*.png; do magick "$f" -strip -define png:compression-level=9 "$f"; done

echo "render-docs: done"
ls -l "$out"

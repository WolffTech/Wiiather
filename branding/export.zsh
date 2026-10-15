#!/bin/zsh
set -euo pipefail

branding_dir=${0:A:h}
developer_dir=$(/usr/bin/xcode-select -p)
icon_tool="${developer_dir:h}/Applications/Icon Composer.app/Contents/Executables/ictool"
if [[ ! -x "$icon_tool" ]]; then
  print -u2 'Icon Composer CLI was not found in the selected Xcode installation.'
  exit 1
fi

"$icon_tool" "$branding_dir/Wiiather.icon" --export-image \
  --output-file "$branding_dir/wiiather-icon.png" \
  --platform macOS --rendition Default --width 1024 --height 1024 \
  --scale 1 --design-generation 26

uv run --with pillow "$branding_dir/../tools/build_hbc_icon.py"

#!/usr/bin/env bash
# Render every mockup page to dark+light PNGs at the capture size.
set -euo pipefail
cd "$(dirname "$0")/../../docs/design/ux-modernization/mockups"
mkdir -p renders
for f in pages/*.html; do
  name="$(basename "$f" .html)"
  npx --yes playwright screenshot --browser=chromium \
    --viewport-size=1024,768 "file://$PWD/$f" "renders/${name}_dark.png"
  npx --yes playwright screenshot --browser=chromium \
    --viewport-size=1024,768 "file://$PWD/$f?theme=light" "renders/${name}_light.png"
done
echo "Rendered $(ls renders/*.png | wc -l | tr -d ' ') PNGs"

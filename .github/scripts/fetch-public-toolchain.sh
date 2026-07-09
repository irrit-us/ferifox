#!/usr/bin/env bash

set -euo pipefail

label="${1:?toolchain label required}"
dest_dir="${2:?destination directory required}"

mkdir -p "$dest_dir"

echo "Resolving ${label}"

mapfile -t resolved < <(
  TOOLCHAIN_LABEL="$label" env -u MOZCONFIG -u MOZ_OBJDIR ./mach python - <<'PY'
import os

from mozbuild.toolchains import toolchain_task_definitions
from mozbuild.util import find_task_from_index

label = os.environ["TOOLCHAIN_LABEL"]
task = toolchain_task_definitions()[label]

print(find_task_from_index(task["optimization"]["index-search"]))
print(task["attributes"]["toolchain-artifact"])
PY
)

task_id="${resolved[0]}"
artifact_path="${resolved[1]}"
artifact_name="$(basename "$artifact_path")"
artifact_url="https://firefox-ci-tc.services.mozilla.com/api/queue/v1/task/${task_id}/artifacts/${artifact_path}"
archive_path="${dest_dir}/${artifact_name}"

echo "Resolved ${label} to task ${task_id} artifact ${artifact_path}"
echo "Downloading ${artifact_name}"

if command -v aria2c >/dev/null 2>&1; then
  aria2c \
    --allow-overwrite=true \
    --auto-file-renaming=false \
    --console-log-level=warn \
    --dir="$dest_dir" \
    --max-connection-per-server=16 \
    --max-tries=5 \
    --min-split-size=10M \
    --out="$artifact_name" \
    --retry-wait=5 \
    --split=16 \
    --summary-interval=15 \
    "$artifact_url"
else
  curl -fL --retry 3 --retry-all-errors "$artifact_url" -o "$archive_path" &
  curl_pid=$!

  while kill -0 "$curl_pid" 2>/dev/null; do
    if [ -f "$archive_path" ]; then
      size_bytes="$(stat -c%s "$archive_path" 2>/dev/null || echo 0)"
      size_mib="$((size_bytes / 1024 / 1024))"
      echo "Download progress for ${artifact_name}: ${size_mib} MiB"
    else
      echo "Waiting for ${artifact_name} download to start"
    fi
    sleep 15
  done

  wait "$curl_pid"
fi

echo "Extracting ${artifact_name}"
tar --zstd -xf "$archive_path" -C "$dest_dir"
rm -f "$archive_path"
echo "Finished ${label}"

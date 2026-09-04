#!/usr/bin/env bash

set -euo pipefail

label="${1:?toolchain label required}"
dest_dir="${2:?destination directory required}"

mkdir -p "$dest_dir"

echo "Resolving ${label}"

resolve_output="$(mktemp)"
trap 'rm -f "$resolve_output"' EXIT

TOOLCHAIN_LABEL="$label" TASK_ID="${TASK_ID:-github-actions-release}" env -u MOZCONFIG -u MOZ_OBJDIR ./mach python - <<'PY' >"$resolve_output"
import os

from mozbuild.toolchains import toolchain_task_definitions
from mozbuild.util import find_task_from_index

label = os.environ["TOOLCHAIN_LABEL"]
task = toolchain_task_definitions()[label]

print(f"TASK_ID={find_task_from_index(task['optimization']['index-search'])}")
print(f"ARTIFACT_PATH={task['attributes']['toolchain-artifact']}")
PY

mapfile -t resolved <"$resolve_output"
task_id="$(sed -n 's/^TASK_ID=//p' "$resolve_output" | tail -n 1)"
artifact_path="$(sed -n 's/^ARTIFACT_PATH=//p' "$resolve_output" | tail -n 1)"

if [ -z "$task_id" ] || [ "$task_id" = "None" ] || [ -z "$artifact_path" ]; then
  echo "Invalid resolution for ${label}" >&2
  printf '%s\n' "${resolved[@]}" >&2
  exit 1
fi

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

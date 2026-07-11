#!/usr/bin/env bash
set -euo pipefail

# Runs the on-device blur benchmark (com.blur.BlurBenchmark) on Firebase Test
# Lab across a matrix of real physical devices, then tells you where the
# numbers land. This is the honest "phone" measurement — the arm64 CI runner
# is server-class and several times faster than a real handset.
#
# Test Lab is a Google Cloud product: it runs via `gcloud firebase test`, NOT
# the `firebase` CLI (which has no classic instrumentation Test Lab command).
#
# One-time prereqs:
#   1. gcloud CLI installed + authenticated:   gcloud auth login
#   2. A Firebase/GCP project with Test Lab. Physical devices need the Blaze
#      (pay-as-you-go) plan; there is a small free daily device quota.
#          export GCLOUD_PROJECT=your-project-id
#   3. JDK 17 + Android SDK/NDK (ANDROID_HOME) to build the APK.
#
# Then: scripts/testlab-benchmark.sh

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

: "${ANDROID_HOME:=$HOME/Library/Android/sdk}"
export ANDROID_HOME

# --- locate Gradle 8.4 -------------------------------------------------------
# AGP 8.2.0 rejects Gradle 9.x (the current Homebrew default) and the repo has
# no committed wrapper, so pin 8.4 the way CI does. Override with GRADLE_BIN.
GRADLE_BIN="${GRADLE_BIN:-}"
if [[ -z "$GRADLE_BIN" ]]; then
  cache_root="$HOME/.cache/rn-blur-gradle"
  GRADLE_BIN="$cache_root/gradle-8.4/bin/gradle"
  if [[ ! -x "$GRADLE_BIN" ]]; then
    echo "==> Fetching Gradle 8.4 (one-time, into $cache_root)"
    mkdir -p "$cache_root"
    curl -sSL -o "$cache_root/gradle-8.4-bin.zip" \
      https://services.gradle.org/distributions/gradle-8.4-bin.zip
    unzip -q -o "$cache_root/gradle-8.4-bin.zip" -d "$cache_root"
  fi
fi

# --- build the self-instrumenting androidTest APK ----------------------------
echo "==> Building androidTest APK"
"$GRADLE_BIN" -p android :assembleDebugAndroidTest --no-daemon
APK="android/build/outputs/apk/androidTest/debug/react-native-blur-debug-androidTest.apk"
[[ -f "$APK" ]] || { echo "APK not found at $APK"; exit 1; }

# --- run on Test Lab ---------------------------------------------------------
: "${GCLOUD_PROJECT:?set GCLOUD_PROJECT to your Firebase project id}"

command -v gcloud >/dev/null || {
  echo "gcloud not found. Install the Google Cloud CLI and run 'gcloud auth login'."
  echo "(The firebase CLI cannot drive Test Lab instrumentation runs.)"
  exit 1
}

# Device matrix: budget -> mid -> flagship, all arm64 (the NEON path). Model
# IDs and available API levels drift over time (the previous default list —
# redfin/oriole/shiba — was already stale by the time this was tested) — list
# current physical devices before trusting these defaults:
#   gcloud firebase test android models list --filter="form=PHYSICAL"
# Override this whole list by exporting TESTLAB_DEVICES (space-separated flags).
# Verified against the catalog and run successfully as of 2026-07-11.
if [[ -n "${TESTLAB_DEVICES:-}" ]]; then
  read -r -a devices <<< "$TESTLAB_DEVICES"
else
  devices=(
    --device model=a06,version=35       # Galaxy A06     — budget
    --device model=SC-53C,version=36    # Galaxy A53 5G  — mid-range
    --device model=caiman,version=35    # Pixel 9 Pro    — flagship
  )
fi

# Self-instrumenting library test: the androidTest APK contains BOTH the code
# under test and the tests, so it is passed as --app and --test. Confirmed
# working this way (no separate host-app module needed) as of 2026-07-11.
echo "==> Launching Test Lab run on ${GCLOUD_PROJECT}"
gcloud firebase test android run \
  --project "$GCLOUD_PROJECT" \
  --type instrumentation \
  --app "$APK" \
  --test "$APK" \
  --test-targets "class com.blur.BlurBenchmark" \
  --timeout 15m \
  "${devices[@]}"

cat <<'EOF'

==> Done. Each device's numbers are in the captured logcat.
    In the Firebase console (Test Lab) or the GCS results bucket printed above,
    open the logcat artifact and grep for "BlurBench" — one JSON object per
    (size, radius) with cold_ms_median and cachehit_ms_median, plus a BEGIN
    line naming the device/API/ABI.
EOF

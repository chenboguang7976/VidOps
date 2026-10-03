#!/usr/bin/env bash
# Prints VERSION=... for $GITHUB_ENV: the release tag (manual release or v* tag push), else <cmake version>-<short sha>.
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ "${RELEASE_TAG:-}" == v* ]]; then
  echo "VERSION=${RELEASE_TAG#v}"
elif [[ "${GITHUB_REF:-}" == refs/tags/v* ]]; then
  echo "VERSION=${GITHUB_REF_NAME#v}"
else
  base=$(sed -n 's/^ *VERSION \([0-9][0-9.]*\)$/\1/p' CMakeLists.txt | head -n1)
  sha=${GITHUB_SHA:-$(git rev-parse HEAD)}
  echo "VERSION=${base}-${sha:0:7}"
fi

#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")/.."
TOKEN="$(cat build/gh_token.txt)"
git remote remove origin 2>/dev/null || true
git remote add origin "https://x-access-token:${TOKEN}@github.com/ardaerdem1890-ship-it/minimal-desktop.git"
git push -u origin main

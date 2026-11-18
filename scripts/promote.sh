#!/usr/bin/env bash
# Fast-forwards main to a feature-base commit whose own push run of the CI workflow passed (D-053).
# Usage: scripts/promote.sh [--check] [commit]   (default commit: origin/feature-base)
set -euo pipefail
cd "$(dirname "$0")/.."
check=false
sha=''
while (($#)); do
  case "$1" in
    --check) check=true ;;
    -*) echo "Unknown argument: $1" >&2; exit 2 ;;
    *) sha="$1" ;;
  esac
  shift
done
git fetch --quiet origin main feature-base
sha="$(git rev-parse --verify "${sha:-origin/feature-base}^{commit}")"
fail() { echo "promote: $sha: $*" >&2; exit 1; }
if [[ "$sha" == "$(git rev-parse origin/main)" ]]; then
  echo "promote: main is already at $sha"
  exit 0
fi
git merge-base --is-ancestor origin/main "$sha" ||
  fail "does not descend from origin/main, so main cannot fast-forward; integrate on feature-base"
git merge-base --is-ancestor "$sha" origin/feature-base || fail "not on origin/feature-base"
repo="$(gh repo view --json nameWithOwner --jq .nameWithOwner)"
# This exact commit's feature-base push run: not an ancestor's, a PR candidate's or a manual run's.
runs="$(gh api "repos/$repo/actions/workflows/ci.yml/runs?head_sha=$sha&event=push&branch=feature-base" \
  --jq '.workflow_runs[] | "\(.id) \(.status) \(.conclusion)"')"
grep -q ' completed success$' <<<"$runs" ||
  fail "no completed, successful full gate of its own; runs: ${runs:-none}"
if "$check"; then
  echo "promote: $sha can fast-forward main"
  exit 0
fi
git push origin "$sha:refs/heads/main"

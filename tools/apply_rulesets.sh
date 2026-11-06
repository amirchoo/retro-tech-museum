#!/usr/bin/env bash
# Creates or updates the branch rulesets in .github/rulesets/*.json on GitHub.
# Needs: gh CLI logged in as a repo admin, and GitHub Pro (or a public repo).
#   bash tools/apply_rulesets.sh
set -euo pipefail
cd "$(dirname "$0")/.."
REPO=$(gh repo view --json nameWithOwner --jq .nameWithOwner)

for f in .github/rulesets/*.json; do
  name=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["name"])' "$f")
  id=$(gh api "repos/$REPO/rulesets" --jq ".[] | select(.name == \"$name\") | .id" || true)
  if [ -n "$id" ]; then
    gh api -X PUT "repos/$REPO/rulesets/$id" --input "$f" --jq '"updated: " + .name'
  else
    gh api -X POST "repos/$REPO/rulesets" --input "$f" --jq '"created: " + .name'
  fi
done

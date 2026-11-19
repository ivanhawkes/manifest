#!/usr/bin/env bash
set -euo pipefail

# Writes the engine's definition records, one file per definition, from the authored YAML. Resolve
# the output before changing directory; CMake passes an absolute build path.
definitions_dir="${1:-$(dirname "$0")/definitions}"
mkdir -p "$definitions_dir"
definitions_dir="$(cd "$definitions_dir" && pwd)"
cd "$(dirname "$0")"

# The files that define the game: the ages, the categories, every item, every
# node of the skill tree and the hulls. Every one of them is a complete YAML document that yq
# loads on its own, and the files refer to each other by id - the categoryId and
# ageId of an item names a category and an age, the skillId and itemId of a
# game-mode file name a node and a good - and never by anchors, so any yq v4 builds
# the records. The references are resolved, and checked, by the content loader.
DEFINITIONS=(
  base/ages.yaml
  trade-items/categories.yaml
  trade-items/chemicals.yaml
  trade-items/metals.yaml
  trade-items/precious-metals.yaml
  trade-items/minerals.yaml
  trade-items/seeds.yaml
  trade-items/grains.yaml
  trade-items/life-support.yaml
  trade-items/food-and-drink.yaml
  trade-items/fuels.yaml
  trade-items/crafts.yaml
  trade-items/valuables.yaml
  trade-items/writing-and-knowledge.yaml
  trade-items/equipment.yaml
  trade-items/construction.yaml
  trade-items/animal-products.yaml
  trade-items/medical.yaml
  trade-items/household-goods.yaml
  trade-items/humans.yaml
  trade-items/textiles.yaml
  trade-items/polymers.yaml
  trade-items/technology.yaml
  trade-items/arms.yaml
  trade-items/fertilisers.yaml
  trade-items/drugs.yaml
  skill-tree/skills.yaml
  skill-tree/ages.yaml
  skill-tree/spaceship.yaml
  skill-tree/shipyards.yaml
  skill-tree/lanes.yaml
  skill-tree/survey.yaml
  skill-tree/habitats.yaml
  skill-tree/piracy.yaml
  skill-tree/law.yaml
  skill-tree/trade.yaml
  skill-tree/services.yaml
  ships/hulls.yaml
)

# Every document is built in a temporary directory and moved into place at the end, so a failed
# build leaves the previous records rather than a partial set for the next build to read.
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

# The definition records: one file per definition, in the form Jettison's DefinitionRegistry
# loads, named by its ID in kebab case. The rule is RegistryName's in
# source/game/content/src/Definitions.cpp, which the loader holds every record to, so a name this
# writes differently is refused there rather than resolved by filename
# (docs/stage-2-content-plan.md, "Records").
kebab='sub("([a-z0-9])([A-Z])"; "${1}-${2}") | sub("([A-Z])([A-Z][a-z])"; "${1}-${2}") | downcase'

# Legacy authoring writes null for an absent optional field. The engine's reader refuses null, so
# these are left out and read as their defaults; a null in any other field is an error, here.
optional_fields='^(description|targetEffectID|exclusiveGroupId)$'

records_dir="$tmp_dir/definitions"

# write_records <directory> <id field> <expression> <files...>: one record per mapping the
# expression yields from the files.
write_records() {
  local directory="$1" id="$2" expression="$3" nulls collisions
  shift 3

  nulls=$(yq e -N "$expression | . as \$record | to_entries[]
      | select(.value == null and (.key | test(\"$optional_fields\") | not))
      | filename + \": \" + \$record.$id + \" has a null \" + .key" "$@")
  if [[ -n "$nulls" ]]; then
    printf 'build.sh: a required field is null:\n%s\n' "$nulls" >&2
    exit 1
  fi

  # yq's split writes each record to its name, so two IDs with one name would leave one record.
  collisions=$(yq e -N "$expression | .$id | $kebab" "$@" | sort | uniq -d)
  if [[ -n "$collisions" ]]; then
    printf 'build.sh: two %s share a record name:\n%s\n' "$directory" "$collisions" >&2
    exit 1
  fi

  # The price is the text as authored, quoted so that no reader takes it for a float.
  mkdir -p "$records_dir/$directory"
  local file sources=()
  for file in "$@"; do
    sources+=("$PWD/$file")
  done
  (cd "$records_dir/$directory" && yq e -N "$expression
      | {\"version\": 1} + with_entries(select(.value != null))
      | with(select(has(\"basePrice\")); .basePrice |= (tostring | . style=\"double\"))
      | ... comments=\"\"" -s ".$id | $kebab" "${sources[@]}")
  for file in "$records_dir/$directory"/*.yml; do
    mv "$file" "${file%.yml}.yaml"
  done
}

write_records ages ageId '.ages[].age' base/ages.yaml
write_records categories categoryId '.categories[].category' trade-items/categories.yaml
item_files=() skill_files=()
for file in "${DEFINITIONS[@]}"; do
  case "$file" in
    trade-items/categories.yaml | base/*) ;;
    trade-items/*) item_files+=("$file") ;;
    skill-tree/*) skill_files+=("$file") ;;
  esac
done
write_records items itemId '.["item-base"][].item' "${item_files[@]}"
write_records skills skillId '.["skill-base"][].skill' "${skill_files[@]}"
write_records hulls hullId '.["hull-base"][].hull' ships/hulls.yaml
write_records name-pools pool 'to_entries[] | {"pool": .key, "names": .value}' \
  universe/solar-system-names.yaml

# Jettison's ship and station definitions (D-062) are authored as the engine's own records, one
# file each under docks/, and written as they are with their comments dropped. A dock hull is named
# by the Manifest.Hull it belongs to.
for kind in pads:pads hulls:dock-hulls stations:stations; do
  mkdir -p "$records_dir/${kind#*:}"
  for record in "docks/${kind%%:*}"/*.yaml; do
    yq e '... comments=""' "$record" > "$records_dir/${kind#*:}/$(basename "$record")"
  done
done

# A game mode is named by its file. The registry lists definitions sorted by name, so the mode
# carries the age order itself, taken from base/ages.yaml, beside its ordered selections.
mkdir -p "$records_dir/game-modes"
for mode in game-mode/*.yaml; do
  MODE_FILE="$mode" yq e -n '
    load("base/ages.yaml") as $ages
    | load(strenv(MODE_FILE)) as $mode
    | {"version": 1,
       "ages": [$ages.ages[].age | {"ageId": .ageId}],
       "items": $mode.items,
       "skills": $mode.skills}
    | ... comments=""
  ' > "$records_dir/game-modes/$(basename "$mode")"
done

# The balance inputs: one record, named by its file.
mkdir -p "$records_dir/tuning"
for tuning in tuning/*.yaml; do
  yq e '{"version": 1} + . | ... comments=""' "$tuning" > "$records_dir/tuning/$(basename "$tuning")"
done

# Replaced whole, so a record whose source was removed does not survive from an earlier build.
rm -rf "$definitions_dir"
mv "$records_dir" "$definitions_dir"

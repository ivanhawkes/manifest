#!/usr/bin/env bash
set -euo pipefail

# Resolve the outputs before changing directory; CMake passes absolute build paths. The first is
# the aggregate documents the legacy loaders read, the second the engine's definition records.
output_dir="${1:-$(dirname "$0")/core-config}"
definitions_dir="${2:-$(dirname "$0")/definitions}"
mkdir -p "$output_dir" "$definitions_dir"
output_dir="$(cd "$output_dir" && pwd)"
definitions_dir="$(cd "$definitions_dir" && pwd)"
cd "$(dirname "$0")"

# The files that define the game: the ages, the categories, every item and every
# node of the skill tree. Every one of them is a complete YAML document that yq
# loads on its own, and the files refer to each other by id - the categoryId and
# ageId of an item names a category and an age, the skillId and itemId of a
# game-mode file name a node and a good - and never by anchors. Nothing has to
# survive the trip from one file to the next, so any yq v4 builds the game.
DEFINITIONS=(
  base/ages.yaml
  trade-items/categories.yaml
  trade-items/chemicals.yaml
  trade-items/metals.yaml
  trade-items/minerals.yaml
  trade-items/misc.yaml
  trade-items/seeds.yaml
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
)

# The lookup the game-mode files select from, built from the definitions in the
# order above:
#
#   .ages         the ages, in age order
#   .categories   the categories
#   .items        a map from itemId to the item definition
#   .skills       a map from skillId to the skill definition
#
# The items and the skills are maps, not lists, because a game-mode file names a
# definition by id: *itemGold in the old merge is Gold in the game-mode file
# and $defs.items["Gold"] here. The *-base keys are read here and never copied to
# the output, so every key of a generated file appears exactly once.
#
# Note: yq's + appends to a list and merges two maps, so += adds one file's
# entries to those of the files before it. The accumulator starts as {}: that is
# the only form yq accumulates across the whole stream, a non-empty initial value
# is restarted for every document, which leaves a game with the last file in it.
registry_expression='
  . as $doc ireduce ({};
      .ages       += ($doc.ages // [])
    | .categories += ($doc.categories // [])
    | .items      += (($doc["item-base"]  // []) | map(.item)  | map({"key": .itemId,  "value": .}) | from_entries)
    | .skills     += (($doc["skill-base"] // []) | map(.skill) | map({"key": .skillId, "value": .}) | from_entries))
'

# Merge one game-mode file with the lookup: every skillId and itemId of the file
# becomes the definition it names, in the - skill: and - item: shape the loader
# reads. The definitions come out in the order the game-mode file lists them, which
# is how the sections of the file group the goods of the document.
#
# An id with no definition resolves to null, so the references are checked before
# the merge: see check_references.
game_mode() {
  local selection="$1" output="$2"
  REGISTRY_FILE="$registry_file" SELECTION_FILE="$selection" yq -n '
    load(strenv(REGISTRY_FILE)) as $defs
    | load(strenv(SELECTION_FILE)) as $sel
    | {"ages": $defs.ages, "categories": $defs.categories}
    | .skills = [ $sel.skills[] | .skillId as $id | {"skill": $defs.skills[$id]} ]
    | .items  = [ $sel.items[]  | .itemId  as $id | {"item":  $defs.items[$id]} ]
  ' > "$output"
}

# Every id a game-mode file references must name a definition. The list of goods
# and nodes of a mode is typed by hand, so a name with no definition behind it is
# the failure this pipeline invites, and yq answers it with a null that the loader
# reads as an entry with no fields. The pair of id lists is compared here, so the
# build stops with the names instead of shipping a game with a hole in it.
check_references() {
  local selection="$1" wanted defined missing
  wanted=$(yq '.skills[].skillId, .items[].itemId' "$selection" | sort)
  defined=$(yq '.skills | keys[], .items | keys[]' "$registry_file" | sort)
  missing=$(comm -13 <(printf '%s\n' "$defined") <(printf '%s\n' "$wanted"))
  if [[ -n "$missing" ]]; then
    printf 'build.sh: %s references ids with no definition:\n%s\n' "$selection" "$missing" >&2
    exit 1
  fi
}

# Two definitions with the same itemId, or two with the same skillId: the map keeps
# the one read last, so the second quietly shadows the first, and the game loses one
# item, one skill. The ids of every file are counted here, so the shadowing names
# itself at the build.
check_unique_ids() {
  local duplicate
  duplicate=$(
    for file in "${DEFINITIONS[@]}"; do
      yq '.["item-base"][]?.item.itemId, .["skill-base"][]?.skill.skillId' "$file"
    done | sort | uniq -d
  )
  if [[ -n "$duplicate" ]]; then
    printf 'build.sh: two definitions share an id:\n%s\n' "$duplicate" >&2
    exit 1
  fi
}

# Every document is built in a temporary directory and moved into place at the
# end. A direct redirect would leave an empty full.yaml behind, newer than its
# inputs, for the next build to read as a complete game.
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

# The definitions are concatenated once for the lookup. The blank line between two
# files keeps the documents apart: a file that ends without a newline would
# otherwise weld its last entry onto the next file's first.
for file in "${DEFINITIONS[@]}"; do
  cat "$file"
  printf '\n'
done > "$tmp_dir/definitions.yaml"

registry_file="$tmp_dir/registry.yaml"
yq ea "$registry_expression" "$tmp_dir/definitions.yaml" > "$registry_file"

check_unique_ids
check_references game-mode/full.yaml
check_references game-mode/simple.yaml

# The whole game: every item and every node of the tree.
game_mode game-mode/full.yaml "$tmp_dir/full.yaml"

# The subset of items used by the simple version of the game, for quick matches
# and testing.
game_mode game-mode/simple.yaml "$tmp_dir/simple.yaml"

# The ages and the categories on their own, for the loaders that read them alone.
yq -n 'load("base/ages.yaml")' > "$tmp_dir/ages.yaml"
yq -n 'load("trade-items/categories.yaml")' > "$tmp_dir/categories.yaml"

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
write_records name-pools pool 'to_entries[] | {"pool": .key, "names": .value}' \
  universe/solar-system-names.yaml

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

# The directories are generated and not committed, so a clean clone has none.
mkdir -p "$output_dir"

mv "$tmp_dir/full.yaml" "$tmp_dir/simple.yaml" "$tmp_dir/ages.yaml" "$tmp_dir/categories.yaml" "$output_dir/"

# Replaced whole, so a record whose source was removed does not survive from an earlier build.
rm -rf "$definitions_dir"
mv "$records_dir" "$definitions_dir"

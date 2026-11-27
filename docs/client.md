# The graphical client

The graphical client, `manifest_client`, is how most players will play
(L15a, [history](history/tasks.md)). It is Jettison's rendered [shell](../../jettison/docs/glossary.md#shell)
with Manifest composed into it: the same `Host::Game` the server and the tests start, and the same
commands and queries as the [terminal UI](console.md).

## How it works

### Composition

`Manifest::Host::MakeGame(scenario)` makes Manifest's `Host::Game`, the one `Host::Start` composes
for the server. The client hands it to `Jettison::App::RunRenderApplication`, and the shell starts
it through a session of its own ([Jettison's J30](../../jettison/docs/history/neutral-sdk.md#j30-shell-session)).
The client keeps a reference to the game, so `Host::HostedServices(game)` reaches the world's
identities and requests wherever the session is held.

The shell mounts the engine's assets first, as `engine`, then Manifest's content as `base`, then
the user's mods and the user's directory. The Manifest package is therefore the first source that is
not the engine's, and the content phase checks it as it checks the server's first source.

`--scenario <name>` is the client's own and is taken off the command line before the shell reads
the rest; with none it plays `trading`. Every other argument is the shell's
([the app shell](../../jettison/docs/app-shell.md)): `--content` names other content, and
`--offscreen`, `--screenshot` and `--user-dir` work as they do for the engine's programs.

### Screens

The screens are RmlUi documents in the package, `ui/manifest/<screen>.rml` over the engine's
`ui/base.rcss` and `ui/manifest/manifest.rcss`, read by the engine's UI
([Jettison's game UI](../../jettison/docs/ui.md#the-game-ui)). Their words are in
`strings/en/manifest.yaml`, under `manifest.`; English is the base every language is read over.
`Manifest::Client::ClientGame` (`source/apps/client/ClientGame.h`), the client's `App::Game`, makes
the data model `manifest`, binds the model's `Screen` to it as `screen`, and binds each action as
an event, which calls the model.

| Screen | What it shows and does |
| --- | --- |
| `start` | The player's name and their company's; *Start trading* provisions them. Shown until the player has a company |
| `company` | The company's credits, its ships, where each is and what it carries, its goods in each warehouse, and its resting orders, each with *Cancel* |
| `market` | The player's report of the market where the ship is docked: each good's best bid and ask, the totes held there and aboard; *Buy* and *Sell* a tote at the ask or the bid, *Load* all held aboard, *Unload* all aboard ashore |
| `travel` | The destinations, with *Go*; the thrust slider, in percent of the cruise, and the acceleration in g, in tenths ([D-107](decision-ledger.md#d-107)) |
| `flight` | A panel beside the screen while a journey is under way: its destination and leg, the reaction mass left, the day, and fast-forward on or off |
| `service` | The life support's oxygen and scrubbers and the fuel; refill the tanks or replace the scrubbers where it is docked, and refuel on docking or not |

Every screen but the start shows the day and the last request's answer, in the player's words. The
navigation buttons change screen, and Escape goes back to the company's. A frame gives the journey's
fast-forward 4 ms (`ClientModel::FastForward`), then takes the shell's snapshots again
(`Shell::Resync`).

For a capture or a scripted run, which has no mouse, `screen <name>` shows a screen and
`press <element>` clicks an element of the screen shown or the flight panel, as console lines:

```bash
manifest_client --offscreen --run "@5 press provision" --run "@30 screen market" \
    --run "@40 press buy-0" --screenshot market.png --after-frames 60
```

### Languages

The words are in English, French, German and Japanese, every key in all four:
`strings/<tag>/manifest.yaml` for the screens, read over English. Jettison ships its own text, the saves screen, the number format
and its reasons, in English and German only, so Manifest ships French and Japanese tables of it too
(`saves.yaml`, `numbers.yaml`, `reasons.yaml`). The player's `ui.language` setting, or the console's
`language <tag>`, chooses one ([Jettison's language](../../jettison/docs/ui.md#the-language)).

Where a language orders a sentence its own way, the key holds the screen's data expression rather
than the document, so `manifest.company.docked` is `Docked at {{ship.dockedAt}}` in English and
`{{ship.dockedAt}}に停泊中` in Japanese. Units and symbols (kg, g, %, /) stay in the documents.

The translations are first drafts. They stress the layout now, with German's long words, French's
diacritics and Japanese kanji, and native translators replace them before release
([scope](scope.md)). A new screen adds its keys to all four tables as it is built; `client_strings`
fails on any it lacks. The [terminal UI](console.md) prints English outside the tables, as the
engine's developer text does ([D-121](decision-ledger.md#d-121)).

### The model

`Manifest::Client::ClientModel` (`source/apps/client`, the `client_model` module) is the player's
side of a local world with nothing of the UI in it, so its suite plays the first loop with no window.
It holds the player's principal and makes a `Screen` from `Query::Observe` as that player's actor:
the company and its credits, its ships, holdings and resting orders, the market where the ship is
docked, and the venues the player has a report of, as destinations. Its actions are the console's
payloads, each admitted through `Command::Requests::Admit` with the player's next sequence:
provisioning (as the trusted harness, as the console's host provisions), a tote bought at the best
ask or sold at the best bid, an order cancelled, a good's totes loaded or unloaded, a journey, the life
support's refill and replacement, and refuelling. It keeps the last request's answer, or that one is
still to come; an action with nothing to act on, such as a purchase with no ship docked, is refused
before anything is sent.

The screens act on the company's first ship, the only one the first loop has. A journey's thrust is
set in percent of its cruise and its acceleration in tenths of g ([D-107](decision-ledger.md#d-107)),
kept within 0.1 g to 3 g; the world refuses more than the hull sustains.

**Fast-forward.** While a journey is under way, `ClientModel::FastForward(budget)` steps the world on
with `SimWorld::FastForward`, its quiet steps passed over, for at most the budget and no further than
the journey's end. The client calls it once a frame, so a journey's days pass in seconds and the loop
never waits on them.

### Saves

The shell's saves (F5, F9 and its saves screen) hold the world, and the client adds its section,
`Manifest.Client`: the player and the next journey's settings (`ClientState`), a binary archive at
version 1, with the player as the save's `manifest.player` detail. A save with no such section is
not the client's and is refused before anything of the world is replaced. The model is made again
from the loaded world and the section once the load is done.

### The scene

The package's `scene/main.yaml` holds a camera and a lamp, which the shell draws behind the screens
until the stations and ships are drawn ([roadmap](roadmap.md#the-graphical-client)). The engine's
render test needs the lamp: it checks that a frame's light clusters list a light.

### Checks

| Suite | What it checks |
| --- | --- |
| `client_model` | The first loop through the model: provisioned at Ore, two totes of furs bought and loaded, flown to Brine with fast-forward, unloaded and sold for more; the journey's bounds, a refusal made before sending, and the save's section restored |
| `client_screens` | Every screen loaded over a started world with nothing RmlUi or the string table counts as a problem, the start giving way to the company's, the navigation, and the flight panel shown under way; then every screen again in French, German and Japanese, every translation taken |
| `client_strings` | Every key English has, the engine's and the client's, in French, German and Japanese and no key beyond them; each text naming English's placeholders and data expressions, and a key with plural forms holding those its language's rule needs (Japanese only `other`). A failure names the language and the key |
| `client_smoke` | The engine's render test sequence played by the client offscreen on lavapipe (`client-smoke.sh`): its save and load carry the client's section, and its game UI stage provisions the player and buys a tote at the market's first ask through the screens |

## Using it

```bash
build/clang-debug/source/apps/client/manifest_client               # the trading scenario
build/clang-debug/source/apps/client/manifest_client --scenario governance
```

It is built in every preset with the engine's presentation (`MANIFEST_BUILD_CLIENT`, off in
`clang-notui`).

## Limits and open work

- The content mounts as `base`, not `manifest`, so an overlay that `requires: manifest` does not
  mount in the client.
- It is not in the [bundle](glossary.md#bundle) yet, and plays only a local world, not a server's.
- Goods, ships and venues are shown by their IDs, not their names.
- Japanese draws as empty boxes: the engine's UI fonts have no CJK glyphs, and no check counts a
  glyph missing ([Jettison's J15](../../jettison/docs/roadmap.md#j15)).
- What is still to come is listed in [the graphical client](roadmap.md#the-graphical-client).

## Source

- `source/apps/client/main.cpp`: the program; `ClientGame.h`, its screens on the shell.
- `mods/manifest/ui/manifest/` and `mods/manifest/scene/main.yaml`: the documents and the scene.
- `source/apps/client/tests/ScreensTests.cpp`, `StringsTests.cpp` and `client-smoke.sh`: the screens
  check, the languages' check and the smoke run.
- `mods/manifest/strings/<tag>/`: the four languages' tables.
- `source/apps/client/include/manifest/client/ClientModel.h`: the model; its suite is
  `tests/ClientModelTests.cpp`.
- `source/game/host/src/Start.cpp`: `MakeGame` and `HostedServices`.

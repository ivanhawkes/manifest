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
the rest. With neither it nor `--connect`, the client opens on the [server picker](#choosing-a-server),
where the player plays `trading` here or connects to a server they saved. Every other argument is the shell's
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
| `start` | The player's name, their company's and their ship's; *Start trading* provisions them. Shown until the player has a company |
| `company` | The company's credits, its ships by their names, where each is and what it carries, its goods in each warehouse, and its resting orders, each with *Cancel* |
| `market` | The player's report of the market where the ship is docked: each good's best bid and ask, the totes held there and aboard; *Buy* and *Sell* a tote at the ask or the bid, *Load* all held aboard, *Unload* all aboard ashore |
| `travel` | The destinations, with *Go*; the thrust slider, in percent of the cruise, and the acceleration in g, in tenths ([D-107](decision-ledger.md#d-107)) |
| `flight` | A panel beside the screen while a journey is under way: the ship's name, its destination and leg, the reaction mass left, the day, and fast-forward on or off |
| `service` | The life support's oxygen and scrubbers and the fuel; refill the tanks or replace the scrubbers where it is docked, and refuel on docking or not |
| `servers` | The [server picker](#choosing-a-server): the servers the player saved, by name and endpoint, each with *Connect* and *Remove*; a server saved by its name, host:port, key and the player's token; *Play here alone*. Shown first when the command line names no server and no scenario |

Every screen but the start and the server picker shows the day and the last request's answer, in the player's words. The
navigation buttons change screen, and Escape goes back to the company's. A frame gives the journey's
fast-forward 4 ms (`ClientModel::FastForward`), then takes the shell's snapshots again
(`Shell::Resync`).

For a capture or a scripted run, which has no mouse, `screen <name>` shows a screen,
`press <element>` clicks an element of the screen shown or the flight panel, and `read <element>`
logs what an element of the screen shown holds (`market held-0: 1`), as console lines. `found
<company name> <ship name>` names the player's company and its ship and starts them as the start
screen does (double quotes keep spaces), locally or on a server:

```bash
manifest_client --scenario trading --offscreen --run "@5 press provision" --run "@30 screen market" \
    --run "@40 press buy-0" --screenshot market.png --after-frames 60
```

### Languages

The words are in English, French, German and Japanese, every key in all four:
`strings/<tag>/manifest.yaml` for the screens, read over English. Jettison ships its own text, the
saves screen, the number format and its reasons (`saves.yaml`, `numbers.yaml`, `reasons.yaml`), in the
same four languages, so Manifest carries none of it. The player's `ui.language` setting, or the console's
`language <tag>`, chooses one ([Jettison's language](../../jettison/docs/ui.md#the-language)).

Where a language orders a sentence its own way, the key holds the screen's data expression rather
than the document, so `manifest.company.docked` is `Docked at {{ship.dockedAt}}` in English and
`{{ship.dockedAt}}に停泊中` in Japanese. Units and symbols (kg, g, %, /) stay in the documents.

**Numbers and the time of day** are the language's, written by the documents' transforms
([Jettison: numbers, dates and times](../../jettison/docs/ui.md#string-tables)): prices with two
digits after the point (`{{good.ask | decimal(2)}}`, 103.97 in English, 103,97 in German), the
acceleration with one (`decimal(1)`), credits and kilograms grouped (`thousands`), and the time of
day from the model's minutes since midnight (`{{screen.minutes | time}}`, 3:05 PM in English, 15:05
in German). The model holds numbers, never their text. The game keeps no calendar, so the day is a
count from 1 and no date is written.

**The Japanese font.** The engine's faces have no Japanese, so the package ships Noto Sans JP,
Noto's own published subset (`Sans/SubsetOTF/JP/NotoSansJP-Regular.otf` of
[notofonts/noto-cjk](https://github.com/notofonts/noto-cjk), tag `Sans2.004`), as `fonts/NotoSansJP-Regular.otf`:
the shell loads every font under a game's `fonts/` as a fallback face after Droid Sans, so a
character no other face has is drawn from it ([Jettison's fonts](../../jettison/docs/ui.md#the-game-ui)).
It is fetched at configure time by `source/external/CMakeLists.txt`, by URL and SHA-256, with its
licence, and staged into the mod by `manifest_mod`: no font is committed. The licence is the SIL Open
Font License 1.1, from the repository's own `LICENSE`; it travels with the pack, as
`share/doc/manifest/notosansjp/LICENSE` in the [bundle](host.md#the-distribution). `client_screens`
fails on a character no face has, which the UI logs and counts as a problem.

The translations are first drafts. They stress the layout now, with German's long words, French's
diacritics and Japanese kanji, and native translators replace them before release
([scope](scope.md)). A new screen adds its keys to all four tables as it is built; `client_strings`
fails on any it lacks. The [terminal UI](console.md) prints English outside the tables, as the
engine's developer text does ([D-121](decision-ledger.md#d-121)).

### The model

`Manifest::Client::ClientModel` (`source/apps/client`, the `client_model` module) is the player's
side of a local world or a server's, with nothing of the UI in it, so its suite plays the first loop
with no window. One class serves both; below is the local world, and [server play](#server-play)
the server's.
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

### Server play

`--connect <host:port>` and `--token-file <file>` play on a [dedicated server](server.md) as the
token's actor. They are the client's own, taken off the command line as `--scenario` is, which is
then the server's and refused beside them. The token is read from the file, never an argument, so
it is in no process list. The key is pinned from the [server list](server.md#connections-are-sealed)
(`Host::DefaultServers`) as the console's `connect` pins it, and an endpoint the list does not name
is refused before anything is sent. Connected, the client hands the shell no `Host::Game`: the
shell runs its own world, physics alone, the content still mounted, and the server steps the game's.

The model is made over the `Remote::Client`. `Refresh` polls it, takes the newest snapshot and the
reply answering the request it waits on; each action is the same payload, sent with `Retry` under the
model's own next sequence rather than given to `Requests::Admit`, as the welcome's actor;
`FastForward` takes nothing. The payload types are
registered by the model, since no world of the game's registers them. An action before the welcome
or after the closing is refused `NotAuthorised` before it is sent.

As its UI opens, before the connection is first polled, the client gives it the world content of its
shell's file system (`Host::DescribeWorldContent(shell.Assets())`): the package and the player's
mods that hold world content. A server whose world content differs is refused
([the content check](server.md#the-content-check)), and a player's cosmetic mod, holding none, does
not count.

The player starts as they do locally, from the start screen or the console's `found`: a principal's
token sends `provision` for itself alone ([D-122](decision-ledger.md#d-122)), the player's name
being the token's, and the ship's name the player typed, which every snapshot carries
([D-123](decision-ledger.md#d-123)). While connected:

- the saves refuse, saying the world is the server's: `WriteSave` and `ReadSave` throw, so nothing
  is saved or loaded;
- the flight panel hides its fast-forward toggle (`Screen::connected`);
- a closed connection shows why in the status line, in the player's words
  (`manifest.closed.<reason>`, one key for each of Jettison's `Net::Reason`s); a server refused for
  its content shows `manifest.closed.content`, `{0}` in every language, the package
  `Host::ContentDifference` names in Jettison's English words.

A snapshot comes at most once a second (`kSnapshotInterval`), so the status line may say a request
is done before the screen shows it; the screen catches up at the next snapshot. On lavapipe the
first after the founding was seen to take about four seconds.

**Reconnecting.** The model keeps each request it sends until it hears it complete or refused, and
numbers them itself from the first welcome's sequence, as the [terminal
UI](console.md#connected-to-a-server) does. A connection lost after a welcome, closed, timed out or
the server shutting down (`Remote::Redials`, which the console shares), is dialled again with the
same token, its key pinned from the server list again, no more often than once a second
(`kRedialInterval`), until it is back. Meanwhile the status line says so
(`manifest.reconnecting`), and an action is kept, not sent. Once welcomed again, the model sends
what it kept, oldest first, under the sequences it first sent them with. The server's [request
ledger](glossary.md#request-ledger) answers a sequence it admitted as that request, so nothing runs
twice ([D-125](decision-ledger.md#d-125), [D-126](decision-ledger.md#d-126)). A refused request is
not sent again. A refused connection (its credential, key, protocol or content), a full server or a
message the server closes on is not dialled again: the status line says why, and an action is
refused `NotAuthorised`, as after any closing. So is a dial that cannot be made, such as one to an
endpoint the server list no longer names, which the log reports.

```bash
manifest-token --tokens tokens.yaml issue principal Ada > ada.token    # the server's operator
manifest_client --connect 127.0.0.1:47720 --token-file ada.token --run '@2s found "Ada Freight" "Long Shot"'
```

### Choosing a server

Started with no `--connect` and no `--scenario`, the client opens on the server picker (`servers`).
Its model is `Manifest::Client::ServerPicker` (`client_model`), with nothing of the UI in it. It lists
the servers of the player's [server list](server.md#connections-are-sealed),
`$XDG_CONFIG_HOME/manifest/servers.yaml` (`Host::PlayerServers`), that hold a name and a token beside
their pinned key ([D-127](decision-ledger.md#d-127)): the console's format with two fields added, so a
list without them reads as it did.

- *Save* adds a server, or replaces what the list holds of its endpoint: its name, its `host:port`,
  its public key as its operator gave it, and the player's token. With no key, the list must pin one
  for the endpoint already, as the build's may (`share/manifest/servers.yaml`,
  `Host::ShippedServers`); the first save copies the build's list into the player's, so its pins
  stay. The list is written whole and durably (`Remote::WriteServers`), its owner's alone (0600) from
  the moment it is made, in a directory made private (0700) if it is new.
- *Remove* takes the server's name and token out of the list and keeps its pinned key.
- *Connect* dials the server as `--connect` does (`Client::Dial`), its key pinned from the list as
  the console pins it, with the token the list holds. The picker stays until the server's welcome; the
  start screen follows, or the company's once the player has one, and server play goes on as above, a
  lost connection dialled again with the token the list holds then. A connection closed before its
  welcome, for its credential, key, content or anything else, is dropped, and the picker says why in
  the closing's words (`manifest.closed.<reason>`).
- *Play here alone* plays the `trading` world the shell started.

What the picker refuses before anything is sent its status line says in the player's words
(`manifest.servers.refused.<reason>`): no name, an endpoint that is not `host:port`, no token, no key
pinned and none given, a pinned key that is not one, a host that does not resolve, and a list that
cannot be read or written. The log has the details, the list's path among them, and never a token. The
token is typed into a password field, never shown after, and kept in no save, crash report or log:
only the list holds it.

A shell's world is fixed as it starts, so a server picked leaves the local world the shell started
going on unplayed, and the saves refuse while connected, as with `--connect`. Once a server is
connected, the picker is not shown again in that run.

### The scene

The package's `scene/main.yaml` holds a camera and a lamp, which the shell draws behind the screens
until the stations and ships are drawn ([roadmap](roadmap.md#the-graphical-client)). The engine's
render test needs the lamp: it checks that a frame's light clusters list a light. The client changes
no scene, so nothing waits on `Shell::SceneReady` yet; a jump's effect will hold on it while the new
system's models load ([roadmap](roadmap.md#the-graphical-client)).

### Checks

| Suite | What it checks |
| --- | --- |
| `client_model` | The first loop through the model: provisioned at Ore, a nameless ship refused and then the ship named, two totes of furs bought and loaded, flown to Brine with fast-forward, unloaded and sold for more; the journey's bounds, a refusal made before sending, and the save's section restored. Then over an in-process server: nothing sent before the welcome, the company founded as the token's principal with a Japanese ship's name seen in the snapshot, a tote bought and seen held, and the server's stop shown as `Shutdown`; a server whose world content is not the client's (the compressed copy's) refused, naming the package, with nothing sent; and over a server restarted on its port, a refused request dropped and not sent again, a buy whose completion was lost sent again and run once (held once, paid once, the ledger at the next request), one made while reconnecting sent once back, and a server that no longer takes the token not dialled again (`BadCredential`). The server picker: a server saved with its token, the player's list made theirs alone (0600, in 0700 directories) with the build's pins kept, renamed, removed with its pin kept, and each refusal made before anything is written; connected to an in-process server as the token's player, a token the server does not take closed `BadCredential`, and an endpoint not saved, a host that does not resolve and a key that is not one refused; no token in the log |
| `client_screens` | Every screen loaded over a started world with nothing RmlUi or the string table counts as a problem, the start giving way to the company's, which shows the ship by its name, the navigation, and the flight panel shown under way with the ship's name, the acceleration, a price and the time of day in English's and German's forms; then every screen again in French, German and Japanese, every translation taken. The server picker shown first, listing a saved server by its name and endpoint, a save with nothing typed refused in English, every language's words taken, and *Play here alone* giving way to the start screen |
| `client_strings` | Every key English has, the engine's (read from its assets alone, too) and the client's, in French, German and Japanese and no key beyond them; each text naming English's placeholders and data expressions, and a key with plural forms holding those its language's rule needs (Japanese only `other`). The engine's `numbers.*` forms are its own, each language's (a 24-hour clock names no am or pm), and the engine's ui suite checks them. A failure names the language and the key |
| `client_smoke` | The engine's render test sequence played by the client offscreen on lavapipe (`client-smoke.sh`): its save and load carry the client's section, and its game UI stage provisions the player and buys a tote at the market's first ask through the screens |
| `client_serve` | Server play end to end (`client-serve.sh`): `manifest --serve` on a new trading world and an ephemeral port, the client offscreen on lavapipe founding "Ada Freight" and its ship "Long Shot" with `found`, reading the ship's name on the company screen, buying a tote with `press buy-0` and reading it held; the server stopped with SIGINT and started again, a second run, given no server, lists the server saved in the player's list with their token, connects to it from the server picker, and reads the ship's name and the tote still held; no log holds the token |

## Using it

```bash
build/clang-debug/source/apps/client/manifest_client               # the server picker
build/clang-debug/source/apps/client/manifest_client --scenario governance
build/clang-debug/source/apps/client/manifest_client --connect 127.0.0.1:47720 --token-file ada.token
```

It is built in every preset with the engine's presentation (`MANIFEST_BUILD_CLIENT`, off in
`clang-notui`).

## Limits and open work

- The content mounts as `base`, not `manifest`, so an overlay that `requires: manifest` does not
  mount in the client.
- It is not in the [bundle](glossary.md#bundle) yet.
- Connected from the server picker, the local world the shell started goes on unplayed, and the picker
  is not shown again in that run.
- Goods and venues are shown by their IDs, not their names.
- The Japanese font is one weight, Regular.
- What is still to come is listed in [the graphical client](roadmap.md#the-graphical-client).

## Source

- `source/apps/client/main.cpp`: the program, `--connect` and `--token-file` among its options;
  `ClientGame.h`, its screens on the shell.
- `mods/manifest/ui/manifest/` and `mods/manifest/scene/main.yaml`: the documents and the scene.
- `source/apps/client/tests/ScreensTests.cpp`, `StringsTests.cpp`, `client-smoke.sh` and
  `client-serve.sh`: the screens check, the languages' check, the smoke run and server play.
- `mods/manifest/strings/<tag>/`: the four languages' tables.
- `source/apps/client/include/manifest/client/ClientModel.h`: the model; its suite is
  `tests/ClientModelTests.cpp`.
- `source/apps/client/include/manifest/client/ServerPicker.h` and `mods/manifest/ui/manifest/servers.rml`:
  the server picker; its tests are `tests/ServerPickerTests.cpp`, in `client_model`.
- `source/game/host/src/Start.cpp`: `MakeGame` and `HostedServices`.

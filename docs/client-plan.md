# The graphical client: open plans

The designs of the [graphical client](client.md)'s queued work. Each section is one task of the
[task queue](delivery-order.md); a section goes when its task is done, its record in
[history](history.md). Everything else still to come for the client is in the
[roadmap](roadmap.md#the-graphical-client).

## L52: server play, the tracer

A thin slice through every layer, end to end: one player, in the graphical client, on a dedicated
server, provisioned, buying a tote and seeing it held, and the world keeping it across a restart.
It proves the client, the protocol, the server, the journal and the screens together before any of
them is widened. One player is enough; the server already serves several
([N03](history/stage-4.md#n03-several-clients-reconnects-and-bounds--2026-11-20)).

### What it stands on

Every capability is built; the work is Manifest's composition of them.

| The client needs | What serves it | Gap |
| --- | --- | --- |
| A sealed connection to a server, its key pinned | `Remote::Client`, `Remote::ReadServers` and `PinnedKey`, `Host::DefaultServers`, as the console's `connect` uses them | None |
| Its actions sent, and their answers | `Remote::Client::Submit`, and `Poll`'s replies: the receipt and the `Completed` outcome | None |
| What the screens show | `Remote::Client::Latest`, the actor's `Query::Snapshot`, the same `Query::Observe` makes locally; `ClientModel::MakeScreen` reads the snapshot alone | None |
| The shell with no world of the game's | `App::RunRenderApplication` with no `Host::Game`: the shell's own world, physics alone, the content still mounted | None expected; the first step confirms the screens open so |
| No local saves while connected | `App::Game::WriteSave` and `ReadSave` throwing, which leaves nothing saved or loaded | None |
| The player's company on a server | `Company::Provision`, an administrator's alone today ([D-080](decision-ledger.md#d-080)) | A game rule, selected as [D-122](decision-ledger.md#d-122): Manifest's |

### Provisioning on a server

Locally the client provisions the player as the trusted harness does. On a server only an
administrator's token may send `provision` today, so a player's client cannot start. Every player
starts with a company they name ([D-122](decision-ledger.md#d-122)): the server admits `provision`
from a principal's token for that principal alone, the principal taken from the token, never the
payload, and refused `AlreadyProvisioned` once it has a company; an administrator still provisions
anyone. The start screen names the company on a server as it does locally, the player's name being
the token's. For the tracer the client's console names it too, as the game starts:
`found <company name>` (double quotes keep spaces), the line a `--run` gives, so the test needs no
typing into a field.

### The design

- **The model, two ways to the world.** `ClientModel` is made over a local world, as now, or over a
  `Remote::Client`: one class, not an interface and two implementations. Over a connection,
  `Refresh` polls the client and takes the newest snapshot and the reply answering the request it
  waits on; each action is the same payload, given to `Submit` rather than `Requests::Admit`;
  `FastForward` takes nothing, since the server steps its own world. The principal is the
  welcome's.
- **The program.** `--connect <host:port>` and `--token-file <file>`, the client's own, taken
  off the command line as `--scenario` is. The token is read from a file, never an argument, so it
  is in no process list. The key is pinned from the server list as the console pins it. Connected,
  the client hands the shell no `Host::Game`.
- **The screens.** The flight panel's fast-forward toggle is hidden while connected, and a closed
  connection shows why in the status line (`Closed()`, in the player's words). The saves refuse,
  saying the world is the server's.
- **Waiting.** A snapshot comes at most once a second (`kSnapshotInterval`), so a screen may show
  an answer before the snapshot that holds it; the status line says the request is done, and the
  screen catches up at the next snapshot. Nothing is retried.

### The steps

1. The server admits a principal's own `provision` (D-122), with `company` cases: its own once,
   refused a second time and for another principal.
2. `ClientModel` over a `Remote::Client`, and a `client_model` case that plays the slice over an
   in-process `Remote::Server`, as the console's tests serve one.
3. `ClientGame` and `main`: `--connect` and `--token-file`, no local world, the `found` console
   command, the saves refused, the toggle hidden, the closing shown.
4. `client_serve`, a script beside `client-smoke.sh` and `serve-smoke.sh`: a key, a token and a
   server list written in a temporary directory, `manifest --serve` on a new trading world and an
   ephemeral port, the client offscreen on lavapipe driven by `--run` lines (`found "Ada Freight"`,
   `screen market`, `press buy-0`, `screen company`) and a screenshot. The server is stopped with
   SIGINT and started again, and a second run shows the tote held. It reads what the screen holds,
   not pixels: the client logs the screen's state at the last `--run` line, or a console command
   prints it.
5. The documents: the client's server play in [the graphical client](client.md), its line in
   [the server](server.md), provisioning in [companies](companies.md), D-122 implemented, and the
   history row.

### The checks

`company`, `client_model`, `client_screens`, `client_smoke`, `client_serve`, `remote`, `console` and
`cli_serve` in `clang-debug`; then the gate on `feature-base` once merged. No world-layout
change is expected, so no hash pin should move.

### After the tracer

Each its own row when taken up, in this order unless the user reorders them:

- **The content check.** The welcome carries the server's world content
  (`Host::EncodeWorldContent`, Jettison's J63), and the client, comparing it with its own, refuses
  a server whose world it cannot read, naming the package (`Host::ContentDifference`).
- **Reconnecting.** A dropped connection made again, and the request whose completion never came
  sent again under its sequence (`Remote::Client::Retry`), as [L41](delivery-order.md#l41) asks of
  the TUI.
- **Choosing a server in the client.** A screen of the server list, the endpoint and the token, the
  token kept private in the user's directory.
- **The bundle.** The client shipped beside the TUI.

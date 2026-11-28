# The graphical client: open plans

The designs of the [graphical client](client.md)'s queued work. Each section is one task of the
[task queue](delivery-order.md); a section goes when its task is done, its record in
[history](history.md). Everything else still to come for the client is in the
[roadmap](roadmap.md#the-graphical-client).

## After the tracer

Server play's tracer is done (L52, [server play](client.md#server-play)). What follows it, each its
own row when taken up, in this order unless the user reorders them:

- **Reconnecting.** A dropped connection made again, and the request whose completion never came
  sent again under its sequence (`Remote::Client::Retry`), as the TUI does
  ([L41](history/tasks.md#l41), [D-125](decision-ledger.md#d-125)).
- **Choosing a server in the client.** A screen of the server list, the endpoint and the token, the
  token kept private in the user's directory.
- **The bundle.** The client shipped beside the TUI.

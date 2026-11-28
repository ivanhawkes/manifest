# The graphical client: open plans

The designs of the [graphical client](client.md)'s queued work. Each section is one task of the
[task queue](delivery-order.md); a section goes when its task is done, its record in
[history](history.md). Everything else still to come for the client is in the
[roadmap](roadmap.md#the-graphical-client).

## After the tracer

Server play's tracer is done (L52, [server play](client.md#server-play)). What follows it, each its
own row when taken up, in this order unless the user reorders them:

- **The content check** ([L53](delivery-order.md#l53)). The welcome carries the server's world content
  (`Host::EncodeWorldContent`, Jettison's J63), and the client, comparing it with its own, refuses
  a server whose world it cannot read, naming the package (`Host::ContentDifference`).
- **Reconnecting.** A dropped connection made again, and the request whose completion never came
  sent again under its sequence (`Remote::Client::Retry`), as [L41](delivery-order.md#l41) asks of
  the TUI.
- **Choosing a server in the client.** A screen of the server list, the endpoint and the token, the
  token kept private in the user's directory.
- **The bundle.** The client shipped beside the TUI.

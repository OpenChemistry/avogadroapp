# avogadro/rpc

`AvogadroRPC`, a small static library (Qt Core and Network only) adapted from
MoleQueue. It carries JSON-RPC 2.0 messages over a local socket
(`QLocalServer`/`QLocalSocket`): `JsonRpc` parses and validates incoming
packets, `Message` builds requests and replies, and
`LocalSocketConnection[Listener]` move the bytes.

This directory is only the transport. What the methods **do** is decided in
[../rpclistener.cpp](../rpclistener.cpp) and `MainWindow::handleCommand()`,
and by the plugins. See the top-level [AGENTS.md](../../AGENTS.md) for the
rules that apply everywhere.

## The wire format is a public contract

Each packet is a `QDataStream` (`Qt_4_8`) `QByteArray`: a 4-byte big-endian
length followed by UTF-8 JSON. Clients outside this repository implement the
same framing, notably `avogadro.connect` in
`avogadrolibs/python/avogadro/connect.py` and the test harness in
`tests/rpc/`. Do not change the framing, the socket naming, or the shape of
replies without changing those clients and bumping `rpcProtocolVersion` in
`rpclistener.cpp`. Adding a new method does not need a bump.

## Where a new command goes

1. **A scriptable action on a molecule** (select atoms, measure, optimize,
   add hydrogens, run a calculation) is a **plugin command**. Declare it with
   `QtGui::CommandRegistry` in the plugin in avogadrolibs: add a
   `CommandSpec` through `commandRegistry()` and forward `handleCommand()` to
   `commandRegistry().handle()`. The registry validates and types the
   options, rejects unknown ones, and describes the command through
   `commandSchema()`. `MainWindow` discovers it from `registerCommands()`, so
   no app change is needed. When adding a command to a plugin that still
   parses its options by hand, prefer migrating that plugin to the registry
   first.
2. **A built-in** belongs here only when it concerns application state that
   no plugin owns: the list of open molecules, layers, views, the camera,
   tools and display types, undo and redo, files, versions. For a new one:
   - add a row to `builtinCommands` in `rpclistener.cpp`, kept sorted, with a
     description and the right `async` flag;
   - answer it in one place: `RpcListener::messageReceived()` for read-backs,
     or `MainWindow::handleCommand()` for anything that changes the document;
   - add a test in `tests/rpc/test_builtins.py` (one verb and its errors) or
     a scenario under `tests/rpc/scenarios/` (several steps).

## Conventions for replies

- Read-backs (`listMolecules`, `getCamera`, `activeDialog`, ...) reply with
  their payload directly as `result`.
- Commands reply `true` straight away, or, when the request passed
  `"wait": true`, `{"status": "finished"}` once the work is done, with
  `message` and `data` added only when they are not empty. A plugin that
  finishes later returns `CommandStatus::Started` and emits its result
  through the plugin's command signals. Commands answered in
  `rpclistener.cpp` itself (`openFile`, `loadMolecule`, `saveGraphic`) finish
  before replying and reply `true` even with `"wait": true`.
- `wait` and `timeout` are reserved request parameters, removed before any
  handler sees the options. Do not use them as option names.
- Reuse the error codes defined at the top of `rpclistener.cpp`: -1 request
  failed, -2 command failed or timed out, -3 plugin busy, -32601 method not
  found. Give a specific `message` that says what was wrong.
- Requests that read or write molecule files are held until the Open Babel
  formats are registered (`needsPluginFormats()`); add a new file method to
  that list.

## Security

Anything that can open the socket can drive Avogadro, so treat every request
as hostile input.

- **Local sockets only.** Do not add a TCP, HTTP or WebSocket listener, or any
  way for a remote host to reach the RPC server.
- **No arbitrary execution.** Do not add a method that runs shell commands,
  loads a plugin or script from a path given in the request, or evaluates
  code. Plugins that run external programs do so with arguments they build
  themselves.
- Validate every parameter's type and range before use, and refuse what does
  not fit: indices must be whole JSON numbers (`"0"` and `true` are refused),
  and a missing parameter is an error, not a default that does something
  surprising.
- Never block on the user. A request must not open a modal dialog; under
  `--skip-dialogs` (implied by `--rpc-name`) fail with an error reply instead.
- Do not log request parameters or replies. They can contain whole molecules
  and file paths, and `qDebug`/`qWarning` output ends up in crash-report
  breadcrumbs.
- `kill` always refuses: there is no testing mode that enables it. Keep
  shutdown out of reach of ordinary clients; if a testing mode is added, gate
  `kill` on the flag `main()` parsed, never on a scan of the raw arguments
  (which would match an option's value, such as `--rpc-name --testing`).

## Dataflow tracing playbook

Messy repos hide their I/O. This playbook gives the search order that finds it anyway.

### Start from binaries and manifests

Open the build file first: `CMakeLists.txt`, `package.json`, `Cargo.toml`, `pyproject.toml`. Note the target names and the entry files. Then open the entry files. In this workspace the build entry is `tools/build.ps1` and the CMake root is `CMakeLists.txt:14` for the JUCE path. Record equivalents for the repo under review.

### Entry patterns

Search in this order and record `path:line` for each hit:

1. Process entry: `main(`, `JUCEApplication`, `Cli::`, `argparse`, `commander`, `clap::Parser`.
2. Host entry: `processBlock`, `VST3`, `CLAP`, `AudioProcessor`, request handlers, `app.get(`, `app.post(`.
3. File and env input: `fopen`, `ifstream`, `ReadFile`, `std::filesystem`, `getenv`, `GetEnvironmentVariable`, config loaders.
4. Queue and IPC: `SPSC`, `FIFO`, `pipe`, `socket`, `recv`, `mmap`.

Read around each hit for the data type. The type decides the edge label later. A handler without a payload type is a TODO, not a traced entry.

### Exit patterns

Search for writes with the same care:

1. Audio and UI out: `processBlock` writes, `draw`, `repaint`, `setValue`, telemetry posts.
2. File out: `fwrite`, `ofstream`, `WriteFile`, artifact writers.
3. Network out: `send`, `fetch(`, `HttpClient`, `publish`.
4. Host return: return codes, thrown errors that cross an ABI, callback invocations.

Typed errors count as exits. In this workspace `E_BAD_MZ`, `E_BAD_PE`, `E_TRUNCATED`, `E_OOB`, `E_TOO_MANY_SECTIONS` are exits back to the caller. List them with their return sites.

### Taint walk

Pick one entry. Follow the variable by name through at most 3 hops. Rename at each hop is common in messy code. Use grep on the variable name, then on the assigned name. Stop at the sink and record the chain. Long chains belong in an L1 diagram. Short chains stay in L0.

Drop logging, metrics, and test-only branches from the chain. Fold retry loops into one edge labeled `retry`.

### Flux record

Each traced path becomes one JSON object in `assets/flux.json`:

```json
{
  "id": "pe-file-to-grain",
  "kind": "file-read",
  "path": "src/opcoda_core/pe/parser.cpp",
  "line": 88,
  "data": "pe_bytes: vector<uint8_t>",
  "to": "entropy_table"
}
```

Fields `path`, `line`, `data`, and `to` are required. A record without them stays out of the diagrams.

### Stuck cases

When the entry point is generated code, a macro, or a framework callback, note the generator and the registration site. Example: JUCE processor registration, route decorators. The registration line is the traceable source. Do not guess the framework internals.

# Data store

The web module's persistence is a `dataStore` backend behind the
`database`/`collection`/`model` facade (config `"backend"`):
* `"file"` — the default: each database is a directory, each collection a
  subdirectory, and each document a JSON file named by `"_id"`. Filters
  support equality and `{"$in", [...]}`; writes are atomic (temp+rename).
  Point it at a directory with `{"path", "./data"}`.

This replaces the MongoDB driver entirely (no server, no driver), keeping
the same document-oriented API.

# Open Issues

## Search behavior

Current search behavior is intentional for now, but may need revision later.

- `GET /search` uses normal FTS search, not prefix search.
- Prefix-like queries such as `de` do not match `dec` unless the query itself is changed to a prefix form.
- Incremental/prefix behavior currently belongs to `GET /search/live`.

## Text file search scope

Uploaded `text/*` files are currently searchable by filename, not by file body.

- In SQLite FTS, text files are indexed through `original_filename`.
- Their file contents are stored as uploaded files and are not inserted into `content_text`.
- This means a text file like `.bashrc` matches filename-based queries, but not body text queries.

## Future options

Possible future directions if search UX needs improvement:

- Add optional prefix search support to CLI `find`
- Add a dedicated `--prefix` flag for CLI `find`
- Expand indexing so uploaded text files can also be searched by body content
- Revisit the boundary between `/search` and `/search/live`


# Third-Party Notices

Karing does not vendor third-party source code in this repository.

The CLI and server may link against system or package-manager provided
libraries at build or runtime:

- Drogon and Trantor for the HTTP server.
- SQLite for local database storage.
- jsoncpp for JSON parsing and serialization.
- libcurl for the CLI HTTP backend.

Container images and CI workflows use third-party base images and actions:

- `drogonframework/drogon` for container builds and Linux CI jobs.
- `debian:bookworm-slim` for the runtime container image.
- GitHub Actions maintained by GitHub and Docker.

Those components remain under their respective licenses. Package managers,
container registries, and operating system distributions provide their own
license metadata for the exact versions used.

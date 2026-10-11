# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- `packages/stcc4.yaml`: the STCC4 with Home Assistant-ready entities,
  configurable through `stcc4_*` substitutions.
- ESP32, ESP32-C3 and ESP32-S3 examples on ESP-IDF that between them use
  every component option, sharing `examples/common/base.yaml` (encrypted
  API and OTA, fallback hotspot, safe mode button, esphome-lib
  diagnostics and I2C scan).
- Docker Compose `validate`, `build`, `lint` and `dashboard` services;
  pre-commit hooks; Renovate; CI that builds every example with caching and
  a weekly ESPHome beta build; Lint and Release workflows.
- `docs/configuration.md`, `docs/overview.md`, `AGENTS.md`, `SECURITY.md`
  and the datasheet's license file.

### Changed

- Followed [esphome-ai-coding-rules](https://github.com/romkey/esphome-ai-coding-rules):
  repo layout, workflows and docs.
- `CONFIGURATION.md` moved to `docs/configuration.md`.
- The CRC test is a pytest in `tests/`.

### Fixed

- The `stcc4` text sensor platform failed validation because it depended on
  a nonexistent `stcc4` component.
- Compiler warnings on ESP-IDF: `printf` formats for `uint32_t`, and an
  unsequenced `pos++` when building I2C command buffers.
- The README said the STCC4 supports up to 400 kHz I2C; the datasheet lists
  Fast-mode Plus (1 MHz).

### Removed

- The old examples (`example*.yaml`, `local-example.yaml`), `.ci/`, and
  `docker-compose.yml`, replaced by the above.
- The `reference/` submodule entry, which had no `.gitmodules`.

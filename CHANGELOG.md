# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

<!--
## [Unreleased]
-->

## [v2024.0.5] - 2025-06-03
### Fixed
* The `usb1` backend can now check for an error return when a verb signature has no return value(s)
* Pass missing endpoint number for `clearHalt()` in `usb1` backend
### Changed
* Dropped support for Python 3.8
* Modernize older parts of the codebase for Python 3.0 (thank you @claui!)
  - Replace `native_str` invocation with `str`
  - Replace `raise_from` with with native `raise ... from`
  - Replace `raise_with_traceback` with builtin method
  - Remove now unused dependency on `future`


## [v2024.0.3] - 2024-12-18
### Fixed
* Serial packets were framed incorrectly when using the GreatFET UART interface with parity set to one of: ODD, EVEN or PARITY_STUCK_AT_ONE


## [v2024.0.2] - 2024-08-19
### Added
* Windows support for Cynthion.


[Unreleased]: https://github.com/greatscottgadgets/libgreat/compare/v2024.0.5...HEAD
[v2024.0.5]: https://github.com/greatscottgadgets/libgreat/compare/v2024.0.3...v2024.0.5
[v2024.0.3]: https://github.com/greatscottgadgets/libgreat/compare/v2024.0.2...v2024.0.3
[v2024.0.2]: https://github.com/greatscottgadgets/libgreat/compare/v2024.0.1...v2024.0.2
[v2024.0.1]: https://github.com/greatscottgadgets/libgreat/compare/v2024.0.0...v2024.0.1
[v2024.0.0]: https://github.com/greatscottgadgets/libgreat/releases/tag/v2024.0.0

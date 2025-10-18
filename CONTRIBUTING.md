# Contributing to vcuda-hook

Thanks for your interest in improving **vcuda-hook**! This guide explains how to propose changes, report issues, and collaborate smoothly.

## Quick Start
- Fork the repository and create a topic branch from `main`.
- Keep changes focused; separate unrelated fixes into distinct pull requests.
- Follow the existing code style; prefer modern C++ (C++17) and add concise comments only when the logic is non-trivial.

## Coding Guidelines
- Maintain cross-platform portability where feasible (Linux/WSL is the primary target).
- Keep headers light; place implementations in `.cpp` files when possible.
- Use `spdlog` for logging and route new configuration knobs through existing utilities.
- When touching hooks, log enough diagnostics to troubleshoot without overwhelming production logs.
- Update documentation (README, SECURITY, etc.) whenever behaviour or configuration changes.

## Commit & PR Checklist
- Rebase on the latest `main` branch; resolve conflicts locally.
- Write descriptive commit messages (imperative mood, e.g., `Add shared memory metric aggregation`).
- Include tests or samples when adding new functionality; explain why if testing is not possible.
- Link related issues (`Fixes #123`) and describe the impact, risks, and verification steps in the PR body.

## Reporting Issues
Before filing a bug:
- Search open issues to avoid duplicates.
- Collect versions (driver, CUDA, OS) and reproduction steps.
- Attach logs with `VCUDA_LOG_LEVEL=debug` when available.

Security concerns should be reported privately—see `SECURITY.md` for contact details.

## Code of Conduct
We follow the guidelines described in [CODE_OF_CONDUCT.md](./CODE_OF_CONDUCT.md). By participating, you agree to uphold these standards.

Happy hacking!

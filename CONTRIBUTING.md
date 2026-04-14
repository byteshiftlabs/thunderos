# Contributing to ThunderOS

Thank you for your interest in contributing to ThunderOS! This guide will help you get started.

## Getting Started

1. **Read the documentation**
   - [README.md](README.md) - Project overview and quick start
   - [ROADMAP.md](ROADMAP.md) - Planned features and versioning
   - [CHANGELOG.md](CHANGELOG.md) - What's already implemented
   - [docs/](docs/) - Full technical documentation (build with `make html`)

2. **Check current status**
   - Review open issues and pull requests

3. **Set up your environment**
   - Follow setup instructions in [docs/source/development.rst](docs/source/development.rst)
   - Initialize submodules: `git submodule update --init --recursive`
   - Run the authoritative verification path first: `make docker-verify`
   - Use native `make`, `make qemu`, and `make debug` only after the Docker path is understood

## How to Contribute

### Reporting Bugs

- Open an issue with clear reproduction steps
- Include QEMU version, toolchain version, and OS
- Attach relevant error messages or kernel output

### Suggesting Features

- Check [ROADMAP.md](ROADMAP.md) first - feature might be planned
- Open an issue describing the feature and its use case
- Wait for maintainer feedback before implementing

### Submitting Code

1. **Choose the right branch**
   - **Active development**: `dev/vX.Y.Z` (check README for current version)
   - **Never merge directly to `main`** - it's protected

2. **Follow coding standards**
   - See [docs/source/development/code_quality.rst](docs/source/development/code_quality.rst)
   - Use `-O0` optimization (for debugging)
   - Run `make` to check for compiler warnings

3. **Write meaningful commit messages**
   - Keep messages short and concise
   - Use past participle form ("Added", "Fixed", "Updated", not "Add", "Fix", "Update")
   - Example: `Added syscalls: getppid, kill, gettime and comprehensive documentation`
   - Example: `Enhanced PMM and kmalloc documentation`
   - Example: `Fixed memory leak in kmalloc`
   - Be specific about what changed, not why (details go in PR description)

4. **Write tests**
   - Add tests in `tests/` directory
   - Follow the KUnit-inspired framework in [docs/source/internals/testing_framework.rst](docs/source/internals/testing_framework.rst)

5. **Document your changes**
   - Update relevant `.rst` files in `docs/source/`
   - Add entry to [CHANGELOG.md](CHANGELOG.md) under "Unreleased"
   - Comment your code clearly

6. **Submit a pull request**
   - Target the current `dev/vX.Y.Z` branch
   - Write a clear PR description
   - Reference related issues
   - Include the verification command you ran (`make docker-verify`, or explain why a narrower check was necessary)
   - Wait for review

### Working With The Userland Submodule

- `external/userland/` is versioned by the submodule commit recorded in this repository.
- If your change requires userland work, commit and push the `thunderos-userland` change first, then update the submodule pointer here intentionally.
- Before opening a PR that touches the submodule pointer, verify with `make docker-verify` from the ThunderOS root.

## AI Usage Policy

ThunderOS uses AI assistance for development. See the [ai-dev-prompts](https://github.com/byteshiftlabs/ai-dev-prompts) repository for guidelines on:
- Workflow conventions and prompt guidelines
- Code review and error handling standards
- Model-specific adapter layers

## Code of Conduct

- Be respectful and constructive
- Focus on technical merit
- Help newcomers learn RISC-V and OS development
- Remember: this is an educational project

## Questions?

- Open a GitHub Discussion for general questions
- Use issues for specific bugs or features
- Check existing documentation first

## License

By contributing, you agree that your contributions will be licensed under the same license as ThunderOS. See [LICENSE](LICENSE).

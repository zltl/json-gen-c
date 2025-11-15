# Contributing to json-gen-c

Thanks for taking the time to improve json-gen-c! Whether you are fixing a bug, improving docs, or creating new features, your help makes the project better for everyone.

## Ways to Contribute

- **Report bugs**: open an issue with reproduction steps, logs, and environment info.
- **Request features**: describe the use case you're trying to solve and any constraints.
- **Improve documentation**: clarify tricky areas, add tutorials, or fix typos.
- **Submit code changes**: fix issues, add tests, or improve performance.

## Development Environment

1. **Clone the repo**
   ```bash
   git clone https://github.com/zltl/json-gen-c.git
   cd json-gen-c
   ```
2. **Build**
   ```bash
   make            # default build
   make -j$(nproc) # faster parallel build
   ```
3. **Run tests**
   ```bash
   make test
   ```
4. **Optional builds**
   ```bash
   make example    # sample program
   make benchmark  # performance harness
   make sanitize   # sanitizer-enabled build
   ```

## Coding Guidelines

- Stick to the existing C11 style in the repository.
- Keep warnings at bay: the CI treats all warnings as errors.
- Prefer small, focused commits with clear messages.
- Add or update tests when fixing bugs or adding features.
- Document new user-facing behavior in `README.md` or `docs/`.

## Pull Request Checklist

- [ ] Tests pass locally (`make test`).
- [ ] New code is covered by tests or clearly explained why not.
- [ ] Documentation is updated if behavior changes.
- [ ] Commit messages explain the *why* of the change.

## Communication

- Use GitHub Issues for bugs and questions.
- Reference related issues or PRs in your commits.
- Be kind and constructive—assume positive intent.

Thanks again for helping json-gen-c grow! 🙌

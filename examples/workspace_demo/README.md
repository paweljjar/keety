# Kattery Monorepo Workspace Demo

This example demonstrates a multi-package KeetScript monorepo workspace managed by `kattery`.

## Project Structure

```
workspace_demo/
├── pedigree.json              # Workspace root manifest declaring "workspaces": ["packages/*"]
├── packages/
│   ├── math_utils/            # Member package providing math utilities
│   │   ├── pedigree.json
│   │   └── index.keet
│   └── app/                   # Member package application consuming math-utils
│       ├── pedigree.json
│       └── src/
│           └── main.keet
```

## Running the Example

1. **Install workspace dependencies and link members:**

```bash
kattery install
```

This discovers member packages, links them into `keet_modules/` at the root, and generates a unified `pedigree.lock`.

2. **Execute the application entry point:**

```bash
kattery run start
```

Or target the application package directly:

```bash
kattery run start -w demo-app
```

3. **Run tests across all workspace members:**

```bash
kattery run test --workspaces
```

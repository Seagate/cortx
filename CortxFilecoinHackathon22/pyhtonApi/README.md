# Python project

## CORTX IPFS Bridge

Build a bridge between CORTX and IPFS as API.

### Run

Start Server with `poetry run src/flask_server.py`. Connect to the API on port 5000
Connect to CORTX `poetry run src/cortx_s3.py`.

Issues:

- IPFS daemon v.11 not supported by the python IPFS library.
- ⌚

## Project management tools

First time I'm setting up a python project with proper management tools. Let this be a proof of concept.

### Poetry

package manager

### safety

`poetry add safety`

Scans for knows vulnerabilities.

### .editorconfig

Styleguide for multi language projects.

### Pre-commit

`poetry add pre-commit`

Runs checks before a commit.

### MyPy

Static type checker.

### nitpick

`poetry run nitpick check/fix`
Checks the project for structural correctness and fixes issues.

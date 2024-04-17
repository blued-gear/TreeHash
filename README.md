# TreeHash

TreeHash is a utility to create and verify hashes for a file-tree.\
It operates on a file containing the file-paths (recursive) relative to a given directory and their hashes.

The sources consist of the library (which manages the hashes) and a CLI frontend.

## Usage
To see all options run `TreeHash-CLI --help`

Example Workflow:
```sh
cd someDirToHash
# create hashes of all files (and print which files were processed)
TreeHash-CLI -r . -f ./hashes.json -l a -m update

# ... change and add some files ...

# update hashes
TreeHash-CLI -r . -f ./hashes.json -l a -m update
# or 1. update known files which were modified, 2. hash unknown files
TreeHash-CLI -r . -f ./hashes.json -l a -m update_mod
TreeHash-CLI -r . -f ./hashes.json -l a -m update_new

# ... delete some files ...

# check what was deleted
TreeHash-CLI -r . -f ./hashes.json --check-removed
# remove deleted files from hashes
TreeHash-CLI -r . -f ./hashes.json -c
```

To exclude files and directories from hashing use `-e <relative path>` (can be used multiple times).\
To hash only specific files and directories use `-i <relative path>` (can be used multiple times).\
(`-i` and `-e` can be combined; e.g. to exclude a sub-dir in an include.)

## Repo
The GitHub Repo is a mirror from my GitLab.\
To get prebuild binaries, go [here](https://projects.chocolatecakecodes.goip.de/blued_gear/treehash).

// ---------------------------------------------------------------------------
// Playerbot Git revision.
//
// Rewritten by scripts/update-version.sh (git post-commit hook) on every
// commit, so /bot shows the exact commit the binary was built from, and ZIP
// deployments (no .git) carry the version in the source tree.
//
// To update manually after a commit (if the hook is not installed):
//   scripts/update-version.sh
// ---------------------------------------------------------------------------
#ifndef PLAYERBOT_VERSION_H
#define PLAYERBOT_VERSION_H

#ifndef PLAYERBOT_REV
#define PLAYERBOT_REV "f26e6cd"
#endif

#endif // PLAYERBOT_VERSION_H

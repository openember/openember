#!/usr/bin/env bash
# Create a product project from templates/device (ember new).
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: ember new <name> [options]

Create an OpenEmber product project (golden layout).

Options:
  --path <dir>         Parent directory (default: current directory)
  --model <name>       Initial products/<model>/ name (default: default)
  --openember <path>   Framework tree to link (default: this OpenEmber checkout)
  --no-symlink         Do not create ./openember symlink (you must add submodule later)
  --force              Allow non-empty destination if it only has .git
  -h, --help           Show this help

Example:
  ember new my-usv
  ember new acme-rover --path ~/ws --model rover-m1
EOF
}

die() { echo "ember new: $*" >&2; exit 1; }

slugify() {
  # my-usv / Acme Rover → my_usv / acme_rover (C++ / binary friendly)
  echo "$1" | tr '[:upper:]' '[:lower:]' | sed -E 's/[^a-z0-9]+/_/g; s/^_+//; s/_+$//; s/_+/_/g'
}

FRAMEWORK_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TEMPLATE="${FRAMEWORK_ROOT}/templates/device"

NAME=""
PARENT="."
MODEL="default"
OPENEMBER_SRC="${FRAMEWORK_ROOT}"
DO_SYMLINK=1
FORCE=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    --path)
      PARENT="${2:-}"
      [[ -n "${PARENT}" ]] || die "--path needs a value"
      shift 2
      ;;
    --model)
      MODEL="${2:-}"
      [[ -n "${MODEL}" ]] || die "--model needs a value"
      shift 2
      ;;
    --openember)
      OPENEMBER_SRC="${2:-}"
      [[ -n "${OPENEMBER_SRC}" ]] || die "--openember needs a value"
      shift 2
      ;;
    --no-symlink)
      DO_SYMLINK=0
      shift
      ;;
    --force)
      FORCE=1
      shift
      ;;
    -*)
      die "unknown option: $1"
      ;;
    *)
      if [[ -z "${NAME}" ]]; then
        NAME="$1"
        shift
      else
        die "unexpected argument: $1"
      fi
      ;;
  esac
done

[[ -n "${NAME}" ]] || { usage >&2; exit 1; }
[[ -d "${TEMPLATE}" ]] || die "template missing: ${TEMPLATE}"

if [[ ! "${NAME}" =~ ^[A-Za-z][A-Za-z0-9._-]*$ ]]; then
  die "invalid name '${NAME}' (use letters/digits/._- , start with a letter)"
fi
[[ "${MODEL}" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ ]] ||
  die "invalid model '${MODEL}' (use letters/digits/._-, start with a letter or digit)"

SLUG="$(slugify "${NAME}")"
[[ -n "${SLUG}" ]] || die "could not derive slug from name"

PARENT="$(cd "${PARENT}" && pwd)"
DEST="${PARENT}/${NAME}"
OPENEMBER_SRC="$(cd "${OPENEMBER_SRC}" && pwd)"
[[ -f "${OPENEMBER_SRC}/CMakeLists.txt" && -f "${OPENEMBER_SRC}/scripts/ember" ]] || \
  die "not an OpenEmber tree: ${OPENEMBER_SRC}"

if [[ -e "${DEST}" ]]; then
  if [[ "${FORCE}" -eq 1 ]]; then
    # allow empty or git-only
    local_count="$(find "${DEST}" -mindepth 1 -maxdepth 1 ! -name '.git' | wc -l)"
    [[ "${local_count}" -eq 0 ]] || die "destination not empty: ${DEST}"
  else
    die "destination exists: ${DEST} (use --force if empty except .git)"
  fi
else
  mkdir -p "${DEST}"
fi

echo "[ember new] name=${NAME} slug=${SLUG} model=${MODEL}"
echo "[ember new] dest=${DEST}"
echo "[ember new] openember=${OPENEMBER_SRC}"

# Copy template (no .git)
cp -a "${TEMPLATE}/." "${DEST}/"

# Rename products/default → products/${MODEL} when needed
if [[ "${MODEL}" != "default" ]]; then
  if [[ -d "${DEST}/products/default" ]]; then
    mv "${DEST}/products/default" "${DEST}/products/${MODEL}"
  fi
fi

replace_in_tree() {
  local root="$1"
  # Limit to text-ish files we generate
  while IFS= read -r -d '' f; do
    # skip binary
    if grep -Iq . "${f}" 2>/dev/null; then
      sed -i \
        -e "s/__PRODUCT_NAME__/${NAME}/g" \
        -e "s/__PRODUCT_SLUG__/${SLUG}/g" \
        -e "s/__MODEL__/${MODEL}/g" \
        "${f}"
    fi
  done < <(find "${root}" -type f -print0)
}

replace_in_tree "${DEST}"

chmod +x "${DEST}/scripts/build.sh"

if [[ "${DO_SYMLINK}" -eq 1 ]]; then
  # Prefer relative symlink when possible
  rel=""
  if command -v realpath >/dev/null 2>&1; then
    rel="$(realpath --relative-to="${DEST}" "${OPENEMBER_SRC}" 2>/dev/null || true)"
  fi
  if [[ -n "${rel}" ]]; then
    ln -sfn "${rel}" "${DEST}/openember"
  else
    ln -sfn "${OPENEMBER_SRC}" "${DEST}/openember"
  fi
  echo "[ember new] linked openember -> $(readlink "${DEST}/openember")"
else
  mkdir -p "${DEST}/openember"
  cat > "${DEST}/openember/README.md" <<EOF
Add OpenEmber here:

  git submodule add <openember-url> openember
  # or: ln -s /path/to/openember openember
EOF
fi

# Convenience symlink for default launch if model is not literally "default"
if [[ -f "${DEST}/products/${MODEL}/launch.yaml" ]]; then
  mkdir -p "${DEST}/configs"
  ln -sfn "../products/${MODEL}/launch.yaml" "${DEST}/configs/default.launch.yaml"
fi

cat <<EOF

[ember new] created ${DEST}

Next:
  cd ${DEST}
  ./scripts/build.sh
  ./build/${MODEL}/bin/openember_${SLUG}_app --router

Add modules under modules/, SKUs under products/<model>/.
EOF

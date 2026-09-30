# Embedded by writeShellApplication, which supplies game_binary and dependencies.
for argument in "$@"; do
  case "$argument" in
    --help|-h)
      printf '%s\n' \
        "Usage: KF_DISC=/path/to/disc.iso kings-field [--language ja|en] [--saves DIRECTORY]" \
        "Language defaults to KF_LANGUAGE, or en. English is generated automatically from your Japanese disc." \
        "Supply KF_DISC once; cached Japanese resources support both languages afterward." \
        "Verified resources are reused from the XDG cache; saves are stored separately." \
        "Explicit --data or --disc/--extract-to/--extract-only options bypass automatic caching."
      exit 0
      ;;
  esac
done

# Preserve the direct application's explicit resource/extraction workflows.
for argument in "$@"; do
  case "$argument" in
    --data|--disc|--extract-to|--extract-only|--japanese-data)
      exec "$game_binary" "$@"
      ;;
  esac
done

language="${KF_LANGUAGE-en}"
arguments=("$@")
for ((index = 0; index < ${#arguments[@]}; index++)); do
  case "${arguments[index]}" in
    --language)
      index=$((index + 1))
      language="${arguments[index]-}"
      ;;
    --saves) index=$((index + 1)) ;;
    *) printf 'Unknown option: %s. Use --help.\n' "${arguments[index]}" >&2; exit 1 ;;
  esac
done
case "$language" in
  ja) cache_name=resources ;;
  en) cache_name=resources-en ;;
  *) printf 'Unsupported language: %s. Use ja or en.\n' "$language" >&2; exit 1 ;;
esac

if [[ -n "${japanese_resources:-}" && "$language" == ja ]]; then
  exec "$game_binary" --data "$japanese_resources" "$@" --language ja
fi

case "${XDG_CACHE_HOME:-}" in
  /*) cache_base="$XDG_CACHE_HOME/kings-field/SLPS-00017" ;;
  *) cache_base="${HOME:?HOME or an absolute XDG_CACHE_HOME is required}/.cache/kings-field/SLPS-00017" ;;
esac
umask 077
mkdir -p -- "$cache_base"
exec {cache_lock}>"$cache_base/import.lock"
flock -x "$cache_lock"
japanese_directory="${japanese_resources:-$cache_base/resources}"
data_directory="$cache_base/$cache_name"
for directory in "$japanese_directory" "$data_directory"; do
  if [[ -L "$directory" || ( -e "$directory" && ! -d "$directory" ) ]]; then
    printf 'Resource cache is not an ordinary directory: %s\n' "$directory" >&2
    exit 1
  fi
done
publish_resources() {
  local destination="$1"
  shift
  local import_directory
  import_directory=$(mktemp -d -- "$cache_base/import.XXXXXX")
  if ! "$game_binary" "$@" --extract-to "$import_directory/data" --extract-only; then
    printf 'Import failed. Any partial output remains in %s; no cache was replaced.\n' "$import_directory" >&2
    exit 1
  fi
  mv -T --no-clobber -- "$import_directory/data" "$destination"
  if [[ -e "$import_directory/data" ]]; then
    printf 'Cache destination appeared during import; extracted files remain in %s.\n' "$import_directory" >&2
    exit 1
  fi
  rmdir -- "$import_directory"
}
if [[ ! -d "$data_directory" ]]; then
  if [[ ! -d "$japanese_directory" ]]; then
    if [[ -z "${KF_DISC:-}" ]]; then
      printf '%s\n' 'First launch needs KF_DISC=/path/to/Japanese.iso (or BIN/CUE). Later launches use the cache.' >&2
      exit 1
    fi
    if [[ ! -f "$KF_DISC" || ! -r "$KF_DISC" ]]; then
      printf 'Cannot read disc image: %s\n' "$KF_DISC" >&2
      exit 1
    fi
    printf 'Importing Japanese resources from %s\n' "$KF_DISC" >&2
    publish_resources "$japanese_directory" --language ja --disc "$KF_DISC"
  fi
  if [[ "$language" == en ]]; then
    printf '%s\n' 'Preparing English resources from the Japanese cache…' >&2
    publish_resources "$data_directory" --language en --data "$japanese_directory"
  fi
fi
flock -u "$cache_lock"
exec {cache_lock}>&-
exec "$game_binary" --data "$data_directory" --japanese-data "$japanese_directory" "$@" --language "$language"

#!/usr/bin/env bash
# Updates authenticated application binaries; never enters the save/content roots.
set -Eeuo pipefail
umask 077
bundle=$(realpath -- "${1:?Missing patch payload}")
shift
app_root="${XDG_DATA_HOME:-${HOME:?}/.local/share}/SARecomp-app"
headless=0
while (($#)); do
    case "$1" in
        --app-root) app_root=${2:?Missing application folder}; shift 2 ;;
        --headless) headless=1; shift ;;
        *) printf 'Unknown argument: %s\n' "$1" >&2; exit 1 ;;
    esac
done
title='Sonic Adventure Recompiled - Performance Patch'
notice() {
    printf '%s\n' "$1"
    if (( !headless )) && command -v kdialog >/dev/null && [[ -n ${DISPLAY:-}${WAYLAND_DISPLAY:-} ]]; then
        kdialog --title "$title" --msgbox "$1" || true
    fi
}
error_shown=0
popup_pid=
fail() {
    printf 'Patch failed: %s\n' "$1" >&2
    error_shown=1
    if (( !headless )) && command -v kdialog >/dev/null && [[ -n ${DISPLAY:-}${WAYLAND_DISPLAY:-} ]]; then
        kdialog --title "$title" --error "$1" || true
    fi
    exit 1
}
[[ $EUID != 0 ]] || fail 'Run this patch as your normal user, without sudo.'
[[ -d $app_root && ! -L $app_root ]] || fail 'The installed game application folder was not found.'
app_root=$(realpath -- "$app_root")
[[ -O $app_root ]] || fail 'This installation belongs to another user.'
[[ ! -L $app_root/installation.lock ]] || fail 'The installation lock is invalid.'
exec 9>"$app_root/installation.lock"
flock -n 9 || fail 'Another installation or patch is running.'
sha() { sha256sum -- "$1" | cut -d ' ' -f 1; }
regular() { [[ -f $1 && ! -L $1 && -O $1 ]]; }
declare -A supported=()
target_hash= target_size= base_hash= delta_hash= tool_hash= diagnostics= patch_id=
reference_backups=()
while IFS=$'\t' read -r kind first second; do
    case "$kind" in
        target) target_size=$first; target_hash=$second ;;
        base) base_hash=$first ;;
        delta) delta_hash=$first ;;
        tool) tool_hash=$first ;;
        supported) supported[$first]=1 ;;
        diagnostics) diagnostics=$first ;;
        patch-id) patch_id=$first ;;
        reference-backup) reference_backups+=("$first") ;;
        SARECOMP-RUNTIME-PATCH-1|'') ;;
        *) fail 'Unsupported patch metadata.' ;;
    esac
done < "$bundle/patch.tsv"
backup_suffix=pre-native-math-v1
if [[ -n $diagnostics ]]; then
    [[ $diagnostics == on || $diagnostics == off ]] || fail 'Invalid diagnostics policy.'
    backup_suffix=pre-diagnostics-v1
    title="Sonic Adventure Recompiled - Diagnostics ${diagnostics^^}"
fi
if [[ -n $patch_id ]]; then
    [[ $patch_id =~ ^[a-z0-9][a-z0-9-]{0,47}$ && -z $diagnostics ]] || fail 'Invalid performance patch identity.'
    backup_suffix="pre-$patch_id"
fi
for name in "${reference_backups[@]}"; do
    [[ $name =~ ^game\.pre-[a-z0-9-]+$ ]] || fail 'Invalid patch reference name.'
done
[[ $target_hash =~ ^[0-9a-f]{64}$ && $target_size =~ ^[0-9]+$ && $base_hash =~ ^[0-9a-f]{64}$ ]] || fail 'Invalid patch identity.'
[[ $(sha "$bundle/game.delta.zst") == "$delta_hash" && $(sha "$bundle/zstd") == "$tool_hash" ]] || fail 'The downloaded patch is damaged.'
dirs=() hashes=() actions=() committed=()
reference= ready_source= stage=
success=0
cleanup() {
    local result=$?
    trap - EXIT HUP INT TERM
    if [[ -n $popup_pid ]]; then kill "$popup_pid" 2>/dev/null || true; fi
    if (( !success )); then
        for index in "${committed[@]}"; do
            local dir=${dirs[$index]}
            if [[ ${actions[$index]} == update ]]; then
                ln -- "$dir/game.$backup_suffix" "$stage/restore-game-$index" &&
                    mv -f -- "$stage/restore-game-$index" "$dir/game" || true
            fi
            if [[ ${actions[$index]} != mode ]]; then
                ln -- "$dir/resources/payload-files.tsv.$backup_suffix" "$stage/restore-manifest-$index" &&
                    mv -f -- "$stage/restore-manifest-$index" "$dir/resources/payload-files.tsv" || true
            fi
            if [[ -n $diagnostics ]]; then
                if [[ -f $stage/previous-policy-$index ]]; then
                    mv -f -- "$stage/previous-policy-$index" "$dir/.sarecomp-diagnostics" || true
                else rm -f -- "$dir/.sarecomp-diagnostics"; fi
            fi
        done
        printf 'Update did not complete. Original program backups have been retained.\n' >&2
        if (( !headless && !error_shown )) && command -v kdialog >/dev/null && [[ -n ${DISPLAY:-}${WAYLAND_DISPLAY:-} ]]; then
            kdialog --title "$title" --error 'The patch did not complete. Existing program backups, saves and settings were retained. Run from a terminal to see the error details.' || true
        fi
    fi
    if [[ -n $stage && $stage == "$app_root"/.native-math-patch.* ]]; then rm -rf -- "$stage"; fi
    exit "$result"
}
trap cleanup EXIT
trap 'exit 130' HUP INT TERM
if (( !headless )) && command -v kdialog >/dev/null && [[ -n ${DISPLAY:-}${WAYLAND_DISPLAY:-} ]]; then
    kdialog --title "$title" --passivepopup 'Checking and updating the installed program. Please wait...' 120 &
    popup_pid=$!
fi
printf 'Checking installed launch paths...\n'
for dir in "$app_root"/1.0-candidate-*; do
    [[ -d $dir && ! -L $dir && -O $dir ]] || continue
    [[ ${dir##*/} =~ ^1\.0-candidate-[0-9a-f]{16}$ ]] || continue
    regular "$dir/game" && regular "$dir/resources/payload-files.tsv" || continue
    [[ ! -L $dir/resources && $(stat -c %d "$dir") == $(stat -c %d "$app_root") ]] || fail 'The application folder layout is unsupported.'
    record=$(awk -F '\t' '$1=="game" {print $3}' "$dir/resources/payload-files.tsv")
    [[ $record =~ ^[0-9a-f]{64}$ ]] || fail 'An installed game manifest is invalid.'
    [[ $record == "$target_hash" || -n ${supported[$record]:-} ]] || continue
    current=$(sha "$dir/game")
    if [[ $current == "$target_hash" ]]; then
        ready_source="$dir/game"
        if [[ $record == "$target_hash" ]]; then
            [[ -n $diagnostics ]] || continue
            action=mode
        else action=repair; fi
    else
        [[ $current == "$record" && -n ${supported[$current]:-} ]] || fail 'An installed program was modified or damaged; it has not been overwritten.'
        action=update
        [[ $current != "$base_hash" ]] || reference="$dir/game"
        # A Diagnostics update preserved the prior native-math executable.
        # Authenticate that separate reference; rollback still backs up current.
        if [[ -z $reference ]]; then
            for name in "${reference_backups[@]}"; do
                candidate="$dir/$name"
                if regular "$candidate" && [[ $(sha "$candidate") == "$base_hash" ]]; then
                    reference=$candidate
                    break
                fi
            done
        fi
    fi
    if [[ -n $diagnostics && ( -e $dir/.sarecomp-diagnostics || -L $dir/.sarecomp-diagnostics ) ]]; then
        regular "$dir/.sarecomp-diagnostics" || fail 'The internal diagnostics policy is not a regular user-owned file.'
    fi
    dirs+=("$dir"); hashes+=("$current"); actions+=("$action")
done
if (( ${#dirs[@]} == 0 )); then
    [[ -n $ready_source ]] || fail 'No supported v5 installation was found. No game data was changed.'
    success=1; notice 'This performance patch is already installed.'; exit 0
fi
[[ -n $reference || -n $ready_source ]] || fail 'The v5 program required for this patch was not found. No reinstallation has been started.'
# Steam shortcuts may still name an older version directory. Update every
# authenticated installed launch path, so the existing Steam entry keeps working.
for process in /proc/[0-9]*/exe; do
    running=$(readlink -- "$process" 2>/dev/null || true)
    for dir in "${dirs[@]}"; do [[ $running != "$dir/game" ]] || fail 'Close Sonic Adventure Recompiled before applying the patch.'; done
done
available=$(df -PB1 -- "$app_root" | awk 'NR==2 {print $4}')
required_space=67108864
[[ -n $ready_source ]] || required_space=$((required_space + target_size))
[[ $available =~ ^[0-9]+$ ]] && (( available > required_space )) || fail 'The initial program update needs about 1.7 GB of free space.'
stage=$(mktemp -d "$app_root/.native-math-patch.XXXXXXXX")
if [[ -n $ready_source ]]; then
    ln -- "$ready_source" "$stage/game"
else
    printf 'Applying binary delta (the original game files are not needed)...\n'
    "$bundle/zstd" -d -q -M2048MB --patch-from="$reference" "$bundle/game.delta.zst" -o "$stage/game"
fi
[[ $(stat -c %s "$stage/game") == "$target_size" && $(sha "$stage/game") == "$target_hash" ]] || fail 'The reconstructed program failed verification.'
chmod 755 "$stage/game"
sync -f "$stage/game"
# Prepare all files and reversible backups before changing any launch path.
for index in "${!dirs[@]}"; do
    dir=${dirs[$index]}
    [[ $(sha "$dir/game") == "${hashes[$index]}" ]] || fail 'The installed program changed while the patch was running.'
    if [[ -n $diagnostics ]]; then
        [[ ! -f $dir/.sarecomp-diagnostics ]] || cp -- "$dir/.sarecomp-diagnostics" "$stage/previous-policy-$index"
        printf 'SARECOMP-DIAGNOSTICS-1\n%s\n' "$diagnostics" > "$stage/policy-$index"
    fi
    [[ ${actions[$index]} != mode ]] || continue
    backup="$dir/game.$backup_suffix"
    if [[ ${actions[$index]} == update ]]; then
        if [[ -e $backup || -L $backup ]]; then
            regular "$backup" && [[ $(sha "$backup") == "${hashes[$index]}" ]] || fail 'A different program backup already exists.'
        else ln -- "$dir/game" "$backup"; fi
    fi
    backup="$dir/resources/payload-files.tsv.$backup_suffix"
    if [[ -e $backup || -L $backup ]]; then
        regular "$backup" && cmp -s -- "$backup" "$dir/resources/payload-files.tsv" || fail 'A different manifest backup already exists.'
    else ln -- "$dir/resources/payload-files.tsv" "$backup"; fi
    awk -F '\t' -v size="$target_size" -v hash="$target_hash" 'BEGIN {OFS="\t"} $1=="game" {print "game",size,hash;next} {print}' "$dir/resources/payload-files.tsv" > "$stage/manifest-$index"
    ln -- "$stage/game" "$stage/game-$index"
done
printf 'Publishing verified programs...\n'
for index in "${!dirs[@]}"; do
    dir=${dirs[$index]}; committed+=("$index")
    if [[ ${actions[$index]} == update ]]; then
        mv -f -- "$stage/game-$index" "$dir/game"
    fi
    if [[ ${actions[$index]} != mode ]]; then
        mv -f -- "$stage/manifest-$index" "$dir/resources/payload-files.tsv"
    fi
    if [[ -n $diagnostics ]]; then
        mv -f -- "$stage/policy-$index" "$dir/.sarecomp-diagnostics"
        sync -f "$dir/.sarecomp-diagnostics"
    fi
    sync -f "$dir/game"
done
success=1
if [[ -n $popup_pid ]]; then kill "$popup_pid" 2>/dev/null || true; popup_pid=; fi
if [[ -n $patch_id ]]; then
    notice "CPU performance update installed successfully (${#dirs[@]} launch paths).

Start the game using your existing Steam or desktop shortcut.
Saves, Chao data, settings and diagnostics preference have been preserved."
elif [[ -n $diagnostics ]]; then
    notice "Internal diagnostics ${diagnostics^^} (${#dirs[@]} launch paths).

Start the game using your existing Steam or desktop shortcut.
The selected policy takes effect on the next launch.
Saves, Chao data and game settings have been preserved."
else notice "Performance patch installed successfully (${#dirs[@]} launch paths).

Start the game using your existing Steam or desktop shortcut.
Saves, Chao data and settings have been preserved.
Original timing now uses the same optimized calculations as Recompiled."
fi

#!/usr/bin/env bash
# Git Bash is the VS Code terminal. Keep this path relative so the spaces in
# the repo path are not split, then hand PowerShell a Windows path.
script_dir="$(cd "$(dirname "$0")" && pwd)"
win_script="$(cygpath -w "$script_dir/program-keyboard-dfu.ps1")"
export MSYS_NO_PATHCONV=1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$win_script"
exit $?

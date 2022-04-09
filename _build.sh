#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"
rm -rf _build
mkdir _build
gcc clearfd.c -o _build/clearfd -O3 --static
gcc login_tty.c -o _build/login_tty -O3 --static -lutil
strip _build/*
if ! which busybox >/dev/null 2>/dev/null </dev/null; then
	echo "ERROR: BusyBox is not found on this system."
	exit 1
fi
if ldd "$(which busybox)" >/dev/null 2>/dev/null </dev/null; then
	echo "ERROR: The installed BusyBox is not statically linked."
	exit 1
fi
cp "$(realpath "$(which busybox)")" _build/busybox
cp reinit _build/reinit
echo "Done."

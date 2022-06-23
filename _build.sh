#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"
rm -rf _build
mkdir _build
gcc clearfd.c -o _build/clearfd -O3 --static -Werror
gcc login_tty.c -o _build/login_tty -O3 --static -lutil -Werror
gcc takeover_process.c -o _build/takeover_process -O3 --static -Werror
gcc initenv.c -o _build/initenv -O3 --static -Werror
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
echo '#!/bin/false' >_build/busybox_aliases
for i in $(busybox --list); do
	echo "alias $i='/._tmp_reinit/busybox $i'" >>_build/busybox_aliases
done
cp reinit _build/reinit
cp prepare _build/prepare
echo "Done."

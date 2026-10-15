#!/usr/bin/env bash
# Builds third-party libraries that devkitPro does not package for the Wii and
# installs them into $DEVKITPRO/portlibs/wii.
set -euo pipefail

: "${DEVKITPRO:=/opt/devkitpro}"
: "${DEVKITPPC:=$DEVKITPRO/devkitPPC}"
export DEVKITPRO DEVKITPPC PATH="$DEVKITPPC/bin:$PATH"

GRRLIB_TAG=v4.6.1
# Tags can move, so check the tag still points at the reviewed commit.
GRRLIB_COMMIT=8feae891610ccd08ea2d2cb5d987fc74c47ad57c
MBEDTLS_VERSION=3.6.7
MBEDTLS_SHA256=a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6

PREFIX="$DEVKITPRO/portlibs/wii"
MACHDEP="-DGEKKO -mrvl -mcpu=750 -meabi -mhard-float"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

build_grrlib() {
	git clone -q --depth 1 --branch "$GRRLIB_TAG" https://github.com/GRRLIB/GRRLIB.git "$WORK/GRRLIB"
	local commit
	commit="$(git -C "$WORK/GRRLIB" rev-parse HEAD)"
	if [ "$commit" != "$GRRLIB_COMMIT" ]; then
		echo "GRRLIB $GRRLIB_TAG is $commit, expected $GRRLIB_COMMIT" >&2
		exit 1
	fi
	make -C "$WORK/GRRLIB/GRRLIB" all install
}

build_mbedtls() {
	local tarball="$WORK/mbedtls.tar.bz2"
	curl -fsSL -o "$tarball" \
		"https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-$MBEDTLS_VERSION/mbedtls-$MBEDTLS_VERSION.tar.bz2"
	echo "$MBEDTLS_SHA256  $tarball" | sha256sum -c -
	tar -xjf "$tarball" -C "$WORK"
	local src="$WORK/mbedtls-$MBEDTLS_VERSION"

	# No POSIX sockets, filesystem, monotonic clock, or OS entropy on the Wii.
	# The app supplies its own socket BIO, mbedtls_ms_time(), and
	# mbedtls_hardware_poll().
	config() { python3 "$src/scripts/config.py" -f "$src/include/mbedtls/mbedtls_config.h" "$@"; }
	local opt
	for opt in MBEDTLS_NET_C MBEDTLS_TIMING_C MBEDTLS_FS_IO MBEDTLS_PSA_ITS_FILE_C \
		MBEDTLS_PSA_CRYPTO_STORAGE_C MBEDTLS_SELF_TEST; do
		config unset "$opt"
	done
	config set MBEDTLS_NO_PLATFORM_ENTROPY
	config set MBEDTLS_ENTROPY_HARDWARE_ALT
	config set MBEDTLS_PLATFORM_MS_TIME_ALT

	make -C "$src/library" \
		CC=powerpc-eabi-gcc AR=powerpc-eabi-ar \
		CFLAGS="-O2 $MACHDEP" \
		libmbedcrypto.a libmbedx509.a libmbedtls.a

	mkdir -p "$PREFIX/lib" "$PREFIX/include"
	cp "$src"/library/libmbed{crypto,x509,tls}.a "$PREFIX/lib/"
	rm -rf "$PREFIX/include/mbedtls" "$PREFIX/include/psa"
	cp -r "$src/include/mbedtls" "$src/include/psa" "$PREFIX/include/"
}

build_grrlib
build_mbedtls
echo "Installed GRRLIB $GRRLIB_TAG and mbedTLS $MBEDTLS_VERSION into $PREFIX"

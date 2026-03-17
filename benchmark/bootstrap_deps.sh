#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DEPS_DIR="${BENCH_DEPS_DIR:-$SCRIPT_DIR/.deps}"
PREFIX_DIR="${BENCH_PREFIX:-$DEPS_DIR/prefix}"
YYJSON_DIR="${YYJSON_DIR:-$SCRIPT_DIR/yyjson}"
YYJSON_REPO="${YYJSON_REPO:-https://github.com/ibireme/yyjson.git}"

log() {
    printf '[benchmark] %s\n' "$*"
}

die() {
    printf '[benchmark] error: %s\n' "$*" >&2
    exit 1
}

run_root_command() {
    if [ "$(id -u)" -eq 0 ]; then
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        sudo "$@"
    else
        die "need root privileges to run '$*' (install sudo or run as root)"
    fi
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"
}

install_apt_packages() {
    local packages
    local missing
    local pkg

    if ! command -v apt-get >/dev/null 2>&1 || ! command -v dpkg-query >/dev/null 2>&1; then
        die "automatic benchmark dependency install currently supports Debian/Ubuntu (apt-get) only"
    fi

    packages=(
        build-essential
        cmake
        pkg-config
        libcjson-dev
        libjansson-dev
        libjson-c-dev
        rapidjson-dev
        libbenchmark-dev
    )
    missing=()

    for pkg in "${packages[@]}"; do
        if ! dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null | grep -q 'install ok installed'; then
            missing+=("$pkg")
        fi
    done

    if [ "${#missing[@]}" -eq 0 ]; then
        log "system benchmark packages already installed"
        return
    fi

    log "installing apt packages: ${missing[*]}"
    run_root_command apt-get update
    run_root_command apt-get install -y "${missing[@]}"
}

prepare_yyjson_checkout() {
    if [ -d "$YYJSON_DIR/.git" ]; then
        log "using existing yyjson checkout at $YYJSON_DIR"
        return
    fi

    if [ -e "$YYJSON_DIR" ]; then
        die "path exists but is not a yyjson git checkout: $YYJSON_DIR"
    fi

    log "cloning yyjson into $YYJSON_DIR"
    git clone --depth=1 "$YYJSON_REPO" "$YYJSON_DIR"
}

build_yyjson() {
    log "building yyjson into $PREFIX_DIR"
    cmake -S "$YYJSON_DIR" -B "$YYJSON_DIR/build" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DCMAKE_INSTALL_PREFIX="$PREFIX_DIR"
    cmake --build "$YYJSON_DIR/build" --parallel
    cmake --install "$YYJSON_DIR/build"
}

main() {
    require_command git
    require_command cmake
    require_command make
    require_command gcc
    require_command g++

    mkdir -p "$DEPS_DIR"

    install_apt_packages
    prepare_yyjson_checkout
    build_yyjson

    log "benchmark dependency prefix ready at $PREFIX_DIR"
    log "build and run with: make benchmark-repro"
}

main "$@"

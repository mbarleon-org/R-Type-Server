#!/usr/bin/env bash

MAG=$'\033[1;35m'
BOLD=$'\033[1m'
GREEN=$'\033[1;32m'
RED=$'\033[1;31m'
ILC=$'\033[3m'
ORG=$'\033[1;33m'
RST=$'\033[0m'

function _prompt()
{
    local ans
    local tty=/dev/tty
    if [ -e "$tty" ] && [ -r "$tty" ] && [ -w "$tty" ]; then
        printf "%b" "${MAG}${BOLD}[❓] PROMPT:\t${RST} ${ILC}$1:${RST} " > "$tty"
        if read -r ans < "$tty"; then
            printf '%s' "$ans"
        else
            printf ''
        fi
    else
        printf "%b" "${MAG}${BOLD}[❓] PROMPT:\t${RST} ${ILC}$1:${RST} "
        if read -r ans; then
            printf '%s' "$ans"
        else
            printf ''
        fi
    fi
}

function _error()
{
    echo -e "${RED}${BOLD}[❌] ERROR:\n${RST}\t$1\n\t${ILC}\"$2\"${RST}"
    exit 84
}

function _success()
{
    echo -e "${GREEN}[✅] SUCCESS:\t${RST} ${ILC}$1${RST}"
}

function _info()
{
    echo -e "${ORG}[🚧] RUNNING:\t${RST} ${ILC}$1${RST}"
}

function _cpus() {
    if command -v nproc >/dev/null 2>&1; then
        nproc
    elif [ "$(uname -s)" = "Darwin" ]; then
        sysctl -n hw.ncpu
    else
        echo 4
    fi
}

function _ensure_tools() {
    missing=()
    # always require git and cmake
    for t in git cmake; do
        if ! command -v "$t" >/dev/null 2>&1; then
            missing+=("$t")
        fi
    done

    # decide which build tool is expected based on env or best-effort detection
    local desired_generator=""
    if [ -n "${CMAKE_GENERATOR:-}" ]; then
        desired_generator="${CMAKE_GENERATOR}"
    elif [ -n "${BUILD_SYSTEM:-}" ]; then
        desired_generator="${BUILD_SYSTEM}"
    else
        # fall back to what _choose_build_system would pick (prefer ninja when present)
        if command -v ninja >/dev/null 2>&1; then
            desired_generator="Ninja"
        else
            desired_generator="Unix Makefiles"
        fi
    fi

    local required_build_tool=""
    if [[ "${desired_generator}" == *Makefiles* ]]; then
        required_build_tool="make"
    elif [[ "${desired_generator}" == "Ninja" ]]; then
        required_build_tool="ninja"
    else
        if command -v ninja >/dev/null 2>&1; then
            required_build_tool="ninja"
        elif command -v make >/dev/null 2>&1; then
            required_build_tool="make"
        else
            required_build_tool="none"
        fi
    fi

    if [ "${required_build_tool}" = "none" ]; then
        missing+=("ninja or make")
    else
        if ! command -v "${required_build_tool}" >/dev/null 2>&1; then
            missing+=("${required_build_tool}")
        fi
    fi

    if [ ${#missing[@]} -ne 0 ]; then
        hint="Please install: ${missing[*]}"
        _error "required tool(s) missing" "$hint"
    fi

    _success "required tools found (git, cmake, ${required_build_tool})"
}

function _invoke_build_tool() {
    local build_system="$1"
    local target="$2"

    if [[ "${build_system}" == *Makefiles* ]]; then
        printf '%s' "make -j$(_cpus) ${target}"
    elif [[ "${build_system}" == "Ninja" ]]; then
        printf '%s' "ninja -j$(_cpus) ${target}"
    else
        if command -v ninja >/dev/null 2>&1; then
            printf '%s' "ninja -j$(_cpus) ${target}"
        elif command -v make >/dev/null 2>&1; then
            printf '%s' "make -j$(_cpus) ${target}"
        else
            _error "No supported build tool found" "install ninja or make"
        fi
    fi
}

function _run_build_tool() {
    local build_system="$1"
    local target="$2"
    local cmd

    cmd=$(_invoke_build_tool "${build_system}" "${target}")

    if [ "${DRY_RUN:-}" = "1" ]; then
        _success "DRY RUN: ${cmd}"
        return 0
    fi

    if [[ "${build_system}" == *Makefiles* ]]; then
        make -j"$(_cpus)" "${target}"
        return $?
    elif [[ "${build_system}" == "Ninja" ]]; then
        ninja -j"$(_cpus)" "${target}"
        return $?
    else
        if command -v ninja >/dev/null 2>&1; then
            ninja -j"$(_cpus)" "${target}"
            return $?
        elif command -v make >/dev/null 2>&1; then
            make -j"$(_cpus)" "${target}"
            return $?
        else
            _error "No supported build tool found" "install ninja or make"
        fi
    fi
}

function _clone_vcpkg() {
    local dest="$1"
    local default=$(realpath "$VCPKG_ROOT")

    if [ -z "${dest}" ]; then
        dest=$( _prompt "Where to install vcpkg? (default: $default)" )
        dest="${dest:-$default}"
    fi
    if [ "${dest#~}" != "${dest}" ]; then
        dest="${dest/#\~/$HOME}"
    fi
    dest="${dest%/}"
    if [[ "${dest}" != */vcpkg ]]; then
        dest="${dest}/vcpkg"
    fi
    parent_dir="$(dirname "$dest")"
    mkdir -p "$parent_dir" || return 1
    git clone https://github.com/microsoft/vcpkg.git "${dest}" || return 1
    printf '%s' "${dest}"
}

function _bootstrap_vcpkg() {
    local dir="$1"
    if [ -z "${dir}" ]; then
        _info "No directory provided to _bootstrap_vcpkg"
        return 1
    fi
    if [ ! -d "${dir}" ]; then
        _info "vcpkg directory not found: ${dir}"
        return 1
    fi
    pushd "${dir}" > /dev/null || return 1
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*|WindowsNT)
            ./bootstrap-vcpkg.bat
            ;;
        *)
            ./bootstrap-vcpkg.sh
            ;;
    esac
    popd > /dev/null || true
}

function _install_and_bootstrap_vcpkg() {
    local answer

    while true; do
        answer=$( _prompt "vcpkg not detected, would you like to install it? (y/n)" )
        case "${answer}" in
            [Yy]*)
                _info "Installing vcpkg..."
                local dir
                dir=$( _clone_vcpkg ) || { _info "vcpkg clone failed"; break; }
                _bootstrap_vcpkg "${dir}"
                _success "Successfully installed vcpkg"
                break
                ;;
            [Nn]*)
                _info "Skipping vcpkg installation."
                break
                ;;
            *)
                _info "Please answer y or n."
                ;;
        esac
    done
}

function _setVcpkgTargets() {
    _info "Setting VCPKG targets"
    if [ -z "${VCPKG_ROOT:-}" ]; then
        VCPKG_ROOT="$HOME/vcpkg"
        _info "VCPKG_ROOT not set; defaulting to ${VCPKG_ROOT}"
    else
        if [ "$VCPKG_ROOT" != $(realpath "$VCPKG_ROOT") ]; then
            VCPKG_ROOT=$(realpath "$VCPKG_ROOT")
            _info "Expanded VCPKG_ROOT to ${VCPKG_ROOT}"
        fi
    fi

    if [ ! -d "$VCPKG_ROOT" ]; then
        if [ "${AUTO_VCPKG:-}" = "1" ]; then
            _info "Auto-installing vcpkg into ${VCPKG_ROOT}"
            dir=$( _clone_vcpkg "$VCPKG_ROOT" ) || { _info "vcpkg clone failed"; }
            if [ -n "${dir}" ]; then
                _bootstrap_vcpkg "${dir}"
                _success "Successfully installed vcpkg"
            fi
        else
            _install_and_bootstrap_vcpkg
        fi
    fi

    if [ -z "${VCPKG_TARGET_TRIPLET:-}" ]; then
        case "$(uname -s)" in
            Darwin)
                case "$(uname -m)" in
                    arm64) VCPKG_TARGET_TRIPLET="arm64-osx" ;;
                    *) VCPKG_TARGET_TRIPLET="x64-osx" ;;
                esac
                ;;
            Linux)
                case "$(uname -m)" in
                    aarch64|arm64) VCPKG_TARGET_TRIPLET="arm64-linux" ;;
                    *) VCPKG_TARGET_TRIPLET="x64-linux" ;;
                esac
                ;;
            MINGW*|MSYS*|CYGWIN*|WindowsNT)
                case "$(uname -m)" in
                    aarch64|arm64) VCPKG_TARGET_TRIPLET="arm64-windows" ;;
                    *) VCPKG_TARGET_TRIPLET="x64-windows" ;;
                esac
                ;;
            *)
                VCPKG_TARGET_TRIPLET="x64-linux" ;;
        esac
        _info "Auto-selected VCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET} based on platform"
    fi

    VCPKG_FLAGS=()
    if [ -f "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" ]; then
        VCPKG_FLAGS+=("-DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
        if [ -n "${VCPKG_TARGET_TRIPLET:-}" ]; then
            VCPKG_FLAGS+=("-DVCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}")
        fi
        _success "Using vcpkg toolchain from ${VCPKG_ROOT} (triplet=${VCPKG_TARGET_TRIPLET})"
    else
        _info "vcpkg toolchain file not found at ${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake; vcpkg integration skipped"
    fi
}

function _choose_build_system() {
    if [ -n "${CMAKE_GENERATOR:-}" ]; then
        printf '%s' "${CMAKE_GENERATOR}"
    elif ! command -v ninja >/dev/null 2>&1; then
        printf '%s' "Unix Makefiles"
    else
        printf '%s' "Ninja"
    fi
}

function _configure_and_build() {
    local build_type="$1"; shift
    local extra_cmake_flags=("$@")

    _ensure_tools
    _info "updating external submodules..."
    git submodule update --init --recursive
    _success "updated external submodules !"
    _setVcpkgTargets
    extra_cmake_flags+=("${VCPKG_FLAGS[@]}")
    mkdir -p build
    cd build || _error "mkdir failed" "could not cd into build/"
    local build_system=$(_choose_build_system)
    _info "configuring with CMake (${build_system}, ${build_type})..."
    cmake_cmd=(cmake .. -G "${build_system}" -DCMAKE_BUILD_TYPE="${build_type}" "${extra_cmake_flags[@]}")
    if [ "${DRY_RUN:-}" = "1" ]; then
        _success "DRY RUN: ${cmake_cmd[*]}"
        return 0
    fi
    "${cmake_cmd[@]}" || _error "cmake configuration failed" "check CMake output above"

    _info "building target r-type_server with ${build_system}..."
    if _run_build_tool "${build_system}" "r-type_server"; then
        _success "compiled r-type_server"
        exit 0
    else
        _error "compilation error" "failed to compile r-type_server"
    fi
}

function _all() {
    _configure_and_build "Release"
}

function _debug() {
    _fclean
    _configure_and_build "Debug" -DENABLE_DEBUG=ON
}

function _tests_run()
{
    _ensure_tools
    _setVcpkgTargets
    mkdir -p build
    cd build || _error "mkdir failed" "could not cd into build/"
    local build_system=$(_choose_build_system)
    _info "configuring tests with CMake (${build_system}, Debug)..."
    cmake .. -G "${build_system}" -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTS=ON "${VCPKG_FLAGS[@]}" \
        || _error "cmake configuration failed" "check CMake output above"

    _info "building unit tests (rtype_srv_unit_tests)…"
    if ! _run_build_tool "${build_system}" "rtype_srv_unit_tests"; then
        _error "unit tests compilation error" "failed to compile rtype_srv_unit_tests"
    fi
    cd .. || _error "cd failed" "could not cd .."

    _info "running unit tests…"
    if ! ./rtype_srv_unit_tests; then
        _error "unit tests error" "unit tests failed!"
    fi
    _success "unit tests succeed!"

    if [ "$(uname -s)" == 'Darwin' ]; then
        _info "generating coverage (llvm-cov)…"
        xcrun llvm-profdata merge -sparse unit_tests-*.profraw -o unit_tests.profdata
        xcrun llvm-cov report ./rtype_srv_unit_tests -instr-profile=unit_tests.profdata -object ./lib*.dylib -ignore-filename-regex='.*/tests/.*' -enable-name-compression > ../code_coverage.txt
        cd ..
    else
        if command -v gcovr >/dev/null 2>&1; then
            _info "generating coverage (gcovr)…"
            gcovr -r . --exclude tests/ > code_coverage.txt
        else
            _info "gcovr not found; skipping coverage generation."
            echo "gcovr not installed; coverage skipped." > code_coverage.txt
        fi
    fi
    cat code_coverage.txt
}

function _clean()
{
    rm -rf build
}

function _fclean()
{
    _clean
    rm -rf r-type_ecs \
        ./*.so ./*.dylib ./*.dll ./*.lib ./*.exp ./*.a ./*.ilk ./*.pdb \
        rtype_srv_unit_tests rtype_srv_unit_tests.exe r-type_server r-type_server.exe \
        unit_tests plugins code_coverage.txt unit_tests-*.profraw unit_tests.profdata vgcore* cmake-build-debug
}

if [ $# -eq 0 ]; then
    _all
fi

for args in "$@"
do
    case $args in
        --auto-vcpkg)
            AUTO_VCPKG=1
            if [ $# -eq 1 ]; then
                _all
            fi
            ;;
        --dry-run)
            DRY_RUN=1
            if [ $# -eq 1 ]; then
                _all
            fi
            ;;
        -h|--help)
            cat << EOF
USAGE:
      $0    builds r-type_server project

ARGUMENTS:
      $0 [-h|--help]    displays this message
      $0 [-d|--debug]   debug flags compilation
      $0 [-c|--clean]   clean the project
      $0 [-f|--fclean]  fclean the project
      $0 [-t|--tests]   run unit tests
      $0 [-r|--re]      fclean then rebuild (release)
EOF
            exit 0
            ;;
        -c|--clean)
            _clean
            exit 0
            ;;
        -f|--fclean)
            _fclean
            exit 0
            ;;
        -d|--debug)
            _debug
            exit 0
            ;;
        -t|--tests)
            _tests_run
            exit 0
            ;;
        -r|--re)
            _fclean
            _all
            exit 0
            ;;
        *)
            _error "Invalid arguments:" "$args"
            ;;
    esac
done

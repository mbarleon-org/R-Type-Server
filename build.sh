#!/usr/bin/env bash

GREEN="\033[1;32m"
RED="\033[1;31m"
ILC="\033[3m"
ORG="\033[1;33m"
RST="\033[0m"

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
    for t in git cmake ninja; do
        if ! command -v "$t" >/dev/null 2>&1; then
            _error "command '$t' not found" "please install '$t' (or use 'nix develop') 🤓"
        fi
    done
    _success "required tools found (git, cmake, ninja)"
}

function _configure_and_build() {
    local build_type="$1"; shift
    local extra_cmake_flags=("$@")

    _ensure_tools
    _info "updating external submodules..."
    git submodule update --init --recursive
    _success "updated external submodules !"

    mkdir -p build
    cd build || _error "mkdir failed" "could not cd into build/"
    _info "configuring with CMake (Ninja, ${build_type})..."
    cmake .. -G Ninja -DCMAKE_BUILD_TYPE="${build_type}" "${extra_cmake_flags[@]}" \
        || _error "cmake configuration failed" "check CMake output above"

    _info "building target r-type_server with Ninja..."
    if ninja -j"$(_cpus)" r-type_server; then
        _success "compiled r-type_server"
        exit 0
    fi
    _error "compilation error" "failed to compile r-type_server"
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
    mkdir -p build
    cd build || _error "mkdir failed" "could not cd into build/"
    _info "configuring tests with CMake (Ninja, Debug)..."
    cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTS=ON \
        || _error "cmake configuration failed" "check CMake output above"

    _info "building unit tests (rtype_srv_unit_tests)…"
    if ! ninja -j"$(_cpus)" rtype_srv_unit_tests; then
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
        *.so *.dylib *.dll *.lib *.exp *.a \
        rtype_srv_unit_tests rtype_srv_unit_tests.exe r-type_server r-type_server.exe \
        unit_tests plugins code_coverage.txt unit_tests-*.profraw unit_tests.profdata vgcore* cmake-build-debug
}

if [ $# -eq 0 ]; then
    _all
fi

for args in "$@"
do
    case $args in
        -h|--help)
            cat << EOF
USAGE:
      $0    builds r-type_server project (Ninja)

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

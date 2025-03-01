#!/usr/bin/env bash

# hack of https://betterdev.blog/minimal-safe-bash-script-template
# because LOL bash scripting. added comments from the post

# stop if a command fails
# -e : command fails
# -u : 
set -Eeuo pipefail
trap cleanup SIGINT SIGTERM ERR EXIT

# attempt to change to the directory of this script
script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd -P)

usage() {
  cat << EOF
$(basename "${BASH_SOURCE[0]}") [options] {test ids}

helper script for piping data from makedata to PractRand. Mainly to
simply repeat the same test on a series of different generators.

example: run what I'm considering the standard suite on vprng which
is 10 repeatable tests (makedata --test-id on [0,9]) with data sizes
from 2MB to 512GB.

  ./practrand_run.sh 0 1 2 3 4 5 6 7 8 9

The composed command is echoed to the terminal. If PractRand hits
a clear fail then that run will be stop but remaining runs will
proceed.

script options:
  -h, --help      print this help and exit

makedata options:
  --cvprng        cvprng instead of vprng
  --channel [n]   only channel 'n' (disabled)
  --32            32-bit instead of 64-bit channels (disabled)

PractRand options:
  --lo [n]        inital number of bytes to test  (-tlmin)
  --hi [n]        maximum number of bytes to test (-tlmax)
  --seed [n]      -seed
EOF
  exit
}

cleanup() {
  trap - SIGINT SIGTERM ERR EXIT
  # script cleanup here
}

setup_colors() {
  if [[ -t 2 ]] && [[ -z "${NO_COLOR-}" ]] && [[ "${TERM-}" != "dumb" ]]; then
    NOFORMAT='\033[0m' RED='\033[0;31m' GREEN='\033[0;32m' ORANGE='\033[0;33m' BLUE='\033[0;34m' PURPLE='\033[0;35m' CYAN='\033[0;36m' YELLOW='\033[1;33m' BOLD='\033[1m'
  else
    NOFORMAT='' RED='' GREEN='' ORANGE='' BLUE='' PURPLE='' CYAN='' YELLOW='' BOLD=''
  fi
}

msg() {
  echo >&2 -e "${1-}"
}

die() {
  local msg=$1
  local code=${2-1} # default exit status 1
  msg "$msg"
  exit "$code"
}

LO=2MB
HI=512GB
PRNG="vprng"
TYPE="all"
MOPT=""
SEED=1


parse_params() {
  # default values of variables set from params
  flag=0
  param=''

  while :; do
    case "${1-}" in
    -h | --help) usage ;;
    --debug) set -x ;;
    --no-color) NO_COLOR=1 ;;
    -f | --flag) flag=1 ;; # example flag
    -p | --param) # example named parameter
      param="${2-}"
      shift
      ;;
    --cvprng) MOPT="${MOPT} --cvprng"
	      PRNG="cvprng" ;;
    --hi) HI="${2-}"
	  shift ;;
    --lo) LO="${2-}"
	  shift ;;
    -?*) die "unknown option: $1" ;;
    *) break ;;
    esac
    shift
  done

  args=("$@")

  # check required params and arguments
  # [[ -z "${param-}" ]] && die "missing required parameter: param"
  [[ ${#args[@]} -eq 0 ]] && die "missing test ids"

  return 0
}

parse_params "$@"
setup_colors

# run the test(s)
for arg in "${args[@]}"; do
    FILE=data/practrand_${PRNG}_${TYPE}_${arg}.txt

    # show the command to be executed in the terminal
    msg "${GREEN}./makedata ${MOPT} --test-id=${arg} | RNG_test stdin64 -tlmin $LO -tlmax $HI -seed ${SEED} >> ${FILE} ${NOFORMAT}"  # temp hack
    msg ""

    # also place it at the top of the output file
    echo "./makedata ${MOPT} --test-id=${arg} | RNG_test stdin64 -tlmin $LO -tlmax $HI -seed ${SEED}" >> $FILE

    # and the --dryrun option output as well (info is in stderr. stdout feeds RNG_test)
    ./makedata ${MOPT} --test-id=${arg} --dryrun 2> $FILE 
    
    # run the actual test
    # the "|| true" is needed because there'll be a SIGPIPE (exit code 141)
    ./makedata ${MOPT} --test-id=${arg} | RNG_test stdin64 -tlmin $LO -tlmax $HI -seed ${SEED} | tee -a $FILE 

    msg "${GREEN}----------------------------------------------------------------------------${NOFORMAT}"
done


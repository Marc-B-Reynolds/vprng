#!/usr/bin/env bash

# hack of https://betterdev.blog/minimal-safe-bash-script-template
# because LOL bash scripting. added comments from the post

# TODO:
# add single generator testing support
# add more practrand options support.  '-tf 2' is catching defects very fast

# stop if some things go sideways
# -e : command return non-zero status
# -u : undefine bash variable
# pipefail: if any a list of piped commands have non-zero status
set -Eeuo pipefail
trap cleanup SIGINT SIGTERM ERR EXIT

# attempt to change to the directory of this script
script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" &>/dev/null && pwd -P)

usage() {
  cat << EOF
$(basename "${BASH_SOURCE[0]}") [options] {test ids}

helper script for piping data from makedata to PractRand. Mainly to
simply repeat the same test on a series of different generators
and to avoid dumb commandline modification mistakes.

example: run what I'm considering the standard suite on vprng which
is 10 repeatable tests (makedata --test-id on [0,9]) with data sizes
from 2MB to 512GB.

  ./practrand_run.sh 0 1 2 3 4 5 6 7 8 9

The composed command is echoed to the terminal. If PractRand hits
a clear fail then that run will be stop but remaining runs will
proceed. Each run is written out to the file. Example: 0 is
written to:

  data/practrand_vprng_all_0.txt

script options:
  -h, --help      print this help and exit

makedata options:
  --alt name      uses makedata_name where name is variant defined
                  by "name.h". 
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
POPT=""
SEED=1
MAKEDATA="makedata"
FPREFIX="practrand"
COMBINED=""

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
	      COMBINED="c" ;;
    --hi) HI="${2-}"
	  shift ;;
    --lo) LO="${2-}"
	  shift ;;
    --alt) ALT="${2-}"
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

# hacky auto build the makedata variant if it isn't compiled
if [[ -v ALT ]]; then
    MAKEDATA="makedata_${ALT}"
    PRNG="${ALT}"

  if [ -f "${MAKEDATA}" ] && [ -x "${MAKEDATA}" ]; then
      : 
  else
      msg "${GREEN}----------------------------------------------------------------------------${NOFORMAT}"
      msg "${YELLOW}NOTE: ${GREEN}${MAKEDATA}${NOFORMAT}: not found so trying ${BOLD}make${NOFORMAT}"

      status=0
      
      make ${MAKEDATA} 2> /dev/null || status=$?
      if [ $status -eq 0 ]; then
         msg "${YELLOW}success${NOFORMAT}"
      else
	 if [ $status -eq 2 ]; then
           msg "  no make target so trying manually"
	   status=0

	   # use CC or first of {gcc,clang,cc}  (+x thing is -u hack)
	   if [ -z "${CC+x}" ]; then
  	     CC=$(command -v gcc || command -v clang || echo cc)
	   fi

	   ${CC} -DVPRNG_INCLUDE=\"${ALT}.h\" -g3 -O3 -I..  -march=native makedata.c -o ${MAKEDATA} || status=$?
	 else
           die "${RED}error${NOFORMAT}: make failed"
	 fi
      fi

      msg "${GREEN}----------------------------------------------------------------------------${NOFORMAT}"
  fi
else
    : 
fi


# validate parameters are all integers on [0,255] (for test id values)
for arg in "${args[@]}"; do
  if [[ "$arg" =~ ^[0-9]+$ ]] && [ "$arg" -le 255 ]; then
    continue
  else
    die "${RED}error${NOFORMAT}: parameters must be integers on [0,255]  (got: $arg)"
 fi
done

# validate RNG_test range options
if [[ "$HI" =~ ^[0-9]+(|K|M|G|T|P)B?$ ]]; then
  :
else
  die "${RED}error${NOFORMAT}: ${GREEN}--hi${NOFORMAT}  (got: $HI)"
fi

if [[ "$LO" =~ ^[0-9]+(|K|M|G|T|P)B?$ ]]; then
  :
else
  die "${RED}error${NOFORMAT}: ${GREEN}--lo${NOFORMAT}  (got: $LO)"
fi


# run the test(s)
FILEBASE="data/${FPREFIX}_${COMBINED}${PRNG}_${TYPE}"

run_tests () {

for arg in "${args[@]}"; do
    FILE="${FILEBASE}_${arg}.txt"

    # show the command to be executed in the terminal
    msg "${GREEN}./${MAKEDATA} ${MOPT} --test-id=${arg} | RNG_test stdin64 ${POPT} -tlmin $LO -tlmax $HI -seed ${SEED} >> ${FILE} ${NOFORMAT}"  # temp hack
    msg ""

    # also place it at the top of the output file
    echo "./${MAKEDATA} ${MOPT} --test-id=${arg} | RNG_test stdin64 ${POPT} -tlmin $LO -tlmax $HI -seed ${SEED}" >> $FILE

    # and the --dryrun option output as well (info is in stderr. stdout feeds RNG_test et al.)
    ./${MAKEDATA} ${MOPT} --test-id=${arg} --dryrun 2> $FILE 
    
    # run the actual test
    #   sending any stderr from 'makedata' to FILE
    #   the "|| true" is needed because there'll be a SIGPIPE (exit code 141) when 'RNG_test' closes the stream
    ./${MAKEDATA} ${MOPT} --test-id=${arg} 2> >(tee -a $FILE >&2) | RNG_test stdin64 ${POPT} -tlmin $LO -tlmax $HI -seed ${SEED} | tee -a $FILE || true

    msg "${GREEN}----------------------------------------------------------------------------${NOFORMAT}"
done
}


run_tests


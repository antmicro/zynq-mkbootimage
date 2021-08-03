#!/bin/sh

# CONFIG -------------------------------------------------- #
# Remember useful directories
DIR=$(git rev-parse --show-toplevel)
TESTS=$DIR/tests
LOG=$TESTS/results.log
PARSER=$TESTS/parser

# Set aliases to color control sequences
RESET=$(printf "\e[1;0m")
GREEN=$(printf "\e[1;32m")
RED=$(printf "\e[1;31m")

# Set pass/fail counters
pass=0
fail=0

# The routine implements a test pair. The kind of test
# is determined by the $1 argument.
#
# Negative tests work only on parser/bad_* files.
# The other files are expected to be correct.
testparser() {
  for file in $PARSER/*; do
    printf "\nLogs for $(basename $file):\n" >> $LOG

    base=$(basename $file)
    negative=$(expr 0 "<" "(" $base : "^bad_" ")" )

    # XOR between program's success and test positivity
    $DIR/mkbootimage -p -u $file 1> /dev/null 2>> $LOG
    if [ $(expr $? = 0) != $negative ]; then
      pass=$(expr $pass + 1)
      msg="${GREEN}pass"
    else
      fail=$(expr $fail + 1)
      msg="${RED}fail"
    fi
    printf "%s$RESET: %s\n" $msg $(basename $file)
  done
}

# It is encouraged for future tests to be placed here
# and implemented in an analogous way with the `testparser`
# test routine, with both negative and positive tests.

# The test files should be placed in a subdirectory of
# a name describing their role, like parser/ is for
# parser test files.

# TESTING ------------------------------------------------- #
# Check whether the mkbootimage binary exists
if [ ! -f "$DIR/mkbootimage" ]; then
  printf "Build a mkbootimage binary before testing\n"
  exit 1
fi

# Init the log with a date information
printf "Performed on $(date)\n" > $LOG

# Perform parser tests
testparser

# RESULT INFORMATION -------------------------------------- #
printf "\npassed: %s\nfailed: %s\n\n" $pass $fail
if [ $fail -eq 0 ]; then
  printf "Everything ${GREEN}PASSED${RESET}\n"
  exit 0
else
  printf "Something ${RED}FAILED${RESET}\n"
  printf "Read %s to investigate details\n" $(basename $LOG)
  exit 1
fi


#!/bin/sh

# CONFIG -------------------------------------------------- #
# Remember useful directories
ORIGIN=$(pwd)
DIR=$(git rev-parse --show-toplevel)
TESTS=$DIR/tests
LOG=$TESTS/results.log
PARSER=$TESTS/parser
EXTRACT=$TESTS/extraction
OFFSETS=$TESTS/offsets

cd $DIR

# Set aliases to color control sequences
RESET=$(printf "\e[1;0m")
GREEN=$(printf "\e[1;32m")
RED=$(printf "\e[1;31m")

# Set pass/fail counters
pass=0
fail=0

# Define routines for passing and failing
passtest() {
  pass=$(expr $pass + 1)
  printf "${GREEN}pass${RESET}: %s\n" "$1"
}

failtest() {
  fail=$(expr $fail + 1)
  printf "${RED}fail${RESET}: %s\n" "$1"
}

# The routine implements a test pair. The kind of test
# is determined by the $1 argument.
#
# Negative tests work only on parser/bad_* files.
# The other files are expected to be correct.
testparser() {
  for file in $PARSER/*; do
    printf "\nLogs for $(basename $file) parsing:\n" >> $LOG

    base=$(basename $file)
    negative=$(expr 0 "<" "(" $base : "^bad_" ")" )

    # XOR between program's success and test positivity
    $DIR/mkbootimage -p -u $file 1> /dev/null 2>> $LOG
    if [ $(expr $? = 0) != $negative ]; then
      passtest $(basename $file)
    else
      failtest $(basename $file)
    fi
  done
}

# Create a fake boot image out of project's files, then
# unpack it and compar with the original ones.
testextraction() {
  # Create a simple BIF from the list in the "files" file
  BIF=$EXTRACT/boot.bif
  BIN=$EXTRACT/boot.bin
  TMP=$EXTRACT/tmp

  printf "the_rom_image:{" > $BIF
  for file in $(cat $EXTRACT/files); do
    printf "%s " $file >> $BIF
  done
  printf "}" >> $BIF

  # Create a dummy boot image and unpack it
  printf "\nLogs for image extraction:\n" >> $LOG
  $DIR/mkbootimage -u $BIF $BIN 1> /dev/null 2>> $LOG

  # Extract the files back
  mkdir $TMP
  cd $TMP
  $DIR/exbootimage -ux $BIN 1> /dev/null 2>> $LOG

  # Test the files
  for file in $(cat $EXTRACT/files); do
    printf "\nLogs for $(basename $file) extraction:\n" >> $LOG

    if diff $file $TMP/$(basename $file) 1> /dev/null 2>> $LOG; then
      passtest $(basename $file)
    else
      failtest $(basename $file)
    fi
  done

  rm $BIF $BIN
  rm -rf $TMP
  cd $DIR
}

# Check if wrong offsets are detected properly
testoffseterrors() {
  for file in $OFFSETS/*; do
    printf "\nLogs for $(basename $file) offsets:\n" >> $LOG

    base=$(basename $file)
    negative=$(expr 0 "<" "(" $base : "^bad_" ")" )
    $DIR/exbootimage -l $file 1> /dev/null 2>> $LOG

    # XOR between program's success and test positivity
    if [ $(expr $? = 0) != $negative ]; then
      passtest $(basename $file)
    else
      failtest $(basename $file)
    fi
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

if [ ! -f "$DIR/exbootimage" ]; then
  printf "Build an exbootimage binary before testing\n"
  exit 1
fi

# Init the log with a date information
printf "Performed on $(date)\n" > $LOG

# Perform parser tests
testparser
testextraction
testoffseterrors

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


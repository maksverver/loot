#!/bin/bash
#
# Example loot box script, which will create a file called "dummy" in the user's
# home directory whenever the box is open. This isn't useful for anything; it's
# just an example to demonstrate how to write loot box scripts.

# Set option to exit with an error as soon as any command fails.
set -e

# Path to the file we will create when the box is opened.
DUMMY_FILE=$HOME/dummy

# The command to execute is passed as the first argument to the script.
case "$1" in

status)
	# We should respond with a single line of output: "opened" or "closed",
	# depending on whether the file exists or not.
	if [ -e "$DUMMY_FILE" ]; then
		echo opened
	else
		echo closed
	fi
	;;

open)
	# Create an empty file (if it does not exist).
	touch "$DUMMY_FILE"
	;;

close)
	# Before removing the file, first check if it has nonzero size. In that
	# case, it was not created by this script, and we don't want to remove
	# it (in case it contains something important).
	if [ -s "$DUMMY_FILE" ]; then
		echo "$DUMMY_FILE is not empty"
		exit 1
	fi
	# Now just delete the file. The -f flag makes sure the command succeeds
	# even if the file did not exist. This is not strictly necessary, but
	# it's nice if "open" and "close" are idempotent operations.
	rm -f "$DUMMY_FILE"
	;;

*)
	echo "Unknown command: '$1'!" >&2
	exit 1

esac

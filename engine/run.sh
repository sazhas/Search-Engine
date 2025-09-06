#!/bin/bash

# Log file locations
LOG1="crawler.log"
LOG2="html_parser.log"

# Function to run a program in a loop
run_program() {
    local program="$1"
    local logfile="$2"

    while true; do
        echo "$(date): Starting $program" >> "$logfile"
        $program >> "$logfile" 2>&1
        echo "$(date): $program crashed or exited, restarting..." >> "$logfile"
        sleep 1
    done
}

# Trap Ctrl+C to exit cleanly
trap 'echo "Stopping all programs..."; kill $PID1 $PID2 2>/dev/null; exit 0' INT

# Start the first program in the background
run_program "./crawler" "$LOG1" &
PID1=$!

# Start the second program in the background
run_program "./html_parser" "$LOG2" &
PID2=$!

echo "Both programs are running. Press Ctrl+C to stop."
echo "Logs: $LOG1 and $LOG2"

# Wait for both background processes
wait

#!/bin/bash

# GitHut Automated Data Refresh Scheduler
#
# This script helps set up automated scheduling for GitHut data refresh.
# It can be run manually or configured as a cron job.

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Path to the refresh script
REFRESH_SCRIPT="$SCRIPT_DIR/refresh.js"

# Path to log file
LOG_FILE="$SCRIPT_DIR/logs/scheduled-refresh.log"

# Create logs directory if it doesn't exist
mkdir -p "$SCRIPT_DIR/logs"

# Function to log messages
log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOG_FILE"
}

# Check if Node.js is installed
if ! command -v node &> /dev/null; then
    log "ERROR: Node.js is not installed. Please install Node.js first."
    exit 1
fi

# Check if MongoDB is running
if ! pgrep -x "mongod" > /dev/null; then
    log "WARNING: MongoDB does not appear to be running. Starting refresh anyway..."
fi

# Run the refresh script
log "Starting GitHut data refresh..."
log "Command: node $REFRESH_SCRIPT"

cd "$SCRIPT_DIR"

if node "$REFRESH_SCRIPT" >> "$LOG_FILE" 2>&1; then
    log "Data refresh completed successfully"
    exit 0
else
    log "ERROR: Data refresh failed with exit code $?"
    exit 1
fi

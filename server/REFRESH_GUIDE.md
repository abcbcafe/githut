# GitHut Data Refresh Guide

Complete guide for refreshing GitHut data with the latest information from GitHub Archive.

## Table of Contents

1. [Overview](#overview)
2. [Setup](#setup)
3. [Running the Refresh](#running-the-refresh)
4. [Configuration](#configuration)
5. [Troubleshooting](#troubleshooting)
6. [Maintenance](#maintenance)

## Overview

The GitHut data refresh system automatically:

1. Downloads hourly data from GitHub Archive (http://data.githubarchive.org/)
2. Processes and aggregates the data by language and event type
3. Stores the data in MongoDB for efficient querying
4. Generates CSV exports for the D3.js visualizations
5. Maintains backward compatibility with the existing visualization code

**Data Pipeline:**

```
GitHub Archive → Download → MongoDB → Aggregate → CSV → D3.js Visualizations
```

## Setup

### Prerequisites

Ensure you have the following installed:

- **Node.js** v8 or higher
- **MongoDB** v3.0 or higher
- **npm** (comes with Node.js)

### Installation Steps

1. **Install Node.js dependencies:**

   ```bash
   cd server
   npm install
   ```

2. **Start MongoDB:**

   ```bash
   # If MongoDB is not running, start it:
   sudo systemctl start mongodb
   # or
   sudo service mongodb start
   # or (macOS with Homebrew)
   brew services start mongodb-community
   ```

3. **Verify MongoDB is running:**

   ```bash
   # Check if MongoDB is accepting connections
   mongo --eval "db.adminCommand('ping')"
   ```

4. **Configure the refresh settings (optional):**

   Edit `server/refresh-config.js` to customize:
   - MongoDB connection settings
   - Date ranges
   - Export options
   - Logging preferences

## Running the Refresh

### Option 1: Direct Node.js Execution

```bash
cd server
node refresh.js
```

This will:
- Check MongoDB for the last processed date
- Download all new data since then
- Generate updated CSV files

### Option 2: Using the Shell Script

```bash
cd server
./schedule-refresh.sh
```

This wrapper script:
- Checks prerequisites
- Runs the refresh
- Logs output to `logs/scheduled-refresh.log`

### Command Line Options

```bash
# Show help (if implemented)
node refresh.js --help

# Force re-download all data (ignores existing data)
node refresh.js --force

# Download specific date range
node refresh.js --from 2024-01-01 --to 2024-03-31

# Only regenerate CSV exports (don't download new data)
node refresh.js --skip-download

# Only download data (don't generate exports)
node refresh.js --skip-export
```

### Incremental Updates

By default, the refresh script is **incremental**:

- It checks MongoDB for the last processed hourly file
- Downloads only new data since that time
- This makes subsequent runs much faster

### Full Refresh

To completely rebuild your dataset:

```bash
# 1. Clear existing MongoDB data
mongo github --eval "db.events.drop(); db.languages.drop();"

# 2. Run with --force flag
node refresh.js --force
```

## Configuration

### refresh-config.js

Edit this file to customize behavior:

#### MongoDB Settings

```javascript
mongodb: {
  host: "127.0.0.1",      // MongoDB host
  port: 27017,            // MongoDB port
  database: "github",     // Database name
  user: "",               // Username (if auth enabled)
  password: ""            // Password (if auth enabled)
}
```

#### Data Collection Settings

```javascript
dataCollection: {
  lookbackMonths: 3,      // How far back to look for missing data
  dataLagHours: 8,        // GitHub Archive data availability lag
  archiveUrl: "http://data.githubarchive.org",
  dataPath: "./data/"     // Local cache directory
}
```

#### Export Settings

```javascript
export: {
  outputPath: "./exports/",
  generateTimeSeries: true,      // active_quarters.csv
  generateQuarterly: true,       // q1-2024.csv, etc.
  generateLanguageMetadata: true // languages.csv
}
```

#### Logging Settings

```javascript
logging: {
  level: "info",                    // debug, info, warn, error
  logToFile: true,
  logFilePath: "./logs/refresh.log"
}
```

## Output Files

The refresh script generates these CSV files in `server/exports/`:

### 1. Quarterly Files (q1-2024.csv, q2-2024.csv, etc.)

Used by the **Parallel Coordinates** visualization.

Format:
```csv
repository_language,type,active_repos_by_url,events,year,quarter
JavaScript,PushEvent,294832,3068862,2024,1
Python,PushEvent,142272,1616028,2024,1
...
```

### 2. Time Series File (active_quarters.csv)

Used by the **Small Multiples** visualization.

Format:
```csv
repository_language,active_repos_by_url,year,quarter
JavaScript,294832,2024,1
JavaScript,301245,2024,2
...
```

### 3. Language Metadata (languages.csv)

Lists all languages with total event counts.

Format:
```csv
name,total_events
JavaScript,50000000
Python,30000000
...
```

## Troubleshooting

### "MongoDB connection failed"

**Problem:** Cannot connect to MongoDB

**Solutions:**
1. Check if MongoDB is running: `ps aux | grep mongod`
2. Start MongoDB: `sudo systemctl start mongodb`
3. Verify connection settings in `refresh-config.js`
4. Check MongoDB logs: `/var/log/mongodb/mongod.log`

### "Download failed" or "404 Not Found"

**Problem:** GitHub Archive file not available

**Solutions:**
1. GitHub Archive has a delay of ~8 hours (configured in `dataLagHours`)
2. Some historical files may be missing - the script will skip them
3. Check the URL manually: http://data.githubarchive.org/2024-01-01-0.json.gz

### "Disk space" errors

**Problem:** Not enough disk space

**Solutions:**
1. Check available space: `df -h`
2. Clean up old data files: `rm server/data/*.json*`
3. Compact MongoDB: `mongo github --eval "db.runCommand({compact: 'events'})"`

### Script runs but no CSV files generated

**Problem:** Export phase failed

**Solutions:**
1. Check if MongoDB has data: `mongo github --eval "db.events.count()"`
2. Check logs: `cat server/logs/refresh.log`
3. Run with debug logging: Edit `refresh-config.js` and set `level: "debug"`
4. Try export only: `node refresh.js --skip-download`

### "Cannot find module" errors

**Problem:** Missing Node.js dependencies

**Solution:**
```bash
cd server
npm install
```

## Maintenance

### Regular Tasks

1. **Monitor disk space:**
   ```bash
   df -h
   du -sh /var/lib/mongodb
   ```

2. **Check logs periodically:**
   ```bash
   tail -100 server/logs/refresh.log
   ```

3. **Backup MongoDB:**
   ```bash
   mongodump --db github --out /backup/mongodb/
   ```

4. **Clean old data files (optional):**
   ```bash
   # The raw .json files are not needed after processing
   find server/data -name "*.json" -mtime +30 -delete
   find server/data -name "*.json.gz" -mtime +30 -delete
   ```

### Database Maintenance

```bash
# Check database size
mongo github --eval "db.stats()"

# Compact collections to reclaim space
mongo github --eval "db.runCommand({compact: 'events'})"
mongo github --eval "db.runCommand({compact: 'languages'})"

# Create indexes for better performance (optional)
mongo github --eval "db.events.createIndex({date: 1})"
mongo github --eval "db.languages.createIndex({date: 1})"
```

### Monitoring

Create a simple monitoring script (`monitor.sh`):

```bash
#!/bin/bash

# Check if last refresh was successful
LAST_REFRESH=$(stat -c %Y server/exports/active_quarters.csv 2>/dev/null)
NOW=$(date +%s)
HOURS_SINCE=$(( (NOW - LAST_REFRESH) / 3600 ))

if [ $HOURS_SINCE -gt 48 ]; then
    echo "WARNING: Data is $HOURS_SINCE hours old!"
    exit 1
fi

echo "OK: Data refreshed $HOURS_SINCE hours ago"
```

## Best Practices

1. **Start with a small date range** to test:
   ```bash
   node refresh.js --from 2024-01-01 --to 2024-01-02
   ```

2. **Monitor the first few runs** to ensure everything works correctly

3. **Set up appropriate refresh frequency:**
   - Daily: Good for keeping data current
   - Weekly: Balanced approach for most use cases
   - Monthly: Suitable for historical analysis

4. **Keep logs** for at least 30 days for troubleshooting

5. **Backup before major updates:**
   ```bash
   mongodump --db github --out backup-$(date +%Y%m%d)
   cp -r server/exports exports-backup-$(date +%Y%m%d)
   ```

6. **Document any configuration changes** you make

## Performance Tips

1. **Use incremental updates** instead of full refreshes when possible

2. **Run during off-peak hours** (e.g., 2-4 AM) to minimize impact

3. **Adjust batch size** in `refresh-config.js` if you have memory constraints

4. **Monitor MongoDB memory usage:**
   ```bash
   mongostat
   ```

5. **Consider data retention policies** - you may not need every hourly file forever

## Getting Help

If you encounter issues:

1. Check the logs: `server/logs/refresh.log`
2. Run with debug logging: Set `level: "debug"` in `refresh-config.js`
3. Test MongoDB connection: `mongo github --eval "db.stats()"`
4. Verify GitHub Archive is accessible: `curl -I http://data.githubarchive.org/2024-01-01-0.json.gz`
5. Review this guide and `CRON_SETUP.md`

For GitHub Archive documentation:
- Website: http://www.githubarchive.org/
- GitHub: https://github.com/igrigorik/githubarchive.org

## Example Workflow

Here's a complete example of setting up and running the refresh:

```bash
# 1. Setup
cd /path/to/githut/server
npm install
sudo systemctl start mongodb

# 2. Test with a small date range
node refresh.js --from 2024-01-01 --to 2024-01-07

# 3. Check the output
ls -lh exports/
head -20 exports/active_quarters.csv

# 4. If successful, run a full refresh
node refresh.js

# 5. Set up automated refresh
chmod +x schedule-refresh.sh
crontab -e
# Add: 0 3 * * * /path/to/githut/server/schedule-refresh.sh

# 6. Monitor
tail -f logs/scheduled-refresh.log
```

## Appendix: Data Format Reference

### GitHub Archive Event Format

Each event in GitHub Archive contains:
- `type`: Event type (PushEvent, WatchEvent, etc.)
- `repository`: Repository information
  - `language`: Programming language
  - `url`: Repository URL
- `created_at`: Timestamp

### Supported Event Types

The refresh script processes these GitHub event types:
- **PushEvent**: Code pushes
- **WatchEvent**: Repository stars
- **ForkEvent**: Repository forks
- **IssuesEvent**: Issue activity
- **CreateEvent**: Repository/branch creation

These match the original GitHut implementation and Google BigQuery queries.

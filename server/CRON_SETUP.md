# GitHut Automated Data Refresh - Cron Setup Guide

This guide explains how to set up automated data refresh for GitHut using cron.

## Prerequisites

1. **MongoDB** must be installed and running
2. **Node.js** must be installed (with npm)
3. All dependencies must be installed: `npm install`

## Quick Start

### Option 1: Manual Execution

Run the refresh script manually whenever you want to update the data:

```bash
cd server
node refresh.js
```

### Option 2: Using the Shell Script

Make the shell script executable and run it:

```bash
cd server
chmod +x schedule-refresh.sh
./schedule-refresh.sh
```

### Option 3: Automated Scheduling with Cron

Set up a cron job to run the refresh automatically on a schedule.

## Cron Configuration Examples

### Daily Refresh at 3 AM

```bash
# Edit your crontab
crontab -e

# Add this line (replace /path/to/githut with your actual path):
0 3 * * * /path/to/githut/server/schedule-refresh.sh
```

### Weekly Refresh (Every Sunday at 2 AM)

```bash
0 2 * * 0 /path/to/githut/server/schedule-refresh.sh
```

### Monthly Refresh (First day of month at 1 AM)

```bash
0 1 1 * * /path/to/githut/server/schedule-refresh.sh
```

### Every 6 Hours

```bash
0 */6 * * * /path/to/githut/server/schedule-refresh.sh
```

## Cron Time Format

```
* * * * * command to execute
│ │ │ │ │
│ │ │ │ └─── Day of week (0-7, Sunday = 0 or 7)
│ │ │ └───── Month (1-12)
│ │ └─────── Day of month (1-31)
│ └───────── Hour (0-23)
└─────────── Minute (0-59)
```

## Recommended Schedule

For production use, we recommend:

- **Daily refresh** if you want near real-time data
- **Weekly refresh** for moderate freshness with lower resource usage
- **Monthly refresh** for archived/historical analysis

GitHub Archive data is available with a delay of several hours, so more frequent than daily updates may not provide much benefit.

## Verifying Cron Setup

1. **List your current cron jobs:**
   ```bash
   crontab -l
   ```

2. **Check if cron service is running:**
   ```bash
   # On Ubuntu/Debian
   sudo service cron status

   # On CentOS/RHEL
   sudo service crond status

   # On systems with systemd
   sudo systemctl status cron
   ```

3. **Check the logs:**
   ```bash
   tail -f /path/to/githut/server/logs/scheduled-refresh.log
   ```

## Troubleshooting

### Cron Job Not Running

- **Check cron logs:** `/var/log/cron` or `/var/log/syslog`
- **Verify script permissions:** `chmod +x schedule-refresh.sh`
- **Use absolute paths:** Cron has a minimal environment, always use full paths
- **Check PATH:** Cron may not have Node.js in its PATH

### Script Failing

- **Check MongoDB:** Ensure MongoDB is running
- **Check Node.js:** Verify Node.js is installed and accessible
- **Review logs:** Check `server/logs/refresh.log` for detailed error messages
- **Test manually:** Run `./schedule-refresh.sh` to see errors directly

### Disk Space Issues

GitHub Archive data can be large. Monitor your disk space:

```bash
df -h
```

Consider cleaning old data files:

```bash
# Remove old downloaded archive files (optional)
rm -f /path/to/githut/server/data/*.json
rm -f /path/to/githut/server/data/*.json.gz
```

## Advanced Configuration

### Environment Variables for Cron

If you need to set environment variables, create a wrapper script or use cron's environment settings:

```bash
# In crontab:
SHELL=/bin/bash
PATH=/usr/local/bin:/usr/bin:/bin
MONGODB_HOST=localhost
MONGODB_PORT=27017

0 3 * * * /path/to/githut/server/schedule-refresh.sh
```

### Email Notifications

Cron can email you the output of jobs. Set up email in crontab:

```bash
MAILTO=your-email@example.com
0 3 * * * /path/to/githut/server/schedule-refresh.sh
```

### Running with Different Options

Edit `schedule-refresh.sh` to pass options to the refresh script:

```bash
# Only regenerate exports (don't download new data)
node "$REFRESH_SCRIPT" --skip-download

# Force re-download all data
node "$REFRESH_SCRIPT" --force

# Download specific date range
node "$REFRESH_SCRIPT" --from 2024-01-01 --to 2024-12-31
```

## Docker/Container Environments

If running in Docker, use Docker's cron or an external scheduler:

```dockerfile
# In your Dockerfile
RUN apt-get update && apt-get install -y cron
COPY server/schedule-refresh.sh /etc/cron.daily/githut-refresh
RUN chmod +x /etc/cron.daily/githut-refresh
```

## Monitoring

Consider adding monitoring to track refresh success:

1. **Check last modification time of CSV files:**
   ```bash
   ls -lt server/exports/
   ```

2. **Set up alerts** if refresh fails
3. **Monitor MongoDB size:**
   ```bash
   du -sh /var/lib/mongodb
   ```

## Best Practices

1. **Test first:** Run the refresh manually before setting up cron
2. **Monitor initially:** Watch the first few automated runs
3. **Set appropriate frequency:** Balance freshness vs. resource usage
4. **Keep logs:** Retain logs for troubleshooting
5. **Document changes:** Keep notes on configuration changes
6. **Backup data:** Regular backups of MongoDB and CSV exports

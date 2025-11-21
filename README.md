## GitHut

GitHut (http://githut.info) is an attempt to visualize and explore the complexity of the universe of programming languages used across the repositories hosted on GitHub.

Programming languages are not simply the tool developers use to create programs or express algorithms but also instruments to code and decode creativity. By observing the history of languages we can enjoy the quest of humankind for a better way to solve problems, to facilitate collaboration between people and to reuse the effort of others.

Github is the largest code host in the world, with 3.5 million users. It's the place where the open-source development community offers access to most of its projects. By analyzing how languages are used in GitHub it is possible to understand the popularity of programming languages among developers and also to discover the unique characteristics of each language. 

The visualization is based on two type of visualization: a Parallel Coordinates chart and a Small Multiples visualization.

Data is from Github Archive (http://www.githubarchive.org/).

### Automated Data Refresh

GitHut now includes an automated data refresh system that can update the visualizations with the latest GitHub Archive data while maintaining the original visualization style.

#### Quick Start

```bash
# Install dependencies
cd server
npm install

# Run a manual data refresh
node refresh.js

# Or use the shell script
chmod +x schedule-refresh.sh
./schedule-refresh.sh
```

#### Features

- **Automated Download**: Automatically downloads new data from GitHub Archive
- **Incremental Updates**: Picks up from where it left off, processing only new data
- **MongoDB Storage**: Stores raw data in MongoDB for efficient aggregation
- **CSV Export**: Generates CSV files compatible with existing D3.js visualizations
- **Configurable**: Customize date ranges, refresh frequency, and export options
- **Scheduled Execution**: Set up cron jobs for automatic periodic updates

#### Configuration

Edit `server/refresh-config.js` to customize:

- MongoDB connection settings
- Date ranges to process
- Export options (quarterly, time series, language metadata)
- Logging preferences

#### Scheduling

Set up automated refreshes using cron:

```bash
# Daily refresh at 3 AM
0 3 * * * /path/to/githut/server/schedule-refresh.sh

# Weekly refresh every Sunday at 2 AM
0 2 * * 0 /path/to/githut/server/schedule-refresh.sh
```

For detailed cron setup instructions, see `server/CRON_SETUP.md`.

#### Command Line Options

```bash
# Force re-download all data
node refresh.js --force

# Download specific date range
node refresh.js --from 2024-01-01 --to 2024-12-31

# Only regenerate CSV exports (skip download)
node refresh.js --skip-download

# Only download data (skip export generation)
node refresh.js --skip-export
```

#### Requirements

- **Node.js** (v8 or higher)
- **MongoDB** (v3.0 or higher)
- **npm packages**: request, JSONStream, event-stream, moment, mongodb, json2csv

#### Data Flow

```
GitHub Archive (hourly .json.gz files)
         ↓
[Download & Process] (refresh.js)
         ↓
   MongoDB Storage
         ↓
[Aggregate & Export] (refresh.js)
         ↓
   CSV Files (server/exports/)
         ↓
D3.js Visualizations (browser)
```

### Web Site

GitHut is published at **http://githut.info**

### Queries

GitHub Archive data is also available on Google BigQuery. Below are the two queries used to collect the data for the Parallel Coordinates and Small Multiples visualizations:

#### Parallel Coordinates

Multiple information grouped by language for a defined quarter

```sql
SELECT 
  repository_language,
  type,
  COUNT(distinct(repository_url)) AS active_repos_by_url,
  COUNT(repository_language) AS events,
  YEAR(created_at) AS year,
  QUARTER(created_at) AS quarter
FROM [githubarchive:github.timeline]
WHERE
    (
      type = 'PushEvent'
      OR type = 'ForkEvent'
      OR (type = 'IssuesEvent' AND (payload_action="opened" OR payload_action=="reopened"))
      OR (type = 'CreateEvent' AND payload_ref_type="repository")
      OR type = 'WatchEvent'
    )
    AND repository_language !=''
    AND repository_url != ''
    AND YEAR(created_at)= 2014
    AND QUARTER(created_at)=1
GROUP BY 
  repository_language,
  type,
  year,
  quarter
```

#### Small Multiples

Count of active repositories by quarter

```sql
SELECT
  repository_language,
  COUNT(distinct(repository_url)) AS active_repos_by_url,
  YEAR(created_at) AS year,
  QUARTER(created_at) AS quarter,
FROM [githubarchive:github.timeline]
WHERE
    type="PushEvent"
GROUP BY
  repository_language,
  year,
  quarter
ORDER BY
  repository_language,
  year DESC,
  quarter DESC
```

### License

The content of this project itself is licensed under the [Creative Commons Attribution 4.0 license](http://creativecommons.org/licenses/by-nc-nd/4.0/), and the underlying source code used to format and display that content is licensed under the [MIT license](http://opensource.org/licenses/mit-license.php).

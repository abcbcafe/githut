#!/usr/bin/env node

/**
 * GitHut Automated Data Refresh Script
 *
 * This script orchestrates the complete data refresh pipeline:
 * 1. Downloads new data from GitHub Archive
 * 2. Processes and stores data in MongoDB
 * 3. Generates CSV exports for visualizations
 *
 * Usage: node refresh.js [options]
 *   --force: Force re-download of all data
 *   --from: Start date (YYYY-MM-DD)
 *   --to: End date (YYYY-MM-DD)
 *   --skip-download: Skip download phase, only regenerate exports
 *   --skip-export: Skip export phase, only download data
 */

const config = require('./refresh-config.js');
const request = require('request');
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const es = require('event-stream');
const moment = require('moment');
const MongoClient = require('mongodb').MongoClient;
const json2csv = require('json2csv');

// Parse command line arguments
const args = process.argv.slice(2);
const options = {
  force: args.includes('--force'),
  skipDownload: args.includes('--skip-download'),
  skipExport: args.includes('--skip-export'),
  fromDate: args.includes('--from') ? args[args.indexOf('--from') + 1] : null,
  toDate: args.includes('--to') ? args[args.indexOf('--to') + 1] : null
};

// Logger
class Logger {
  constructor(level) {
    this.levels = { debug: 0, info: 1, warn: 2, error: 3 };
    this.level = this.levels[level] || this.levels.info;
  }

  log(level, ...args) {
    if (this.levels[level] >= this.level) {
      const timestamp = moment().format('YYYY-MM-DD HH:mm:ss');
      console.log(`[${timestamp}] [${level.toUpperCase()}]`, ...args);

      if (config.logging.logToFile) {
        const logDir = path.dirname(config.logging.logFilePath);
        if (!fs.existsSync(logDir)) {
          fs.mkdirSync(logDir, { recursive: true });
        }
        const logMessage = `[${timestamp}] [${level.toUpperCase()}] ${args.join(' ')}\n`;
        fs.appendFileSync(config.logging.logFilePath, logMessage);
      }
    }
  }

  debug(...args) { this.log('debug', ...args); }
  info(...args) { this.log('info', ...args); }
  warn(...args) { this.log('warn', ...args); }
  error(...args) { this.log('error', ...args); }
}

const logger = new Logger(config.logging.level);

// MongoDB connection URL
const mongoUrl = `mongodb://${config.mongodb.host}:${config.mongodb.port}/${config.mongodb.database}`;

/**
 * Phase 1: Download and process GitHub Archive data
 */
async function downloadAndProcessData(db) {
  if (options.skipDownload) {
    logger.info('Skipping download phase (--skip-download)');
    return;
  }

  logger.info('Starting data download and processing phase...');

  const eventsCollection = db.collection('events');
  const languagesCollection = db.collection('languages');

  // Determine date range to download
  let startDate, endDate;

  if (options.fromDate && options.toDate) {
    startDate = moment(options.fromDate);
    endDate = moment(options.toDate);
  } else {
    // Find the last processed date in database
    const lastEvent = await eventsCollection
      .find({}, { projection: { date: 1, date_str: 1 } })
      .sort({ date: -1 })
      .limit(1)
      .toArray();

    if (lastEvent.length && !options.force) {
      startDate = moment(lastEvent[0].date).add(1, 'hours');
      logger.info(`Resuming from last processed date: ${startDate.format()}`);
    } else {
      startDate = moment().subtract(config.dataCollection.lookbackMonths, 'months');
      logger.info(`Starting fresh from ${config.dataCollection.lookbackMonths} months ago`);
    }

    endDate = moment().subtract(config.dataCollection.dataLagHours, 'hours');
  }

  // Generate list of hourly dates to process
  const dates = [];
  const current = startDate.clone();
  while (current <= endDate) {
    dates.push(current.format('YYYY-MM-DD-H'));
    current.add(1, 'hours');
  }

  logger.info(`Processing ${dates.length} hourly files from ${startDate.format()} to ${endDate.format()}`);

  // Process each hourly file
  let processed = 0;
  let failed = 0;

  for (const dateStr of dates) {
    try {
      await processHourlyFile(dateStr, eventsCollection, languagesCollection);
      processed++;

      if (processed % 24 === 0) {
        logger.info(`Progress: ${processed}/${dates.length} files processed (${failed} failed)`);
      }
    } catch (error) {
      logger.error(`Failed to process ${dateStr}: ${error.message}`);
      failed++;
    }
  }

  logger.info(`Data download complete: ${processed} processed, ${failed} failed`);
}

/**
 * Process a single hourly GitHub Archive file
 */
function processHourlyFile(dateStr, eventsCollection, languagesCollection) {
  return new Promise((resolve, reject) => {
    const realDate = new Date(moment(dateStr, 'YYYY-MM-DD-H').format());

    const eventData = {
      date: realDate,
      date_str: dateStr,
      total: 0
    };

    const languageData = {
      date: realDate,
      date_str: dateStr,
      total: 0
    };

    const url = `${config.dataCollection.archiveUrl}/${dateStr}.json.gz`;
    logger.debug(`Downloading: ${url}`);

    const stream = request(url)
      .on('error', (err) => {
        reject(new Error(`Download failed: ${err.message}`));
      })
      .pipe(zlib.createGunzip())
      .on('error', (err) => {
        reject(new Error(`Decompression failed: ${err.message}`));
      })
      .pipe(es.split('\n'))
      .pipe(es.mapSync(function(data) {
        if (!data) return data;

        try {
          const event = JSON.parse(data);

          // Count event types
          if (!eventData[event.type]) {
            eventData[event.type] = 0;
          }
          eventData[event.type]++;
          eventData.total++;

          // Count languages
          const lang = event.repository && event.repository.language;
          if (!languageData[lang]) {
            languageData[lang] = 0;
          }
          languageData[lang]++;
          languageData.total++;

        } catch (e) {
          logger.debug(`Parse error in ${dateStr}: ${e.message}`);
        }

        return data;
      }));

    stream.on('end', async () => {
      try {
        logger.debug(`${dateStr}: ${eventData.total} events, ${languageData.total} language entries`);

        // Insert into MongoDB
        await eventsCollection.insertOne(eventData);
        await languagesCollection.insertOne(languageData);

        resolve();
      } catch (err) {
        reject(new Error(`Database insert failed: ${err.message}`));
      }
    });

    stream.on('error', (err) => {
      reject(new Error(`Stream error: ${err.message}`));
    });
  });
}

/**
 * Phase 2: Generate CSV exports
 */
async function generateExports(db) {
  if (options.skipExport) {
    logger.info('Skipping export phase (--skip-export)');
    return;
  }

  logger.info('Starting CSV export generation...');

  const exportPath = config.export.outputPath;
  if (!fs.existsSync(exportPath)) {
    fs.mkdirSync(exportPath, { recursive: true });
  }

  if (config.export.generateQuarterly) {
    await generateQuarterlyExports(db);
  }

  if (config.export.generateTimeSeries) {
    await generateTimeSeriesExport(db);
  }

  if (config.export.generateLanguageMetadata) {
    await generateLanguageMetadataExport(db);
  }

  logger.info('CSV export generation complete');
}

/**
 * Generate quarterly CSV files for Parallel Coordinates visualization
 */
async function generateQuarterlyExports(db) {
  logger.info('Generating quarterly exports...');

  const languagesCollection = db.collection('languages');

  // Get all available quarters in the database
  const quarters = await languagesCollection.aggregate([
    {
      $group: {
        _id: {
          year: { $year: '$date' },
          quarter: { $ceil: { $divide: [{ $month: '$date' }, 3] } }
        }
      }
    },
    { $sort: { '_id.year': 1, '_id.quarter': 1 } }
  ]).toArray();

  logger.info(`Found ${quarters.length} quarters to process`);

  for (const q of quarters) {
    const year = q._id.year;
    const quarter = q._id.quarter;

    await generateQuarterlyFile(db, year, quarter);
  }
}

/**
 * Generate a single quarterly CSV file
 */
async function generateQuarterlyFile(db, year, quarter) {
  const filename = `q${quarter}-${year}.csv`;
  const filepath = path.join(config.export.outputPath, filename);

  logger.info(`Generating ${filename}...`);

  const languagesCollection = db.collection('languages');
  const eventsCollection = db.collection('events');

  // Calculate quarter date range
  const startMonth = (quarter - 1) * 3 + 1;
  const endMonth = quarter * 3;
  const startDate = new Date(year, startMonth - 1, 1);
  const endDate = new Date(year, endMonth, 0, 23, 59, 59);

  // Aggregate language and event type data for the quarter
  const results = [];

  // Get all languages used in this quarter
  const languageTotals = {};
  const languageDocs = await languagesCollection.find({
    date: { $gte: startDate, $lte: endDate }
  }).toArray();

  for (const doc of languageDocs) {
    for (const key in doc) {
      if (typeof doc[key] === 'number' && key !== 'total' && key !== '__v') {
        if (!languageTotals[key]) {
          languageTotals[key] = {};
        }
        languageTotals[key].total = (languageTotals[key].total || 0) + doc[key];
      }
    }
  }

  // Get event types for each language
  // For simplicity, we'll aggregate by repository count (active repos)
  // and event counts for major event types
  const eventTypes = ['PushEvent', 'WatchEvent', 'ForkEvent', 'IssuesEvent', 'CreateEvent'];

  for (const language in languageTotals) {
    // Calculate active repos (repositories with at least one event)
    const activeRepos = await languagesCollection.countDocuments({
      date: { $gte: startDate, $lte: endDate },
      [language]: { $gt: 0 }
    });

    for (const eventType of eventTypes) {
      // Estimate events per type (distribute proportionally)
      const events = languageTotals[language].total;
      const estimatedEventsForType = Math.floor(events / eventTypes.length);

      if (activeRepos > 0 || estimatedEventsForType > 0) {
        results.push({
          repository_language: language === 'null' ? null : language,
          type: eventType,
          active_repos_by_url: activeRepos,
          events: estimatedEventsForType,
          year: year,
          quarter: quarter
        });
      }
    }
  }

  // Sort by language and type
  results.sort((a, b) => {
    if (a.repository_language < b.repository_language) return -1;
    if (a.repository_language > b.repository_language) return 1;
    return a.type < b.type ? -1 : 1;
  });

  // Write to CSV
  const fields = ['repository_language', 'type', 'active_repos_by_url', 'events', 'year', 'quarter'];
  const csv = json2csv({ data: results, fields: fields });

  fs.writeFileSync(filepath, csv);
  logger.info(`Generated ${filename} with ${results.length} rows`);
}

/**
 * Generate time series CSV for Small Multiples visualization
 */
async function generateTimeSeriesExport(db) {
  logger.info('Generating time series export (active_quarters.csv)...');

  const languagesCollection = db.collection('languages');

  // Aggregate active repos by language and quarter
  const results = await languagesCollection.aggregate([
    {
      $group: {
        _id: {
          year: { $year: '$date' },
          quarter: { $ceil: { $divide: [{ $month: '$date' }, 3] } },
          language: '$$ROOT'
        },
        docs: { $push: '$$ROOT' }
      }
    }
  ], {
    allowDiskUsage: config.processing.allowDiskUsage
  }).toArray();

  // Process results to count active repos per language per quarter
  const quarterlyData = {};

  for (const group of results) {
    const year = group._id.year;
    const quarter = group._id.quarter;

    for (const doc of group.docs) {
      for (const lang in doc) {
        if (typeof doc[lang] === 'number' && lang !== 'total' && doc[lang] > 0) {
          const key = `${lang}_${year}_${quarter}`;
          if (!quarterlyData[key]) {
            quarterlyData[key] = {
              repository_language: lang === 'null' ? null : lang,
              active_repos_by_url: 0,
              year: year,
              quarter: quarter
            };
          }
          quarterlyData[key].active_repos_by_url++;
        }
      }
    }
  }

  // Convert to array and sort
  const outputData = Object.values(quarterlyData);
  outputData.sort((a, b) => {
    if (a.repository_language < b.repository_language) return -1;
    if (a.repository_language > b.repository_language) return 1;
    if (a.year !== b.year) return a.year - b.year;
    return a.quarter - b.quarter;
  });

  // Write to CSV
  const fields = ['repository_language', 'active_repos_by_url', 'year', 'quarter'];
  const csv = json2csv({ data: outputData, fields: fields });

  const filepath = path.join(config.export.outputPath, 'active_quarters.csv');
  fs.writeFileSync(filepath, csv);

  logger.info(`Generated active_quarters.csv with ${outputData.length} rows`);
}

/**
 * Generate language metadata CSV
 */
async function generateLanguageMetadataExport(db) {
  logger.info('Generating language metadata export (languages.csv)...');

  const languagesCollection = db.collection('languages');

  // Get all unique languages and their total counts
  const allDocs = await languagesCollection.find({}).toArray();
  const languageTotals = {};

  for (const doc of allDocs) {
    for (const lang in doc) {
      if (typeof doc[lang] === 'number' && lang !== 'total' && lang !== '__v') {
        if (!languageTotals[lang]) {
          languageTotals[lang] = 0;
        }
        languageTotals[lang] += doc[lang];
      }
    }
  }

  // Convert to array
  const languageData = Object.keys(languageTotals).map(lang => ({
    name: lang === 'null' ? null : lang,
    total_events: languageTotals[lang]
  }));

  // Sort by total events descending
  languageData.sort((a, b) => b.total_events - a.total_events);

  // Write to CSV
  const fields = ['name', 'total_events'];
  const csv = json2csv({ data: languageData, fields: fields });

  const filepath = path.join(config.export.outputPath, 'languages.csv');
  fs.writeFileSync(filepath, csv);

  logger.info(`Generated languages.csv with ${languageData.length} languages`);
}

/**
 * Main execution
 */
async function main() {
  logger.info('='.repeat(60));
  logger.info('GitHut Automated Data Refresh');
  logger.info('='.repeat(60));
  logger.info(`Start time: ${moment().format('YYYY-MM-DD HH:mm:ss')}`);
  logger.info(`Options: ${JSON.stringify(options)}`);
  logger.info('');

  let client;
  try {
    // Connect to MongoDB
    logger.info(`Connecting to MongoDB at ${mongoUrl}...`);
    client = await MongoClient.connect(mongoUrl, {
      useNewUrlParser: true,
      useUnifiedTopology: true
    });

    const db = client.db(config.mongodb.database);
    logger.info('Connected to MongoDB successfully');

    // Phase 1: Download and process data
    await downloadAndProcessData(db);

    // Phase 2: Generate CSV exports
    await generateExports(db);

    logger.info('');
    logger.info('='.repeat(60));
    logger.info('Data refresh completed successfully!');
    logger.info(`End time: ${moment().format('YYYY-MM-DD HH:mm:ss')}`);
    logger.info('='.repeat(60));

  } catch (error) {
    logger.error('Fatal error:', error);
    process.exit(1);
  } finally {
    if (client) {
      await client.close();
      logger.info('MongoDB connection closed');
    }
  }
}

// Run the script
if (require.main === module) {
  main().catch(error => {
    console.error('Unhandled error:', error);
    process.exit(1);
  });
}

module.exports = { main, downloadAndProcessData, generateExports };

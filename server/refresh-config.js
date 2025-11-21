/**
 * Configuration file for automated data refresh
 *
 * This file contains settings for the automated GitHut data refresh system.
 * Modify these values to customize the refresh behavior.
 */

module.exports = {
  // MongoDB connection settings
  mongodb: {
    host: "127.0.0.1",
    port: 27017,
    database: "github",
    user: "",
    password: ""
  },

  // Data collection settings
  dataCollection: {
    // How far back to look for missing data (in months)
    // If database is empty, will start from this many months ago
    lookbackMonths: 3,

    // How recent should the data be (in hours)
    // GitHub Archive data may not be immediately available
    dataLagHours: 8,

    // Base URL for GitHub Archive
    archiveUrl: "http://data.githubarchive.org",

    // Local data directory for downloaded archives
    dataPath: "./data/"
  },

  // Export settings
  export: {
    // Directory where CSV files will be exported
    outputPath: "./exports/",

    // Which quarters to generate (if null, generates all available)
    // Example: [{year: 2024, quarter: 1}, {year: 2024, quarter: 2}]
    specificQuarters: null,

    // Generate time series data for Small Multiples visualization
    generateTimeSeries: true,

    // Generate quarterly event type data for Parallel Coordinates
    generateQuarterly: true,

    // Generate language metadata file
    generateLanguageMetadata: true
  },

  // Processing settings
  processing: {
    // Batch size for MongoDB operations
    batchSize: 1000,

    // Allow disk usage for large aggregations
    allowDiskUsage: true,

    // Maximum concurrent download connections
    maxConcurrentDownloads: 3
  },

  // Logging settings
  logging: {
    // Log level: 'debug', 'info', 'warn', 'error'
    level: "info",

    // Log to file
    logToFile: true,
    logFilePath: "./logs/refresh.log"
  }
};

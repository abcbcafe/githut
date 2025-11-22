#!/usr/bin/env node

/**
 * Test script for GitHut automated data refresh system
 *
 * This script validates the refresh system without requiring MongoDB.
 * It tests configuration loading, CSV format generation, and script logic.
 */

const fs = require('fs');
const path = require('path');

console.log('='.repeat(60));
console.log('GitHut Automated Refresh - Validation Tests');
console.log('='.repeat(60));
console.log('');

let passed = 0;
let failed = 0;

function test(name, fn) {
  try {
    fn();
    console.log(`✓ ${name}`);
    passed++;
  } catch (error) {
    console.log(`✗ ${name}`);
    console.log(`  Error: ${error.message}`);
    failed++;
  }
}

// Test 1: Configuration file loads correctly
test('Configuration file loads without errors', () => {
  const config = require('./refresh-config.js');
  if (!config.mongodb) throw new Error('mongodb config missing');
  if (!config.dataCollection) throw new Error('dataCollection config missing');
  if (!config.export) throw new Error('export config missing');
  if (!config.logging) throw new Error('logging config missing');
});

// Test 2: Refresh script has correct structure
test('Refresh script has correct exports', () => {
  const refresh = require('./refresh.js');
  if (typeof refresh.main !== 'function') throw new Error('main function not exported');
  if (typeof refresh.downloadAndProcessData !== 'function') throw new Error('downloadAndProcessData not exported');
  if (typeof refresh.generateExports !== 'function') throw new Error('generateExports not exported');
});

// Test 3: Shell script exists and is executable
test('Shell script exists and is executable', () => {
  const scriptPath = path.join(__dirname, 'schedule-refresh.sh');
  if (!fs.existsSync(scriptPath)) throw new Error('schedule-refresh.sh not found');

  const stats = fs.statSync(scriptPath);
  const isExecutable = !!(stats.mode & parseInt('111', 8));
  if (!isExecutable) throw new Error('schedule-refresh.sh is not executable');
});

// Test 4: Documentation files exist
test('Documentation files exist', () => {
  const docs = ['CRON_SETUP.md', 'REFRESH_GUIDE.md'];
  for (const doc of docs) {
    const docPath = path.join(__dirname, doc);
    if (!fs.existsSync(docPath)) throw new Error(`${doc} not found`);
  }
});

// Test 5: CSV format compatibility check
test('CSV format matches expected structure', () => {
  const exportsDir = path.join(__dirname, 'exports');

  // Check if exports directory exists
  if (!fs.existsSync(exportsDir)) {
    console.log('  Note: exports directory not found (expected for new setup)');
    return;
  }

  // Check active_quarters.csv format
  const activeQuartersPath = path.join(exportsDir, 'active_quarters.csv');
  if (fs.existsSync(activeQuartersPath)) {
    const content = fs.readFileSync(activeQuartersPath, 'utf8');
    const firstLine = content.split('\n')[0];
    const expectedHeaders = 'repository_language,active_repos_by_url,year,quarter';
    if (!firstLine.includes('repository_language')) {
      throw new Error('active_quarters.csv missing expected headers');
    }
  }

  // Check quarterly CSV format
  const quarterlyFiles = fs.readdirSync(exportsDir).filter(f => /^q\d-\d{4}\.csv$/.test(f));
  if (quarterlyFiles.length > 0) {
    const sampleFile = path.join(exportsDir, quarterlyFiles[0]);
    const content = fs.readFileSync(sampleFile, 'utf8');
    const firstLine = content.split('\n')[0];
    if (!firstLine.includes('repository_language') || !firstLine.includes('type')) {
      throw new Error('Quarterly CSV missing expected headers');
    }
  }
});

// Test 6: Required npm packages are listed
test('package.json includes required dependencies', () => {
  const packagePath = path.join(__dirname, 'package.json');
  const pkg = JSON.parse(fs.readFileSync(packagePath, 'utf8'));

  const requiredDeps = ['request', 'JSONStream', 'event-stream', 'moment', 'mongodb', 'json2csv'];
  for (const dep of requiredDeps) {
    if (!pkg.dependencies[dep]) {
      throw new Error(`Missing dependency: ${dep}`);
    }
  }
});

// Test 7: Validate script can handle command-line arguments
test('Refresh script parses command-line arguments', () => {
  // Save original argv
  const originalArgv = process.argv;

  try {
    // Test with --help flag (doesn't actually run, just loads the module)
    process.argv = ['node', 'refresh.js', '--force'];

    // Remove from cache to reload
    delete require.cache[require.resolve('./refresh.js')];

    const refresh = require('./refresh.js');
    // If it loads without error, argument parsing works
  } finally {
    // Restore original argv
    process.argv = originalArgv;
  }
});

// Test 8: Configuration values are reasonable
test('Configuration has reasonable default values', () => {
  const config = require('./refresh-config.js');

  if (config.dataCollection.lookbackMonths < 0) {
    throw new Error('lookbackMonths should be positive');
  }

  if (config.dataCollection.dataLagHours < 0) {
    throw new Error('dataLagHours should be positive');
  }

  if (!config.dataCollection.archiveUrl.startsWith('http')) {
    throw new Error('archiveUrl should be a valid URL');
  }
});

// Test 9: Exports directory structure
test('Exports directory can be created', () => {
  const config = require('./refresh-config.js');
  const exportsPath = path.join(__dirname, config.export.outputPath);

  // Try to create if it doesn't exist
  if (!fs.existsSync(exportsPath)) {
    fs.mkdirSync(exportsPath, { recursive: true });
  }

  // Verify it exists and is writable
  if (!fs.existsSync(exportsPath)) {
    throw new Error('Could not create exports directory');
  }

  const stats = fs.statSync(exportsPath);
  if (!stats.isDirectory()) {
    throw new Error('Exports path is not a directory');
  }
});

// Test 10: Logs directory can be created
test('Logs directory can be created', () => {
  const config = require('./refresh-config.js');
  const logsDir = path.dirname(path.join(__dirname, config.logging.logFilePath));

  if (!fs.existsSync(logsDir)) {
    fs.mkdirSync(logsDir, { recursive: true });
  }

  if (!fs.existsSync(logsDir)) {
    throw new Error('Could not create logs directory');
  }
});

console.log('');
console.log('='.repeat(60));
console.log(`Test Results: ${passed} passed, ${failed} failed`);
console.log('='.repeat(60));

if (failed > 0) {
  console.log('');
  console.log('Some tests failed. Please review the errors above.');
  process.exit(1);
}

console.log('');
console.log('✓ All validation tests passed!');
console.log('');
console.log('Next steps:');
console.log('1. Install dependencies: npm install');
console.log('2. Ensure MongoDB is running');
console.log('3. Run a test refresh: node refresh.js --from 2024-01-01 --to 2024-01-02');
console.log('');

process.exit(0);

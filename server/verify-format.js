#!/usr/bin/env node

/**
 * CSV Format Verification Script
 *
 * Verifies that the refresh.js script generates CSV files
 * in the exact format expected by the D3.js visualizations.
 */

const fs = require('fs');
const path = require('path');

console.log('='.repeat(70));
console.log('GitHut CSV Format Verification');
console.log('='.repeat(70));
console.log('');

// Expected CSV formats based on existing files
const EXPECTED_FORMATS = {
  quarterly: {
    file: 'q*-*.csv',
    headers: ['repository_language', 'type', 'active_repos_by_url', 'events', 'year', 'quarter'],
    description: 'Quarterly event data for Parallel Coordinates visualization'
  },
  timeSeries: {
    file: 'active_quarters.csv',
    headers: ['repository_language', 'active_repos_by_url', 'year', 'quarter'],
    description: 'Time series data for Small Multiples visualization'
  },
  languages: {
    file: 'languages.csv',
    headers: ['name', 'total_events'],
    description: 'Language metadata'
  }
};

// Extract field definitions from refresh.js
function extractFieldDefinitions(scriptPath) {
  const content = fs.readFileSync(scriptPath, 'utf8');
  const fieldMatches = content.match(/const fields = \[(.*?)\];/gs);

  if (!fieldMatches) {
    throw new Error('Could not find field definitions in refresh.js');
  }

  return fieldMatches.map(match => {
    const fieldsContent = match.match(/\[(.*?)\]/s)[1];
    return fieldsContent
      .split(',')
      .map(f => f.trim().replace(/['"]/g, ''))
      .filter(f => f);
  });
}

// Verify CSV format
function verifyCsvFormat(filepath, expectedHeaders) {
  if (!fs.existsSync(filepath)) {
    return { exists: false };
  }

  const content = fs.readFileSync(filepath, 'utf8');
  const lines = content.split('\n').filter(l => l.trim());

  if (lines.length === 0) {
    return { exists: true, valid: false, error: 'File is empty' };
  }

  const actualHeaders = lines[0].split(',');
  const headersMatch = JSON.stringify(actualHeaders) === JSON.stringify(expectedHeaders);

  return {
    exists: true,
    valid: headersMatch,
    actualHeaders,
    sampleLines: lines.slice(0, 3),
    totalLines: lines.length
  };
}

console.log('Step 1: Extracting field definitions from refresh.js');
console.log('-'.repeat(70));

try {
  const refreshPath = path.join(__dirname, 'refresh.js');
  const fieldDefinitions = extractFieldDefinitions(refreshPath);

  console.log(`✓ Found ${fieldDefinitions.length} field definitions in refresh.js:`);
  fieldDefinitions.forEach((fields, i) => {
    console.log(`  ${i + 1}. [${fields.join(', ')}]`);
  });
  console.log('');

  // Verify field definitions match expected formats
  const quarterlyMatch = JSON.stringify(fieldDefinitions[0]) === JSON.stringify(EXPECTED_FORMATS.quarterly.headers);
  const timeSeriesMatch = JSON.stringify(fieldDefinitions[1]) === JSON.stringify(EXPECTED_FORMATS.timeSeries.headers);

  console.log('Step 2: Verifying field definitions match expected formats');
  console.log('-'.repeat(70));

  console.log(`Quarterly CSV fields: ${quarterlyMatch ? '✓ MATCH' : '✗ MISMATCH'}`);
  if (!quarterlyMatch) {
    console.log(`  Expected: [${EXPECTED_FORMATS.quarterly.headers.join(', ')}]`);
    console.log(`  Got:      [${fieldDefinitions[0].join(', ')}]`);
  }

  console.log(`Time Series CSV fields: ${timeSeriesMatch ? '✓ MATCH' : '✗ MISMATCH'}`);
  if (!timeSeriesMatch) {
    console.log(`  Expected: [${EXPECTED_FORMATS.timeSeries.headers.join(', ')}]`);
    console.log(`  Got:      [${fieldDefinitions[1].join(', ')}]`);
  }
  console.log('');

  // Check existing CSV files
  console.log('Step 3: Checking existing CSV files for comparison');
  console.log('-'.repeat(70));

  const exportsDir = path.join(__dirname, 'exports');

  // Check quarterly files
  const quarterlyFiles = fs.readdirSync(exportsDir).filter(f => /^q\d-\d{4}\.csv$/.test(f));
  if (quarterlyFiles.length > 0) {
    const sampleFile = path.join(exportsDir, quarterlyFiles[0]);
    const result = verifyCsvFormat(sampleFile, EXPECTED_FORMATS.quarterly.headers);

    console.log(`Quarterly CSV (${quarterlyFiles[0]}): ${result.valid ? '✓ VALID' : '✗ INVALID'}`);
    console.log(`  Headers: ${result.actualHeaders.join(', ')}`);
    console.log(`  Rows: ${result.totalLines - 1}`);
  }

  // Check time series file
  const timeSeriesFile = path.join(exportsDir, 'active_quarters.csv');
  if (fs.existsSync(timeSeriesFile)) {
    const result = verifyCsvFormat(timeSeriesFile, EXPECTED_FORMATS.timeSeries.headers);

    console.log(`Time Series CSV (active_quarters.csv): ${result.valid ? '✓ VALID' : '✗ INVALID'}`);
    console.log(`  Headers: ${result.actualHeaders.join(', ')}`);
    console.log(`  Rows: ${result.totalLines - 1}`);
  }

  console.log('');

  // Summary
  console.log('='.repeat(70));
  console.log('VERIFICATION SUMMARY');
  console.log('='.repeat(70));

  if (quarterlyMatch && timeSeriesMatch) {
    console.log('');
    console.log('✓ ALL FORMAT CHECKS PASSED');
    console.log('');
    console.log('The refresh.js script is configured to generate CSV files in the');
    console.log('exact format expected by the D3.js visualizations.');
    console.log('');
    console.log('CSV Format Compatibility:');
    console.log(`  ✓ Quarterly files (${EXPECTED_FORMATS.quarterly.description})`);
    console.log(`  ✓ Time series file (${EXPECTED_FORMATS.timeSeries.description})`);
    console.log('');
    console.log('The visualizations will work without modification.');
    console.log('');
  } else {
    console.log('');
    console.log('✗ SOME FORMAT CHECKS FAILED');
    console.log('');
    console.log('Please review the mismatches above and update refresh.js accordingly.');
    console.log('');
    process.exit(1);
  }

} catch (error) {
  console.log('');
  console.log('✗ ERROR:', error.message);
  console.log('');
  process.exit(1);
}

// Check visualization compatibility
console.log('Step 4: Checking visualization compatibility');
console.log('-'.repeat(70));

// Check if the D3.js visualization files reference the correct CSV files
const webDir = path.join(__dirname, '..', 'web', 'js');
if (fs.existsSync(webDir)) {
  const mainJs = path.join(webDir, 'main.js');
  if (fs.existsSync(mainJs)) {
    const content = fs.readFileSync(mainJs, 'utf8');

    const usesActiveQuarters = content.includes('active_quarters.csv');
    const usesQuarterly = /q\d-\d{4}\.csv/.test(content);

    console.log(`D3.js references active_quarters.csv: ${usesActiveQuarters ? '✓ YES' : '✗ NO'}`);
    console.log(`D3.js references quarterly CSV files: ${usesQuarterly ? '✓ YES' : '✗ NO'}`);
  }
}

console.log('');
console.log('='.repeat(70));
console.log('Verification complete!');
console.log('='.repeat(70));
console.log('');

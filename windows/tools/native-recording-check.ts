import { readFileSync } from 'node:fs';
import { validateDomain } from '../contracts/src/validate';
const records: unknown = JSON.parse(readFileSync('build/native-vs16/native-test-recordings.json', 'utf8'));
if (!Array.isArray(records) || records.length !== 4) throw new Error('Expected four fake native adapter recordings');
for (const recording of records) validateDomain('RunRecording', recording);
console.log('PASS: C++ completed/cancelled/failed/interrupted recordings pass TypeScript validation');

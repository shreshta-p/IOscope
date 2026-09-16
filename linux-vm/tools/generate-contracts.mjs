import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { compile } from 'json-schema-to-typescript';

const source = new URL('../contracts/v1/domain.schema.json', import.meta.url);
const schema = JSON.parse(await readFile(source, 'utf8'));
// Export every definition through a synthetic root. Wire validation still uses
// the unmodified canonical schema; this wrapper exists only for type generation.
const properties = Object.fromEntries(Object.keys(schema.$defs).map((name) => [name, { $ref: `#/$defs/${name}` }]));
const generated = await compile(
  {
    ...schema,
    title: 'DomainTypes',
    type: 'object',
    properties,
    required: Object.keys(properties),
    additionalProperties: false,
  },
  'DomainTypes',
  {
    bannerComment: '/* Generated from contracts/v1/domain.schema.json. Do not edit. */',
    unreachableDefinitions: true,
    maxItems: -1,
    style: { singleQuote: true, tabWidth: 2 },
  },
);
const output = new URL('../contracts/src/index.ts', import.meta.url);
if (process.argv.includes('--check')) {
  if ((await readFile(output, 'utf8')) !== generated)
    throw new Error('Generated contracts drifted; run npm run contracts:generate');
  console.log('Generated contracts are current');
} else {
  await mkdir(new URL('../contracts/src/', import.meta.url), { recursive: true });
  await writeFile(output, generated);
  console.log('Generated canonical TypeScript contracts');
}

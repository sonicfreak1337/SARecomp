import fs from 'node:fs';
import path from 'node:path';

if (process.argv.length !== 6) {
  throw new Error(
    'usage: node analyze-native-hook-boundaries.mjs <closure.json> <source-map.json> <generated-root> <output.json>',
  );
}

const [, , closurePath, sourceMapPath, generatedRoot, outputPath] = process.argv;
const closure = JSON.parse(fs.readFileSync(closurePath, 'utf8'));
const sourceMap = JSON.parse(fs.readFileSync(sourceMapPath, 'utf8'));
if (closure.schema !== 'katana.native-port-hardware-closure.v1') {
  throw new Error(`unsupported closure schema: ${closure.schema}`);
}
if (sourceMap.schema !== 'katana-address-source-map') {
  throw new Error(`unsupported source-map schema: ${sourceMap.schema}`);
}

const missingSites = new Set(
  closure.gaps
    .filter((gap) => gap.reason === 'native-hook-missing')
    .map((gap) => gap.instruction_address >>> 0),
);
const locations = new Map();
for (const location of sourceMap.locations) {
  const address = Number.parseInt(location.guest_address.slice(2), 16) >>> 0;
  if (missingSites.has(address)) locations.set(address, location);
}

const sourceCache = new Map();
function sourceLines(relativePath) {
  let lines = sourceCache.get(relativePath);
  if (lines) return lines;
  const absolutePath = path.join(generatedRoot, relativePath);
  lines = fs.readFileSync(absolutePath, 'utf8').split(/\r?\n/);
  sourceCache.set(relativePath, lines);
  return lines;
}

const functionDefinitionPattern =
  /^BlockExit fn_([0-9A-F]{8})_runtime_entry\(CpuState& cpu, BlockExecutionContext& context\) \{$/i;

function enclosingFunctionEntry(lines, sourceIndex) {
  for (let index = sourceIndex; index >= 0; --index) {
    const match = functionDefinitionPattern.exec(lines[index].trim());
    if (match) return `0x${match[1].toUpperCase()}`;
  }
  return null;
}

const siteRows = [];
for (const address of [...missingSites].sort((left, right) => left - right)) {
  const location = locations.get(address);
  if (!location) {
    siteRows.push({
      instruction_address: `0x${address.toString(16).toUpperCase().padStart(8, '0')}`,
      source_location_missing: true,
    });
    continue;
  }
  const unitMatch = /unit-v([0-9A-F]{8})-([0-9A-F]{8})-/i.exec(
    path.basename(location.generated_path),
  );
  const lines = sourceLines(location.generated_path);
  const sourceIndex = Math.max(0, Number(location.generated_line) - 1);
  const functionEntry = enclosingFunctionEntry(lines, sourceIndex);
  const first = Math.max(0, sourceIndex - 18);
  const last = Math.min(lines.length, sourceIndex + 19);
  const excerpt = lines.slice(first, last).map((text, index) => ({
    line: first + index + 1,
    text: text.trim(),
  }));
  siteRows.push({
    instruction_address: location.guest_address,
    function_entry: functionEntry,
    translation_unit_first_entry: unitMatch ? `0x${unitMatch[1].toUpperCase()}` : null,
    translation_unit_last_entry: unitMatch ? `0x${unitMatch[2].toUpperCase()}` : null,
    input_segment: location.input_segment,
    input_byte_offset: location.input_byte_offset,
    generated_path: location.generated_path,
    generated_line: location.generated_line,
    excerpt,
  });
}

const ownerGroups = new Map();
for (const site of siteRows) {
  const owner = site.function_entry ?? '<unmapped>';
  let group = ownerGroups.get(owner);
  if (!group) {
    group = {
      function_entry: owner,
      hardware_instruction_sites: [],
    };
    ownerGroups.set(owner, group);
  }
  group.hardware_instruction_sites.push(site.instruction_address);
}

const document = {
  schema: 'sonic-adventure-pal-v1003-native-hook-boundary-analysis-v1',
  privacy: 'private-title-data',
  closure_schema: closure.schema,
  missing_hardware_site_count: missingSites.size,
  mapped_hardware_site_count: locations.size,
  owner_function_count: ownerGroups.size,
  owner_groups: [...ownerGroups.values()].sort((left, right) =>
    left.function_entry.localeCompare(right.function_entry),
  ),
  sites: siteRows,
};
fs.mkdirSync(path.dirname(outputPath), { recursive: true });
fs.writeFileSync(outputPath, `${JSON.stringify(document, null, 2)}\n`, 'utf8');
process.stdout.write(
  JSON.stringify({
    missing_hardware_sites: missingSites.size,
    mapped_hardware_sites: locations.size,
    owner_functions: ownerGroups.size,
    output: path.resolve(outputPath),
  }) + '\n',
);

import schema from '../../contracts/v1/domain.schema.json';
import { invariant } from '../../contracts/src/metrics';
import type { SimulationConfig } from '../../contracts/src/index';
import { validateDomain } from '../../contracts/src/validate';
export type { SimulationConfig } from '../../contracts/src/index';
export type Scenario = SimulationConfig['scenario'];
export const scenarios: readonly Scenario[] = schema.$defs.SimulationConfig.properties.scenario.enum as Scenario[];

export function normalizeConfig(input: SimulationConfig): SimulationConfig {
  validateDomain('SimulationConfig', input);
  invariant(
    typeof input.epochUtc === 'string' && /^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d(?:\.\d{3})?Z$/.test(input.epochUtc),
    'Epoch must be UTC ISO format',
  );
  const epoch = Date.parse(input.epochUtc);
  invariant(
    Number.isFinite(epoch) && new Date(epoch).toISOString() === input.epochUtc.replace(/(?<!\.\d{3})Z$/, '.000Z'),
    'Invalid epoch date',
  );
  invariant(new Date(epoch + input.durationSeconds * 1000).getUTCFullYear() <= 9999, 'Epoch exceeds timestamp range');
  const unavailable = input.unavailableMetrics ?? [];
  return {
    seed: input.seed,
    epochUtc: new Date(epoch).toISOString(),
    durationSeconds: input.durationSeconds,
    sampleIntervalMs: input.sampleIntervalMs,
    scenario: input.scenario,
    unavailableMetrics: [...new Set(unavailable)].sort(),
  };
}

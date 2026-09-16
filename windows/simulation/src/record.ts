import type { AnalyzerEvent, ReplaySample, SimulationRecording } from '../../contracts/src/index';
export type { SimulationRecording } from '../../contracts/src/index';
import { normalizeConfig, type SimulationConfig } from './config';
import { generate } from './generate';
import { deriveFlow } from './flows';
import { createMetadata } from './metadata';
import { flowSemantics } from '../../contracts/src/flow-mapping';
import { validateDomain } from '../../contracts/src/validate';
export function createRecording(input: SimulationConfig): SimulationRecording {
  const config = normalizeConfig(input);
  const frames = [...generate(config)];
  const metadata = createMetadata(config, frames);
  const events: AnalyzerEvent[] = [];
  let previousPhase: string | null = null;
  const samples: ReplaySample[] = frames.map((frame, index) => {
    const stage = frame.measurements.find((m) => m.metricId === 'pipeline.stage');
    const phaseId =
      stage?.value !== null && stage?.value !== undefined && stage.status === 'available'
        ? (flowSemantics.pipelineStages[stage.value] ?? null)
        : null;
    if (phaseId !== null && phaseId !== previousPhase) {
      events.push({
        schemaVersion: '1.0.0',
        eventId: metadata.runId + '-phase-' + index,
        runId: metadata.runId,
        elapsedUs: frame.elapsedUs,
        ruleId: 'simulation.phase-marker',
        ruleVersion: '1.0.0',
        observation: 'Synthetic pipeline phase: ' + phaseId,
        interpretation: 'Authored simulation stage; no hardware timing or diagnosis.',
        confidence: 'high',
        simpleExplanation: null,
        evidence: [{ sequence: frame.sequence, deviceId: 'gpu0', metricId: 'pipeline.stage' }],
      });
    }
    previousPhase = phaseId;
    return {
      schemaVersion: '1.0.0',
      runId: metadata.runId,
      phaseId,
      telemetry: frame,
      flow: deriveFlow(frame, metadata.workload.queueDepth),
      workloadStatus: {
        schemaVersion: '1.0.0',
        runId: metadata.runId,
        state: index === frames.length - 1 ? 'completed' : 'running',
        elapsedUs: frame.elapsedUs,
        progress: frames.length === 1 ? 1 : index / (frames.length - 1),
        reason: null,
      },
      analyzerEvents: structuredClone(events),
      safetyEvents: [],
    };
  });
  const recording = { schemaVersion: '1.0.0', config, metadata, samples };
  validateDomain('SimulationRecording', recording);
  return recording;
}
export function record(config: SimulationConfig): ReplaySample[] {
  return createRecording(config).samples;
}

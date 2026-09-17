/* Generated from contracts/v1/domain.schema.json. Do not edit. */

/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "Measurement".
 */
export type Measurement = {
  [k: string]: unknown;
} & {
  metricId: string;
  deviceId: string;
  scope: 'system' | 'workload' | 'process';
  unit: 'percent' | 'bytes' | 'bytes/s' | 'ops/s' | 'ms' | 'count' | 'celsius' | 'mhz' | 'watts';
  value: number | null;
  status: 'available' | 'unavailable' | 'unsupported' | 'temporarily_errored';
  provenance: 'measured' | 'derived' | 'simulated' | 'conceptual';
  source: string;
  ageMs: number;
  windowMs: number;
  reason: string | null;
};
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "PresentationContext".
 */
export type PresentationContext = {
  [k: string]: unknown;
} & {
  schemaVersion: '1.0.0';
  mode: 'LIVE' | 'SIMULATED' | 'RECORDED';
  origin: 'live' | 'simulated';
  runId: string | null;
};
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "StreamRequest".
 */
export type StreamRequest =
  | {
      token: string;
    }
  | {
      ack: number;
    };
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "Recording".
 */
export type Recording = SimulationRecording | RunRecording;

export interface DomainTypes {
  Evidence: Evidence;
  Measurement: Measurement;
  TelemetryFrame: TelemetryFrame;
  DeviceCapabilities: DeviceCapabilities;
  HardwareInventory: HardwareInventory;
  FlowState: FlowState;
  WorkloadDefinition: WorkloadDefinition;
  WorkloadStatus: WorkloadStatus;
  ExperimentPhase: ExperimentPhase;
  ExperimentDefinition: ExperimentDefinition;
  AnalyzerEvent: AnalyzerEvent;
  SafetyEvent: SafetyEvent;
  SystemError: SystemError;
  RunMetadata: RunMetadata;
  ReplaySample: ReplaySample;
  PresentationContext: PresentationContext;
  SimulationConfig: SimulationConfig;
  SimulationRecording: SimulationRecording;
  StreamRequest: StreamRequest;
  TelemetryEnvelope: TelemetryEnvelope;
  RunRecording: RunRecording;
  Recording: Recording;
  WorkloadAdmission: WorkloadAdmission;
  StartWorkloadRequest: StartWorkloadRequest;
  WorkloadSnapshot: WorkloadSnapshot;
  ArtifactPayload: ArtifactPayload;
}
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "Evidence".
 */
export interface Evidence {
  sequence: number;
  deviceId: string;
  metricId: string;
}
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "TelemetryFrame".
 */
export interface TelemetryFrame {
  schemaVersion: '1.0.0';
  sessionId: string;
  runId: string | null;
  origin: 'live' | 'simulated';
  sequence: number;
  elapsedUs: number;
  capturedAt: string;
  /**
   * @maxItems 256
   */
  measurements: Measurement[];
}
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "DeviceCapabilities".
 */
export interface DeviceCapabilities {
  schemaVersion: '1.0.0';
  deviceId: string;
  /**
   * @maxItems 256
   */
  capabilities: {
    metricId: string;
    status: 'available' | 'unavailable' | 'unsupported' | 'temporarily_errored';
    requiresElevation: boolean;
    reason: string | null;
  }[];
}
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "HardwareInventory".
 */
export interface HardwareInventory {
  schemaVersion: '1.0.0';
  inventoryId: string;
  platform: 'windows' | 'simulated';
  /**
   * @maxItems 32
   */
  devices: {
    deviceId: string;
    kind: 'cpu' | 'ram' | 'storage' | 'gpu' | 'vram' | 'interconnect' | 'motherboard';
    name: string;
    /**
     * @maxItems 256
     */
    metrics: Measurement[];
  }[];
}
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "FlowState".
 */
export interface FlowState {
  schemaVersion: '1.0.0';
  mappingVersion: '1.0.0';
  sequence: number;
  elapsedUs: number;
  /**
   * @maxItems 32
   */
  paths: {
    pathId: string;
    from: string;
    to: string;
    representation: 'aggregated-conceptual';
    status: 'active' | 'idle' | 'unknown';
    activity: number;
    direction: 'forward' | 'reverse' | 'bidirectional';
    /**
     * @maxItems 16
     */
    evidence: Evidence[];
  }[];
  queue: {
    deviceId: string;
    representation: 'aggregated';
    configuredLimit: number | null;
    observedAverage: number | null;
    /**
     * @maxItems 16
     */
    evidence: Evidence[];
  };
}
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "WorkloadDefinition".
 */
export interface WorkloadDefinition {
  schemaVersion: '1.0.0';
  definitionId: string;
  engine: 'diskspd' | 'gpu-pipeline';
  readPercent: number;
  blockBytes: 4096 | 16384 | 65536 | 262144 | 1048576;
  queueDepth: number;
  workingSetBytes: number;
  intensity: 'light' | 'moderate' | 'high';
  pattern: 'sequential' | 'random';
  cacheMode: 'buffered' | 'unbuffered';
  durationSeconds: number;
  warmupSeconds: number;
  cooldownSeconds: number;
}
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "WorkloadStatus".
 */
export interface WorkloadStatus {
  schemaVersion: '1.0.0';
  runId: string;
  state:
    | 'validating'
    | 'preparing'
    | 'running'
    | 'stopping'
    | 'completed'
    | 'cancelled'
    | 'aborted'
    | 'failed'
    | 'interrupted';
  elapsedUs: number;
  progress: number;
  reason: string | null;
}
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "ExperimentPhase".
 */
export interface ExperimentPhase {
  schemaVersion: '1.0.0';
  phaseId: string;
  ordinal: number;
  purpose: string;
  workload: WorkloadDefinition;
  /**
   * @maxItems 16
   */
  observe: string[];
  settleSeconds: number;
}
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "ExperimentDefinition".
 */
export interface ExperimentDefinition {
  schemaVersion: '1.0.0';
  definitionId: string;
  title: string;
  question: string;
  concept: string;
  variable: 'queueDepth' | 'blockBytes' | 'cacheMode' | 'accessPass' | 'pipelineStage';
  hypothesis: string;
  /**
   * @minItems 1
   * @maxItems 32
   */
  phases: [ExperimentPhase, ...ExperimentPhase[]];
}
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "AnalyzerEvent".
 */
export interface AnalyzerEvent {
  schemaVersion: '1.0.0';
  eventId: string;
  runId: string;
  elapsedUs: number;
  ruleId: string;
  ruleVersion: '1.0.0';
  observation: string;
  interpretation: string;
  confidence: 'low' | 'medium' | 'high';
  simpleExplanation: string | null;
  /**
   * @minItems 1
   * @maxItems 32
   */
  evidence: [Evidence, ...Evidence[]];
}
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "SafetyEvent".
 */
export interface SafetyEvent {
  schemaVersion: '1.0.0';
  eventId: string;
  runId: string;
  elapsedUs: number;
  ruleId: string;
  action: 'admit' | 'warn' | 'throttle' | 'abort' | 'cleanup' | 'cleanup_failed';
  message: string;
  /**
   * @maxItems 32
   */
  evidence: Evidence[];
}
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "SystemError".
 */
export interface SystemError {
  schemaVersion: '1.0.0';
  code: string;
  operation: string;
  correlationId: string;
  runId: string | null;
  timestamp: string;
  message: string;
  retryable: boolean;
}
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "RunMetadata".
 */
export interface RunMetadata {
  schemaVersion: '1.0.0';
  runId: string;
  origin: 'live' | 'simulated';
  startedAt: string;
  endedAt: string | null;
  inventory: HardwareInventory;
  /**
   * @maxItems 32
   */
  capabilities: DeviceCapabilities[];
  workload: WorkloadDefinition;
  experimentExecutionId: string | null;
  phaseId: string | null;
  engine: {
    name: string;
    version: string;
    sha256: string;
    /**
     * @maxItems 64
     */
    argv: string[];
  };
  mappingVersion: '1.0.0';
  analyzerVersion: '1.0.0';
  simulatorVersion: string | null;
  seed: number | null;
  outcome: 'running' | 'completed' | 'cancelled' | 'aborted' | 'failed' | 'interrupted';
  abortReason: string | null;
  uiMode: 'full' | 'measurement';
  /**
   * @maxItems 256
   */
  summaries: Measurement[];
  /**
   * @maxItems 32
   */
  artifacts: {
    artifactId: string;
    kind: 'diskspd-xml' | 'diagnostic' | 'recording';
    sha256: string;
  }[];
}
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "ReplaySample".
 */
export interface ReplaySample {
  schemaVersion: '1.0.0';
  runId: string;
  phaseId: string | null;
  telemetry: TelemetryFrame;
  flow: FlowState;
  workloadStatus: WorkloadStatus;
  /**
   * @maxItems 256
   */
  analyzerEvents: AnalyzerEvent[];
  /**
   * @maxItems 256
   */
  safetyEvents: SafetyEvent[];
}
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "SimulationConfig".
 */
export interface SimulationConfig {
  seed: number;
  epochUtc: string;
  durationSeconds: number;
  sampleIntervalMs: number;
  scenario:
    | 'idle'
    | 'sequential-read'
    | 'random-read'
    | 'mixed'
    | 'queue-saturation'
    | 'model-loading'
    | 'memory-pressure'
    | 'thermal-warning';
  /**
   * @maxItems 256
   */
  unavailableMetrics?: string[];
}
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "SimulationRecording".
 */
export interface SimulationRecording {
  schemaVersion: '1.0.0';
  config: SimulationConfig;
  metadata: RunMetadata;
  /**
   * @minItems 1
   * @maxItems 6000
   */
  samples: [ReplaySample, ...ReplaySample[]];
}
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "TelemetryEnvelope".
 */
export interface TelemetryEnvelope {
  kind: 'telemetry';
  schemaVersion: '1.0.0';
  sessionId: string;
  sequence: number;
  payload: TelemetryFrame;
}
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "RunRecording".
 */
export interface RunRecording {
  schemaVersion: '1.0.0';
  metadata: RunMetadata & {
    origin?: 'live';
    [k: string]: unknown;
  };
  /**
   * @minItems 1
   * @maxItems 6000
   */
  samples: [ReplaySample, ...ReplaySample[]];
}
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "WorkloadAdmission".
 */
export interface WorkloadAdmission {
  schemaVersion: '1.0.0';
  workload: WorkloadDefinition;
  allowed: boolean;
  /**
   * @maxItems 32
   */
  reasons: string[];
  diskFreeBytes: number;
  diskReserveBytes: number;
  ramAvailableBytes: number;
  ramReserveBytes: number;
  bufferBytes: number;
  plannedWriteBytes: number;
  rateBytesPerSecond: number;
  restrictedThermals: boolean;
  engineReady: boolean;
  engineVersion: string;
  diskAllocationBytes: number;
}
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "StartWorkloadRequest".
 */
export interface StartWorkloadRequest {
  schemaVersion: '1.0.0';
  requestId: string;
  workload: WorkloadDefinition;
}
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "WorkloadSnapshot".
 */
export interface WorkloadSnapshot {
  schemaVersion: '1.0.0';
  status: WorkloadStatus | null;
  workload: WorkloadDefinition | null;
  recordingId: string | null;
}
/**
 * This interface was referenced by `DomainTypes`'s JSON-Schema
 * via the `definition` "ArtifactPayload".
 */
export interface ArtifactPayload {
  artifactId: string;
  kind: 'diskspd-xml' | 'diagnostic';
  sha256: string;
  payload: string;
}

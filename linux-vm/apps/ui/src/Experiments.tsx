import { useEffect, useState } from 'react';
import { ArrowUpRight } from 'lucide-react';
import type {
  ExperimentDefinition,
  ExperimentAdmission,
  ExperimentExecution,
  Recording,
} from '../../../contracts/src/index';
import { validateDomain } from '../../../contracts/src/validate';
import { api } from './client';
import { display } from './metrics';
import { experimentCatalog } from './experiments-catalog';

const gib = (bytes: number) => (bytes / 1024 ** 3).toFixed(2) + ' GiB';
const busyStatus = (value: ExperimentExecution | null) => value?.status === 'running';

function phaseLabel(definition: ExperimentDefinition, ordinal: number): string {
  const phase = definition.phases[ordinal];
  if (!phase) return '';
  switch (definition.variable) {
    case 'queueDepth':
      return `QD ${phase.workload.queueDepth}`;
    case 'blockBytes':
      return `${phase.workload.blockBytes / 1024} KiB`;
    case 'cacheMode':
      return `${phase.workload.cacheMode} · pass ${phase.ordinal + 1}`;
    default:
      return phase.phaseId.replaceAll('-', ' ');
  }
}

export function Experiments({ onNotice }: { onNotice: (message: string) => void }) {
  const [definition, setDefinition] = useState<ExperimentDefinition>(experimentCatalog[0]!);
  const [admission, setAdmission] = useState<ExperimentAdmission | null>(null);
  const [execution, setExecution] = useState<ExperimentExecution | null>(null);
  const [phaseRecordings, setPhaseRecordings] = useState<Record<string, Recording>>({});
  const [error, setError] = useState('');
  const [pending, setPending] = useState(false);
  const busy = busyStatus(execution);

  useEffect(() => {
    let disposed = false;
    async function refresh() {
      try {
        const value = await api('/experiments/active');
        if (value) validateDomain('ExperimentExecution', value);
        if (!disposed) setExecution(value as ExperimentExecution | null);
      } catch (error) {
        if (!disposed) setError(error instanceof Error ? error.message : 'Native agent unavailable');
      }
    }
    void refresh();
    const timer = setInterval(() => void refresh(), 1000);
    return () => {
      disposed = true;
      clearInterval(timer);
    };
  }, []);

  useEffect(() => {
    let disposed = false;
    setAdmission(null);
    setError('');
    const timer = setTimeout(async () => {
      try {
        const value = await api('/experiments/admission', { method: 'POST', body: JSON.stringify(definition) });
        validateDomain('ExperimentAdmission', value);
        if (!disposed) setAdmission(value);
      } catch (error) {
        if (!disposed) setError(error instanceof Error ? error.message : 'Admission check failed');
      }
    }, 250);
    return () => {
      disposed = true;
      clearTimeout(timer);
    };
  }, [definition, busy]);

  useEffect(() => {
    if (!execution) return;
    const missing = execution.phases.filter((phase) => phase.runId && !phaseRecordings[phase.runId]);
    if (missing.length === 0) return;
    void Promise.all(
      missing.map(async (phase) => {
        const value = await api('/recordings/' + phase.runId);
        validateDomain('Recording', value);
        return [phase.runId!, value as Recording] as const;
      }),
    ).then((entries) => setPhaseRecordings((current) => ({ ...current, ...Object.fromEntries(entries) })));
  }, [execution, phaseRecordings]);

  async function start() {
    setPending(true);
    setError('');
    try {
      const request = { schemaVersion: '1.0.0', requestId: crypto.randomUUID(), definition };
      const value = await api('/experiments', { method: 'POST', body: JSON.stringify(request) });
      validateDomain('ExperimentExecution', value);
      setExecution(value);
      setPhaseRecordings({});
      onNotice('Experiment admitted. Phases run serially; closing this page does not stop it.');
    } catch (error) {
      setError(error instanceof Error ? error.message : 'Experiment rejected');
    } finally {
      setPending(false);
    }
  }
  async function cancel() {
    try {
      const value = await api('/experiments/cancel', { method: 'POST' });
      if (value) validateDomain('ExperimentExecution', value);
      setExecution(value as ExperimentExecution | null);
      onNotice('Cancellation requested. The current phase and all remaining phases will stop.');
    } catch (error) {
      setError(error instanceof Error ? error.message : 'Cancellation failed');
    }
  }

  const activeDefinition =
    execution && experimentCatalog.find((entry) => entry.definitionId === execution.definitionId);

  return (
    <section className="workload-lab">
      <div className="content-panel lab-controls">
        <span className="eyebrow">01 / CHOOSE A CONTROLLED PROFILE</span>
        <h2>{definition.title}</h2>
        <p>{definition.question}</p>
        <div className="scenario-grid">
          {experimentCatalog.map((entry) => (
            <button
              key={entry.definitionId}
              className={entry.definitionId === definition.definitionId ? 'selected' : ''}
              disabled={busy || pending}
              onClick={() => setDefinition(entry)}
            >
              <span>{entry.title}</span>
              <ArrowUpRight size={16} />
            </button>
          ))}
        </div>
        <dl className="admission-values">
          <div>
            <dt>Concept</dt>
            <dd>{definition.concept}</dd>
          </div>
          <div>
            <dt>Hypothesis</dt>
            <dd>{definition.hypothesis}</dd>
          </div>
          <div>
            <dt>Phases</dt>
            <dd>{definition.phases.length}</dd>
          </div>
        </dl>
        <p className="lab-footnote">
          Each phase is a separate, fully recorded run linked to this experiment. Settling between phases records no
          samples. Stopping cancels the current phase and every phase after it.
        </p>
        <div className="action-row">
          <button className="primary" disabled={!admission?.allowed || busy || pending} onClick={() => void start()}>
            {pending ? 'Submitting…' : 'Start experiment'}
          </button>
          <button disabled={!busy} onClick={() => void cancel()}>
            Stop experiment
          </button>
        </div>
      </div>
      <aside className="content-panel lab-admission">
        <span className="eyebrow">02 / AGGREGATE BUDGET</span>
        <h2>
          {busy
            ? 'Experiment active'
            : admission
              ? admission.allowed
                ? 'Ready to run'
                : 'Run blocked'
              : 'Checking resources…'}
        </h2>
        {error && (
          <p role="alert" className="inline-error">
            {error}
          </p>
        )}
        {admission && (
          <>
            <ul className="admission-reasons">
              {admission.reasons.map((reason) => (
                <li key={reason}>{reason}</li>
              ))}
            </ul>
            <dl className="admission-values">
              <div>
                <dt>Total planned writes</dt>
                <dd>{gib(admission.totalWriteBytes)}</dd>
              </div>
              <div>
                <dt>Total wall time (incl. settling)</dt>
                <dd>{admission.totalWallSeconds} s</dd>
              </div>
              <div>
                <dt>Workload engine {admission.engineVersion}</dt>
                <dd>{admission.engineReady ? 'Verified' : 'Unavailable'}</dd>
              </div>
            </dl>
            <table className="phase-table">
              <thead>
                <tr>
                  <th>Phase</th>
                  <th>Planned writes</th>
                  <th>Allowed</th>
                </tr>
              </thead>
              <tbody>
                {admission.phaseAdmissions.map((phase, i) => (
                  <tr key={i}>
                    <td>{phaseLabel(definition, i)}</td>
                    <td>{gib(phase.plannedWriteBytes)}</td>
                    <td>{phase.allowed ? 'Yes' : 'No'}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </>
        )}
        {execution && (
          <div className="lab-status" role="status">
            <span className="eyebrow">EXPERIMENT PROGRESS</span>
            <h3>{execution.status}</h3>
            <p>
              Phase {Math.min(execution.currentPhaseOrdinal + 1, execution.phases.length)} of {execution.phases.length}
            </p>
            <ul className="phase-progress">
              {execution.phases.map((phase, i) => (
                <li
                  key={phase.phaseId}
                  data-state={phase.outcome ?? (i === execution.currentPhaseOrdinal ? 'running' : 'pending')}
                >
                  {activeDefinition ? phaseLabel(activeDefinition, i) : phase.phaseId} —{' '}
                  {phase.outcome ?? (i === execution.currentPhaseOrdinal && busy ? 'running' : 'pending')}
                </li>
              ))}
            </ul>
          </div>
        )}
      </aside>
      {execution && execution.status !== 'running' && execution.phases.some((phase) => phase.runId) && (
        <div className="comparison" style={{ gridColumn: '1 / -1' }}>
          <span className="eyebrow">{execution.status.toUpperCase()} / PHASE EVIDENCE</span>
          <h3>Results by phase</h3>
          <p>Each row is one real, independently recorded phase. Values are the phase's own reported summaries.</p>
          <table>
            <thead>
              <tr>
                <th>Phase</th>
                <th>Outcome</th>
                {(activeDefinition?.phases[0]?.observe ?? []).map((metricId) => (
                  <th key={metricId}>{metricId}</th>
                ))}
              </tr>
            </thead>
            <tbody>
              {execution.phases.map((phase, i) => {
                const recording = phase.runId ? phaseRecordings[phase.runId] : undefined;
                return (
                  <tr key={phase.phaseId}>
                    <td>{activeDefinition ? phaseLabel(activeDefinition, i) : phase.phaseId}</td>
                    <td>{phase.outcome ?? 'not started'}</td>
                    {(activeDefinition?.phases[0]?.observe ?? []).map((metricId) => {
                      const summary = recording?.metadata.summaries.find((s) => s.metricId === metricId);
                      return <td key={metricId}>{summary ? display(summary.value, summary.unit) : '—'}</td>;
                    })}
                  </tr>
                );
              })}
            </tbody>
          </table>
        </div>
      )}
    </section>
  );
}

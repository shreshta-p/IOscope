import { useEffect, useState } from 'react';
import type { WorkloadDefinition, WorkloadAdmission, WorkloadSnapshot } from '../../../contracts/src/index';
import { validateDomain } from '../../../contracts/src/validate';
import { api } from './client';

const initial: WorkloadDefinition = {
  schemaVersion: '1.0.0',
  definitionId: 'lab-read',
  engine: 'diskspd',
  readPercent: 100,
  blockBytes: 4096,
  queueDepth: 1,
  workingSetBytes: 64 * 1024 ** 2,
  intensity: 'light',
  pattern: 'random',
  cacheMode: 'buffered',
  durationSeconds: 5,
  warmupSeconds: 0,
  cooldownSeconds: 0,
};
const gib = (bytes: number) => (bytes / 1024 ** 3).toFixed(2) + ' GiB';
const active = (value: WorkloadSnapshot | null) =>
  value?.status && ['validating', 'preparing', 'running', 'stopping'].includes(value.status.state);

export function WorkloadLab({ onNotice, onRuns }: { onNotice: (message: string) => void; onRuns: () => void }) {
  const [workload, setWorkload] = useState(initial);
  const [admission, setAdmission] = useState<WorkloadAdmission | null>(null);
  const [snapshot, setSnapshot] = useState<WorkloadSnapshot | null>(null);
  const [error, setError] = useState('');
  const [pending, setPending] = useState(false);
  const [revision, setRevision] = useState(0);
  const busy = Boolean(active(snapshot));
  useEffect(() => {
    let disposed = false;
    async function refresh() {
      try {
        const value = await api('/runs/active');
        validateDomain('WorkloadSnapshot', value);
        if (!disposed) setSnapshot(value);
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
        const value = await api('/runs/admission', { method: 'POST', body: JSON.stringify(workload) });
        validateDomain('WorkloadAdmission', value);
        if (!disposed) setAdmission(value);
      } catch (error) {
        if (!disposed) setError(error instanceof Error ? error.message : 'Admission check failed');
      }
    }, 250);
    return () => {
      disposed = true;
      clearTimeout(timer);
    };
  }, [workload, revision, busy]);
  function change<K extends keyof WorkloadDefinition>(key: K, value: WorkloadDefinition[K]) {
    setAdmission(null);
    setWorkload((current) => ({ ...current, [key]: value }));
  }
  async function start() {
    setPending(true);
    setError('');
    try {
      const request = { schemaVersion: '1.0.0', requestId: crypto.randomUUID(), workload };
      const value = await api('/runs', { method: 'POST', body: JSON.stringify(request) });
      validateDomain('WorkloadSnapshot', value);
      setSnapshot(value);
      onNotice('Native workload admitted. Recording continues if this page closes.');
    } catch (error) {
      setError(error instanceof Error ? error.message : 'Workload rejected');
    } finally {
      setPending(false);
      setRevision((value) => value + 1);
    }
  }
  async function cancel() {
    try {
      const value = await api('/runs/cancel', { method: 'POST' });
      validateDomain('WorkloadSnapshot', value);
      setSnapshot(value);
      onNotice('Cancellation requested from the native agent.');
    } catch (error) {
      setError(error instanceof Error ? error.message : 'Cancellation failed');
    }
  }
  return (
    <section className="workload-lab">
      <div className="content-panel lab-controls">
        <span className="eyebrow">01 / DEFINE THE WORK</span>
        <h2>A bounded storage trial</h2>
        <p>
          Preparation creates and writes one temporary file. The measurement then uses that same file; it is removed
          when the run finishes. No workload starts until you press Start.
        </p>
        <div className="trial-footprint" aria-label="Actual trial size">
          <div>
            <span>Temporary test file</span>
            <strong>{workload.workingSetBytes / 1024 ** 2} MiB</strong>
          </div>
          <div>
            <span>Measurement</span>
            <strong>{workload.durationSeconds} seconds</strong>
          </div>
          <div>
            <span>Access</span>
            <strong>{workload.readPercent === 100 ? 'Read only' : `${workload.readPercent}% read`}</strong>
          </div>
        </div>
        <fieldset disabled={busy || pending} className="lab-fields">
          <legend>Workload controls</legend>
          <label>
            Access pattern
            <select
              value={workload.pattern}
              onChange={(event) => change('pattern', event.target.value as WorkloadDefinition['pattern'])}
            >
              <option value="random">Random</option>
              <option value="sequential">Sequential</option>
            </select>
          </label>
          <label>
            Read share
            <select
              value={workload.readPercent}
              onChange={(event) => change('readPercent', Number(event.target.value))}
            >
              {[100, 70, 50, 0].map((value) => (
                <option key={value} value={value}>
                  {value}% read / {100 - value}% write
                </option>
              ))}
            </select>
          </label>
          <label>
            Block size
            <select
              value={workload.blockBytes}
              onChange={(event) => change('blockBytes', Number(event.target.value) as WorkloadDefinition['blockBytes'])}
            >
              {[4096, 16384, 65536, 262144, 1048576].map((value) => (
                <option key={value} value={value}>
                  {value / 1024} KiB
                </option>
              ))}
            </select>
          </label>
          <label>
            Outstanding request limit
            <select value={workload.queueDepth} onChange={(event) => change('queueDepth', Number(event.target.value))}>
              {[1, 2, 4, 8, 16, 32].map((value) => (
                <option key={value}>{value}</option>
              ))}
            </select>
          </label>
          <label>
            Working set
            <select
              value={workload.workingSetBytes}
              onChange={(event) => change('workingSetBytes', Number(event.target.value))}
            >
              {[64, 256, 512, 1024].map((value) => (
                <option key={value} value={value * 1024 ** 2}>
                  {value} MiB
                </option>
              ))}
            </select>
          </label>
          <label>
            Measurement duration
            <select
              value={workload.durationSeconds}
              onChange={(event) => change('durationSeconds', Number(event.target.value))}
            >
              {[5, 10, 15].map((value) => (
                <option key={value} value={value}>
                  {value} seconds
                </option>
              ))}
            </select>
          </label>
          <label>
            Cache mode
            <select
              value={workload.cacheMode}
              onChange={(event) => change('cacheMode', event.target.value as WorkloadDefinition['cacheMode'])}
            >
              <option value="buffered">Buffered I/O (page cache)</option>
              <option value="unbuffered">Direct I/O (bypass page cache)</option>
            </select>
          </label>
        </fieldset>
        <p className="lab-footnote">
          Light intensity · one worker · no warmup or cooldown. Preparation may populate the cache; neither mode
          guarantees a cold-cache measurement.
        </p>
        <div className="action-row">
          <button className="primary" disabled={!admission?.allowed || busy || pending} onClick={() => void start()}>
            {pending ? 'Submitting…' : 'Start native workload'}
          </button>
          <button disabled={!busy} onClick={() => void cancel()}>
            Cancel workload
          </button>
        </div>
      </div>
      <aside className="content-panel lab-admission">
        <span className="eyebrow">02 / SPARE SPACE CHECKS</span>
        <h2>
          {busy
            ? 'Workload active'
            : admission
              ? admission.allowed
                ? 'Ready for a trial'
                : 'Run blocked'
              : 'Checking resources…'}
        </h2>
        <p>Reserves are space left free for other applications. IOscope does not allocate them.</p>
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
                <dt>Test file + recording allowance</dt>
                <dd>{admission.diskAllocationBytes / 1024 ** 2} MiB</dd>
              </div>
              <div>
                <dt>Disk available</dt>
                <dd>{gib(admission.diskFreeBytes)}</dd>
              </div>
              <div>
                <dt>Required disk reserve</dt>
                <dd>{gib(admission.diskReserveBytes)}</dd>
              </div>
              <div>
                <dt>Total free disk space needed</dt>
                <dd>{gib(admission.diskReserveBytes + admission.diskAllocationBytes)}</dd>
              </div>
              <div>
                <dt>RAM available</dt>
                <dd>{gib(admission.ramAvailableBytes)}</dd>
              </div>
              <div>
                <dt>Required RAM reserve</dt>
                <dd>{gib(admission.ramReserveBytes)}</dd>
              </div>
              <div>
                <dt>Maximum planned writes</dt>
                <dd>{gib(admission.plannedWriteBytes)}</dd>
              </div>
              <div>
                <dt>Offered I/O rate</dt>
                <dd>{(admission.rateBytesPerSecond / 1024 ** 2).toFixed(2)} MiB/s</dd>
              </div>
              <div>
                <dt>Workload engine {admission.engineVersion}</dt>
                <dd>{admission.engineReady ? 'Verified' : 'Unavailable'}</dd>
              </div>
            </dl>
            <p>
              {admission.restrictedThermals
                ? 'CPU or SSD temperature coverage is unavailable. Native safety restricts trials to light intensity and 15 seconds or less.'
                : 'Temperature and resource checks continue during execution.'}
            </p>
          </>
        )}
        <button onClick={() => setRevision((value) => value + 1)}>Refresh admission</button>
        {snapshot?.status && (
          <div className="lab-status" role="status">
            <span className="eyebrow">LATEST NATIVE RUN</span>
            <h3>{snapshot.status.state}</h3>
            <p>{snapshot.status.reason ?? 'Measurements and run state are recorded locally.'}</p>
            <p>{(snapshot.status.elapsedUs / 1e6).toFixed(1)} s elapsed, including preparation</p>
            {snapshot.recordingId && <button onClick={onRuns}>Open saved runs</button>}
          </div>
        )}
      </aside>
    </section>
  );
}

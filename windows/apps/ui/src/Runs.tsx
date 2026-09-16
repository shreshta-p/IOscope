import { useEffect, useState } from 'react';
import { Download, Play, Trash2, Save, ArrowRight } from 'lucide-react';
import type { Recording } from '../../../contracts/src/index';
import { validateDomain } from '../../../contracts/src/validate';
import { api, type StoredRun } from './client';
import { display } from './metrics';
import { recordingLabel } from './session';
import { compareMetric } from './comparison';
interface Props {
  current: Recording | null;
  onReplay: (r: Recording) => void;
  onExport: () => void;
  onImport: (f: File | undefined) => void;
  onNotice: (s: string) => void;
}
export function Runs({ current, onReplay, onExport, onImport, onNotice }: Props) {
  const [detail, setDetail] = useState<Recording | null>(null);
  const [runs, setRuns] = useState<StoredRun[]>([]),
    [selected, setSelected] = useState<string[]>([]),
    [comparison, setComparison] = useState<Recording[]>([]),
    [error, setError] = useState('');
  const refresh = () =>
    api('/recordings')
      .then((value) => {
        setRuns(value as StoredRun[]);
        setError('');
      })
      .catch((e) => setError(e.message));
  useEffect(() => {
    void refresh();
  }, []);
  const read = async (id: string) => {
    const value = await api('/recordings/' + id);
    validateDomain('Recording', value);
    return value;
  };
  async function save() {
    if (!current) return;
    try {
      await api('/recordings', { method: 'POST', body: JSON.stringify(current) });
      await refresh();
      onNotice('Run saved in the local SQLite database.');
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Save failed');
    }
  }
  async function compare() {
    try {
      const recordings = await Promise.all(selected.map(read));
      setComparison(recordings);
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Comparison failed');
    }
  }
  async function artifact(id: string) {
    try {
      const value = await api('/artifacts/' + id);
      validateDomain('ArtifactPayload', value);
      const digest = Array.from(
        new Uint8Array(await crypto.subtle.digest('SHA-256', new TextEncoder().encode(value.payload))),
      )
        .map((byte) => byte.toString(16).padStart(2, '0'))
        .join('');
      if (digest !== value.sha256) throw new Error('Artifact checksum mismatch');
      const url = URL.createObjectURL(
        new Blob([value.payload], { type: value.kind === 'diskspd-xml' ? 'application/xml' : 'text/plain' }),
      );
      const link = document.createElement('a');
      link.href = url;
      link.download = id + (value.kind === 'diskspd-xml' ? '.xml' : '.txt');
      link.click();
      URL.revokeObjectURL(url);
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Artifact unavailable');
    }
  }
  return (
    <section className="content-panel">
      <div className="section-heading">
        <div>
          <span className="eyebrow">LOCAL RUN LIBRARY</span>
          <h2>Nothing lost between runs.</h2>
          <p>Replay the original samples, flows, and phase events. Your data stays on this machine.</p>
        </div>
        <button className="primary" disabled={!current} onClick={() => void save()}>
          <Save size={16} />
          Save current run
        </button>
      </div>
      {error && (
        <p role="status" className="inline-error">
          {error.includes('fetch')
            ? 'Native storage is offline. Start the local agent; export and import still work.'
            : error}
        </p>
      )}
      <div className="action-row">
        <button disabled={!current} onClick={onExport}>
          <Download size={15} />
          Export JSON
        </button>
        <label className="file-button">
          Import recording
          <input type="file" accept=".json" onChange={(e) => onImport(e.target.files?.[0])} />
        </label>
        <button disabled={selected.length !== 2} onClick={() => void compare()}>
          Compare selected <ArrowRight size={15} />
        </button>
      </div>
      <div className="run-table">
        <div className="run-row table-head">
          <span />
          <span>PROFILE / SOURCE</span>
          <span>CAPTURED</span>
          <span>SIZE</span>
          <span>ACTIONS</span>
        </div>
        {runs.length === 0 ? (
          <p className="empty">No saved runs yet. Start a simulation, then save its evidence here.</p>
        ) : (
          runs.map((run) => (
            <div className="run-row" key={run.id}>
              <input
                aria-label={'Select ' + run.name}
                type="checkbox"
                checked={selected.includes(run.id)}
                onChange={(e) =>
                  setSelected((s) => (e.target.checked ? [...s, run.id].slice(-2) : s.filter((id) => id !== run.id)))
                }
              />
              <div>
                <strong>{run.name.replaceAll('-', ' ')}</strong>
                <small>
                  {run.origin.toUpperCase()} · {run.id.slice(0, 8)}
                </small>
              </div>
              <span>{new Date(run.startedAt).toLocaleString()}</span>
              <span>{(run.bytes / 1024).toFixed(0)} KiB</span>
              <div className="action-row compact">
                <button
                  aria-label={'Replay ' + run.name}
                  onClick={() =>
                    void read(run.id)
                      .then(onReplay)
                      .catch((e) => setError(e.message))
                  }
                >
                  <Play size={14} />
                </button>
                <button
                  aria-label={'Delete ' + run.name}
                  onClick={() => {
                    if (window.confirm('Delete this saved recording? The source export is not affected.'))
                      void api('/recordings/' + run.id, { method: 'DELETE' })
                        .then(refresh)
                        .catch((e) => setError(e.message));
                  }}
                >
                  <Trash2 size={14} />
                </button>
              </div>
            </div>
          ))
        )}
      </div>
      <div className="action-row">
        <button
          disabled={selected.length !== 1}
          onClick={() =>
            void read(selected[0]!)
              .then(setDetail)
              .catch((e) => setError(e.message))
          }
        >
          Inspect selected run
        </button>
      </div>
      {detail && (
        <div className="comparison">
          <span className="eyebrow">{detail.metadata.origin.toUpperCase()} / RUN EVIDENCE</span>
          <h3>
            {recordingLabel(detail)} · {detail.metadata.outcome}
          </h3>
          <p>{detail.metadata.abortReason ?? 'Original run outcome and measurement scope are preserved.'}</p>
          <p>
            {detail.metadata.engine.name} {detail.metadata.engine.version} · {detail.samples.length} recorded samples
          </p>
          {detail.metadata.summaries.length > 0 ? (
            <table>
              <thead>
                <tr>
                  <th>Measurement</th>
                  <th>Value</th>
                  <th>Scope / window</th>
                </tr>
              </thead>
              <tbody>
                {detail.metadata.summaries.map((value) => (
                  <tr key={value.metricId + '/' + value.scope}>
                    <td>{value.metricId}</td>
                    <td>{display(value.value, value.unit)}</td>
                    <td>
                      {value.scope} / {value.windowMs} ms
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          ) : (
            <p>No workload summary was captured for this run.</p>
          )}
          <div className="action-row">
            {detail.metadata.artifacts
              .filter((value) => value.kind !== 'recording')
              .map((value) => (
                <button key={value.artifactId} onClick={() => void artifact(value.artifactId)}>
                  Download {value.kind}
                </button>
              ))}
          </div>
        </div>
      )}
      {comparison.length === 2 && (
        <div className="comparison">
          <h3>Compared evidence</h3>
          <p>
            Arithmetic means over recorded samples. System scope includes all observed system activity; percentiles are
            not averaged.
          </p>
          <table>
            <thead>
              <tr>
                <th>Metric</th>
                {comparison.map((r, i) => (
                  <th key={i}>{recordingLabel(r)}</th>
                ))}
                <th>Change</th>
              </tr>
            </thead>
            <tbody>
              {[
                'cpu.utilization',
                'storage.read.bytes_per_second',
                'storage.latency.mean',
                'storage.queue.average',
              ].map((id) => {
                const result = compareMetric(comparison[0]!, comparison[1]!, id);
                return (
                  <tr key={id}>
                    <td>
                      {id}
                      <small>{result.reason ?? `${result.counts[0]} / ${result.counts[1]} samples`}</small>
                    </td>
                    <td>{display(result.means[0]!, result.unit)}</td>
                    <td>{display(result.means[1]!, result.unit)}</td>
                    <td>
                      {result.reason ??
                        `${display(result.delta, result.unit)} / ${result.percent === null ? 'No relative baseline' : result.percent.toFixed(1) + '%'}`}
                    </td>
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

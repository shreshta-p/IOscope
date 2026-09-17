import { useEffect, useState } from 'react';
import type { Recording, AnalyzerEvent } from '../../../contracts/src/index';
import { validateDomain } from '../../../contracts/src/validate';
import { api, type StoredRun } from './client';

const confidenceLabel: Record<AnalyzerEvent['confidence'], string> = { low: 'Low', medium: 'Medium', high: 'High' };

export function Analyze() {
  const [runs, setRuns] = useState<StoredRun[]>([]);
  const [selectedId, setSelectedId] = useState<string | null>(null);
  const [detail, setDetail] = useState<Recording | null>(null);
  const [error, setError] = useState('');

  useEffect(() => {
    api('/recordings')
      .then((value) => setRuns(value as StoredRun[]))
      .catch((e) => setError(e instanceof Error ? e.message : 'Native storage unavailable'));
  }, []);

  useEffect(() => {
    if (!selectedId) {
      setDetail(null);
      return;
    }
    let disposed = false;
    api('/recordings/' + selectedId)
      .then((value) => {
        validateDomain('Recording', value);
        if (!disposed) setDetail(value);
      })
      .catch((e) => {
        if (!disposed) setError(e instanceof Error ? e.message : 'Could not load recording');
      });
    return () => {
      disposed = true;
    };
  }, [selectedId]);

  const events = detail
    ? detail.samples.flatMap((sample) =>
        sample.analyzerEvents.map((event) => ({ event, elapsedS: sample.telemetry.elapsedUs / 1e6 })),
      )
    : [];

  return (
    <section className="content-panel">
      <div className="section-heading">
        <div>
          <span className="eyebrow">DETERMINISTIC ANALYZER</span>
          <h2>Evidence, not guesses.</h2>
          <p>
            Versioned rules over recorded telemetry, each event backed by the exact samples that produced it. Missing or
            stale metrics suppress a rule rather than faking a result.
          </p>
        </div>
      </div>
      {error && (
        <p role="alert" className="inline-error">
          {error}
        </p>
      )}
      <fieldset className="lab-fields">
        <legend>Run selection</legend>
        <label>
          Select a saved run
          <select value={selectedId ?? ''} onChange={(e) => setSelectedId(e.target.value || null)}>
            <option value="">Choose a run…</option>
            {runs.map((run) => (
              <option key={run.id} value={run.id}>
                {run.name.replaceAll('-', ' ')} · {new Date(run.startedAt).toLocaleString()}
              </option>
            ))}
          </select>
        </label>
      </fieldset>
      {!selectedId && runs.length === 0 && (
        <p className="empty">No saved runs yet. Save a run from the Runs page, then analyze it here.</p>
      )}
      {detail && events.length === 0 && (
        <p className="empty">
          No analyzer events were observed in this run. That is itself evidence — the rules found nothing to flag, not
          that analysis was skipped.
        </p>
      )}
      {detail && events.length > 0 && (
        <div className="run-table">
          <div className="run-row table-head">
            <span>Time</span>
            <span>Rule</span>
            <span>Observation</span>
            <span>Confidence</span>
          </div>
          {events.map(({ event, elapsedS }) => (
            <div className="run-row" key={event.eventId}>
              <span>{elapsedS.toFixed(1)}s</span>
              <span>{event.ruleId}</span>
              <div>
                <strong>{event.observation}</strong>
                <small>{event.interpretation}</small>
              </div>
              <span>{confidenceLabel[event.confidence]}</span>
            </div>
          ))}
        </div>
      )}
      {detail && events.length > 0 && (
        <div className="comparison">
          <h3>Evidence references</h3>
          <p>Every claim above cites the exact sample sequence, device and metric it was computed from.</p>
          <table>
            <thead>
              <tr>
                <th>Rule</th>
                <th>Sequence</th>
                <th>Device</th>
                <th>Metric</th>
              </tr>
            </thead>
            <tbody>
              {events.flatMap(({ event }) =>
                event.evidence.map((reference, i) => (
                  <tr key={event.eventId + '/' + i}>
                    <td>{event.ruleId}</td>
                    <td>{reference.sequence}</td>
                    <td>{reference.deviceId}</td>
                    <td>{reference.metricId}</td>
                  </tr>
                )),
              )}
            </tbody>
          </table>
        </div>
      )}
    </section>
  );
}

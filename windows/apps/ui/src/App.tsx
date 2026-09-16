import { useEffect, useState } from 'react';
import {
  Activity,
  FlaskConical,
  Layers3,
  History,
  BookOpen,
  ScanLine,
  ArrowUpRight,
  Play,
  Pause,
  SkipBack,
  SkipForward,
  RotateCcw,
  Download,
  Square,
  Cpu,
  ChevronRight,
} from 'lucide-react';
import { createRecording, scenarios, type Scenario } from '../../../simulation/src/index';
import { type Session, changeSource, recordingDuration, playbackDelay } from './session';
import { components, metric, display, metricNames } from './metrics';
import { useLive } from './useLive';
import { deriveFlow } from '../../../simulation/src/flows';
import { Runs } from './Runs';
import { WorkloadLab } from './WorkloadLab';
import { api } from './client';
import { DigitalTwin } from './DigitalTwin';
import { validateDomain } from '../../../contracts/src/validate';
import type { WorkloadSnapshot } from '../../../contracts/src/index';
const navigation = [
  { title: 'Live', icon: Activity },
  { title: 'Workload Lab', icon: Layers3 },
  { title: 'Experiments', icon: FlaskConical },
  { title: 'Runs', icon: History },
  { title: 'Learn', icon: BookOpen },
  { title: 'Analyze', icon: ScanLine },
];
export function App() {
  const [page, setPage] = useState('Live'),
    [selected, setSelected] = useState('nvme0'),
    [scenario, setScenario] = useState<Scenario>('model-loading');
  const [session, setSession] = useState<Session>({
    mode: 'LIVE',
    recording: null,
    index: 0,
    playing: false,
    speed: 1,
    live: null,
  });
  const [notice, setNotice] = useState(''),
    [logical, setLogical] = useState(false),
    [isolated, setIsolated] = useState(false),
    [focus, setFocus] = useState(0),
    [requestedMeasurement, setMeasurement] = useState(false);
  const [nativeWorkload, setNativeWorkload] = useState<WorkloadSnapshot | null>(null);
  const nativeBusy = Boolean(
    nativeWorkload?.status && ['validating', 'preparing', 'running', 'stopping'].includes(nativeWorkload.status.state),
  );
  const measurement = requestedMeasurement || nativeBusy;
  useEffect(() => {
    let disposed = false;
    const refresh = async () => {
      try {
        const value = await api('/runs/active');
        validateDomain('WorkloadSnapshot', value);
        if (!disposed) {
          setNativeWorkload(value);
          if (value.status && ['preparing', 'running', 'stopping'].includes(value.status.state)) setMeasurement(true);
        }
      } catch {
        if (!disposed) setNativeWorkload(null);
      }
    };
    void refresh();
    const timer = setInterval(() => void refresh(), 1000);
    return () => {
      disposed = true;
      clearInterval(timer);
    };
  }, []);
  const sample = session.recording?.samples[session.index],
    frame = session.mode === 'LIVE' ? session.live : (sample?.telemetry ?? null);
  const connected = useLive(setSession);
  const flow =
    session.mode === 'LIVE' && frame
      ? deriveFlow(frame, nativeBusy ? (nativeWorkload?.workload?.queueDepth ?? null) : null)
      : sample?.flow;
  const component = components.find((c) => c.id === selected)!;
  function demo(next: Scenario = scenario) {
    setScenario(next);
    setSession((s) =>
      changeSource(
        s,
        'SIMULATED',
        createRecording({
          seed: 42,
          epochUtc: '2026-09-06T00:00:00Z',
          durationSeconds: 60,
          sampleIntervalMs: 1000,
          scenario: next,
        }),
      ),
    );
    setPage('Live');
  }
  useEffect(() => {
    if (!session.playing || !session.recording) return;
    const timer = setTimeout(
      () =>
        setSession((s) =>
          s.recording && s.index < s.recording.samples.length - 1
            ? { ...s, index: s.index + 1 }
            : { ...s, playing: false },
        ),
      playbackDelay(session.recording, session.index, session.speed),
    );
    return () => clearTimeout(timer);
  }, [session.playing, session.recording, session.index, session.speed]);
  function download() {
    if (!session.recording) return;
    const url = URL.createObjectURL(
      new Blob([JSON.stringify(session.recording, null, 2)], { type: 'application/json' }),
    );
    const a = document.createElement('a');
    a.href = url;
    a.download = session.recording.metadata.runId + '.json';
    a.click();
    URL.revokeObjectURL(url);
    setNotice('Recording exported with original ' + session.recording.metadata.origin + ' provenance.');
  }
  async function importRun(file: File | undefined) {
    if (!file) return;
    try {
      if (file.size > 32 * 1024 * 1024) throw new Error('File exceeds import limit');
      const value: unknown = JSON.parse(await file.text());
      validateDomain('Recording', value);
      setSession((s) => changeSource(s, 'RECORDED', value));
      setPage('Live');
      setNotice('Recorded run loaded. Original source: ' + value.metadata.origin + '.');
    } catch (error) {
      setNotice(error instanceof Error ? error.message : 'Import failed');
    }
  }
  return (
    <div className="app">
      <aside className="sidebar">
        <a className="brand" href="#live" onClick={() => setPage('Live')}>
          <span className="brand-mark">
            <i />
            <i />
            <i />
          </span>
          IOscope<span className="version">01</span>
        </a>
        <div className="sidebar-label">SYSTEMS OBSERVATORY</div>
        <nav aria-label="Primary">
          {navigation.map(({ title, icon: Icon }, i) => (
            <button
              key={title}
              className={page === title ? 'nav-item active' : 'nav-item'}
              onClick={() => setPage(title)}
            >
              <Icon size={18} />
              <span>{title}</span>
              <small>0{i + 1}</small>
            </button>
          ))}
        </nav>
        <div className="sidebar-bottom">
          <div className="local-dot" /> LOCAL WORKSPACE
          <p>
            Observe. Understand.
            <br />
            Run it again.
          </p>
          <div className="build-label">
            WINDOWS / NATIVE FIRST
            <br />
            VERSION 0.6 · DEVELOPMENT
          </div>
        </div>
      </aside>
      <main>
        <header className="topbar">
          <div className="breadcrumbs">
            Workspace <ChevronRight size={13} /> <strong>{page}</strong>
          </div>
          <div className="top-actions">
            <span className={'source-badge ' + session.mode.toLowerCase()}>
              <i />
              {session.mode}
              {session.mode === 'RECORDED' ? ' · ' + session.recording?.metadata.origin.toUpperCase() : ''}
            </span>
            <button
              className="stop"
              onClick={() => {
                setSession((s) => ({ ...s, playing: false }));
                void api('/runs/cancel', { method: 'POST' })
                  .then(() => setNotice('Playback stopped. Native cancellation requested.'))
                  .catch((error) =>
                    setNotice(
                      'Playback stopped; native cancellation could not be confirmed: ' +
                        (error instanceof Error ? error.message : 'Agent unavailable'),
                    ),
                  );
              }}
            >
              <Square size={12} /> Emergency stop
            </button>
          </div>
        </header>
        <div className="workspace">
          <div className="page-heading">
            <div>
              <div className="eyebrow">THE MACHINE, MADE VISIBLE</div>
              <h1>{page === 'Live' ? 'System overview' : page}</h1>
              <p>
                {page === 'Live'
                  ? 'Follow activity from storage to silicon.'
                  : page === 'Workload Lab'
                    ? 'One workload. A clear view of its behavior.'
                    : page === 'Experiments'
                      ? 'Change one variable. Understand what follows.'
                      : page === 'Runs'
                        ? 'Keep the evidence. Revisit the moment.'
                        : page === 'Learn'
                          ? 'Understand the components behind the measurements.'
                          : 'Observations first. Interpretation second.'}
              </p>
            </div>
            <div className="source-controls">
              <button
                className={session.mode === 'LIVE' ? 'selected' : ''}
                onClick={() => setSession((s) => changeSource(s, 'LIVE'))}
              >
                Live system
              </button>
              <button className={session.mode === 'SIMULATED' ? 'selected' : ''} onClick={() => demo()}>
                Simulation <ArrowUpRight size={14} />
              </button>
            </div>
          </div>
          {notice && (
            <div role="status" className="notice">
              {notice}
              <button aria-label="Dismiss notification" onClick={() => setNotice('')}>
                ×
              </button>
            </div>
          )}
          {page === 'Live' ? (
            <>
              <div className="system-strip">
                {[
                  { name: 'CPU', id: 'cpu.utilization', unit: 'percent' },
                  { name: 'RAM AVAILABLE', id: 'ram.available', unit: 'bytes' },
                  { name: 'STORAGE READ', id: 'storage.read.bytes_per_second', unit: 'bytes/s' },
                  { name: 'GPU', id: 'gpu.utilization', unit: 'percent' },
                ].map((x) => (
                  <div key={x.id}>
                    <span>{x.name}</span>
                    <strong>{display(metric(frame, x.id), x.unit)}</strong>
                    <small>
                      {frame
                        ? (frame.measurements.find((m) => m.metricId === x.id)?.provenance ?? 'Unavailable') +
                          ' / system'
                        : 'Waiting for native agent'}
                    </small>
                  </div>
                ))}
              </div>
              <div className="observatory">
                <section className="scene-panel">
                  <div className="scene-toolbar">
                    <div className="segmented">
                      <button className={!logical ? 'selected' : ''} onClick={() => setLogical(false)}>
                        Physical
                      </button>
                      <button className={logical ? 'selected' : ''} onClick={() => setLogical(true)}>
                        Data path
                      </button>
                    </div>
                    <div>
                      <button
                        aria-pressed={measurement}
                        disabled={nativeBusy}
                        onClick={() => setMeasurement(!measurement)}
                      >
                        {measurement ? 'Measurement mode' : 'Full scene'}
                      </button>
                      <button
                        aria-label="Reset camera"
                        onClick={() => {
                          setFocus(0);
                          setIsolated(false);
                        }}
                      >
                        <RotateCcw size={15} />
                      </button>
                      <button onClick={() => setIsolated(!isolated)}>{isolated ? 'Show system' : 'Isolate'}</button>
                    </div>
                  </div>
                  <DigitalTwin
                    selected={selected}
                    onSelect={setSelected}
                    logical={logical}
                    isolated={isolated}
                    focus={focus}
                    flow={flow}
                    playing={!measurement && (session.mode === 'LIVE' ? connected : session.playing)}
                    measurement={measurement}
                    speed={session.speed}
                    reduced={window.matchMedia('(prefers-reduced-motion: reduce)').matches}
                  />

                  <div className="scene-caption">
                    <span>
                      <i className="teal" />
                      Read / host transfer
                    </span>
                    <span>
                      <i className="amber" />
                      Write
                    </span>
                    <span>Aggregated activity · not individual requests</span>
                  </div>
                </section>
                <aside className="inspector">
                  <div className="eyebrow">COMPONENT INSPECTOR</div>
                  <div className="component-tabs" aria-label="Select component">
                    {components.map((c) => (
                      <button
                        key={c.id}
                        className={selected === c.id ? 'selected' : ''}
                        onClick={() => setSelected(c.id)}
                      >
                        {session.mode === 'LIVE' && c.id === 'nvme0' ? 'Storage' : c.name}
                      </button>
                    ))}
                  </div>
                  <span className="chip-icon">
                    <Cpu size={30} />
                  </span>
                  <h2>{session.mode === 'LIVE' && selected === 'nvme0' ? 'Storage' : component.name}</h2>
                  <p>
                    {session.mode === 'LIVE'
                      ? (session.inventory?.devices.find((d) => d.deviceId === selected)?.name ?? component.detail)
                      : component.detail}
                  </p>
                  <button className="text-button" onClick={() => setFocus((f) => f + 1)}>
                    Focus component <ArrowUpRight size={14} />
                  </button>
                  <dl>
                    {component.metrics.map((id) => {
                      const m = frame?.measurements.find((m) => m.metricId === id);
                      return (
                        <div key={id}>
                          <dt>{metricNames[id]}</dt>
                          <dd title={m?.reason ?? m?.provenance}>{display(metric(frame, id), m?.unit ?? '')}</dd>
                        </div>
                      );
                    })}
                  </dl>
                  {selected === 'nvme0' && (
                    <div className="queue">
                      <div className="eyebrow">QUEUE INSPECTOR</div>
                      <div className="queue-track">
                        <span style={{ width: `${Math.min(100, ((flow?.queue.observedAverage ?? 0) / 32) * 100)}%` }} />
                      </div>
                      <p>
                        Observed average <b>{flow?.queue.observedAverage?.toFixed(1) ?? 'Unavailable'}</b>
                      </p>
                      <p>
                        Configured limit <b>{flow?.queue.configuredLimit ?? 'Unavailable'}</b>
                      </p>
                      <small>Aggregated queue representation. No individual requests are traced.</small>
                    </div>
                  )}
                </aside>
              </div>
              <section className="timeline">
                <div className="timeline-top">
                  <div>
                    <span className="eyebrow">SESSION TIMELINE</span>
                    <h3>
                      {sample?.phaseId
                        ? sample.phaseId.replaceAll('-', ' ').toUpperCase()
                        : frame
                          ? 'System activity'
                          : 'No telemetry source connected'}
                    </h3>
                  </div>
                  <div className="playback">
                    <button
                      aria-label="Previous sample"
                      disabled={!sample}
                      onClick={() => setSession((s) => ({ ...s, index: Math.max(0, s.index - 1), playing: false }))}
                    >
                      <SkipBack size={16} />
                    </button>
                    <button
                      aria-label={session.playing ? 'Pause' : 'Play'}
                      disabled={!sample}
                      onClick={() =>
                        setSession((s) => ({
                          ...s,
                          playing: !s.playing,
                          index: s.recording && s.index === s.recording.samples.length - 1 ? 0 : s.index,
                        }))
                      }
                    >
                      {session.playing ? <Pause size={17} /> : <Play size={17} />}
                    </button>
                    <button
                      aria-label="Next sample"
                      disabled={!sample}
                      onClick={() =>
                        setSession((s) => ({
                          ...s,
                          index: Math.min(s.recording!.samples.length - 1, s.index + 1),
                          playing: false,
                        }))
                      }
                    >
                      <SkipForward size={16} />
                    </button>
                    <select
                      aria-label="Playback speed"
                      value={session.speed}
                      onChange={(e) => setSession((s) => ({ ...s, speed: Number(e.target.value) }))}
                    >
                      {[0.25, 0.5, 1, 2, 4].map((v) => (
                        <option key={v} value={v}>
                          {v}×
                        </option>
                      ))}
                    </select>
                  </div>
                </div>
                <input
                  type="range"
                  aria-label="Replay timeline"
                  min={0}
                  max={(session.recording?.samples.length ?? 2) - 1}
                  value={session.index}
                  disabled={!sample}
                  onChange={(e) => setSession((s) => ({ ...s, index: Number(e.target.value), playing: false }))}
                />
                <div className="timeline-labels">
                  <span>{((frame?.elapsedUs ?? 0) / 1e6).toFixed(1)} s</span>
                  <span>
                    {session.recording
                      ? 'Deterministic playback · original evidence preserved'
                      : (session.liveGap ?? 'Connect the native agent, or explore a labeled simulation.')}
                  </span>
                  <span>{recordingDuration(session.recording)} s</span>
                </div>
              </section>
            </>
          ) : page === 'Workload Lab' ? (
            <WorkloadLab onNotice={setNotice} onRuns={() => setPage('Runs')} />
          ) : page === 'Runs' ? (
            <Runs
              current={session.recording}
              onReplay={(r) => {
                setSession((s) => changeSource(s, 'RECORDED', r));
                setPage('Live');
              }}
              onExport={download}
              onImport={(f) => void importRun(f)}
              onNotice={setNotice}
            />
          ) : (
            <section className="content-panel">
              <span className="eyebrow">EXPLORE THE SYSTEM</span>
              <h2>
                {page === 'Analyze'
                  ? 'Evidence needs a source.'
                  : page === 'Learn'
                    ? 'Start with a behavior you can see.'
                    : 'Choose a controlled profile.'}
              </h2>
              <p>Explore an explicitly labeled synthetic scenario while the native connection is unavailable.</p>
              <div className="scenario-grid">
                {scenarios.map((name) => (
                  <button key={name} onClick={() => demo(name)}>
                    <span>{name.replaceAll('-', ' ')}</span>
                    <ArrowUpRight size={18} />
                  </button>
                ))}
              </div>
            </section>
          )}
          <footer>
            <span>
              <i className="local-dot" />
              {session.mode === 'LIVE'
                ? connected
                  ? 'LIVE TELEMETRY · native Windows agent'
                  : 'Native agent not connected'
                : session.mode === 'RECORDED'
                  ? 'RECORDED · original ' + session.recording?.metadata.origin.toUpperCase() + ' evidence'
                  : 'SIMULATED DATA · no hardware workload'}
            </span>
            <span>IOscope / Systems performance laboratory</span>
          </footer>
        </div>
      </main>
    </div>
  );
}

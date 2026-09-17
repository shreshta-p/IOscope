import { useState } from 'react';
import { ArrowUpRight } from 'lucide-react';
import { learnTopics } from './learn-content';
import { experimentCatalog } from './experiments-catalog';

interface Props {
  onSelectComponent: (componentId: string) => void;
  onOpenExperiments: (definitionId: string) => void;
}

export function Learn({ onSelectComponent, onOpenExperiments }: Props) {
  const [topicId, setTopicId] = useState(learnTopics[0]!.id);
  const topic = learnTopics.find((t) => t.id === topicId)!;
  const experiment = topic.experimentId ? experimentCatalog.find((e) => e.definitionId === topic.experimentId) : null;
  const gpuGated = topic.experimentId === 'gpu-pipeline';

  return (
    <section className="workload-lab">
      <div className="content-panel lab-controls">
        <span className="eyebrow">01 / PICK A TOPIC</span>
        <h2>Thirteen short topics.</h2>
        <p>Four sections each: what it is, why it matters, where to see it, and how to try it yourself.</p>
        <div className="scenario-grid">
          {learnTopics.map((t) => (
            <button
              key={t.id}
              className={t.id === topicId ? 'selected' : ''}
              onClick={() => setTopicId(t.id)}
              aria-current={t.id === topicId}
            >
              <span>{t.title}</span>
              <ArrowUpRight size={16} />
            </button>
          ))}
        </div>
      </div>
      <aside className="content-panel lab-admission">
        <span className="eyebrow">02 / {topic.title.toUpperCase()}</span>
        <h2>What</h2>
        <p>{topic.what}</p>
        <h2>Why it matters</h2>
        <p>{topic.why}</p>
        <h2>See in the system</h2>
        <p>{topic.seeInSystem}</p>
        <div className="action-row">
          <button onClick={() => onSelectComponent(topic.componentId)}>
            Highlight in Live view <ArrowUpRight size={14} />
          </button>
        </div>
        <h2>Try it yourself</h2>
        <p>{topic.tryYourself}</p>
        <div className="action-row">
          {experiment && (
            <button
              onClick={() => onOpenExperiments(experiment.definitionId)}
              disabled={gpuGated}
              aria-disabled={gpuGated}
            >
              {gpuGated ? 'Requires a supported GPU' : `Open "${experiment.title}"`}{' '}
              {!gpuGated && <ArrowUpRight size={14} />}
            </button>
          )}
          {!experiment && <span className="lab-footnote">No dedicated experiment exists for this topic yet.</span>}
        </div>
      </aside>
    </section>
  );
}

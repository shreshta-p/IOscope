import { Suspense, Component, useEffect, useMemo, useRef } from 'react';
import { Canvas, useFrame, type ThreeEvent } from '@react-three/fiber';
import { Html, Line, OrbitControls } from '@react-three/drei';
import { Color, InstancedMesh, Object3D, Vector3 } from 'three';
import type { OrbitControls as OrbitControlsImpl } from 'three-stdlib';
import type { FlowState } from '../../../contracts/src/index';
import { components } from './metrics';
type Point = [number, number, number];
const physical: Record<string, Point> = {
  cpu0: [-1.8, 0.45, -0.25],
  ram0: [-3, 0.35, 2.2],
  nvme0: [2.4, 0.4, 2.3],
  gpu0: [1.5, 0.5, -0.35],
  vram0: [3.8, 0.35, 0.05],
};
const logical: Record<string, Point> = {
  nvme0: [-4.2, 0.45, 0],
  ram0: [-1.5, 0.45, 0],
  vram0: [1.3, 0.45, 0],
  gpu0: [4, 0.45, 0],
  cpu0: [-1.5, 0.45, -2.5],
};
const names: Record<string, string> = {
  cpu0: '01 / CPU',
  ram0: '02 / SYSTEM RAM',
  nvme0: '03 / STORAGE',
  gpu0: '04 / GPU',
  vram0: '05 / VRAM',
};
interface Props {
  selected: string;
  onSelect: (id: string) => void;
  logical: boolean;
  isolated: boolean;
  focus: number;
  flow: FlowState | undefined;
  playing: boolean;
  measurement: boolean;
  speed: number;
  reduced: boolean;
}
class SceneBoundary extends Component<{ children: React.ReactNode }, { failed: boolean }> {
  state = { failed: false };
  static getDerivedStateFromError() {
    return { failed: true };
  }
  render() {
    return this.state.failed ? (
      <div className="scene-placeholder">
        <h2>3D rendering unavailable</h2>
        <p>Use the component inspector to explore all metrics.</p>
      </div>
    ) : (
      this.props.children
    );
  }
}
function Box({
  position,
  size,
  color = '#284642',
  metalness = 0.2,
  opacity = 1,
}: {
  position: Point;
  size: Point;
  color?: string;
  metalness?: number;
  opacity?: number;
}) {
  return (
    <mesh position={position}>
      <boxGeometry args={size} />
      <meshStandardMaterial
        color={color}
        metalness={metalness}
        roughness={0.65}
        transparent={opacity < 1}
        opacity={opacity}
      />
    </mesh>
  );
}
function Details() {
  const ref = useRef<InstancedMesh>(null);
  useEffect(() => {
    const dummy = new Object3D();
    for (let i = 0; i < 64; i++) {
      const x = (i % 16) * 0.6 - 4.5,
        z = Math.floor(i / 16) * 1.25 - 2.1;
      dummy.position.set(x, 0.18, z);
      dummy.scale.set(0.16, 0.11, 0.27);
      dummy.updateMatrix();
      ref.current!.setMatrixAt(i, dummy.matrix);
    }
    ref.current!.instanceMatrix.needsUpdate = true;
  }, []);
  return (
    <instancedMesh ref={ref} args={[undefined, undefined, 64]}>
      <boxGeometry />
      <meshStandardMaterial color="#121b1c" roughness={0.55} />
    </instancedMesh>
  );
}
function Fan({ x }: { x: number }) {
  const ref = useRef<InstancedMesh>(null);
  useEffect(() => {
    const dummy = new Object3D();
    for (let i = 0; i < 15; i++) {
      const t = (i / 15) * Math.PI * 2;
      dummy.position.set(Math.cos(t) * 0.56, 0.14, Math.sin(t) * 0.56);
      dummy.rotation.y = -t + 0.5;
      dummy.scale.set(0.7, 0.08, 0.14);
      dummy.updateMatrix();
      ref.current!.setMatrixAt(i, dummy.matrix);
    }
    ref.current!.instanceMatrix.needsUpdate = true;
  }, []);
  return (
    <group position={[x, 0.25, -2.25]}>
      <mesh>
        <cylinderGeometry args={[1.12, 1.12, 0.15, 40]} />
        <meshStandardMaterial color="#172427" />
      </mesh>
      <mesh rotation={[Math.PI / 2, 0, 0]}>
        <torusGeometry args={[1.02, 0.04, 6, 40]} />
        <meshStandardMaterial color="#758585" metalness={0.8} roughness={0.45} />
      </mesh>
      <instancedMesh ref={ref} args={[undefined, undefined, 15]}>
        <boxGeometry />
        <meshStandardMaterial color="#445756" metalness={0.6} roughness={0.5} />
      </instancedMesh>
      <mesh position={[0, 0.19, 0]}>
        <cylinderGeometry args={[0.25, 0.25, 0.14, 24]} />
        <meshStandardMaterial color="#283e3f" metalness={0.6} />
      </mesh>
    </group>
  );
}
function Chip({ id, position, props }: { id: string; position: Point; props: Props }) {
  const chosen = props.selected === id,
    opacity = props.isolated && !chosen ? 0.12 : 1;
  const size: Point =
    id === 'ram0'
      ? [2.5, 0.15, 0.6]
      : id === 'nvme0'
        ? [1, 0.15, 2.1]
        : id === 'vram0'
          ? [0.8, 0.18, 1.7]
          : [1.8, 0.25, 1.6];
  const down = useRef<[number, number]>([0, 0]);
  const click = (e: ThreeEvent<MouseEvent>) => {
    e.stopPropagation();
    if (Math.hypot(e.clientX - down.current[0], e.clientY - down.current[1]) < 5) props.onSelect(id);
  };
  return (
    <group
      position={position}
      onPointerDown={(e) => {
        down.current = [e.clientX, e.clientY];
      }}
      onClick={click}
      onPointerOver={(e) => {
        e.stopPropagation();
        document.body.style.cursor = 'pointer';
      }}
      onPointerOut={() => {
        document.body.style.cursor = 'auto';
      }}
    >
      <Box position={[0, 0, 0]} size={size} color={chosen ? '#386a5b' : '#24443d'} opacity={opacity} />
      <Box
        position={[0, 0.17, 0]}
        size={[size[0] * 0.76, 0.18, size[2] * 0.73]}
        color={id === 'cpu0' ? '#91a8a2' : id === 'gpu0' ? '#637d79' : '#152426'}
        metalness={0.8}
        opacity={opacity}
      />
      {(id === 'cpu0' || id === 'gpu0') && (
        <Box position={[0, 0.28, 0]} size={[0.72, 0.035, 0.67]} color="#829996" metalness={1} opacity={opacity} />
      )}
      {chosen && (
        <Line
          points={[
            [-size[0] / 2 - 0.08, 0.28, -size[2] / 2 - 0.08],
            [size[0] / 2 + 0.08, 0.28, -size[2] / 2 - 0.08],
            [size[0] / 2 + 0.08, 0.28, size[2] / 2 + 0.08],
            [-size[0] / 2 - 0.08, 0.28, size[2] / 2 + 0.08],
            [-size[0] / 2 - 0.08, 0.28, -size[2] / 2 - 0.08],
          ]}
          color="#8feaca"
          lineWidth={1.3}
        />
      )}
      {opacity === 1 && (
        <Html position={[0, 0.55, size[2] / 2 + 0.3]} center zIndexRange={[10, 0]}>
          <span className="canvas-label">{names[id]}</span>
        </Html>
      )}
    </group>
  );
}
function Flows({ props, positions }: { props: Props; positions: Record<string, Point> }) {
  const ref = useRef<InstancedMesh>(null),
    offset = useRef(0);
  const paths = props.flow?.paths ?? [];
  const particlePaths = useMemo(
    () =>
      paths
        .flatMap((path) =>
          path.status === 'active'
            ? Array.from({ length: Math.ceil(path.activity * 64) }, (_, i) => ({
                path,
                i,
                count: Math.ceil(path.activity * 64),
              }))
            : [],
        )
        .slice(0, 256),
    [props.flow],
  );
  useEffect(() => {
    offset.current = 0;
  }, [props.flow?.sequence]);
  useFrame((_state, delta) => {
    if (!ref.current) return;
    if (props.playing && !props.reduced) offset.current = Math.min(1, offset.current + delta * props.speed);
    const dummy = new Object3D();
    for (let index = 0; index < particlePaths.length; index++) {
      const entry = particlePaths[index]!,
        a = positions[entry.path.from],
        b = positions[entry.path.to];
      if (!a || !b) continue;
      const t = (entry.i / entry.count + ((props.flow?.elapsedUs ?? 0) / 1e6 + offset.current) * 0.32) % 1;
      dummy.position.set(a[0] + (b[0] - a[0]) * t, 0.8, a[2] + (b[2] - a[2]) * t);
      dummy.scale.setScalar(props.reduced ? 0 : 0.035 + entry.path.activity * 0.025);
      dummy.updateMatrix();
      ref.current.setMatrixAt(index, dummy.matrix);
      ref.current.setColorAt(index, new Color(entry.path.pathId.includes('write') ? '#e7b66f' : '#86ecd0'));
    }
    ref.current.count = particlePaths.length;
    ref.current.instanceMatrix.needsUpdate = true;
    if (ref.current.instanceColor) ref.current.instanceColor.needsUpdate = true;
  });
  return (
    <>
      {paths.map((path) =>
        positions[path.from] && positions[path.to] ? (
          <Line
            key={path.pathId}
            points={[
              [positions[path.from]![0], 0.76, positions[path.from]![2]],
              [positions[path.to]![0], 0.76, positions[path.to]![2]],
            ]}
            color={path.status === 'unknown' ? '#4a6264' : path.pathId.includes('write') ? '#b78b4d' : '#457e6c'}
            transparent
            opacity={path.status === 'active' ? 0.7 : 0.25}
            lineWidth={1 + path.activity * 2}
          />
        ) : null,
      )}
      <instancedMesh ref={ref} args={[undefined, undefined, 256]} frustumCulled={false}>
        <sphereGeometry args={[1, 6, 4]} />
        <meshBasicMaterial toneMapped={false} />
      </instancedMesh>
    </>
  );
}
function Board(props: Props) {
  const positions = props.logical ? logical : physical;
  const controls = useRef<OrbitControlsImpl>(null),
    target = useRef(new Vector3()),
    cameraTarget = useRef(new Vector3(10, 13, 12)),
    transition = useRef(false);
  useEffect(() => {
    target.current.set(...(props.focus > 0 ? positions[props.selected]! : ([0, 0, 0] as Point)));
    cameraTarget.current
      .copy(target.current)
      .add(new Vector3(...((props.focus > 0 ? [4, 6, 5] : [10, 13, 12]) as [number, number, number])));
    transition.current = true;
  }, [props.focus, props.logical]);
  useFrame(({ camera, gl }, delta) => {
    if (transition.current && controls.current) {
      const alpha = props.reduced ? 1 : 1 - Math.exp(-8 * delta);
      camera.position.lerp(cameraTarget.current, alpha);
      controls.current.target.lerp(target.current, alpha);
      controls.current.update();
      if (camera.position.distanceTo(cameraTarget.current) < 0.01) transition.current = false;
    }
    const performanceWindow = window as Window & { ioscopeSceneStats?: unknown };
    performanceWindow.ioscopeSceneStats = {
      calls: gl.info.render.calls,
      triangles: gl.info.render.triangles,
      geometries: gl.info.memory.geometries,
      textures: gl.info.memory.textures,
    };
  });
  return (
    <>
      <ambientLight intensity={1.2} />
      <directionalLight position={[3, 9, 5]} intensity={3} color="#c8eee3" />
      <directionalLight position={[-6, 4, -4]} intensity={1.5} color="#98b2c2" />
      <group>
        <Box position={[0, -0.17, 0]} size={[11.4, 0.18, 7.7]} color="#132d2a" metalness={0.45} />
        <Box position={[0, -0.33, 0]} size={[11.65, 0.15, 7.95]} color="#2b3b3d" metalness={0.8} />
        {!props.logical && !props.isolated && (
          <>
            <Details />
            <Fan x={-4} />
            <Fan x={4} />
            {Array.from({ length: 12 }, (_, i) => (
              <Line
                key={i}
                points={[
                  [-5.4, 0.02, -3.5 + i * 0.6],
                  [-3 + i * 0.2, 0.02, -3.5 + i * 0.6],
                  [-2 + i * 0.2, 0.02, -2.5 + i * 0.5],
                  [5.4, 0.02, -2.5 + i * 0.5],
                ]}
                color="#4c6a4a"
                opacity={0.35}
                transparent
                lineWidth={0.7}
              />
            ))}
            <Line
              points={[
                [-4, 0.45, -2.2],
                [-2.5, 0.45, -1.5],
                [1.5, 0.45, -1.5],
                [4, 0.45, -2.2],
              ]}
              color="#8e7252"
              lineWidth={7}
            />
          </>
        )}
        {components.map((c) => (
          <Chip key={c.id} id={c.id} position={positions[c.id]!} props={props} />
        ))}
        <Flows props={props} positions={positions} />
        {props.logical && (
          <Html position={[0, 0.1, 2.5]} center>
            <div className="logical-caption">
              APPLICATION → FILESYSTEM → PAGE CACHE → BLOCK I/O → DRIVER → PCIe
              <br />
              <small>Conceptual software path · stages are not individually traced</small>
            </div>
          </Html>
        )}
      </group>
      <OrbitControls
        ref={controls}
        makeDefault
        enableDamping
        minDistance={4}
        maxDistance={24}
        maxPolarAngle={Math.PI * 0.47}
        onStart={() => {
          transition.current = false;
        }}
      />
    </>
  );
}
export function DigitalTwin(props: Props) {
  return (
    <SceneBoundary>
      <Suspense fallback={<div className="scene-placeholder">Loading architecture…</div>}>
        <div className="scene-canvas">
          <Canvas
            frameloop={props.measurement ? 'demand' : 'always'}
            camera={{ position: [10, 13, 12], fov: 39 }}
            dpr={[1, 1.5]}
            gl={{ antialias: true, alpha: true }}
            fallback={<div className="scene-placeholder">WebGL unavailable. Use the component inspector.</div>}
          >
            <Board {...props} />
          </Canvas>
        </div>
        <div className="scene-note">ILLUSTRATIVE LAPTOP BOARD / DRAG TO ORBIT · SCROLL TO ZOOM</div>
      </Suspense>
    </SceneBoundary>
  );
}

const endpoint = ''; // Same-origin in production; Vite proxies /api during development.
let token: string | null = null;
export interface AgentInfo {
  token: string;
  protocolVersion: string;
  agentVersion: string;
  telemetry: boolean;
  workloads: boolean;
}
export async function connect(): Promise<AgentInfo> {
  const response = await fetch(endpoint + '/api/v1/bootstrap', { signal: AbortSignal.timeout(2500) });
  if (!response.ok) throw new Error('Native agent connection rejected');
  const info = (await response.json()) as AgentInfo;
  token = info.token;
  return info;
}
export async function api(path: string, options: RequestInit = {}): Promise<unknown> {
  if (!token) await connect();
  let response = await fetch(endpoint + '/api/v1' + path, {
    ...options,
    headers: { 'Content-Type': 'application/json', Authorization: 'Bearer ' + token, ...options.headers },
  });
  if (response.status === 401) {
    await connect();
    response = await fetch(endpoint + '/api/v1' + path, {
      ...options,
      headers: { 'Content-Type': 'application/json', Authorization: 'Bearer ' + token, ...options.headers },
    });
  }
  const value = (await response.json()) as { message?: string };
  if (!response.ok) throw new Error(value.message ?? 'Native request failed');
  return value;
}
export function agentToken() {
  return token;
}
export interface StoredRun {
  id: string;
  origin: string;
  name: string;
  startedAt: string;
  bytes: number;
}

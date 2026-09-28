export interface SharkResult {
  rc: number;
  out: string;
  err: string;
}

export function runTshark(args: string[]): Promise<SharkResult>;
export function interfaces(): Promise<SharkResult>;
export function version(): string;
export function writeSample(path: string): boolean;

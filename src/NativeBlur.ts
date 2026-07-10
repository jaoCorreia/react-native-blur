import { TurboModule, TurboModuleRegistry } from 'react-native';

export interface Spec extends TurboModule {
  blurImage(inputPath: string, outputPath: string, radius: number, sigma?: number): Promise<boolean>;
}

let Module: Spec | null = null;

try {
  Module = TurboModuleRegistry.getEnforcing<Spec>('BlurModule');
} catch {
  Module = null;
}

export default Module;

export function isTurboModuleAvailable(): boolean {
  return Module !== null;
}

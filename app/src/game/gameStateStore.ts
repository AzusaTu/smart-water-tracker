import { DailyGameState } from './gameRules';

const STORAGE_PREFIX = 'water_game_state:';

type StorageResolver = () => Storage | null;

const defaultStorageResolver: StorageResolver = () => {
  try {
    if (typeof globalThis !== 'undefined' && 'localStorage' in globalThis) {
      return globalThis.localStorage;
    }
  } catch {
    // localStorage can be unavailable in private or restricted environments.
  }
  return null;
};

const isFiniteNumber = (value: unknown): value is number =>
  typeof value === 'number' && Number.isFinite(value);

const isValidState = (value: unknown): value is DailyGameState => {
  if (!value || typeof value !== 'object' || Array.isArray(value)) return false;
  const candidate = value as Record<string, unknown>;

  if (typeof candidate.date !== 'string' || typeof candidate.bossId !== 'string') return false;
  if (typeof candidate.bossDefeated !== 'boolean' || typeof candidate.rewardClaimed !== 'boolean') {
    return false;
  }

  const numericKeys: Array<keyof DailyGameState> = [
    'waterMl',
    'dailyGoalMl',
    'waterEnergy',
    'energyEarned',
    'bossHp',
    'bossMaxHp',
    'points',
    'chests',
    'streakDays',
  ];
  if (!numericKeys.every((key) => isFiniteNumber(candidate[key]) && (candidate[key] as number) >= 0)) {
    return false;
  }

  const bossHp = candidate.bossHp as number;
  const bossMaxHp = candidate.bossMaxHp as number;
  if (bossHp > bossMaxHp) return false;
  if (candidate.bossDefeated !== (bossHp === 0)) return false;

  return true;
};

/**
 * Persists the daily battle per user so a tab switch or reload keeps today's
 * boss progress. Hydration data is never stored here; only derived game state.
 */
export class GameStateStore {
  constructor(private readonly resolveStorage: StorageResolver = defaultStorageResolver) {}

  private key(userId: string): string {
    return `${STORAGE_PREFIX}${userId}`;
  }

  public load(userId: string): DailyGameState | null {
    if (!userId) return null;
    const storage = this.resolveStorage();
    if (!storage) return null;

    try {
      const raw = storage.getItem(this.key(userId));
      if (!raw) return null;
      const parsed: unknown = JSON.parse(raw);
      return isValidState(parsed) ? parsed : null;
    } catch {
      return null;
    }
  }

  public save(userId: string, state: DailyGameState): void {
    if (!userId) return;
    const storage = this.resolveStorage();
    if (!storage) return;

    try {
      storage.setItem(this.key(userId), JSON.stringify(state));
    } catch {
      // Quota or privacy errors must never break the hydration flow.
    }
  }

  public clear(userId: string): void {
    if (!userId) return;
    const storage = this.resolveStorage();
    if (!storage) return;
    try {
      storage.removeItem(this.key(userId));
    } catch {
      // ignore
    }
  }
}

export const gameStateStore = new GameStateStore();

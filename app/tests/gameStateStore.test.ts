import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { createDailyGameState } from '../src/game/gameRules';
import { GameStateStore } from '../src/game/gameStateStore';

class MemoryStorage implements Storage {
  private data = new Map<string, string>();
  get length() { return this.data.size; }
  clear() { this.data.clear(); }
  getItem(key: string) { return this.data.has(key) ? this.data.get(key)! : null; }
  key(index: number) { return Array.from(this.data.keys())[index] ?? null; }
  removeItem(key: string) { this.data.delete(key); }
  setItem(key: string, value: string) { this.data.set(key, value); }
}

describe('GameStateStore', () => {
  let storage: MemoryStorage;
  let store: GameStateStore;

  beforeEach(() => {
    storage = new MemoryStorage();
    store = new GameStateStore(() => storage);
  });

  afterEach(() => storage.clear());

  it('round-trips a daily state per user', () => {
    const state = { ...createDailyGameState('2026-09-10', 2000), points: 300, waterEnergy: 120 };
    store.save('user-a', state);
    expect(store.load('user-a')).toEqual(state);
    expect(store.load('user-b')).toBeNull();
  });

  it('ignores corrupt or foreign payloads instead of throwing', () => {
    storage.setItem('water_game_state:user-a', '{not json');
    expect(store.load('user-a')).toBeNull();
    storage.setItem('water_game_state:user-a', JSON.stringify({ hello: 'world' }));
    expect(store.load('user-a')).toBeNull();
    storage.setItem('water_game_state:user-a', JSON.stringify([1, 2, 3]));
    expect(store.load('user-a')).toBeNull();
  });

  it('rejects payloads that violate the game invariants', () => {
    const base = createDailyGameState('2026-09-10', 2000);
    storage.setItem('water_game_state:user-a', JSON.stringify({ ...base, waterEnergy: -5 }));
    expect(store.load('user-a')).toBeNull();
    storage.setItem('water_game_state:user-a', JSON.stringify({ ...base, bossHp: base.bossMaxHp + 1 }));
    expect(store.load('user-a')).toBeNull();
  });

  it('is a no-op without a user or a storage backend', () => {
    const noStorage = new GameStateStore(() => null);
    const state = createDailyGameState('2026-09-10', 2000);
    expect(() => noStorage.save('user-a', state)).not.toThrow();
    expect(noStorage.load('user-a')).toBeNull();
    store.save('', state);
    expect(storage.length).toBe(0);
  });
});

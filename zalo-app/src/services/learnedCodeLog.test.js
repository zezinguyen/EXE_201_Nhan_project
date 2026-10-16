import { describe, it, expect, vi, beforeEach } from 'vitest';
import { clearLearnedCodes, getLearnedCodes, recordLearnedCode, toPresetJson } from './learnedCodeLog';

const mockStorage = (() => {
  let store = {};
  return {
    getItem: (key) => (key in store ? store[key] : null),
    setItem: (key, value) => { store[key] = String(value); },
    removeItem: (key) => { delete store[key]; },
    clear: () => { store = {}; },
  };
})();
globalThis.localStorage = mockStorage;

describe('learnedCodeLog', () => {
  beforeEach(() => {
    mockStorage.clear();
    vi.spyOn(console, 'log').mockImplementation(() => {});
  });

  it('ghi đè theo phím và giữ riêng theo thiết bị', () => {
    recordLearnedCode('tv_living', 'POWER', { protocol: 'NIKAI', code: '0x1', bits: 24 });
    recordLearnedCode('tv_living', 'POWER', { protocol: 'NIKAI', code: '0x2', bits: 24 });
    recordLearnedCode('tv_living', 'MUTE', { protocol: 'NIKAI', code: '0x3', bits: 24 });
    recordLearnedCode('tv_bedroom', 'POWER', { protocol: 'NEC', code: '0x4', bits: 32 });

    expect(getLearnedCodes('tv_living')).toEqual({
      POWER: { protocol: 'NIKAI', code: '0x2', bits: 24 },
      MUTE: { protocol: 'NIKAI', code: '0x3', bits: 24 },
    });
    clearLearnedCodes('tv_living');
    expect(getLearnedCodes('tv_living')).toEqual({});
    expect(getLearnedCodes('tv_bedroom')).toHaveProperty('POWER');
  });

  it('toPresetJson ra đúng dạng một mục TV_PRESETS, protocol lấy từ phím Nguồn', () => {
    const json = JSON.parse(
      toPresetJson(
        { MUTE: { protocol: 'NEC', code: '0x9', bits: 32 }, POWER: { protocol: 'NIKAI', code: '0xD5F2A', bits: 24 } },
        { id: 'tcl', brand: 'TCL' }
      )
    );
    expect(json).toEqual({
      id: 'tcl',
      brand: 'TCL',
      protocol: 'NIKAI',
      commands: {
        MUTE: { protocol: 'NEC', code: '0x9', bits: 32 },
        POWER: { protocol: 'NIKAI', code: '0xD5F2A', bits: 24 },
      },
    });
  });

  it('in bộ mã ra console mỗi lần học', () => {
    recordLearnedCode('tv_living', 'VOL_UP', { protocol: 'NIKAI', code: '0x5', bits: 24 });
    expect(console.log).toHaveBeenCalledWith(expect.stringContaining('"VOL_UP"'));
  });
});

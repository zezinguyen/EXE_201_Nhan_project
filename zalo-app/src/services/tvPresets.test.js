import { describe, it, expect } from 'vitest';
import { TV_PRESETS, describeIrProtocol, findPresetByIr } from './tvPresets';

describe('tvPresets', () => {
  it('mọi hãng đều có mã POWER (phím dùng để tự nhận hãng)', () => {
    TV_PRESETS.forEach((preset) => expect(preset.commands.POWER).toBeDefined());
  });

  it('nhận ra hãng từ mã Nguồn vừa học, không phân biệt hoa/thường và số 0 đầu', () => {
    expect(findPresetByIr({ protocol: 'SAMSUNG', code: '0xe0e040bf', bits: 32 })?.id).toBe('samsung');
    expect(findPresetByIr({ protocol: 'SONY', code: '0x0A90', bits: 12 })?.id).toBe('sony');
    expect(findPresetByIr({ protocol: 'PANASONIC', code: '0x40040100BCBD', bits: 48 })?.id).toBe('panasonic');
  });

  it('trả null khi khác giao thức, mã lạ hoặc mã raw', () => {
    expect(findPresetByIr({ protocol: 'NEC', code: '0xE0E040BF', bits: 32 })).toBeNull();
    expect(findPresetByIr({ protocol: 'NEC', code: '0x12345678', bits: 32 })).toBeNull();
    expect(findPresetByIr({ protocol: 'UNKNOWN', raw: [4000, 4000, 590, 1050], freq: 38 })).toBeNull();
    expect(findPresetByIr(null)).toBeNull();
  });

  it('mô tả giao thức cho người dùng', () => {
    expect(describeIrProtocol({ protocol: 'NIKAI', code: '0xD5F2A', bits: 24 })).toContain('TCL');
    expect(describeIrProtocol({ protocol: 'XYZ', code: '0x1', bits: 8 })).toBe('XYZ');
    expect(describeIrProtocol({ protocol: 'UNKNOWN', raw: [1], freq: 38 })).toContain('xung thô');
  });

  it('mã NIKAI (họ RCA) hợp lệ: 12 bit cao là đảo của 12 bit thấp', () => {
    TV_PRESETS.flatMap((p) => Object.entries(p.commands))
      .filter(([, ir]) => ir.protocol === 'NIKAI')
      .forEach(([action, ir]) => {
        const value = Number(ir.code);
        expect({ action, ok: ((value >> 12) ^ (value & 0xfff)) === 0xfff }).toEqual({ action, ok: true });
      });
  });

  it('nhận ra Tivi TCL từ mã Nguồn học được', () => {
    expect(findPresetByIr({ protocol: 'NIKAI', code: '0xD5F2A', bits: 24 })?.id).toBe('tcl');
  });
});

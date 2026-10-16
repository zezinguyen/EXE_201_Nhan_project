/**
 * Mã IR Tivi có sẵn theo hãng (IrCode, docs/contracts.md mục 6). Dùng cho phần lớn Tivi của hãng đó;
 * đời máy khác mã thì bấm "Thử" để kiểm tra, không được thì học phím từ remote thật.
 * protocol là tên giao thức của IRremoteESP8266. code là chuỗi hex (JS không giữ đủ 64 bit).
 */
export const TV_PRESETS = [
  {
    id: 'lg',
    brand: 'LG',
    protocol: 'NEC',
    commands: {
      POWER: { protocol: 'NEC', code: '0x20DF10EF', bits: 32 },
      VOL_UP: { protocol: 'NEC', code: '0x20DF40BF', bits: 32 },
      VOL_DOWN: { protocol: 'NEC', code: '0x20DFC03F', bits: 32 },
      MUTE: { protocol: 'NEC', code: '0x20DF906F', bits: 32 },
    },
  },
  {
    id: 'samsung',
    brand: 'Samsung',
    protocol: 'SAMSUNG',
    commands: {
      POWER: { protocol: 'SAMSUNG', code: '0xE0E040BF', bits: 32 },
      VOL_UP: { protocol: 'SAMSUNG', code: '0xE0E0E01F', bits: 32 },
      VOL_DOWN: { protocol: 'SAMSUNG', code: '0xE0E0D02F', bits: 32 },
      MUTE: { protocol: 'SAMSUNG', code: '0xE0E0F00F', bits: 32 },
    },
  },
  {
    id: 'sony',
    brand: 'Sony',
    protocol: 'SONY',
    commands: {
      POWER: { protocol: 'SONY', code: '0xA90', bits: 12 },
      VOL_UP: { protocol: 'SONY', code: '0x490', bits: 12 },
      VOL_DOWN: { protocol: 'SONY', code: '0xC90', bits: 12 },
      MUTE: { protocol: 'SONY', code: '0x290', bits: 12 },
    },
  },
  {
    id: 'panasonic',
    brand: 'Panasonic',
    protocol: 'PANASONIC',
    commands: {
      POWER: { protocol: 'PANASONIC', code: '0x40040100BCBD', bits: 48 },
      VOL_UP: { protocol: 'PANASONIC', code: '0x400401000405', bits: 48 },
      VOL_DOWN: { protocol: 'PANASONIC', code: '0x400401008485', bits: 48 },
      MUTE: { protocol: 'PANASONIC', code: '0x400401004C4D', bits: 48 },
    },
  },
  {
    // Học từ remote Tivi TCL thật (16/10/2026). IRremoteESP8266 gọi giao thức họ RCA này là NIKAI:
    // 24 bit = 12 bit dữ liệu + 12 bit đảo. Remote không có phím YouTube.
    id: 'tcl',
    brand: 'TCL',
    protocol: 'NIKAI',
    commands: {
      POWER:    { protocol: 'NIKAI', code: '0xD5F2A', bits: 24 },
      MUTE:     { protocol: 'NIKAI', code: '0xC0F3F', bits: 24 },
      VOL_UP:   { protocol: 'NIKAI', code: '0xD0F2F', bits: 24 },
      VOL_DOWN: { protocol: 'NIKAI', code: '0xD1F2E', bits: 24 },
      CH_UP:    { protocol: 'NIKAI', code: '0xD2F2D', bits: 24 },
      CH_DOWN:  { protocol: 'NIKAI', code: '0xD3F2C', bits: 24 },
      INPUT:    { protocol: 'NIKAI', code: '0x5CFA3', bits: 24 },
      HOME:     { protocol: 'NIKAI', code: '0xF7F08', bits: 24 },
      MENU:     { protocol: 'NIKAI', code: '0x13FEC', bits: 24 },
      LIST:     { protocol: 'NIKAI', code: '0x9EF61', bits: 24 },
      BACK:     { protocol: 'NIKAI', code: '0xD8F27', bits: 24 },
      UP:       { protocol: 'NIKAI', code: '0xA6F59', bits: 24 },
      DOWN:     { protocol: 'NIKAI', code: '0xA7F58', bits: 24 },
      LEFT:     { protocol: 'NIKAI', code: '0xA9F56', bits: 24 },
      RIGHT:    { protocol: 'NIKAI', code: '0xA8F57', bits: 24 },
      OK:       { protocol: 'NIKAI', code: '0xBFF4', bits: 24 },
      NUM_0:    { protocol: 'NIKAI', code: '0xCFF30', bits: 24 },
      NUM_1:    { protocol: 'NIKAI', code: '0xCEF31', bits: 24 },
      NUM_2:    { protocol: 'NIKAI', code: '0xCDF32', bits: 24 },
      NUM_3:    { protocol: 'NIKAI', code: '0xCCF33', bits: 24 },
      NUM_4:    { protocol: 'NIKAI', code: '0xCBF34', bits: 24 },
      NUM_5:    { protocol: 'NIKAI', code: '0xCAF35', bits: 24 },
      NUM_6:    { protocol: 'NIKAI', code: '0xC9F36', bits: 24 },
      NUM_7:    { protocol: 'NIKAI', code: '0xC8F37', bits: 24 },
      NUM_8:    { protocol: 'NIKAI', code: '0xC7F38', bits: 24 },
      NUM_9:    { protocol: 'NIKAI', code: '0xC6F39', bits: 24 },
    },
  },
];

/**
 * Tìm hãng có mã trùng với một mã vừa học (thường là phím Nguồn). Mã raw (không giải mã được) -> null.
 * So sánh theo giá trị số (BigInt) vì mạch ghi hex không có số 0 đầu, chữ hoa/thường có thể khác.
 */
export function findPresetByIr(ir, action = 'POWER') {
  if (!ir || !ir.protocol || !ir.code || ir.protocol === 'UNKNOWN') return null;
  let value;
  try {
    value = BigInt(ir.code);
  } catch {
    return null;
  }
  return (
    TV_PRESETS.find((preset) => {
      const code = preset.commands[action];
      return code && code.protocol === ir.protocol && BigInt(code.code) === value;
    }) || null
  );
}

// Tên IRremoteESP8266 đặt cho giao thức -> hãng hay gặp, để người dùng hiểu (VD remote TCL giải mã ra NIKAI)
const PROTOCOL_FAMILIES = {
  NIKAI: 'họ RCA — Tivi TCL, Thomson, Nikai',
  NEC: 'NEC — LG, Toshiba và nhiều Tivi Trung Quốc',
  SAMSUNG: 'Samsung',
  SONY: 'Sony',
  PANASONIC: 'Panasonic',
  RC5: 'Philips RC5',
  RC6: 'Philips RC6',
  SHARP: 'Sharp',
};

/** Mô tả giao thức cho người dùng: "NIKAI (họ RCA — Tivi TCL, ...)" hoặc "dạng xung thô" */
export function describeIrProtocol(ir) {
  if (!ir || !ir.protocol || ir.protocol === 'UNKNOWN') return 'dạng xung thô (giao thức lạ)';
  const family = PROTOCOL_FAMILIES[ir.protocol];
  return family ? `${ir.protocol} (${family})` : ir.protocol;
}

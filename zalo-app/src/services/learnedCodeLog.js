/**
 * Ghi lại mã IR đã học trên máy này để xuất JSON (dựng bộ mã có sẵn cho hãng mới, VD TCL).
 * Mạch chỉ trả về tên phím, không trả mã -> app tự giữ mã lúc học. Lưu localStorage (mất thì học lại).
 */

const STORAGE_KEY = 'learnedIrCodes';

function readAll() {
  try {
    return JSON.parse(localStorage.getItem(STORAGE_KEY)) || {};
  } catch {
    return {};
  }
}

function writeAll(all) {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(all));
  } catch {
    // Không có localStorage: vẫn in ra console ở recordLearnedCode
  }
}

/** Mã đã học của một thiết bị: { POWER: IrCode, VOL_UP: IrCode, ... } */
export function getLearnedCodes(deviceId) {
  return readAll()[deviceId] || {};
}

export function clearLearnedCodes(deviceId) {
  const all = readAll();
  delete all[deviceId];
  writeAll(all);
}

/**
 * JSON dạng một mục TV_PRESETS (tvPresets.js). protocol = giao thức của phím Nguồn (hoặc phím đầu tiên).
 */
export function toPresetJson(codes, { id = 'new_brand', brand = 'Hãng mới' } = {}) {
  const first = codes.POWER || Object.values(codes)[0];
  return JSON.stringify({ id, brand, protocol: first?.protocol || 'UNKNOWN', commands: codes }, null, 2);
}

/** Lưu mã vừa học và in JSON cả bộ ra console. Trả về bộ mã mới của thiết bị. */
export function recordLearnedCode(deviceId, action, ir) {
  const all = readAll();
  const codes = { ...(all[deviceId] || {}), [action]: ir };
  all[deviceId] = codes;
  writeAll(all);
  console.log(`[IR] Đã học ${deviceId}/${action}:`, JSON.stringify(ir));
  console.log(`[IR] Bộ mã ${deviceId}:\n${toPresetJson(codes)}`);
  return codes;
}

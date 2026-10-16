import { getSupabase, isSupabaseConfigured } from './supabaseClient';
import { generateRequestId, getHomeRealtimeChannel } from './iotService';

/**
 * Sửa profile trên mạch từ app (TẠM — docs/contracts.md mục 3.8): giao thức điều hòa, phím học cho Tivi.
 * App gửi yêu cầu vào kênh home:<homeId>, mạch trả lời kèm đúng requestId.
 * Khi server quản lý profile (mục 3.7) thì chuyển sang gọi Edge Function.
 */

const DEFAULT_HOME_ID = '00000000-0000-0000-0000-000000000001';
const REPLY_EVENTS = ['profile_info', 'setup_result', 'learn_progress', 'learn_result', 'learn_timeout'];

export const AC_SETUP_ERRORS = {
  E_BUSY: 'Mạch đang học lệnh, thử lại sau',
  E_PROTOCOL: 'Mạch không hỗ trợ giao thức này',
  E_BAD_REQUEST: 'Sai loại thiết bị hoặc dữ liệu không hợp lệ',
  E_SAVE_FAILED: 'Mạch không lưu được cấu hình',
  E_IR_SEND_FAIL: 'Phát hồng ngoại lỗi',
  E_LEARN_FAIL: 'Hai lần bấm không khớp, thử lại',
  E_TIMEOUT: 'Mạch không phản hồi (kiểm tra mạch còn online)',
  E_NOT_CONFIGURED: 'Chưa cấu hình Supabase',
  E_LEARN_TIMEOUT: 'Hết thời gian chờ bấm remote',
};

export function acSetupErrorText(code) {
  return AC_SETUP_ERRORS[code] || code || 'Lỗi không xác định';
}

// requestId -> { onReply(event, payload) }
const pending = new Map();
const boundChannels = new WeakSet();

function bindReplies(channel) {
  if (boundChannels.has(channel)) return;
  boundChannels.add(channel);
  REPLY_EVENTS.forEach((event) => {
    channel.on('broadcast', { event }, ({ payload }) => {
      const waiter = payload && pending.get(payload.requestId);
      if (waiter) waiter.onReply(event, payload);
    });
  });
}

// Chờ kênh vào xong (joined) trước khi gửi, nếu không sẽ lỡ phản hồi đến sớm của mạch
async function waitJoined(channel, timeoutMs = 3000) {
  const start = Date.now();
  while (channel.state !== 'joined' && Date.now() - start < timeoutMs) {
    await new Promise((r) => setTimeout(r, 100));
  }
}

/**
 * Gửi `event` kèm requestId mới, chờ phản hồi.
 * onReply(event, payload) trả về kết quả để kết thúc, hoặc undefined để chờ tiếp (VD learn_progress).
 */
function request(event, payload, { homeId = DEFAULT_HOME_ID, timeoutMs = 5000, onReply }) {
  if (!isSupabaseConfigured()) return Promise.resolve({ ok: false, error: 'E_NOT_CONFIGURED' });
  const channel = getHomeRealtimeChannel(getSupabase(), homeId);
  if (!channel) return Promise.resolve({ ok: false, error: 'E_NOT_CONFIGURED' });
  bindReplies(channel);

  const requestId = generateRequestId();
  return new Promise((resolve) => {
    const finish = (result) => {
      clearTimeout(timer);
      pending.delete(requestId);
      resolve(result);
    };
    const timer = setTimeout(() => finish({ ok: false, error: 'E_TIMEOUT' }), timeoutMs);
    pending.set(requestId, {
      onReply: (replyEvent, replyPayload) => {
        const result = onReply(replyEvent, replyPayload);
        if (result !== undefined) finish(result);
      },
    });
    waitJoined(channel)
      .then(() => channel.send({ type: 'broadcast', event, payload: { ...payload, requestId } }))
      .catch(() => finish({ ok: false, error: 'E_TIMEOUT' }));
  });
}

const resultReply = (event, payload) =>
  event === 'setup_result' ? { ok: Boolean(payload.ok), error: payload.error, profileVersion: payload.profileVersion } : undefined;

/** Danh sách thiết bị trên mạch + giao thức gợi ý: { ok, devices, acProtocols, profileVersion } */
export function fetchBoardProfile(options = {}) {
  return request('profile_get', {}, {
    ...options,
    onReply: (event, payload) =>
      event === 'profile_info'
        ? {
            ok: true,
            devices: payload.devices || [],
            acProtocols: payload.acProtocols || [],
            profileVersion: payload.profileVersion,
          }
        : undefined,
  });
}

/** Phát thử một giao thức (mạch không lưu) */
export function tryAcProtocol({ protocol, power = true, temp = 25, mode = 'cool', fan = 'auto' }, options = {}) {
  return request('ac_try', { protocol, power, temp, mode, fan }, { ...options, onReply: resultReply });
}

/** Lưu giao thức cho điều hòa (mạch tạo điều hòa mới nếu chưa có deviceId) */
export function saveAcProtocol({ deviceId, protocol, name }, options = {}) {
  const payload = { deviceId, protocol };
  if (name) payload.name = name;
  return request('ac_set_protocol', payload, { ...options, onReply: resultReply });
}

/**
 * Học một mã IR: mạch bật mắt thu, người dùng bấm phím trên remote thật 2 lần.
 * Trả về { ok, ir, detected, acSupported } hoặc { ok: false, error }.
 * onProgress(status): 'first_captured' | 'mismatch'
 */
export function learnIrCode({ onProgress, timeoutMs = 20000, ...options } = {}) {
  return request('learn_start', {}, {
    ...options,
    timeoutMs,
    onReply: (event, payload) => {
      if (event === 'learn_progress') {
        if (onProgress) onProgress(payload.status);
        return undefined;
      }
      if (event === 'learn_timeout') return { ok: false, error: 'E_LEARN_TIMEOUT' };
      if (event !== 'learn_result') return undefined;
      if (payload.status !== 'ok') return { ok: false, error: payload.status };
      return {
        ok: true,
        ir: payload.ir,
        detected: payload.detected || 'UNKNOWN',
        acSupported: Boolean(payload.acSupported),
      };
    },
  });
}

/** Tự nhận giao thức điều hòa: học một phím trên remote điều hòa rồi xem `detected` */
export const detectAcProtocol = learnIrCode;

/** Lưu phím đã học (ir từ learnIrCode) cho thiết bị bấm nút; mạch tạo thiết bị nếu chưa có */
export function saveLearnedCommand({ deviceId, deviceName, action, ir }, options = {}) {
  const payload = { deviceId, action, ir };
  if (deviceName) payload.deviceName = deviceName;
  return request('profile_put_command', payload, { ...options, onReply: resultReply });
}

/** Phát thử một IrCode (VD mã Tivi theo hãng), mạch không lưu */
export function tryIrCode(ir, options = {}) {
  return request('ir_try', { ir }, { ...options, onReply: resultReply });
}

/**
 * Lưu bộ mã có sẵn của một hãng (preset trong tvPresets.js) cho Tivi: lưu lần lượt từng phím.
 * Dừng ở phím lỗi đầu tiên: { ok: false, error, action }.
 */
export async function saveTvPreset({ deviceId, deviceName, preset }, options = {}) {
  for (const [action, ir] of Object.entries(preset.commands)) {
    const res = await saveLearnedCommand({ deviceId, deviceName, action, ir }, options);
    if (!res.ok) return { ok: false, error: res.error, action };
  }
  return { ok: true };
}

/** Huỷ việc học mã (tắt mắt thu trên mạch) */
export async function cancelLearn({ homeId = DEFAULT_HOME_ID } = {}) {
  if (!isSupabaseConfigured()) return;
  const channel = getHomeRealtimeChannel(getSupabase(), homeId);
  if (channel) await channel.send({ type: 'broadcast', event: 'learn_cancel', payload: {} }).catch(() => {});
}

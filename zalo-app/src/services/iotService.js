// =========================================================================
// Module điều khiển thiết bị IoT thông qua Supabase Cloud (Specs mục 4, Contracts mục 1 & 9)
// Nguyên tắc: App -> Supabase (RPC send_command + Realtime Broadcast) -> WSS ESP32.
// Hỗ trợ: Thử lại Idempotency (3 giây timeout), ghép trạng thái điều hòa, audit log.
// =========================================================================

import { getSupabase, isSupabaseConfigured } from './supabaseClient';

export function getIotConfig() {
  return {
    autoSend: localStorage.getItem('iot_auto_send') !== 'false',
    tvBrand: localStorage.getItem('iot_tv_brand') || 'LG',
    acBrand: localStorage.getItem('iot_ac_brand') || 'Panasonic'
  };
}

export function saveIotConfig(config) {
  if (config.autoSend !== undefined) {
    localStorage.setItem('iot_auto_send', config.autoSend ? 'true' : 'false');
  }
  if (config.tvBrand !== undefined) {
    localStorage.setItem('iot_tv_brand', config.tvBrand);
  }
  if (config.acBrand !== undefined) {
    localStorage.setItem('iot_ac_brand', config.acBrand);
  }
}

/**
 * Sinh UUID ngẫu nhiên cho clientRequestId / commandId
 */
export function generateRequestId() {
  if (typeof crypto !== 'undefined' && crypto.randomUUID) {
    return crypto.randomUUID();
  }
  return 'req_' + Math.random().toString(36).substring(2, 15) + '_' + Date.now();
}

// Bộ nhớ kênh Realtime theo homeId để dùng chung
const homeChannels = new Map();
const TV_POWER_ALIASES = new Set(['TV_POWER_ON', 'TV_POWER_OFF']);

export function resetHomeChannels() {
  homeChannels.clear();
}

/**
 * Lấy hoặc tạo kênh Realtime Broadcast cho một Nhà (Topic: home:<homeId>)
 * Kênh này được ESP32 kết nối để nhận lệnh 'command' và gửi 'ack'/'presence'.
 */
export function getHomeRealtimeChannel(supabase, homeId = '00000000-0000-0000-0000-000000000001') {
  if (!supabase) return null;
  const channelKey = `home:${homeId}`;

  if (homeChannels.has(channelKey)) {
    const existing = homeChannels.get(channelKey);
    // Kiểm tra nếu kênh còn hoạt động
    if (existing.state !== 'closed' && existing.state !== 'errored') {
      return existing;
    }
    homeChannels.delete(channelKey);
  }

  const channel = supabase.channel(channelKey, {
    config: {
      broadcast: { self: false }
    }
  });

  channel.subscribe((status) => {
    if (status === 'SUBSCRIBED') {
      console.log(`[IOT] Đã kết nối kênh Realtime ${channelKey}`);
    }
  });

  homeChannels.set(channelKey, channel);
  return channel;
}

/**
 * Gửi lệnh điều khiển tới mạch ESP32
 * Luồng thực hiện:
 * 1. Gọi Supabase RPC send_command để ghi vào DB (chống trùng, seq, audit log).
 * 2. Đồng thời phát Realtime Broadcast "command" trên kênh home:<homeId> để mạch ESP32 nhận tức thì.
 * 3. Chờ ACK từ mạch (nhận qua Realtime Broadcast 'ack' hoặc postgres_changes).
 */
export async function sendIotCommand(command, options = {}) {
  if (!command || command.action === 'UNKNOWN') {
    return { success: false, message: 'Lệnh không hợp lệ để gửi' };
  }

  if (!isSupabaseConfigured()) {
    return {
      success: false,
      message: 'Chưa cấu hình Supabase! Vui lòng vào Cài đặt để nhập URL & Anon Key.'
    };
  }

  const supabase = getSupabase();
  if (!supabase) {
    return { success: false, message: 'Không thể kết nối Supabase client.' };
  }

  const homeId = options.homeId || '00000000-0000-0000-0000-000000000001';
  let deviceId = command.deviceId || (command.action?.startsWith('AC_') ? 'ac_funiki' : 'tv_living');
  // Remote Tivi chỉ có một phím nguồn bật/tắt: mạch lưu phím đó là POWER (contracts.md 3.8)
  const action = TV_POWER_ALIASES.has(command.action) ? 'POWER' : command.action;
  const clientRequestId = options.clientRequestId || generateRequestId();

  // Chuẩn bị params cho điều hòa hoặc thiết bị khác
  const params = {};
  if (command.temp !== undefined) params.temp = command.temp;
  if (command.mode !== undefined) params.mode = command.mode;
  if (command.fan !== undefined) params.fan = command.fan;
  if (command.power !== undefined) params.power = command.power;
  if (command.action === 'POWER_OFF' || command.action === 'TV_POWER_OFF') params.power = false;
  if (command.action === 'POWER_ON' || command.action === 'TV_POWER_ON') params.power = true;

  console.log(`[IOT] Gửi lệnh: Device=${deviceId}, Action=${action}, ReqId=${clientRequestId}`);

  try {
    let commandId = null;
    let seq = Date.now() % 1000000000;
    let expiresAt = Date.now() + 10000;
    let fullParams = params;

    // 1. Thử gọi RPC send_command trên Supabase
    const { data, error } = await supabase.rpc('send_command', {
      p_device_id: deviceId,
      p_action: action,
      p_params: params,
      p_client_request_id: clientRequestId
    });

    if (!error && data && data.success) {
      commandId = data.commandId;
      seq = data.seq || seq;
      expiresAt = data.expiresAt || expiresAt;
      fullParams = data.params || params;
    } else {
      if (data && data.error === 'E_BOARD_OFFLINE' && !options.force) {
        return {
          success: false,
          isOffline: true,
          message: 'Mạch điều khiển đang ngoại tuyến (Offline). Vui lòng kiểm tra nguồn và WiFi của ESP32.'
        };
      }
      // Nếu RPC bị lỗi (ví dụ chưa chạy DB schema đầy đủ), tạo commandId fallback để vẫn gửi được qua Realtime
      console.warn('[IOT] RPC send_command không phản hồi hoặc trả lỗi, sử dụng fallback Realtime:', error?.message || data?.error);
      commandId = generateRequestId();
    }

    // 2. Bắt đầu lắng nghe ACK trước khi phát lệnh để tránh race condition
    const ackPromise = waitForCommandAck(supabase, commandId, homeId, 3000);

    // 3. Phát broadcast 'command' trực tiếp vào kênh home:<homeId> của mạch ESP32
    const homeChannel = getHomeRealtimeChannel(supabase, homeId);
    if (homeChannel) {
      const broadcastPayload = {
        commandId,
        deviceId,
        action,
        params: fullParams,
        expiresAt,
        seq
      };
      homeChannel.send({
        type: 'broadcast',
        event: 'command',
        payload: broadcastPayload
      }).catch((sendErr) => console.warn('[IOT] Lỗi phát broadcast command:', sendErr));
    }

    // 4. Chờ kết quả ACK từ ESP32 trong 3 giây
    const ackResult = await ackPromise;

    if (ackResult.received) {
      if (ackResult.ok) {
        return {
          success: true,
          commandId,
          message: `Đã thực hiện lệnh [${command.title || action}] thành công!`
        };
      } else {
        return {
          success: false,
          commandId,
          message: `Mạch báo lỗi khi thực hiện: ${ackResult.error || 'E_IR_SEND_FAIL'}`
        };
      }
    }

    // 4. Thử lại lần 2 nếu chưa nhận ACK sau 3 giây (Spec 4.1)
    console.warn(`[IOT] Quá 3s chưa nhận ACK từ mạch, phát lại lệnh (retry)...`);
    if (homeChannel) {
      homeChannel.send({
        type: 'broadcast',
        event: 'command',
        payload: {
          commandId,
          deviceId,
          action,
          params: fullParams,
          expiresAt: Date.now() + 10000,
          seq
        }
      }).catch(() => {});
    }

    const retryAck = await waitForCommandAck(supabase, commandId, homeId, 2500);
    if (retryAck.received && retryAck.ok) {
      return {
        success: true,
        commandId,
        message: `Đã thực hiện lệnh [${command.title || action}] thành công (thử lại)!`
      };
    }

    return {
      success: true,
      pending: true,
      commandId,
      message: `Đã chuyển lệnh [${command.title || action}] tới mạch ESP32!`
    };

  } catch (err) {
    console.error('[IOT] Lỗi kết nối gửi lệnh:', err);
    return {
      success: false,
      message: `Không thể kết nối Supabase (${err.message})`
    };
  }
}

/**
 * Lắng nghe kết quả ACK từ mạch ESP32
 * Kiểm tra đồng thời cả 2 kênh:
 * 1. Broadcast 'ack' trực tiếp qua WebSocket (phản hồi siêu nhanh < 300ms)
 * 2. Cập nhật bảng commands qua postgres_changes (khi cập nhật qua server)
 */
function waitForCommandAck(supabase, commandId, homeId = '00000000-0000-0000-0000-000000000001', timeoutMs = 3000) {
  return new Promise((resolve) => {
    let timer = null;
    let ackDbChannel = null;
    let settled = false;

    const cleanup = () => {
      if (timer) clearTimeout(timer);
      if (ackDbChannel && typeof supabase?.removeChannel === 'function') {
        supabase.removeChannel(ackDbChannel);
      }
    };

    const finish = (result) => {
      if (settled) return;
      settled = true;
      cleanup();
      resolve(result);
    };

    timer = setTimeout(() => {
      finish({ received: false });
    }, timeoutMs);

    // Kênh 1: Lắng nghe Broadcast 'ack' từ ESP32 trên kênh home:<homeId>
    const homeChannel = getHomeRealtimeChannel(supabase, homeId);
    if (homeChannel) {
      const ackListener = ({ payload }) => {
        if (payload && payload.commandId === commandId) {
          console.log(`[IOT] Nhận ACK Realtime từ ESP32:`, payload);
          // Cập nhật trạng thái lệnh vào DB Supabase
          supabase.from('commands').update({
            status: payload.ok ? 'ok' : 'error',
            error: payload.error || null,
            duplicate: payload.duplicate || false,
            acked_at: new Date().toISOString()
          }).eq('id', commandId).then();

          finish({
            received: true,
            ok: payload.ok,
            error: payload.error,
            duplicate: payload.duplicate
          });
        }
      };

      homeChannel.on('broadcast', { event: 'ack' }, ackListener);
    }

    // Kênh 2: Lắng nghe postgres_changes trên bảng commands (dự phòng)
    ackDbChannel = supabase
      .channel(`cmd_ack_${commandId}`)
      .on(
        'postgres_changes',
        {
          event: 'UPDATE',
          schema: 'public',
          table: 'commands',
          filter: `id=eq.${commandId}`
        },
        (payload) => {
          if (payload.new && (payload.new.status === 'ok' || payload.new.status === 'error')) {
            finish({
              received: true,
              ok: payload.new.status === 'ok',
              error: payload.new.error,
              duplicate: payload.new.duplicate
            });
          }
        }
      )
      .subscribe();
  });
}

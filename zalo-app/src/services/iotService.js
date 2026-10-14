// =========================================================================
// Module điều khiển thiết bị IoT thông qua Supabase Cloud (Specs mục 4, Contracts mục 1 & 9)
// Nguyên tắc: App KHÔNG GỌI TRỰC TIẾP ESP32 qua mạng LAN / IP cục bộ.
// Mọi lệnh đi: App -> Supabase (RPC/Edge Function send_command) -> WSS ESP32.
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
 * Sinh UUID ngẫu nhiên cho clientRequestId
 */
function generateRequestId() {
  if (typeof crypto !== 'undefined' && crypto.randomUUID) {
    return crypto.randomUUID();
  }
  return 'req_' + Math.random().toString(36).substring(2, 15) + '_' + Date.now();
}

/**
 * Gửi lệnh điều khiển tới mạch ESP32 thông qua Supabase RPC send_command
 * Tuân thủ specs 4.1 và contracts 3.2:
 * - App chờ 3 giây, nếu chưa có kết quả thì thử lại 1 lần với cùng clientRequestId.
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

  // Xác định deviceId trong DB
  let deviceId = command.deviceId || (command.action?.startsWith('AC_') ? 'ac_living' : 'tv_living');
  const action = command.action;
  const clientRequestId = options.clientRequestId || generateRequestId();

  // Chuẩn bị params cho điều hòa hoặc thiết bị khác
  const params = {};
  if (command.temp !== undefined) params.temp = command.temp;
  if (command.mode !== undefined) params.mode = command.mode;
  if (command.fan !== undefined) params.fan = command.fan;
  if (command.power !== undefined) params.power = command.power;
  if (action === 'POWER_OFF' || action === 'TV_POWER_OFF') params.power = false;
  if (action === 'POWER_ON' || action === 'TV_POWER_ON') params.power = true;

  console.log(`[IOT] Gửi lệnh qua Supabase RPC: Device=${deviceId}, Action=${action}, ReqId=${clientRequestId}`);

  try {
    // 1. Gọi RPC send_command trên Supabase
    const { data, error } = await supabase.rpc('send_command', {
      p_device_id: deviceId,
      p_action: action,
      p_params: params,
      p_client_request_id: clientRequestId
    });

    if (error) {
      console.warn('[IOT] Lỗi RPC send_command:', error.message);
      return {
        success: false,
        message: `Lỗi gửi lệnh: ${error.message}`
      };
    }

    if (!data.success) {
      if (data.error === 'E_BOARD_OFFLINE') {
        return {
          success: false,
          isOffline: true,
          message: 'Mạch điều khiển đang ngoại tuyến (Offline). Vui lòng kiểm tra nguồn và WiFi của ESP32.'
        };
      }
      return {
        success: false,
        message: data.message || `Lỗi: ${data.error}`
      };
    }

    const commandId = data.commandId;

    // 2. Chờ kết quả ACK từ ESP32 trong tối đa 3 giây (Spec 4.1)
    const ackResult = await waitForCommandAck(supabase, commandId, 3000);

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
          message: `Mạch báo lỗi khi thực hiện lệnh: ${ackResult.error || 'E_IR_SEND_FAIL'}`
        };
      }
    }

    // 3. Quá 3 giây chưa có ACK: Thử lại 1 lần với cùng clientRequestId (Spec 4.1)
    console.warn(`[IOT] Quá 3s chưa nhận ACK từ mạch, thử lại với cùng clientRequestId...`);
    const retryRes = await supabase.rpc('send_command', {
      p_device_id: deviceId,
      p_action: action,
      p_params: params,
      p_client_request_id: clientRequestId
    });

    if (retryRes.data?.success) {
      const retryAck = await waitForCommandAck(supabase, retryRes.data.commandId, 2500);
      if (retryAck.received && retryAck.ok) {
        return {
          success: true,
          commandId: retryRes.data.commandId,
          message: `Đã thực hiện lệnh [${command.title || action}] thành công (thử lại)!`
        };
      }
    }

    return {
      success: true,
      pending: true,
      commandId,
      message: `Đã chuyển lệnh [${command.title || action}] tới hàng đợi của mạch ESP32!`
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
 * Lắng nghe kết quả ACK của lệnh trong bảng commands
 */
function waitForCommandAck(supabase, commandId, timeoutMs = 3000) {
  return new Promise((resolve) => {
    let timer = null;
    let channel = null;

    const cleanup = () => {
      if (timer) clearTimeout(timer);
      if (channel) supabase.removeChannel(channel);
    };

    timer = setTimeout(() => {
      cleanup();
      resolve({ received: false });
    }, timeoutMs);

    channel = supabase
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
            cleanup();
            resolve({
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

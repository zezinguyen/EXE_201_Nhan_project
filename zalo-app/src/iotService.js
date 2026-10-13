// Module điều khiển thiết bị IoT (ESP32 / ESP8266 / Smart TV / Điều hòa) thực tế
export function getIotConfig() {
  const endpoint = localStorage.getItem('iot_endpoint') || 'http://192.168.1.50';
  const autoSend = localStorage.getItem('iot_auto_send') === 'true';
  const tvBrand = localStorage.getItem('iot_tv_brand') || 'LG';
  const acBrand = localStorage.getItem('iot_ac_brand') || 'Panasonic';
  return { endpoint, autoSend, tvBrand, acBrand };
}

export function saveIotConfig(config) {
  if (config.endpoint !== undefined) {
    localStorage.setItem('iot_endpoint', config.endpoint.trim());
  }
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

// Gửi lệnh điều khiển thực tế tới ESP32 / IoT Controller
export async function sendIotCommand(command, overrideConfig = null) {
  if (!command || command.action === 'UNKNOWN') {
    return { success: false, message: 'Lệnh không hợp lệ để gửi tới IoT' };
  }

  let endpoint = '';
  let tvBrand = 'LG';
  let acBrand = 'Panasonic';
  if (typeof overrideConfig === 'string') {
    endpoint = overrideConfig;
    const cfg = getIotConfig();
    tvBrand = cfg.tvBrand || 'LG';
    acBrand = cfg.acBrand || 'Panasonic';
  } else if (overrideConfig && typeof overrideConfig === 'object') {
    endpoint = overrideConfig.endpoint || '';
    tvBrand = overrideConfig.tvBrand || 'LG';
    acBrand = overrideConfig.acBrand || 'Panasonic';
  } else {
    const cfg = getIotConfig();
    endpoint = cfg.endpoint || '';
    tvBrand = cfg.tvBrand || 'LG';
    acBrand = cfg.acBrand || 'Panasonic';
  }

  let baseEndpoint = (endpoint || 'http://192.168.1.50').trim().replace(/\/+$/, '');
  if (!baseEndpoint.startsWith('http://') && !baseEndpoint.startsWith('https://')) {
    baseEndpoint = `http://${baseEndpoint}`;
  }

  const targetUrl = `${baseEndpoint}/api/tv/command`;

  const payload = {
    action: command.action,
    title: command.title,
    irCode: command.irCode || '0x00000000',
    device: command.device || (command.action?.startsWith('AC_') ? 'ac' : 'Smart TV'),
    channelQuery: command.channelQuery || '',
    youtubeUrl: command.youtubeUrl || '',
    tvBrand: tvBrand,
    acBrand: command.acBrand || acBrand,
    temp: command.temp || 26,
    power: command.power !== undefined ? command.power : true,
    timestamp: Date.now()
  };

  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), 3500);

  try {
    const response = await fetch(targetUrl, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify(payload),
      signal: controller.signal
    });

    clearTimeout(timeoutId);

    if (response.ok) {
      let data = {};
      try {
        data = await response.json();
      } catch {
        // Một số ESP32 WebServer trả về text thô
      }
      return {
        success: true,
        message: data.message || `Đã phát lệnh [${command.title}] tới TV qua ESP32 thành công!`
      };
    } else {
      return {
        success: false,
        message: `ESP32 phản hồi lỗi HTTP ${response.status}`
      };
    }
  } catch (err) {
    clearTimeout(timeoutId);
    console.warn('Lỗi kết nối IoT ESP32:', err);

    const isTimeout = err.name === 'AbortError';
    const errorMsg = isTimeout
      ? `Hết thời gian chờ (Timeout) tới ${baseEndpoint}. Vui lòng kiểm tra nguồn và WiFi của ESP32.`
      : `Không thể kết nối tới ESP32 tại ${baseEndpoint} (${err.message}).`;

    return {
      success: false,
      isNetworkError: true,
      message: errorMsg
    };
  }
}

// Kiểm tra trạng thái trực tuyến (Ping Test) của ESP32
export async function testIotConnection(endpoint) {
  let baseEndpoint = (endpoint || getIotConfig().endpoint).trim().replace(/\/+$/, '');
  if (!baseEndpoint.startsWith('http://') && !baseEndpoint.startsWith('https://')) {
    baseEndpoint = `http://${baseEndpoint}`;
  }

  const startTime = Date.now();
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), 2500);

  try {
    const response = await fetch(`${baseEndpoint}/api/ping`, {
      method: 'GET',
      signal: controller.signal
    });
    clearTimeout(timeoutId);

    const latency = Date.now() - startTime;
    return {
      ok: response.ok,
      latency,
      message: `🟢 Kết nối tốt (${latency}ms) tới ESP32`
    };
  } catch (err) {
    clearTimeout(timeoutId);
    return {
      ok: false,
      message: `🔴 Không kết nối được tới ${baseEndpoint} (${err.name === 'AbortError' ? 'Timeout' : err.message})`
    };
  }
}

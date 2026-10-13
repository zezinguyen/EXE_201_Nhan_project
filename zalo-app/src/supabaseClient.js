import { createClient } from '@supabase/supabase-js';

// Lấy thông tin cấu hình từ file .env hoặc bộ nhớ LocalStorage của trình duyệt
export function getSupabaseConfig() {
  const envUrl = (typeof import.meta !== 'undefined' && import.meta.env?.VITE_SUPABASE_URL) || '';
  const envKey = (typeof import.meta !== 'undefined' && import.meta.env?.VITE_SUPABASE_ANON_KEY) || '';

  const storedUrl = typeof localStorage !== 'undefined' ? localStorage.getItem('supabase_url') : '';
  const storedKey = typeof localStorage !== 'undefined' ? localStorage.getItem('supabase_anon_key') : '';

  const url = (storedUrl || envUrl || '').trim();
  const anonKey = (storedKey || envKey || '').trim();

  return { url, anonKey };
}

export function saveSupabaseConfig({ url, anonKey }) {
  if (typeof localStorage === 'undefined') return;
  if (url !== undefined) localStorage.setItem('supabase_url', url.trim());
  if (anonKey !== undefined) localStorage.setItem('supabase_anon_key', anonKey.trim());
  resetSupabaseClient();
}

let supabaseInstance = null;
let lastUsedConfig = { url: '', anonKey: '' };

export function getSupabase() {
  const { url, anonKey } = getSupabaseConfig();

  if (!url || !anonKey) {
    return null;
  }

  // Tái sử dụng instance nếu config không thay đổi
  if (supabaseInstance && lastUsedConfig.url === url && lastUsedConfig.anonKey === anonKey) {
    return supabaseInstance;
  }

  try {
    supabaseInstance = createClient(url, anonKey, {
      realtime: {
        params: {
          eventsPerSecond: 10
        }
      }
    });
    lastUsedConfig = { url, anonKey };
    return supabaseInstance;
  } catch (err) {
    console.warn('Lỗi khởi tạo Supabase Client:', err);
    return null;
  }
}

export function resetSupabaseClient() {
  supabaseInstance = null;
  lastUsedConfig = { url: '', anonKey: '' };
}

export function isSupabaseConfigured() {
  const { url, anonKey } = getSupabaseConfig();
  return Boolean(url && anonKey && url.startsWith('http'));
}

// Kiểm tra kết nối thực tế tới Supabase (Ping / Healthcheck)
export async function testSupabaseConnection(customConfig = null) {
  const config = customConfig || getSupabaseConfig();
  if (!config.url || !config.anonKey) {
    return {
      success: false,
      message: 'Chưa nhập Supabase Project URL hoặc Anon Key'
    };
  }

  const startTime = Date.now();
  try {
    const testClient = createClient(config.url.trim(), config.anonKey.trim());
    const { error } = await testClient.from('devices').select('id, name').limit(1);

    const latency = Date.now() - startTime;
    if (error) {
      if (error.code === '42P01') {
        // Table not found -> kết nối ok nhưng chưa chạy SQL
        return {
          success: true,
          tableMissing: true,
          latency,
          message: `Kết nối thành công (${latency}ms) nhưng chưa có bảng 'devices'. Vui lòng chạy file supabase_schema.sql!`
        };
      }
      return {
        success: false,
        latency,
        message: `Lỗi Supabase: ${error.message} (${error.code || 'API Error'})`
      };
    }

    return {
      success: true,
      latency,
      message: `🟢 Kết nối Supabase thành công (${latency}ms)! Đã sẵn sàng đồng bộ Realtime.`
    };
  } catch (err) {
    return {
      success: false,
      message: `Không kết nối được Supabase: ${err.message}`
    };
  }
}

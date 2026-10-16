import { createClient } from '@supabase/supabase-js';

// Lấy thông tin cấu hình từ file .env hoặc bộ nhớ LocalStorage của trình duyệt
export function getSupabaseConfig() {
  const envUrl = (typeof import.meta !== 'undefined' && import.meta.env?.VITE_SUPABASE_URL) || '';
  const envKey = (typeof import.meta !== 'undefined' && import.meta.env?.VITE_SUPABASE_ANON_KEY) || '';

  const storedUrl = typeof localStorage !== 'undefined' ? localStorage.getItem('supabase_url') : '';
  const storedKey = typeof localStorage !== 'undefined' ? localStorage.getItem('supabase_anon_key') : '';

  const rawUrl = (storedUrl || envUrl || '').trim();
  const url = rawUrl.replace(/\/rest\/v1\/?$/i, '').replace(/\/+$/, '');
  const anonKey = (storedKey || envKey || '').trim();

  return { url, anonKey };
}

export function saveSupabaseConfig({ url, anonKey }) {
  if (typeof localStorage === 'undefined') return;
  if (url !== undefined) {
    const cleanUrl = url.trim().replace(/\/rest\/v1\/?$/i, '').replace(/\/+$/, '');
    localStorage.setItem('supabase_url', cleanUrl);
  }
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

  const cleanUrl = url.trim().replace(/\/+$/, '');
  const cleanKey = anonKey.trim();

  // Tái sử dụng instance nếu config không thay đổi
  if (supabaseInstance && lastUsedConfig.url === cleanUrl && lastUsedConfig.anonKey === cleanKey) {
    return supabaseInstance;
  }

  try {
    supabaseInstance = createClient(cleanUrl, cleanKey, {
      realtime: {
        params: {
          eventsPerSecond: 10
        }
      }
    });
    lastUsedConfig = { url: cleanUrl, anonKey: cleanKey };
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
  const cleanUrl = (config.url || '').trim().replace(/\/+$/, '');
  const cleanKey = (config.anonKey || '').trim();

  if (!cleanUrl || !cleanKey) {
    return {
      success: false,
      message: 'Chưa nhập Supabase Project URL hoặc Anon Key'
    };
  }

  const startTime = Date.now();
  try {
    const testClient = createClient(cleanUrl, cleanKey);
    const { error } = await testClient.from('devices').select('id, name').limit(1);

    const latency = Date.now() - startTime;
    if (error) {
      const code = String(error.code || '');
      const msg = String(error.message || '');
      // PGRST125 (Invalid path), PGRST204/205, hoặc 42P01 nghĩa là Supabase đã kết nối thành công nhưng chưa có bảng devices
      if (
        code === 'PGRST125' ||
        code === 'PGRST204' ||
        code === 'PGRST205' ||
        code === '42P01' ||
        msg.includes('Invalid path') ||
        msg.includes('relation') ||
        msg.includes('does not exist')
      ) {
        return {
          success: true,
          tableMissing: true,
          latency,
          message: `🟢 Kết nối Supabase thành công (${latency}ms)! Nhưng bạn chưa chạy file supabase_schema.sql trong SQL Editor. Hãy mở mục SQL Editor trên Supabase, dán file SQL và bấm RUN nhé!`
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

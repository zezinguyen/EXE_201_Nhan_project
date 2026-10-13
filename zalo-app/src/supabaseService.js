import { getSupabase, isSupabaseConfigured } from './supabaseClient';

// Lấy danh sách thiết bị từ Supabase (kèm dự phòng local)
export async function fetchDevicesFromSupabase() {
  if (!isSupabaseConfigured()) return null;

  const supabase = getSupabase();
  if (!supabase) return null;

  try {
    const { data, error } = await supabase
      .from('devices')
      .select('*')
      .order('id');

    if (error) {
      console.warn('Lỗi fetch devices từ Supabase:', error.message);
      return null;
    }

    if (data && data.length > 0) {
      return data.map((d) => ({
        id: d.id,
        name: d.name,
        active: Boolean(d.active),
        statusText: d.status_text || (d.active ? 'Đang bật' : 'Đã tắt'),
        temp: d.temp || 26,
        mode: d.mode || 'cool',
        brand: d.brand || 'Chung',
        irCode: d.ir_code || '0x00000000'
      }));
    }

    return null;
  } catch (err) {
    console.warn('Lỗi kết nối fetchDevices:', err);
    return null;
  }
}

// Cập nhật trạng thái một thiết bị lên Supabase
export async function syncDeviceToSupabase(deviceId, { active, statusText, temp, mode, brand, irCode }) {
  if (!isSupabaseConfigured()) return false;

  const supabase = getSupabase();
  if (!supabase) return false;

  try {
    const updatePayload = {
      active: Boolean(active),
      status_text: statusText || (active ? 'Đang bật' : 'Đã tắt'),
      updated_at: new Date().toISOString()
    };

    if (temp !== undefined) updatePayload.temp = temp;
    if (mode !== undefined) updatePayload.mode = mode;
    if (brand !== undefined) updatePayload.brand = brand;
    if (irCode !== undefined) updatePayload.ir_code = irCode;

    const { error } = await supabase
      .from('devices')
      .update(updatePayload)
      .eq('id', deviceId);

    if (error) {
      console.warn(`Lỗi cập nhật thiết bị ${deviceId} lên Supabase:`, error.message);
      return false;
    }

    return true;
  } catch (err) {
    console.warn('Lỗi syncDeviceToSupabase:', err);
    return false;
  }
}

// Lưu lịch sử câu lệnh giọng nói vào Supabase
export async function recordVoiceCommandLog({ transcript, action, title, device, irCode, source = 'voice' }) {
  if (!isSupabaseConfigured()) return false;

  const supabase = getSupabase();
  if (!supabase) return false;

  try {
    const { error } = await supabase
      .from('voice_command_logs')
      .insert([
        {
          transcript: transcript || '',
          action: action || 'UNKNOWN',
          title: title || 'Lệnh điều khiển',
          device: device || 'Chung',
          ir_code: irCode || '0x00000000',
          source: source,
          created_at: new Date().toISOString()
        }
      ]);

    if (error) {
      console.warn('Lỗi ghi log câu lệnh vào Supabase:', error.message);
      return false;
    }

    return true;
  } catch (err) {
    console.warn('Lỗi recordVoiceCommandLog:', err);
    return false;
  }
}

// Đăng ký nhận thông báo Realtime khi trạng thái thiết bị thay đổi
export function subscribeDeviceRealtime(onDeviceUpdated) {
  if (!isSupabaseConfigured()) return () => {};

  const supabase = getSupabase();
  if (!supabase) return () => {};

  try {
    const channel = supabase
      .channel('smart_home_devices_realtime')
      .on(
        'postgres_changes',
        { event: 'UPDATE', schema: 'public', table: 'devices' },
        (payload) => {
          if (payload.new && onDeviceUpdated) {
            const raw = payload.new;
            onDeviceUpdated({
              id: raw.id,
              name: raw.name,
              active: Boolean(raw.active),
              statusText: raw.status_text || (raw.active ? 'Đang bật' : 'Đã tắt'),
              temp: raw.temp || 26,
              mode: raw.mode || 'cool',
              brand: raw.brand || 'Chung',
              irCode: raw.ir_code || '0x00000000'
            });
          }
        }
      )
      .subscribe();

    return () => {
      supabase.removeChannel(channel);
    };
  } catch (err) {
    console.warn('Lỗi subscribe Supabase Realtime:', err);
    return () => {};
  }
}

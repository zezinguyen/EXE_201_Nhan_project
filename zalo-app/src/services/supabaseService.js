import { getSupabase, isSupabaseConfigured } from './supabaseClient';

/**
 * Lấy toàn bộ dữ liệu cấu hình Nhà: danh sách Mạch, Thiết bị, Trạng thái điều hòa, Remote ảo
 */
export async function fetchHomeData() {
  if (!isSupabaseConfigured()) return null;
  const supabase = getSupabase();
  if (!supabase) return null;

  try {
    // 1. Lấy danh sách Mạch điều khiển (Boards)
    const { data: boardsData, error: boardErr } = await supabase
      .from('boards')
      .select('*')
      .order('created_at', { ascending: true });

    if (boardErr) {
      console.warn('Lỗi fetch boards:', boardErr.message);
    }

    const now = Date.now();
    const boards = (boardsData || []).map((b) => {
      const lastSeen = b.last_seen_at ? new Date(b.last_seen_at).getTime() : 0;
      // Online nếu last_seen_at trong vòng 6 phút (360 giây, Spec 3.6)
      const isOnline = (now - lastSeen) <= (6 * 60 * 1000);
      return {
        id: b.id,
        name: b.name || 'Mạch IR ESP32',
        room: b.room || 'Phòng khách',
        battery: b.battery !== undefined ? b.battery : 100,
        charging: Boolean(b.charging),
        usb: Boolean(b.usb),
        rssi: b.rssi || -60,
        firmware: b.firmware || '0.1.0',
        profileVersion: b.profile_version || 1,
        pairingStatus: b.pairing_status || 'paired',
        isOnline,
        lastSeenAt: b.last_seen_at
      };
    });

    // 2. Lấy danh sách Thiết bị (Home Devices) và Trạng thái (Device State)
    const { data: devData, error: devErr } = await supabase
      .from('home_devices')
      .select('*')
      .order('id');

    if (devErr) {
      console.warn('Lỗi fetch home_devices:', devErr.message);
    }

    const { data: stateData } = await supabase
      .from('device_state')
      .select('*');

    const stateMap = {};
    (stateData || []).forEach((s) => {
      stateMap[s.device_id] = s;
    });

    const devices = (devData || []).map((d) => {
      const st = stateMap[d.id] || {};
      return {
        id: d.id,
        boardId: d.board_id,
        name: d.name,
        room: d.room || 'Phòng khách',
        type: d.type || 'tv',
        brand: d.brand || 'Chung',
        active: st.power !== undefined ? Boolean(st.power) : false,
        temp: st.temp || 24,
        mode: st.mode || 'cool',
        fan: st.fan || 'auto',
        statusText: (st.power || d.type === 'tv') ? (st.power ? 'Đang bật' : 'Đã tắt') : 'Sẵn sàng'
      };
    });

    // 3. Lấy danh sách Remote ảo tự tạo (Custom Remotes)
    const { data: remotesData } = await supabase
      .from('custom_remotes')
      .select('*');

    const customRemotes = (remotesData || []).map((r) => ({
      id: r.id,
      deviceId: r.device_id,
      name: r.name,
      buttons: Array.isArray(r.buttons) ? r.buttons : []
    }));

    // 4. Lấy danh sách Thành viên gia đình (Home Members)
    const { data: membersData } = await supabase
      .from('home_members')
      .select('*')
      .order('created_at', { ascending: true });

    const homeMembers = (membersData || []).map((m) => ({
      id: m.id,
      homeId: m.home_id,
      userId: m.user_id,
      role: m.role || 'member',
      displayName: m.display_name || (m.role === 'owner' ? 'Chủ nhà' : 'Thành viên'),
      avatarUrl: m.avatar_url || '',
      createdAt: m.created_at
    }));

    // 5. Lấy mã mời đang kích hoạt của nhà
    const { data: inviteData } = await supabase
      .from('home_invites')
      .select('*')
      .eq('is_active', true)
      .gt('expires_at', new Date().toISOString())
      .order('created_at', { ascending: false })
      .limit(1);

    const activeInvite = inviteData && inviteData.length > 0 ? {
      inviteCode: inviteData[0].invite_code,
      role: inviteData[0].role,
      expiresAt: inviteData[0].expires_at,
      usedCount: inviteData[0].used_count,
      maxUses: inviteData[0].max_uses
    } : null;

    return {
      boards,
      devices,
      customRemotes,
      homeMembers,
      activeInvite,
      homeId: '00000000-0000-0000-0000-000000000001',
      homeName: 'Nhà thông minh của tôi'
    };
  } catch (err) {
    console.warn('Lỗi trong fetchHomeData:', err);
    return null;
  }
}

/**
 * Ghép đôi Mạch ESP32 mới vào nhà bằng mã 6 số (Specs 10.2, Contracts 7)
 */
export async function pairBoardWithCode(pairingCode, boardName = 'Mạch IR ESP32', room = 'Phòng khách') {
  if (!isSupabaseConfigured()) return { success: false, message: 'Chưa cấu hình Supabase' };
  const supabase = getSupabase();
  if (!supabase) return { success: false, message: 'Không có kết nối Supabase' };

  try {
    const { data, error } = await supabase.rpc('pair_board', {
      p_pairing_code: pairingCode.trim(),
      p_board_name: boardName.trim(),
      p_room: room.trim()
    });

    if (error) {
      return { success: false, message: error.message };
    }
    return data;
  } catch (err) {
    return { success: false, message: err.message };
  }
}

/**
 * Lấy danh sách mã IR học từ nút bấm vật lý đang chờ gán (Specs 7.1 Cách 2)
 */
export async function fetchPendingLearnedCodes() {
  if (!isSupabaseConfigured()) return [];
  const supabase = getSupabase();
  if (!supabase) return [];

  try {
    const { data, error } = await supabase
      .from('learned_codes')
      .select('*')
      .eq('status', 'pending')
      .order('created_at', { ascending: false });

    if (error) {
      console.warn('Lỗi fetch learned_codes:', error.message);
      return [];
    }
    return data || [];
  } catch (err) {
    console.warn('Lỗi fetchPendingLearnedCodes:', err);
    return [];
  }
}

/**
 * Gán mã IR vừa học vào thiết bị/remote ảo (Specs 7.1)
 */
export async function assignLearnedCode({ codeId, deviceId, action, label, voiceAliases = [] }) {
  if (!isSupabaseConfigured()) return { success: false, message: 'Chưa cấu hình Supabase' };
  const supabase = getSupabase();
  if (!supabase) return { success: false, message: 'Không có kết nối Supabase' };

  try {
    const { data, error } = await supabase.rpc('assign_learned_code', {
      p_code_id: codeId,
      p_device_id: deviceId,
      p_action: action,
      p_label: label,
      p_voice_aliases: voiceAliases
    });

    if (error) {
      return { success: false, message: error.message };
    }
    return data;
  } catch (err) {
    return { success: false, message: err.message };
  }
}

/**
 * Ghi log khẩu lệnh giọng nói để lưu vết lịch sử
 */
export async function recordVoiceCommandLog({ transcript, action, title, device, irCode, source = 'voice' }) {
  if (!isSupabaseConfigured()) return false;
  const supabase = getSupabase();
  if (!supabase) return false;

  try {
    await supabase.from('commands').insert([
      {
        id: (typeof crypto !== 'undefined' && crypto.randomUUID) ? crypto.randomUUID() : 'log_' + Date.now(),
        home_id: '00000000-0000-0000-0000-000000000001',
        board_id: 'board_living_01',
        device_id: device || 'tv_living',
        action: action || 'UNKNOWN',
        params: { transcript, title, source, irCode },
        seq: 0,
        expires_at: Date.now() + 10000,
        status: 'ok',
        acked_at: new Date().toISOString()
      }
    ]);
    return true;
  } catch {
    return false;
  }
}

/**
 * Đăng ký Realtime theo dõi thay đổi mạch, điều hòa, mã học mới và thành viên gia đình
 * Đồng thời lắng nghe Presence & Broadcast trực tiếp từ mạch ESP32 trên kênh home:<homeId>
 */
export function subscribeHomeRealtime({ 
  onDeviceStateUpdated, 
  onBoardUpdated, 
  onLearnedCodeAdded, 
  onMembersUpdated,
  onLearnEvent,
  homeId = '00000000-0000-0000-0000-000000000001'
}) {
  if (!isSupabaseConfigured()) return () => {};
  const supabase = getSupabase();
  if (!supabase) return () => {};

  try {
    // 1. Kênh theo dõi Postgres Database Changes
    const dbChannel = supabase
      .channel('home_db_changes')
      .on(
        'postgres_changes',
        { event: '*', schema: 'public', table: 'device_state' },
        (payload) => {
          if (payload.new && onDeviceStateUpdated) {
            onDeviceStateUpdated(payload.new);
          }
        }
      )
      .on(
        'postgres_changes',
        { event: '*', schema: 'public', table: 'boards' },
        (payload) => {
          if (payload.new && onBoardUpdated) {
            onBoardUpdated(payload.new);
          }
        }
      )
      .on(
        'postgres_changes',
        { event: 'INSERT', schema: 'public', table: 'learned_codes' },
        (payload) => {
          if (payload.new && onLearnedCodeAdded) {
            onLearnedCodeAdded(payload.new);
          }
        }
      )
      .on(
        'postgres_changes',
        { event: '*', schema: 'public', table: 'home_members' },
        (payload) => {
          if (onMembersUpdated) {
            onMembersUpdated(payload);
          }
        }
      )
      .on(
        'postgres_changes',
        { event: '*', schema: 'public', table: 'home_invites' },
        (payload) => {
          if (onMembersUpdated) {
            onMembersUpdated(payload);
          }
        }
      )
      .subscribe();

    // 2. Kênh Realtime trực tiếp với ESP32: home:<homeId>
    // Nhận Presence (Pin, Sạc, RSSI, Online/Offline) & Broadcast (Learn Events, ACK)
    const esp32Channel = supabase.channel(`home:${homeId}`, {
      config: { broadcast: { self: false } }
    });

    esp32Channel
      .on('presence', { event: 'sync' }, () => {
        const presenceState = esp32Channel.presenceState();
        Object.keys(presenceState).forEach((boardId) => {
          const list = presenceState[boardId];
          if (list && list.length > 0) {
            const st = list[0];
            const updatedBoard = {
              id: boardId,
              isOnline: true,
              battery: st.battery !== undefined ? st.battery : 100,
              charging: Boolean(st.charging),
              usb: Boolean(st.usb),
              rssi: st.rssi || -60,
              firmware: st.firmware || '0.1.0',
              profileVersion: st.profileVersion || 1,
              last_seen_at: new Date().toISOString()
            };
            if (onBoardUpdated) {
              onBoardUpdated(updatedBoard);
            }
            // Tự động giữ bảng boards tươi mới trên DB
            supabase.from('boards').update({
              last_seen_at: new Date().toISOString(),
              battery: updatedBoard.battery,
              charging: updatedBoard.charging,
              usb: updatedBoard.usb,
              rssi: updatedBoard.rssi
            }).eq('id', boardId).then();
          }
        });
      })
      .on('broadcast', { event: 'learn_progress' }, ({ payload }) => {
        if (onLearnEvent) onLearnEvent({ event: 'learn_progress', ...payload });
      })
      .on('broadcast', { event: 'learn_result' }, ({ payload }) => {
        if (onLearnEvent) onLearnEvent({ event: 'learn_result', ...payload });
      })
      .on('broadcast', { event: 'learn_timeout' }, ({ payload }) => {
        if (onLearnEvent) onLearnEvent({ event: 'learn_timeout', ...payload });
      })
      .subscribe();

    return () => {
      supabase.removeChannel(dbChannel);
      supabase.removeChannel(esp32Channel);
    };
  } catch (err) {
    console.warn('Lỗi subscribeHomeRealtime:', err);
    return () => {};
  }
}

/**
 * Gửi lệnh yêu cầu ESP32 bật mắt thu học lệnh IR (Specs 7.1, Contracts 3.5)
 */
export async function sendLearnStart(requestId, homeId = '00000000-0000-0000-0000-000000000001') {
  if (!isSupabaseConfigured()) return false;
  const supabase = getSupabase();
  if (!supabase) return false;

  try {
    const channel = supabase.channel(`home:${homeId}`);
    await channel.send({
      type: 'broadcast',
      event: 'learn_start',
      payload: { requestId }
    });
    return true;
  } catch {
    return false;
  }
}

/**
 * Gửi lệnh huỷ học lệnh IR trên ESP32
 */
export async function sendLearnCancel(homeId = '00000000-0000-0000-0000-000000000001') {
  if (!isSupabaseConfigured()) return false;
  const supabase = getSupabase();
  if (!supabase) return false;

  try {
    const channel = supabase.channel(`home:${homeId}`);
    await channel.send({
      type: 'broadcast',
      event: 'learn_cancel',
      payload: {}
    });
    return true;
  } catch {
    return false;
  }
}

/**
 * Tạo mã mời gia đình mới (hiệu lực 24h, tối đa 10 người)
 */
export async function createHomeInvite(homeId = '00000000-0000-0000-0000-000000000001', role = 'member', createdBy = null) {
  if (!isSupabaseConfigured()) return { success: false, message: 'Chưa cấu hình Supabase' };
  const supabase = getSupabase();
  if (!supabase) return { success: false, message: 'Không có kết nối Supabase' };

  try {
    const { data, error } = await supabase.rpc('create_home_invite', {
      p_home_id: homeId,
      p_role: role,
      p_created_by: createdBy
    });

    if (error) {
      return { success: false, message: error.message };
    }
    return data;
  } catch (err) {
    return { success: false, message: err.message };
  }
}

/**
 * Gia nhập Nhà thông minh bằng mã mời gia đình
 */
export async function joinHomeWithInvite(inviteCode, userId, displayName = 'Thành viên mới') {
  if (!isSupabaseConfigured()) return { success: false, message: 'Chưa cấu hình Supabase' };
  const supabase = getSupabase();
  if (!supabase) return { success: false, message: 'Không có kết nối Supabase' };

  try {
    const { data, error } = await supabase.rpc('join_home_with_invite', {
      p_invite_code: inviteCode.trim(),
      p_user_id: userId,
      p_display_name: displayName.trim()
    });

    if (error) {
      return { success: false, message: error.message };
    }
    return data;
  } catch (err) {
    return { success: false, message: err.message };
  }
}

/**
 * Gỡ thành viên khỏi Nhà thông minh (Chỉ Chủ nhà mới có quyền)
 */
export async function removeHomeMember(homeId, memberUserId) {
  if (!isSupabaseConfigured()) return { success: false, message: 'Chưa cấu hình Supabase' };
  const supabase = getSupabase();
  if (!supabase) return { success: false, message: 'Không có kết nối Supabase' };

  try {
    const { data, error } = await supabase.rpc('remove_home_member', {
      p_home_id: homeId,
      p_member_user_id: memberUserId
    });

    if (error) {
      return { success: false, message: error.message };
    }
    return data;
  } catch (err) {
    return { success: false, message: err.message };
  }
}


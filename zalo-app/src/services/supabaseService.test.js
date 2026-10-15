import { describe, it, expect, vi, beforeEach } from 'vitest';
import { subscribeHomeRealtime, sendLearnStart, sendLearnCancel } from './supabaseService';
import * as supabaseClientModule from './supabaseClient';

describe('supabaseService - Seam 2 (Firmware Realtime Connection & Handshake)', () => {
  beforeEach(() => {
    vi.restoreAllMocks();
  });

  describe('Slice 1: Nhận diện Firmware Online và giải mã Presence từ ESP32', () => {
    it('nhận diện ESP32 online và bóc tách đúng pin, sạc, RSSI khi nhận sự kiện Presence sync', () => {
      vi.spyOn(supabaseClientModule, 'isSupabaseConfigured').mockReturnValue(true);

      let presenceSyncHandler = null;
      let esp32PresenceState = {};

      const mockEsp32Channel = {
        on: vi.fn((type, filter, handler) => {
          if (type === 'presence' && filter.event === 'sync') {
            presenceSyncHandler = handler;
          }
          return mockEsp32Channel;
        }),
        presenceState: vi.fn(() => esp32PresenceState),
        subscribe: vi.fn(() => mockEsp32Channel)
      };

      const mockDbChannel = {
        on: vi.fn().mockReturnThis(),
        subscribe: vi.fn().mockReturnThis()
      };

      const mockUpdateBoards = vi.fn().mockReturnValue({
        eq: vi.fn().mockReturnValue({
          then: vi.fn()
        })
      });

      const mockSupabase = {
        channel: vi.fn((name) => {
          if (name.startsWith('home:')) return mockEsp32Channel;
          return mockDbChannel;
        }),
        from: vi.fn((table) => {
          if (table === 'boards') {
            return { update: mockUpdateBoards };
          }
          return { update: vi.fn() };
        }),
        removeChannel: vi.fn()
      };

      vi.spyOn(supabaseClientModule, 'getSupabase').mockReturnValue(mockSupabase);

      const onBoardUpdated = vi.fn();

      // 1. App bắt đầu lắng nghe Realtime của Nhà
      subscribeHomeRealtime({
        onBoardUpdated,
        homeId: 'home_test_001'
      });

      // 2. Kiểm tra App đã join đúng topic home:home_test_001
      expect(mockSupabase.channel).toHaveBeenCalledWith('home:home_test_001', expect.anything());
      expect(presenceSyncHandler).not.toBeNull();

      // 3. Giả lập firmware ESP32 vừa online và gửi gói tin Presence
      esp32PresenceState = {
        board_living_01: [
          {
            battery: 78,
            charging: false,
            usb: true,
            rssi: -64,
            firmware: '0.1.0',
            profileVersion: 3
          }
        ]
      };

      // Kích hoạt sự kiện Presence sync
      presenceSyncHandler();

      // 4. Xác minh callback onBoardUpdated được gọi với dữ liệu chuẩn hoá
      expect(onBoardUpdated).toHaveBeenCalledWith(
        expect.objectContaining({
          id: 'board_living_01',
          isOnline: true,
          battery: 78,
          charging: false,
          usb: true,
          rssi: -64,
          firmware: '0.1.0',
          profileVersion: 3,
          last_seen_at: expect.any(String)
        })
      );

      // 5. Xác minh đã tự động cập nhật bảng boards trên database
      expect(mockUpdateBoards).toHaveBeenCalledWith(
        expect.objectContaining({
          battery: 78,
          charging: false,
          usb: true,
          rssi: -64
        })
      );
    });
  });

  describe('Slice 2: Giao tiếp Học lệnh IR 2 chiều giữa App và ESP32', () => {
    it('gửi lệnh learn_start và nhận sự kiện tiến trình/kết quả từ ESP32', async () => {
      vi.spyOn(supabaseClientModule, 'isSupabaseConfigured').mockReturnValue(true);

      const broadcastListeners = {};
      let sentMsg = null;

      const mockEsp32Channel = {
        on: vi.fn((type, filter, handler) => {
          if (type === 'broadcast') {
            broadcastListeners[filter.event] = handler;
          }
          return mockEsp32Channel;
        }),
        presenceState: vi.fn(() => ({})),
        subscribe: vi.fn(() => mockEsp32Channel),
        send: vi.fn(async (msg) => {
          sentMsg = msg;
          return 'ok';
        })
      };

      const mockSupabase = {
        channel: vi.fn().mockReturnValue(mockEsp32Channel),
        from: vi.fn().mockReturnValue({ update: vi.fn().mockReturnValue({ eq: vi.fn().mockResolvedValue({}) }) }),
        removeChannel: vi.fn()
      };

      vi.spyOn(supabaseClientModule, 'getSupabase').mockReturnValue(mockSupabase);

      const onLearnEvent = vi.fn();

      // App đăng ký lắng nghe
      subscribeHomeRealtime({
        onLearnEvent,
        homeId: 'home_test_001'
      });

      // 1. App gửi lệnh kích hoạt mắt thu trên ESP32
      const started = await sendLearnStart('req_learn_999', 'home_test_001');
      expect(started).toBe(true);
      expect(sentMsg).toEqual({
        type: 'broadcast',
        event: 'learn_start',
        payload: { requestId: 'req_learn_999' }
      });

      // 2. ESP32 phát broadcast learn_progress (đã nhận lần 1)
      expect(broadcastListeners['learn_progress']).toBeDefined();
      broadcastListeners['learn_progress']({
        payload: { requestId: 'req_learn_999', status: 'first_captured' }
      });
      expect(onLearnEvent).toHaveBeenCalledWith({
        event: 'learn_progress',
        requestId: 'req_learn_999',
        status: 'first_captured'
      });

      // 3. ESP32 phát broadcast learn_result (thành công)
      expect(broadcastListeners['learn_result']).toBeDefined();
      broadcastListeners['learn_result']({
        payload: {
          requestId: 'req_learn_999',
          status: 'ok',
          ir: { protocol: 'NEC', code: '0x20DF10EF', bits: 32 }
        }
      });
      expect(onLearnEvent).toHaveBeenCalledWith({
        event: 'learn_result',
        requestId: 'req_learn_999',
        status: 'ok',
        ir: { protocol: 'NEC', code: '0x20DF10EF', bits: 32 }
      });

      // 4. App gửi lệnh huỷ học lệnh
      sentMsg = null;
      const cancelled = await sendLearnCancel('home_test_001');
      expect(cancelled).toBe(true);
      expect(sentMsg).toEqual({
        type: 'broadcast',
        event: 'learn_cancel',
        payload: {}
      });
    });
  });

  describe('Slice 3: Hủy đăng ký và giải phóng tài nguyên (Cleanup)', () => {
    it('gọi removeChannel cho cả DB channel và ESP32 channel khi unsubscribe', () => {
      vi.spyOn(supabaseClientModule, 'isSupabaseConfigured').mockReturnValue(true);

      const mockDbChannel = { on: vi.fn().mockReturnThis(), subscribe: vi.fn().mockReturnThis() };
      const mockEsp32Channel = { on: vi.fn().mockReturnThis(), subscribe: vi.fn().mockReturnThis(), presenceState: vi.fn(() => ({})) };

      const mockSupabase = {
        channel: vi.fn((name) => (name.startsWith('home:') ? mockEsp32Channel : mockDbChannel)),
        from: vi.fn().mockReturnValue({ update: vi.fn() }),
        removeChannel: vi.fn()
      };

      vi.spyOn(supabaseClientModule, 'getSupabase').mockReturnValue(mockSupabase);

      const unsubscribe = subscribeHomeRealtime({ homeId: 'home_test_001' });

      // Gọi hàm unsubscribe
      unsubscribe();

      // Kiểm tra cả 2 kênh đều được huỷ đăng ký
      expect(mockSupabase.removeChannel).toHaveBeenCalledWith(mockDbChannel);
      expect(mockSupabase.removeChannel).toHaveBeenCalledWith(mockEsp32Channel);
    });
  });
});

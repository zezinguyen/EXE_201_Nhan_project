import { describe, it, expect, vi, beforeEach } from 'vitest';
import {
  getActiveApiKey,
  analyzeVoiceWithGemini,
  postVilaoChatCompletion
} from './geminiService';

// Polyfill localStorage cho Vitest
const mockStorage = (() => {
  let store = {};
  return {
    getItem: (k) => store[k] || null,
    setItem: (k, v) => { store[k] = String(v); },
    removeItem: (k) => { delete store[k]; },
    clear: () => { store = {}; }
  };
})();
globalThis.localStorage = mockStorage;

describe('geminiService - Seam (Voice AI & Natural Language Command Parsing)', () => {
  beforeEach(() => {
    vi.restoreAllMocks();
    mockStorage.clear();
  });

  describe('Slice 1: Quản lý API Key & Xác thực', () => {
    it('ưu tiên key truyền trực tiếp vào hàm', () => {
      mockStorage.setItem('gemini_api_key', 'key_storage');
      const key = getActiveApiKey('key_direct');
      expect(key).toBe('key_direct');
    });

    it('lấy key từ localStorage khi không truyền trực tiếp', () => {
      mockStorage.setItem('gemini_api_key', 'sk-test-storage-key');
      const key = getActiveApiKey('');
      expect(key).toBe('sk-test-storage-key');
    });

    it('báo lỗi khi phân tích lệnh mà không có API Key nào', async () => {
      const origKey = import.meta.env.VITE_GEMINI_API_KEY;
      import.meta.env.VITE_GEMINI_API_KEY = '';
      try {
        await expect(analyzeVoiceWithGemini('bật tivi', '')).rejects.toThrow(
          /Chưa cấu hình API Key/i
        );
      } finally {
        import.meta.env.VITE_GEMINI_API_KEY = origKey;
      }
    });
  });

  describe('Slice 2: Phân tích khẩu lệnh văn bản (analyzeVoiceWithGemini)', () => {
    it('phân tích lệnh với key vilao.ai (sk-...) và làm sạch markdown ```json', async () => {
      // Mock fetch trả về JSON được bọc bởi code fence ```json ... ```
      const mockFetch = vi.fn().mockResolvedValue({
        ok: true,
        json: async () => ({
          choices: [
            {
              message: {
                content: '```json\n{"action":"TV_POWER_ON","title":"Bật Tivi","irCode":"0x20DF10EF","icon":"📺","device":"Smart TV"}\n```'
              }
            }
          ]
        })
      });
      globalThis.fetch = mockFetch;

      const result = await analyzeVoiceWithGemini('mở tivi lên', 'sk-mock-key-123');

      expect(mockFetch).toHaveBeenCalled();
      expect(result).toEqual(
        expect.objectContaining({
          action: 'TV_POWER_ON',
          title: 'Bật Tivi',
          irCode: '0x20DF10EF',
          icon: '📺',
          device: 'Smart TV',
          _modelUsed: expect.stringContaining('vilao.ai')
        })
      );
    });

    it('gọi đúng Google Gemini native API khi key là Google Key (không có sk-)', async () => {
      const mockFetch = vi.fn().mockResolvedValue({
        ok: true,
        json: async () => ({
          candidates: [
            {
              content: {
                parts: [
                  {
                    text: '{"action":"VOL_UP","title":"Tăng âm lượng","irCode":"0x20DF40BF","icon":"🔊","device":"Smart TV"}'
                  }
                ]
              }
            }
          ]
        })
      });
      globalThis.fetch = mockFetch;

      const result = await analyzeVoiceWithGemini('cho to tiếng lên', 'AIzaSyMockKey');

      expect(mockFetch).toHaveBeenCalledWith(
        expect.stringContaining('generativelanguage.googleapis.com'),
        expect.anything()
      );
      expect(result).toEqual(
        expect.objectContaining({
          action: 'VOL_UP',
          title: 'Tăng âm lượng'
        })
      );
    });
  });

  describe('Slice 3: Cơ chế Endpoint Fallback trong postVilaoChatCompletion', () => {
    it('tự động thử endpoint dự phòng khi endpoint đầu tiên gặp lỗi', async () => {
      let callCount = 0;
      const mockFetch = vi.fn().mockImplementation((url) => {
        callCount++;
        if (callCount === 1) {
          // Lần gọi đầu tiên lỗi mạng
          return Promise.reject(new Error('Network error on local proxy'));
        }
        // Lần gọi thứ 2 thành công
        return Promise.resolve({
          ok: true,
          json: async () => ({
            choices: [{ message: { content: '{"ok":true}' } }]
          })
        });
      });
      globalThis.fetch = mockFetch;

      const res = await postVilaoChatCompletion(
        { model: 'gemini-3.8-flash', messages: [] },
        'sk-fallback-test',
        ['https://primary.example.com/chat/completions', 'https://backup.example.com/chat/completions']
      );

      expect(callCount).toBeGreaterThanOrEqual(2);
      expect(res.choices[0].message.content).toBe('{"ok":true}');
    });
  });
});

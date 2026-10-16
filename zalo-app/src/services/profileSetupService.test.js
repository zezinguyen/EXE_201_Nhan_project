import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import * as supabaseClientModule from './supabaseClient';
import { resetHomeChannels } from './iotService';
import {
  fetchBoardProfile,
  tryAcProtocol,
  saveAcProtocol,
  saveLearnedCommand,
  saveTvPreset,
  tryIrCode,
  detectAcProtocol,
  acSetupErrorText,
} from './profileSetupService';

// Kênh giả: giữ handler theo event; reply(event, payload) giả lập mạch trả lời
function createFakeChannel(onSend) {
  const handlers = {};
  const channel = {
    state: 'joined',
    sent: [],
    on: vi.fn((type, { event }, cb) => {
      (handlers[event] ||= []).push(cb);
      return channel;
    }),
    subscribe: vi.fn(() => channel),
    send: vi.fn(async (msg) => {
      channel.sent.push(msg);
      if (onSend) setTimeout(() => onSend(msg, channel.reply), 0);
      return 'ok';
    }),
    reply: (event, payload) => (handlers[event] || []).forEach((cb) => cb({ payload })),
  };
  return channel;
}

function useChannel(channel) {
  vi.spyOn(supabaseClientModule, 'isSupabaseConfigured').mockReturnValue(true);
  vi.spyOn(supabaseClientModule, 'getSupabase').mockReturnValue({ channel: () => channel });
}

describe('profileSetupService', () => {
  beforeEach(() => {
    vi.restoreAllMocks();
    resetHomeChannels();
  });
  afterEach(() => vi.useRealTimers());

  it('fetchBoardProfile trả danh sách thiết bị và giao thức gợi ý', async () => {
    const channel = createFakeChannel((msg, reply) => {
      reply('profile_info', {
        requestId: msg.payload.requestId,
        profileVersion: 5,
        devices: [{ id: 'ac_funiki', type: 'ac', name: 'Điều hòa', acProtocol: 'COOLIX', commands: [] }],
        acProtocols: ['COOLIX', 'MIDEA'],
      });
    });
    useChannel(channel);

    const res = await fetchBoardProfile();
    expect(channel.sent[0].event).toBe('profile_get');
    expect(res).toEqual({
      ok: true,
      profileVersion: 5,
      devices: [{ id: 'ac_funiki', type: 'ac', name: 'Điều hòa', acProtocol: 'COOLIX', commands: [] }],
      acProtocols: ['COOLIX', 'MIDEA'],
    });
  });

  it('bỏ qua phản hồi của requestId khác', async () => {
    vi.useFakeTimers();
    const channel = createFakeChannel((msg, reply) => {
      reply('setup_result', { requestId: 'nguoi-khac', ok: true });
    });
    useChannel(channel);

    const promise = tryAcProtocol({ protocol: 'COOLIX' }, { timeoutMs: 1000 });
    await vi.advanceTimersByTimeAsync(1001);
    expect(await promise).toEqual({ ok: false, error: 'E_TIMEOUT' });
  });

  it('tryAcProtocol gửi trạng thái điều hòa và trả lỗi của mạch', async () => {
    const channel = createFakeChannel((msg, reply) => {
      reply('setup_result', { requestId: msg.payload.requestId, ok: false, error: 'E_PROTOCOL', profileVersion: 5 });
    });
    useChannel(channel);

    const res = await tryAcProtocol({ protocol: 'XYZ', temp: 22 });
    expect(channel.sent[0]).toMatchObject({
      event: 'ac_try',
      payload: { protocol: 'XYZ', power: true, temp: 22, mode: 'cool', fan: 'auto' },
    });
    expect(res).toEqual({ ok: false, error: 'E_PROTOCOL', profileVersion: 5 });
    expect(acSetupErrorText(res.error)).toBe('Mạch không hỗ trợ giao thức này');
  });

  it('saveAcProtocol gửi deviceId + protocol (+ name nếu có)', async () => {
    const channel = createFakeChannel((msg, reply) => {
      reply('setup_result', { requestId: msg.payload.requestId, ok: true, profileVersion: 6 });
    });
    useChannel(channel);

    const res = await saveAcProtocol({ deviceId: 'ac_funiki', protocol: 'MIDEA' });
    expect(channel.sent[0].event).toBe('ac_set_protocol');
    expect(channel.sent[0].payload).toMatchObject({ deviceId: 'ac_funiki', protocol: 'MIDEA' });
    expect(channel.sent[0].payload).not.toHaveProperty('name');
    expect(res).toEqual({ ok: true, error: undefined, profileVersion: 6 });
  });

  it('saveLearnedCommand gửi mã IR đã học cho thiết bị bấm nút', async () => {
    const channel = createFakeChannel((msg, reply) => {
      reply('setup_result', { requestId: msg.payload.requestId, ok: true, profileVersion: 7 });
    });
    useChannel(channel);
    const ir = { protocol: 'NEC', code: '0x20DF10EF', bits: 32 };

    const res = await saveLearnedCommand({ deviceId: 'tv_living', deviceName: 'Tivi', action: 'POWER', ir });
    expect(channel.sent[0]).toMatchObject({
      event: 'profile_put_command',
      payload: { deviceId: 'tv_living', deviceName: 'Tivi', action: 'POWER', ir },
    });
    expect(res).toEqual({ ok: true, error: undefined, profileVersion: 7 });
  });

  it('tryIrCode gửi ir_try với mã cần thử', async () => {
    const channel = createFakeChannel((msg, reply) => {
      reply('setup_result', { requestId: msg.payload.requestId, ok: true, profileVersion: 5 });
    });
    useChannel(channel);
    const ir = { protocol: 'SAMSUNG', code: '0xE0E040BF', bits: 32 };

    expect((await tryIrCode(ir)).ok).toBe(true);
    expect(channel.sent[0]).toMatchObject({ event: 'ir_try', payload: { ir } });
  });

  it('saveTvPreset lưu từng phím và dừng ở phím lỗi đầu tiên', async () => {
    const channel = createFakeChannel((msg, reply) => {
      const fail = msg.payload.action === 'VOL_UP';
      reply('setup_result', { requestId: msg.payload.requestId, ok: !fail, error: fail ? 'E_SAVE_FAILED' : undefined });
    });
    useChannel(channel);
    const preset = {
      commands: {
        POWER: { protocol: 'NEC', code: '0x1', bits: 32 },
        VOL_UP: { protocol: 'NEC', code: '0x2', bits: 32 },
        MUTE: { protocol: 'NEC', code: '0x3', bits: 32 },
      },
    };

    const res = await saveTvPreset({ deviceId: 'tv_living', preset });
    expect(res).toEqual({ ok: false, error: 'E_SAVE_FAILED', action: 'VOL_UP' });
    expect(channel.sent.map((m) => m.payload.action)).toEqual(['POWER', 'VOL_UP']);
  });

  it('detectAcProtocol báo tiến trình rồi trả giao thức nhận ra', async () => {
    const channel = createFakeChannel((msg, reply) => {
      const requestId = msg.payload.requestId;
      reply('learn_progress', { requestId, status: 'first_captured' });
      reply('learn_result', {
        requestId,
        status: 'ok',
        ir: { protocol: 'UNKNOWN', raw: [1, 2], freq: 38 },
        detected: 'COOLIX',
        acSupported: true,
      });
    });
    useChannel(channel);
    const onProgress = vi.fn();

    const res = await detectAcProtocol({ onProgress });
    expect(channel.sent[0].event).toBe('learn_start');
    expect(onProgress).toHaveBeenCalledWith('first_captured');
    expect(res).toMatchObject({ ok: true, detected: 'COOLIX', acSupported: true });
  });

  it('detectAcProtocol báo hết giờ khi mạch gửi learn_timeout', async () => {
    const channel = createFakeChannel((msg, reply) => {
      reply('learn_timeout', { requestId: msg.payload.requestId, status: 'timeout' });
    });
    useChannel(channel);

    expect(await detectAcProtocol()).toEqual({ ok: false, error: 'E_LEARN_TIMEOUT' });
  });

  it('trả E_NOT_CONFIGURED khi chưa cấu hình Supabase', async () => {
    vi.spyOn(supabaseClientModule, 'isSupabaseConfigured').mockReturnValue(false);
    expect(await fetchBoardProfile()).toEqual({ ok: false, error: 'E_NOT_CONFIGURED' });
  });
});

import { describe, it, expect } from 'vitest';
import { parseVoiceCommand } from './voiceCommandParser';

describe('voiceCommandParser - Seam: Edge cases & Unknown input', () => {
  it('returns null when input is empty or null', () => {
    expect(parseVoiceCommand('')).toBeNull();
    expect(parseVoiceCommand(null)).toBeNull();
  });

  it('returns UNKNOWN action when command is not recognized', () => {
    const result = parseVoiceCommand('xin chào thế giới');
    expect(result).not.toBeNull();
    expect(result.action).toBe('UNKNOWN');
    expect(result.irCode).toBe('0x00000000');
  });
});

describe('voiceCommandParser - Seam: Core Intent Matching', () => {
  it('identifies TV Power On command (with and without diacritics)', () => {
    expect(parseVoiceCommand('bật tivi')).toMatchObject({
      action: 'TV_POWER_ON',
      irCode: '0x20DF10EF'
    });
    expect(parseVoiceCommand('bat tivi')).toMatchObject({
      action: 'TV_POWER_ON',
      irCode: '0x20DF10EF'
    });
  });

  it('identifies TV Power Off command', () => {
    expect(parseVoiceCommand('tắt tivi')).toMatchObject({
      action: 'TV_POWER_OFF',
      irCode: '0x20DF10EF'
    });
    expect(parseVoiceCommand('tat tivi')).toMatchObject({
      action: 'TV_POWER_OFF',
      irCode: '0x20DF10EF'
    });
  });

  it('identifies Volume and Mute commands', () => {
    expect(parseVoiceCommand('tăng âm')).toMatchObject({
      action: 'VOL_UP',
      irCode: '0x20DF40BF'
    });
    expect(parseVoiceCommand('giảm âm')).toMatchObject({
      action: 'VOL_DOWN',
      irCode: '0x20DFC03F'
    });
    expect(parseVoiceCommand('tắt tiếng')).toMatchObject({
      action: 'MUTE',
      irCode: '0x20DF906F'
    });
  });

  it('identifies TV channel switching commands', () => {
    expect(parseVoiceCommand('mở vtv1')).toMatchObject({
      action: 'CH_VTV1',
      irCode: '0x20DF00FF'
    });
    expect(parseVoiceCommand('mở vtv3')).toMatchObject({
      action: 'CH_VTV3',
      irCode: '0x20DF08F7'
    });
  });

  it('identifies open YouTube app command', () => {
    expect(parseVoiceCommand('mở youtube')).toMatchObject({
      action: 'APP_YOUTUBE',
      irCode: '0x20DF58A7'
    });
    expect(parseVoiceCommand('mo youtube')).toMatchObject({
      action: 'APP_YOUTUBE',
      irCode: '0x20DF58A7'
    });
  });
});

describe('voiceCommandParser - Seam: Regional Dialect Semantics', () => {
  it('handles Northern & Central dialect expressions (vô tuyến, đài, thời sự)', () => {
    expect(parseVoiceCommand('mở vô tuyến')).toMatchObject({
      action: 'TV_POWER_ON'
    });
    expect(parseVoiceCommand('thời sự')).toMatchObject({
      action: 'CH_VTV1'
    });
    expect(parseVoiceCommand('đừng kêu')).toMatchObject({
      action: 'MUTE'
    });
  });

  it('handles Southern dialect expressions (bự lên, nhỏ lại côi, nghỉ coi, cho coi)', () => {
    expect(parseVoiceCommand('cho bự lên')).toMatchObject({
      action: 'VOL_UP'
    });
    expect(parseVoiceCommand('nhỏ lại côi')).toMatchObject({
      action: 'VOL_DOWN'
    });
    expect(parseVoiceCommand('nghỉ coi tivi')).toMatchObject({
      action: 'TV_POWER_OFF'
    });
    expect(parseVoiceCommand('cho coi tivi')).toMatchObject({
      action: 'TV_POWER_ON'
    });
  });
});

describe('voiceCommandParser - Seam: YouTube Dynamic Entity Extraction', () => {
  it('extracts specific YouTube channel query and generates valid search URL', () => {
    const result = parseVoiceCommand('mở kênh VTV24 trên youtube');
    expect(result).toMatchObject({
      action: 'YOUTUBE_SEARCH',
      channelQuery: 'VTV24',
      youtubeUrl: 'https://www.youtube.com/results?search_query=VTV24',
      irCode: '0x20DF58A7'
    });
  });

  it('strips polite/padding words and extracts music/video topics', () => {
    const result = parseVoiceCommand('mở ca nhạc trữ tình trên mạng cho tôi');
    expect(result).toMatchObject({
      action: 'YOUTUBE_SEARCH',
      channelQuery: 'Ca nhạc trữ tình'
    });
    expect(result.youtubeUrl).toContain('search_query=');
  });

  it('detects traditional music or drama intent without explicit mention of youtube', () => {
    const result = parseVoiceCommand('nghe cải lương hồ quảng');
    expect(result).toMatchObject({
      action: 'YOUTUBE_SEARCH',
      channelQuery: 'Cải lương hồ quảng'
    });
  });
});

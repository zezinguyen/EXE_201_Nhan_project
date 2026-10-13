export const IR_COMMANDS = [
  {
    keywords: [
      'mở youtube', 'bật youtube', 'xem youtube', 'vào youtube', 'mở yt', 'bật yt', 'youtube', 'yt',
      'mở vô túp', 'bật vô túp', 'coi vô túp', 'mở du túp', 'bật du túp', 'coi du túp', 'xem du túp',
      'mở du tu be', 'vô túp côi', 'du túp coi', 'mở mạng coi'
    ],
    action: 'APP_YOUTUBE',
    title: 'Mở ứng dụng YouTube',
    irCode: '0x20DF58A7',
    device: 'Smart TV (Hồng ngoại)',
    icon: '▶️',
    description: 'Phát mã IR mở trực tiếp ứng dụng YouTube trên TV'
  },
  {
    keywords: [
      'bật tivi', 'bật tv', 'mở tivi', 'mở tv', 'khởi động tivi', 'bật màn hình',
      'bật đài', 'mở đài', 'bật ti di', 'mở ti di', 'bật vô tuyến', 'mở vô tuyến',
      'bật tivi lên', 'mở tivi lên', 'bật tivi côi', 'mở tivi côi', 'bật dùm tivi', 'mở giùm tivi',
      'cho coi tivi', 'cho xem tivi'
    ],
    action: 'TV_POWER_ON',
    title: 'Bật nguồn Tivi',
    irCode: '0x20DF10EF',
    device: 'Smart TV (Hồng ngoại)',
    icon: '📺',
    description: 'Phát xung IR Power On để bật Tivi'
  },
  {
    keywords: [
      'tắt tivi', 'tắt tv', 'đóng tivi', 'tắt màn hình', 'tắt đài', 'tắt ti di', 'tắt vô tuyến',
      'tắt tivi côi', 'tắt giùm tivi', 'tắt dùm tivi', 'nghỉ coi tivi', 'thôi coi tivi', 'tắt máy đi'
    ],
    action: 'TV_POWER_OFF',
    title: 'Tắt nguồn Tivi',
    irCode: '0x20DF10EF',
    device: 'Smart TV (Hồng ngoại)',
    icon: '🔌',
    description: 'Phát xung IR Power Off để tắt Tivi'
  },
  {
    keywords: [
      'tăng âm', 'to lên', 'tăng volume', 'tăng tiếng', 'to tiếng', 'cho to lên', 'vặn to lên',
      'lớn lên', 'bự lên', 'cho lớn tiếng', 'kêu to lên', 'to lên côi', 'to lên chút', 'bự lên chút'
    ],
    action: 'VOL_UP',
    title: 'Tăng âm lượng (+)',
    irCode: '0x20DF40BF',
    device: 'Smart TV (Hồng ngoại)',
    icon: '🔊',
    description: 'Phát xung IR Volume Up (+)'
  },
  {
    keywords: [
      'giảm âm', 'nhỏ lại', 'giảm volume', 'giảm tiếng', 'nhỏ tiếng', 'cho nhỏ lại', 'vặn nhỏ lại',
      'bớt tiếng', 'hạ tiếng', 'nhỏ bớt', 'bé lại', 'nhỏ lại côi', 'nhỏ lại chút', 'bớt ồn'
    ],
    action: 'VOL_DOWN',
    title: 'Giảm âm lượng (-)',
    irCode: '0x20DFC03F',
    device: 'Smart TV (Hồng ngoại)',
    icon: '🔉',
    description: 'Phát xung IR Volume Down (-)'
  },
  {
    keywords: [
      'tắt tiếng', 'im lặng', 'mute', 'câm', 'ngắt tiếng', 'đừng kêu'
    ],
    action: 'MUTE',
    title: 'Tắt tiếng (Mute)',
    irCode: '0x20DF906F',
    device: 'Smart TV (Hồng ngoại)',
    icon: '🔇',
    description: 'Tắt/Bật tiếng tạm thời'
  },
  {
    keywords: ['vtv1', 'kênh vtv1', 'mở vtv1', 'kênh 1', 'đài vtv1'],
    action: 'CH_VTV1',
    title: 'Chuyển sang kênh VTV1',
    irCode: '0x20DF00FF',
    device: 'Smart TV (Hồng ngoại)',
    icon: '📡',
    description: 'Chọn kênh truyền hình số 1'
  },
  {
    keywords: ['vtv3', 'kênh vtv3', 'mở vtv3', 'kênh 3', 'đài vtv3'],
    action: 'CH_VTV3',
    title: 'Chuyển sang kênh VTV3',
    irCode: '0x20DF08F7',
    device: 'Smart TV (Hồng ngoại)',
    icon: '📡',
    description: 'Chọn kênh truyền hình số 3'
  }
];

export const REGIONAL_VOCABULARY = {
  YOUTUBE: [
    'youtube', 'yt', 'du tup', 'du tup be', 'du tu be', 'vo tup', 'u tup', 'du túp', 
    'du tu bê', 'vô túp', 'u túp', 'mở mạng', 'xem mạng', 'xem clip', 'video'
  ],
  TV: [
    'tivi', 'ti vi', 'ti di', 'tivi', 'tv', 'đài', 'dai', 'vô tuyến', 'vo tuyen', 'truyền hình', 'màn hình'
  ],
  POWER_ON: [
    'bật', 'mở', 'khởi động', 'lên', 'cho coi', 'cho xem', 'bật lên', 'mở lên', 'xem tivi'
  ],
  POWER_OFF: [
    'tắt', 'đóng', 'tắt đi', 'nghỉ coi', 'thôi coi', 'tắt dùm', 'tắt giùm', 'tắt máy'
  ],
  VOL_UP: [
    'to lên', 'to hơn', 'lớn lên', 'lớn hơn', 'tăng âm', 'tăng tiếng', 'bự lên', 'bự hơn', 
    'vặn to', 'kêu to', 'cho to', 'to tiếng', 'lớn tiếng'
  ],
  VOL_DOWN: [
    'nhỏ lại', 'nhỏ hơn', 'bớt tiếng', 'giảm âm', 'giảm tiếng', 'hạ tiếng', 'vặn nhỏ', 
    'nhỏ bớt', 'cho nhỏ', 'nhỏ tiếng', 'bé lại'
  ],
  MUTE: [
    'tắt tiếng', 'im lặng', 'câm', 'mute', 'ngắt tiếng', 'đừng kêu'
  ]
};

export function removeVietnameseTones(str) {
  if (!str) return '';
  return str
    .normalize('NFD')
    .replace(/[\u0300-\u036f]/g, '')
    .replace(/đ/g, 'd')
    .replace(/Đ/g, 'D')
    .toLowerCase()
    .trim();
}

export function parseVoiceCommand(text) {
  if (!text || !text.trim()) {
    return null;
  }

  const lower = text.toLowerCase().trim();
  const noTone = removeVietnameseTones(lower);

  // 0. Bóc tách kênh hoặc nội dung YouTube cụ thể
  const ytKeywords = ['youtube', 'yt', 'du tup', 'du túp', 'vo tup', 'vô túp', 'u tup', 'u túp', 'mạng'];
  const hasYtMention = ytKeywords.some(k => lower.includes(k) || noTone.includes(k));

  let extractedChannel = '';

  const channelMatch = text.match(/(?:mở|bật|vào|xem|coi)\s+(?:kênh|kenh)\s+([^,.]+)/i);
  if (channelMatch && channelMatch[1]) {
    extractedChannel = channelMatch[1].replace(/(?:trên|tren)\s+(?:youtube|yt|du\s*túp|du\s*tup|vô\s*túp|vo\s*tup|mạng)/gi, '').trim();
  }

  if (!extractedChannel && hasYtMention) {
    const onYtMatch = text.match(/(?:mở|bật|vào|xem|coi|nghe)\s+(.+?)\s+(?:trên|tren)\s+(?:youtube|yt|du\s*túp|du\s*tup|vô\s*túp|vo\s*tup|mạng)/i);
    if (onYtMatch && onYtMatch[1]) {
      extractedChannel = onYtMatch[1].replace(/^(?:kênh|kenh)\s+/i, '').trim();
    }
  }

  if (!extractedChannel && hasYtMention) {
    const enterYtMatch = text.match(/(?:vào|vao|mở|mo|bật|bat)\s+(?:youtube|yt|du\s*túp|du\s*tup|vô\s*túp|vo\s*tup)\s+(?:xem|coi|bật|mở|nghe)\s+(.+)/i);
    if (enterYtMatch && enterYtMatch[1]) {
      extractedChannel = enterYtMatch[1].replace(/^(?:kênh|kenh)\s+/i, '').trim();
    }
  }

  if (!extractedChannel) {
    const musicMatch = text.match(/^(?:mở|bật|nghe|cho nghe|cho coi|cho xem)\s+(nhạc\s+[^,.]+|ca nhạc\s+[^,.]+|cải lương\s+[^,.]+|hài\s+[^,.]+|phim\s+[^,.]+)/i);
    if (musicMatch && musicMatch[1] && !lower.includes('tivi') && !lower.includes('đài')) {
      extractedChannel = musicMatch[1].trim();
    }
  }

  if (extractedChannel) {
    extractedChannel = extractedChannel
      .replace(/\s+(?:cho\s+tao|cho\s+tôi|cho\s+em|cho\s+anh|giúp|dùm|giùm|với|nhé|nha|đi|coi|xem)$/gi, '')
      .trim();

    if (extractedChannel.length > 1 && !['youtube', 'yt', 'du tup', 'du túp'].includes(extractedChannel.toLowerCase())) {
      const cleanTitle = extractedChannel.charAt(0).toUpperCase() + extractedChannel.slice(1);
      return {
        action: 'YOUTUBE_SEARCH',
        title: `Mở kênh "${cleanTitle}" trên YouTube`,
        channelQuery: cleanTitle,
        youtubeUrl: `https://www.youtube.com/results?search_query=${encodeURIComponent(cleanTitle)}`,
        irCode: '0x20DF58A7',
        device: 'Smart TV (Hồng ngoại + Cast)',
        icon: '▶️',
        description: `Phát mã IR mở YouTube trên TV và mở luồng kênh "${cleanTitle}" để Cast`
      };
    }
  }

  // 1. So khớp với danh sách IR commands cơ bản
  for (const cmd of IR_COMMANDS) {
    if (
      cmd.keywords.some((keyword) => {
        const lowerKeyword = keyword.toLowerCase();
        const noToneKeyword = removeVietnameseTones(keyword);
        return lower.includes(lowerKeyword) || noTone.includes(noToneKeyword);
      })
    ) {
      return cmd;
    }
  }

  // 2. Thuật toán phân tích ngữ nghĩa phương ngữ 3 miền (Dialect Semantic Intent Matching)
  const hasYouTube = REGIONAL_VOCABULARY.YOUTUBE.some(w => lower.includes(w) || noTone.includes(removeVietnameseTones(w)));
  const hasTV = REGIONAL_VOCABULARY.TV.some(w => lower.includes(w) || noTone.includes(removeVietnameseTones(w)));
  const hasOn = REGIONAL_VOCABULARY.POWER_ON.some(w => lower.includes(w) || noTone.includes(removeVietnameseTones(w)));
  const hasOff = REGIONAL_VOCABULARY.POWER_OFF.some(w => lower.includes(w) || noTone.includes(removeVietnameseTones(w)));
  const hasVolUp = REGIONAL_VOCABULARY.VOL_UP.some(w => lower.includes(w) || noTone.includes(removeVietnameseTones(w)));
  const hasVolDown = REGIONAL_VOCABULARY.VOL_DOWN.some(w => lower.includes(w) || noTone.includes(removeVietnameseTones(w)));
  const hasMute = REGIONAL_VOCABULARY.MUTE.some(w => lower.includes(w) || noTone.includes(removeVietnameseTones(w)));

  if (hasYouTube) {
    return IR_COMMANDS.find(c => c.action === 'APP_YOUTUBE');
  }
  if (lower.includes('vtv1') || lower.includes('thời sự') || noTone.includes('thoi su') || lower.includes('kênh 1')) {
    return IR_COMMANDS.find(c => c.action === 'CH_VTV1');
  }
  if (lower.includes('vtv3') || lower.includes('kênh 3') || noTone.includes('kenh 3')) {
    return IR_COMMANDS.find(c => c.action === 'CH_VTV3');
  }
  if (hasVolUp) {
    return IR_COMMANDS.find(c => c.action === 'VOL_UP');
  }
  if (hasVolDown) {
    return IR_COMMANDS.find(c => c.action === 'VOL_DOWN');
  }
  if (hasMute) {
    return IR_COMMANDS.find(c => c.action === 'MUTE');
  }
  if (hasOff && (hasTV || lower.includes('tắt') || lower.includes('nghỉ') || lower.includes('đi ngủ') || lower.includes('thôi'))) {
    return IR_COMMANDS.find(c => c.action === 'TV_POWER_OFF');
  }
  if (hasOn && (hasTV || lower.includes('bật') || lower.includes('mở') || lower.includes('xem') || lower.includes('coi'))) {
    return IR_COMMANDS.find(c => c.action === 'TV_POWER_ON');
  }

  return {
    action: 'UNKNOWN',
    title: 'Lệnh chưa được cấu hình',
    irCode: '0x00000000',
    device: 'Chưa xác định',
    icon: '❓',
    description: `Chưa có mã hồng ngoại tương ứng cho câu nói "${text}"`
  };
}

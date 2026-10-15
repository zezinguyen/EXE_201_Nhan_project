import { convertBlobToWav } from '../utils/wavUtils';


export const GEMINI_MODELS = [
  'gemini-3.8-flash',
  'gemini-2.5-flash',
  'gemini-2.0-flash',
  'gemini-1.5-flash'
];

export function getActiveApiKey(providedKey) {
  if (providedKey && typeof providedKey === 'string' && providedKey.trim()) {
    return providedKey.trim();
  }
  if (typeof localStorage !== 'undefined') {
    const stored = localStorage.getItem('gemini_api_key');
    if (stored && stored.trim()) return stored.trim();
  }
  if (typeof import.meta !== 'undefined' && import.meta.env && import.meta.env.VITE_GEMINI_API_KEY) {
    const envKey = import.meta.env.VITE_GEMINI_API_KEY;
    if (envKey && envKey.trim()) return envKey.trim();
  }
  return '';
}

function cleanJsonOutput(text) {
  if (!text) return '';
  let cleaned = text.trim();
  if (cleaned.startsWith('```')) {
    cleaned = cleaned.replace(/^```(?:json)?\s*/i, '').replace(/\s*```$/, '').trim();
  }
  return cleaned;
}

export async function postVilaoChatCompletion(payload, apiKey, customEndpoints = null) {
  const endpoints = Array.isArray(customEndpoints) && customEndpoints.length > 0 ? [...customEndpoints] : [];
  if (endpoints.length === 0) {
    if (typeof window !== 'undefined' && (window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1')) {
      endpoints.push('/api-ai/v1/chat/completions');
    }
    const configuredBase = (typeof import.meta !== 'undefined' && import.meta.env?.VITE_AI_BASE_URL) || 'https://api.vilao.ai/v1';
    const directEndpoint = `${configuredBase.replace(/\/+$/, '')}/chat/completions`;
    if (!endpoints.includes(directEndpoint)) {
      endpoints.push(directEndpoint);
    }
    if (!endpoints.includes('https://api.vilao.ai/v1/chat/completions')) {
      endpoints.push('https://api.vilao.ai/v1/chat/completions');
    }
  }

  let lastErr = null;
  for (const endpoint of endpoints) {
    try {
      const response = await fetch(endpoint, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${apiKey.trim()}`
        },
        body: JSON.stringify(payload)
      });

      if (response.ok) {
        return await response.json();
      }

      const errBody = await response.json().catch(() => ({}));
      lastErr = new Error(errBody.error?.message || `Lỗi API HTTP ${response.status} (${endpoint})`);
    } catch (netErr) {
      lastErr = netErr;
      console.warn(`Kết nối endpoint ${endpoint} thất bại, thử endpoint dự phòng:`, netErr);
    }
  }

  throw lastErr || new Error('Không thể kết nối đến máy chủ AI vilao.ai');
}

export async function callVilaoApi(prompt, apiKey) {
  const result = await postVilaoChatCompletion({
    model: 'gemini-3.8-flash',
    messages: [
      {
        role: 'user',
        content: prompt
      }
    ],
    max_tokens: 150,
    temperature: 0.1
  }, apiKey);

  const content = result.choices?.[0]?.message?.content;
  if (!content) {
    throw new Error('AI không trả về nội dung dữ liệu');
  }
  const cleaned = cleanJsonOutput(content);
  const parsed = JSON.parse(cleaned);
  parsed._modelUsed = 'vilao.ai (gemini-3.8-flash)';
  return parsed;
}

export async function callGoogleGeminiApi(payload, apiKey) {
  let lastError = null;

  for (const model of GEMINI_MODELS) {
    try {
      const endpoint = `https://generativelanguage.googleapis.com/v1beta/models/${model}:generateContent?key=${apiKey.trim()}`;
      const response = await fetch(endpoint, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      });

      if (response.ok) {
        const result = await response.json();
        const outputText = result.candidates?.[0]?.content?.parts?.[0]?.text;
        if (!outputText) {
          throw new Error(`Gemini AI (${model}) không trả về dữ liệu`);
        }
        const cleaned = cleanJsonOutput(outputText);
        const parsed = JSON.parse(cleaned);
        parsed._modelUsed = `Google Gemini (${model})`;
        return parsed;
      }

      const errBody = await response.json().catch(() => ({}));
      lastError = new Error(errBody.error?.message || `Lỗi HTTP ${response.status}`);
      if (response.status === 404) continue;
      throw lastError;
    } catch (err) {
      if (err.message && err.message.includes('404')) continue;
      throw err;
    }
  }

  throw lastError || new Error('Không thể kết nối đến Google Gemini');
}

export async function analyzeVoiceWithGemini(userText, apiKey) {
  const activeKey = getActiveApiKey(apiKey);
  if (!activeKey) {
    throw new Error('Chưa cấu hình API Key trong file .env');
  }

  const prompt = `Phân loại khẩu lệnh Smart TV: "${userText}"
Hiểu phương ngữ Bắc/Trung/Nam, tiếng người già, trẻ nhỏ, cách nói nhại (vô túp, nét phờ lích, ti di, vô tuyến), câu tự nhiên.

DANH SÁCH LỆNH:
1. YOUTUBE_SEARCH: Khi người dùng muốn xem kênh cụ thể, ca sĩ, phim, video trên YouTube (VD: "mở kênh VTV24", "xem MixiGaming", "mở FAP TV", "bật nhạc thiếu nhi", "xem hài Hoài Linh", "mở nhạc Trịnh").
   -> IR: 0x20DF58A7 | Icon: ▶️ | Title: Mở kênh [Tên kênh] trên YouTube | channelQuery: "[Tên kênh/nội dung]"
2. APP_YOUTUBE: Mở chung ứng dụng YouTube (không có tên kênh cụ thể) -> IR: 0x20DF58A7 | Icon: ▶️ | Title: Mở YouTube | channelQuery: ""
3. APP_NETFLIX: Netflix, nét phờ lích, coi phim... -> IR: 0x20DF5EA1 | Icon: 🍿 | Title: Mở Netflix
4. TV_POWER_ON: Bật tivi, mở tivi, mở đài, mở máy, xem tivi... -> IR: 0x20DF10EF | Icon: 📺 | Title: Bật Tivi
5. TV_POWER_OFF: Tắt tivi, tắt đài, đi ngủ, nghỉ coi, dẹp tivi... -> IR: 0x20DF10EF | Icon: 🔌 | Title: Tắt Tivi
6. VOL_UP: Tăng âm lượng, to lên, lớn tiếng, bự tiếng... -> IR: 0x20DF40BF | Icon: 🔊 | Title: Tăng âm lượng
7. VOL_DOWN: Giảm âm lượng, nhỏ lại, bớt ồn, bé lại... -> IR: 0x20DFC03F | Icon: 🔉 | Title: Giảm âm lượng
8. MUTE: Tắt tiếng, im lặng, câm tiếng, ngắt tiếng... -> IR: 0x20DF906F | Icon: 🔇 | Title: Tắt tiếng (Mute)
9. CH_VTV1: Thời sự, vtv1, đài 1... -> IR: 0x20DF00FF | Icon: 📡 | Title: Kênh VTV1
10. CH_VTV3: VTV3, kênh 3, giải trí... -> IR: 0x20DF08F7 | Icon: 📡 | Title: Kênh VTV3
11. UNKNOWN: Không liên quan Tivi -> IR: 0x00000000 | Icon: ❓ | Title: Lệnh chưa xác định

Trả về DUY NHẤT một JSON hợp lệ:
{"action":"TÊN_LỆNH","title":"Tên ngắn","channelQuery":"Tên kênh nếu có","irCode":"Mã IR","icon":"Emoji","device":"Smart TV (Hồng ngoại + Cast)","explanation":"Giải thích ngắn 1 câu"}`;

  if (activeKey.startsWith('sk-')) {
    return await callVilaoApi(prompt, activeKey);
  }

  const payload = {
    contents: [{ parts: [{ text: prompt }] }],
    generationConfig: {
      responseMimeType: 'application/json',
      temperature: 0.1,
      maxOutputTokens: 150
    }
  };

  return await callGoogleGeminiApi(payload, activeKey);
}

export function blobToBase64(blob) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onloadend = () => {
      const base64String = reader.result.split(',')[1];
      resolve(base64String);
    };
    reader.onerror = reject;
    reader.readAsDataURL(blob);
  });
}

export async function transcribeAndAnalyzeAudioWithGemini(audioBlob, apiKey) {
  const activeKey = getActiveApiKey(apiKey);
  if (!activeKey) {
    throw new Error('Chưa cấu hình API Key trong file .env');
  }

  const prompt = `
Bạn là Trợ lý AI Nhà thông minh chuyên nghe âm thanh giọng nói tiếng Việt từ micro:

QUY TẮC NHẬN DIỆN:
1. TRÍCH XUẤT TRUNG THỰC: Hãy nghe kỹ và ghi lại CHÍNH XÁC lời nói tiếng Việt của người dùng vào trường "transcript".
   - Kể cả khi câu nói là câu trò chuyện, câu hát, lời chào ("alo", "chào bạn", "thử mic"), VẪN PHẢI ghi đầy đủ vào "transcript".
   - TUYỆT ĐỐI KHÔNG ghi nhận xét (như "quá nhỏ", "tiếng ồn", "im lặng") vào trường "transcript".
2. KHI HOÀN TOÀN KHÔNG CÓ TIẾNG NGƯỜI NÓI (chỉ có im lặng, quạt gió, rè mic):
   Bắt buộc trả về:
   {
     "transcript": "",
     "action": "UNKNOWN",
     "title": "Không có tiếng nói",
     "channelQuery": "",
     "irCode": "0x00000000",
     "icon": "❓",
     "device": "Chưa xác định",
     "explanation": "Không phát hiện tiếng người nói."
   }

DANH SÁCH LỆNH TIVI (Chỉ phân loại khi người dùng ra lệnh điều khiển):
1. YOUTUBE_SEARCH: Khi người dùng muốn xem kênh, bài hát, ca sĩ, phim trên YouTube (VD: "mở kênh VTV24", "xem MixiGaming", "mở FAP TV", "bật nhạc thiếu nhi", "xem hài Hoài Linh", "mở nhạc Sơn Tùng")
   -> IR: 0x20DF58A7 | Icon: ▶️ | Title: Mở [Nội dung] trên YouTube | channelQuery: "[Tên kênh/nội dung]"
2. APP_YOUTUBE: Mở chung ứng dụng YouTube -> IR: 0x20DF58A7 | Icon: ▶️ | Title: Mở YouTube | channelQuery: ""
3. APP_NETFLIX: Netflix, nét phờ lích, coi phim... -> IR: 0x20DF5EA1 | Icon: 🍿 | Title: Mở Netflix | channelQuery: ""
4. TV_POWER_ON: Bật tivi, mở tivi, mở đài, lên tivi... -> IR: 0x20DF10EF | Icon: 📺 | Title: Bật Tivi | channelQuery: ""
5. TV_POWER_OFF: Tắt tivi, tắt đài, tắt máy, đi ngủ... -> IR: 0x20DF10EF | Icon: 🔌 | Title: Tắt Tivi | channelQuery: ""
6. VOL_UP: Tăng âm lượng, cho to lên, vặn to lên, lớn tiếng... -> IR: 0x20DF40BF | Icon: 🔊 | Title: Tăng âm lượng | channelQuery: ""
7. VOL_DOWN: Giảm âm lượng, nhỏ lại, bớt ồn, bé lại... -> IR: 0x20DFC03F | Icon: 🔉 | Title: Giảm âm lượng | channelQuery: ""
8. MUTE: Tắt tiếng, im lặng, câm tiếng, ngắt tiếng... -> IR: 0x20DF906F | Icon: 🔇 | Title: Tắt tiếng (Mute) | channelQuery: ""
9. CH_VTV1: Thời sự, kênh 1, vtv1, tin tức... -> IR: 0x20DF00FF | Icon: 📡 | Title: Kênh VTV1 | channelQuery: ""
10. CH_VTV3: Kênh 3, vtv3, phim truyện... -> IR: 0x20DF08F7 | Icon: 📡 | Title: Kênh VTV3 | channelQuery: ""
11. UNKNOWN: Câu nói không điều khiển Tivi -> IR: 0x00000000 | Icon: ❓ | Title: Lệnh chưa xác định | channelQuery: ""

Trả về DUY NHẤT một JSON hợp lệ:
{
  "transcript": "Lời nói tiếng Việt thực sự nghe được (chuỗi rỗng nếu hoàn toàn không có người nói)",
  "action": "TÊN_HÀNH_ĐỘNG",
  "title": "Tên hành động ngắn gọn",
  "channelQuery": "Từ khóa tìm kiếm nếu có",
  "irCode": "Mã IR tương ứng",
  "icon": "Emoji",
  "device": "Smart TV (Hồng ngoại)",
  "explanation": "Giải thích ngắn 1 câu"
}
`;

  if (activeKey.startsWith('sk-')) {
    // Gọi qua vilao.ai (gemini-3.8-flash với input_audio WAV)
    let wavBlob = audioBlob;
    try {
      wavBlob = await convertBlobToWav(audioBlob);
    } catch (e) {
      console.warn('Lỗi convertBlobToWav, dùng blob gốc:', e);
    }

    // Nếu không có âm thanh giọng nói thực tế (im lặng / mic rè nền) -> trả về kết quả im lặng ngay lập tức
    if (wavBlob && wavBlob.isSilence) {
      return {
        transcript: '',
        action: 'UNKNOWN',
        title: 'Chưa nhận diện được giọng nói',
        channelQuery: '',
        irCode: '0x00000000',
        icon: '❓',
        device: 'Chưa xác định',
        explanation: 'Không phát hiện tiếng người nói (âm thanh micro quá nhỏ hoặc im lặng).'
      };
    }

    const base64Wav = await blobToBase64(wavBlob);
    const result = await postVilaoChatCompletion({
      model: 'gemini-3.8-flash',
      messages: [
        {
          role: 'user',
          content: [
            {
              type: 'text',
              text: prompt
            },
            {
              type: 'input_audio',
              input_audio: {
                data: base64Wav,
                format: 'wav'
              }
            }
          ]
        }
      ],
      max_tokens: 500,
      temperature: 0.1
    }, activeKey);

    const content = result.choices?.[0]?.message?.content;
    if (!content) {
      throw new Error('AI không trả về nội dung dữ liệu');
    }
    const cleaned = cleanJsonOutput(content);
    const parsed = JSON.parse(cleaned);

    // BẢO VỆ CHỐNG TỰ ĐỘNG TẠO LỆNH:
    // Nếu không có transcript thực tế từ lời nói người dùng, ép action thành UNKNOWN
    if (!parsed.transcript || !parsed.transcript.trim() ||
        parsed.transcript.toLowerCase().includes('không phát hiện') ||
        parsed.transcript.toLowerCase().includes('im lặng') ||
        parsed.transcript.toLowerCase().includes('không có tiếng') ||
        parsed.transcript.toLowerCase().includes('tiếng ồn')) {
      parsed.transcript = '';
      parsed.action = 'UNKNOWN';
      parsed.title = 'Chưa nhận diện được giọng nói';
    }

    parsed._modelUsed = 'vilao.ai (gemini-3.8-flash audio)';
    return parsed;
  }

  const base64Audio = await blobToBase64(audioBlob);
  let mimeType = audioBlob.type ? audioBlob.type.split(';')[0] : 'audio/webm';
  if (!mimeType || mimeType === 'audio') mimeType = 'audio/webm';

  const payload = {
    contents: [
      {
        parts: [
          { inlineData: { mimeType: mimeType, data: base64Audio } },
          { text: prompt }
        ]
      }
    ],
    generationConfig: {
      responseMimeType: 'application/json',
      temperature: 0.1
    }
  };

  const parsed = await callGoogleGeminiApi(payload, activeKey);
  if (!parsed.transcript || !parsed.transcript.trim() ||
      parsed.transcript.toLowerCase().includes('không phát hiện') ||
      parsed.transcript.toLowerCase().includes('im lặng') ||
      parsed.transcript.toLowerCase().includes('không có tiếng') ||
      parsed.transcript.toLowerCase().includes('tiếng ồn')) {
    parsed.transcript = '';
    parsed.action = 'UNKNOWN';
    parsed.title = 'Chưa nhận diện được giọng nói';
  }
  return parsed;
}

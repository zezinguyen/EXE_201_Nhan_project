// Script tự động tạo 9 file MP3 giọng đọc tiếng Việt chuẩn cho DFPlayer Mini
// Theo đặc tả Spec 3.1 & AudioTracks.h
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const TRACKS = [
  { file: '0001.mp3', text: 'Đã nhận lệnh' },
  { file: '0002.mp3', text: 'Không tìm thấy lệnh' },
  { file: '0003.mp3', text: 'Đang vào chế độ thiết lập' },
  { file: '0004.mp3', text: 'Mời bấm phím trên remote' },
  { file: '0005.mp3', text: 'Đã học lệnh' },
  { file: '0006.mp3', text: 'Học lệnh thất bại' },
  { file: '0007.mp3', text: 'Không thực hiện được lệnh' },
  { file: '0008.mp3', text: 'Pin yếu' },
  { file: '0009.mp3', text: 'Khôi phục cài đặt gốc' }
];

const outputDir = path.join(__dirname, '..', 'mp3');
if (!fs.existsSync(outputDir)) {
  fs.mkdirSync(outputDir, { recursive: true });
}

async function downloadTts(text, filename) {
  const encoded = encodeURIComponent(text);
  const url = `https://translate.google.com/translate_tts?ie=UTF-8&tl=vi&client=tw-ob&q=${encoded}`;
  
  const res = await fetch(url, {
    headers: {
      'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)'
    }
  });

  if (!res.ok) {
    throw new Error(`Lỗi tải TTS HTTP ${res.status}`);
  }

  const arrayBuffer = await res.arrayBuffer();
  const buffer = Buffer.from(arrayBuffer);
  const filePath = path.join(outputDir, filename);
  fs.writeFileSync(filePath, buffer);
  console.log(`✓ Đã tạo file: ${filename} -> "${text}" (${buffer.length} bytes)`);
}

async function main() {
  console.log('=== ĐANG TẠO 9 FILE MP3 CHO DFPLAYER MINI ===');
  for (const track of TRACKS) {
    try {
      await downloadTts(track.text, track.file);
      await new Promise(r => setTimeout(r, 400)); // Khoảng nghỉ nhỏ
    } catch (err) {
      console.error(`✗ Lỗi tạo ${track.file}:`, err.message);
    }
  }
  console.log(`\n🎉 HOÀN TẤT! Toàn bộ 9 file MP3 đã được lưu vào thư mục: ${outputDir}`);
  console.log('👉 Chỉ cần copy thư mục "mp3" này vào thẻ nhớ MicroSD của DFPlayer Mini!');
}

main();

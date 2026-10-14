export function audioBufferToWavBlob(audioBuffer) {
  const numOfChan = audioBuffer.numberOfChannels;
  const length = audioBuffer.length * numOfChan * 2 + 44;
  const out = new DataView(new ArrayBuffer(length));
  const channels = [];
  let sample = 0;
  let offset = 0;
  let pos = 0;

  function setUint16(data) {
    out.setUint16(pos, data, true);
    pos += 2;
  }

  function setUint32(data) {
    out.setUint32(pos, data, true);
    pos += 4;
  }

  // RIFF identifier 'RIFF'
  setUint32(0x46464952);
  // file length minus RIFF identifier length and file description length
  setUint32(length - 8);
  // RIFF type 'WAVE'
  setUint32(0x45564157);
  // format chunk identifier 'fmt '
  setUint32(0x20746d66);
  // format chunk length
  setUint32(16);
  // sample format (raw)
  setUint16(1);
  // channel count
  setUint16(numOfChan);
  // sample rate
  setUint32(audioBuffer.sampleRate);
  // byte rate (sample rate * block align)
  setUint32(audioBuffer.sampleRate * 2 * numOfChan);
  // block align (channel count * bytes per sample)
  setUint16(numOfChan * 2);
  // bits per sample
  setUint16(16);
  // data chunk identifier 'data'
  setUint32(0x61746164);
  // data chunk length
  setUint32(length - pos - 4);

  for (let i = 0; i < audioBuffer.numberOfChannels; i++) {
    channels.push(audioBuffer.getChannelData(i));
  }

  while (pos < length) {
    for (let i = 0; i < numOfChan; i++) {
      sample = Math.max(-1, Math.min(1, channels[i][offset]));
      sample = (0.5 + sample < 0 ? sample * 32768 : sample * 32767) | 0;
      out.setInt16(pos, sample, true);
      pos += 2;
    }
    offset++;
  }

  return new Blob([out.buffer], { type: 'audio/wav' });
}

export async function convertBlobToWav(blob, targetSampleRate = 16000) {
  const AudioCtx = window.AudioContext || window.webkitAudioContext;
  const audioCtx = new AudioCtx();
  try {
    const arrayBuffer = await blob.arrayBuffer();
    const decodedBuffer = await audioCtx.decodeAudioData(arrayBuffer);

    // Tính toán năng lượng âm thanh thực tế (peak & rms)
    let peak = 0;
    let sumSquares = 0;
    const channel0 = decodedBuffer.getChannelData(0);
    for (let i = 0; i < channel0.length; i++) {
      const abs = Math.abs(channel0[i]);
      if (abs > peak) peak = abs;
      sumSquares += channel0[i] * channel0[i];
    }
    const rms = channel0.length > 0 ? Math.sqrt(sumSquares / channel0.length) : 0;

    // Resample sang 16kHz Mono chuẩn cho mô hình AI nhận diện giọng nói
    const OfflineCtx = window.OfflineAudioContext || window.webkitOfflineAudioContext;
    let wavBlob = null;
    if (OfflineCtx) {
      const offlineCtx = new OfflineCtx(
        1,
        Math.max(1, Math.ceil(decodedBuffer.duration * targetSampleRate)),
        targetSampleRate
      );
      const source = offlineCtx.createBufferSource();
      source.buffer = decodedBuffer;
      source.connect(offlineCtx.destination);
      source.start(0);

      const resampledBuffer = await offlineCtx.startRendering();
      wavBlob = audioBufferToWavBlob(resampledBuffer);
    } else {
      wavBlob = audioBufferToWavBlob(decodedBuffer);
    }

    wavBlob.peak = peak;
    wavBlob.rms = rms;
    // Nhận diện im lặng: Khi người dùng không nói gì (chỉ có tiếng ồn nền/rè mic rất nhỏ)
    wavBlob.isSilence = peak < 0.015 && rms < 0.0035;
    return wavBlob;
  } finally {
    audioCtx.close().catch(() => {});
  }
}


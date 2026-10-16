/**
 * Audio Capture Manager - Deep Module
 * Che giấu toàn bộ cấu hình Web Audio API, AnalyserNode FFT, Time-Domain RMS/Peak,
 * MediaRecorder codecs, và quản lý giải phóng tài nguyên microphone phần cứng.
 */

// Tính toán âm lượng người nói (Voice Volume 0-100) kết hợp FFT và Time-Domain
export function calculateVoiceVolume(dataArray, timeData = null) {
  if (!dataArray || dataArray.length === 0) return 0;
  let peak = 0;
  let voiceSum = 0;
  const voiceBins = Math.min(dataArray.length, 48);
  for (let i = 0; i < voiceBins; i++) {
    if (dataArray[i] > peak) peak = dataArray[i];
    voiceSum += dataArray[i];
  }
  const voiceAvg = voiceBins > 0 ? voiceSum / voiceBins : 0;
  let scaled = (voiceAvg * 0.4 + peak * 0.6) / 1.8;

  // Nếu có dữ liệu sóng thời gian (timeData) và có năng lượng trong dải giọng nói
  if (timeData && timeData.length > 0 && (peak > 8 || voiceAvg > 4)) {
    let timePeak = 0;
    for (let i = 0; i < timeData.length; i++) {
      const dev = Math.abs(timeData[i] - 128);
      if (dev > timePeak) timePeak = dev;
    }
    const timeScaled = (timePeak / 128) * 100 * 2.5;
    scaled = Math.max(scaled, timeScaled);
  }

  // Nếu năng lượng chỉ ở dải tần số cao ngoài giọng nói (> bin 48) mà không có giọng nói
  if (peak === 0 && voiceAvg === 0) {
    return 0;
  }

  return Math.min(100, Math.round(scaled));
}

// Trích xuất các dải tần số âm thanh (Voice Frequency Spectrum Bands: 0-100) cho sóng âm động
export function getFrequencyBands(dataArray, numBands = 20) {
  if (!dataArray || dataArray.length === 0) {
    return new Array(numBands).fill(0);
  }
  const voiceBins = Math.min(dataArray.length, 48);
  const binStep = Math.max(1, Math.floor(voiceBins / numBands));
  const bands = [];

  for (let b = 0; b < numBands; b++) {
    const start = b * binStep;
    const end = Math.min(voiceBins, start + binStep);
    let sum = 0;
    let peak = 0;
    let count = 0;
    for (let i = start; i < end; i++) {
      if (dataArray[i] > peak) peak = dataArray[i];
      sum += dataArray[i];
      count++;
    }
    const avg = count > 0 ? sum / count : 0;
    const scaled = (avg * 0.4 + peak * 0.6) / 1.7;
    bands.push(Math.min(100, Math.round(scaled)));
  }

  return bands;
}

function isVirtualOrUnwantedMic(label) {
  if (!label) return false;
  const l = label.toLowerCase();
  return (
    l.includes('deskin') ||
    l.includes('deskln') ||
    l.includes('virtual') ||
    l.includes('stereo mix') ||
    l.includes('camo') ||
    l.includes('droidcam') ||
    l.includes('iriun') ||
    l.includes('obs')
  );
}

// Quét danh sách thiết bị Microphone thực tế
export async function getAudioInputDevices() {
  if (typeof navigator === 'undefined' || !navigator.mediaDevices) {
    return [];
  }
  try {
    const tempStream = await navigator.mediaDevices.getUserMedia({ audio: true });
    tempStream.getTracks().forEach((t) => t.stop());

    const devices = await navigator.mediaDevices.enumerateDevices();
    const audioInputs = devices.filter((d) => d.kind === 'audioinput');

    // Sắp xếp: micro thực lên đầu, micro ảo xuống cuối
    return audioInputs.sort((a, b) => {
      const aBad = isVirtualOrUnwantedMic(a.label);
      const bBad = isVirtualOrUnwantedMic(b.label);
      if (aBad && !bBad) return 1;
      if (!aBad && bBad) return -1;
      return 0;
    });
  } catch (err) {
    console.warn('Lỗi quét thiết bị Microphone:', err);
    return [];
  }
}

// Chọn micro ưu tiên (loại bỏ micro ảo, stereo mix)
export function pickDefaultMicrophone(devices) {
  if (!devices || devices.length === 0) return '';

  // Ưu tiên 1: Microphone Array hoặc thiết bị Realtek/Intel vật lý
  const preferred = devices.find(
    (m) =>
      m.label &&
      !isVirtualOrUnwantedMic(m.label) &&
      (m.label.toLowerCase().includes('array') ||
       m.deviceId === 'default' ||
       m.label.toLowerCase().includes('realtek') ||
       m.label.toLowerCase().includes('intel'))
  );
  if (preferred) return preferred.deviceId;

  // Ưu tiên 2: Bất kỳ micro nào không phải ảo
  const realMic = devices.find((m) => m.label && !isVirtualOrUnwantedMic(m.label));
  return realMic ? realMic.deviceId : (devices[0]?.deviceId || '');
}

// Kiểm tra codec MediaRecorder được hỗ trợ tốt nhất
export function getSupportedAudioMimeType() {
  if (typeof MediaRecorder === 'undefined') return 'audio/webm';
  if (MediaRecorder.isTypeSupported('audio/webm;codecs=opus')) {
    return 'audio/webm;codecs=opus';
  }
  if (MediaRecorder.isTypeSupported('audio/webm')) {
    return 'audio/webm';
  }
  if (MediaRecorder.isTypeSupported('audio/mp4')) {
    return 'audio/mp4';
  }
  return 'audio/webm';
}

// Khởi tạo AudioContext kèm AnalyserNode và kiểm tra trạng thái
export async function createAudioAnalyser(stream) {
  const AudioCtx = window.AudioContext || window.webkitAudioContext;
  if (!AudioCtx) return null;

  const ctx = new AudioCtx();
  if (ctx.state === 'suspended') {
    try {
      await ctx.resume();
    } catch (e) {
      console.warn('AudioContext resume warning:', e);
    }
  }

  const analyser = ctx.createAnalyser();
  analyser.fftSize = 256;
  analyser.smoothingTimeConstant = 0.3;
  analyser.minDecibels = -85;
  analyser.maxDecibels = -15;

  const source = ctx.createMediaStreamSource(stream);
  source.connect(analyser);

  // Thêm silent GainNode kết nối tới destination để trình duyệt luôn chạy audio render clock
  const silentGain = ctx.createGain();
  silentGain.gain.value = 0.0;
  analyser.connect(silentGain);
  silentGain.connect(ctx.destination);

  return { ctx, analyser, source, silentGain };
}

// Bắt đầu phiên kiểm tra Microphone (Đo volume + thu âm thử phát lại)
export async function testMicrophone(deviceId, {
  onVolume,
  onFrequencyBands,
  onRecordedUrl,
  onError,
  onMutedChange,
  onTrackInfo
} = {}) {
  let stream = null;
  let animFrameId = null;
  let analyserSetup = null;
  let mediaRecorder = null;
  const audioChunks = [];

  try {
    const tryConstraints = [];
    if (deviceId && deviceId !== 'default') {
      tryConstraints.push({
        audio: {
          deviceId: { exact: deviceId },
          autoGainControl: true,
          echoCancellation: true,
          noiseSuppression: true
        }
      });
      tryConstraints.push({
        audio: {
          deviceId: { ideal: deviceId },
          autoGainControl: true,
          echoCancellation: true,
          noiseSuppression: true
        }
      });
    }
    tryConstraints.push({
      audio: {
        autoGainControl: true,
        echoCancellation: true,
        noiseSuppression: true
      }
    });
    tryConstraints.push({ audio: true });

    let lastError = null;
    for (const c of tryConstraints) {
      try {
        stream = await navigator.mediaDevices.getUserMedia(c);
        if (stream && stream.getAudioTracks().length > 0) break;
      } catch (e) {
        lastError = e;
      }
    }

    if (!stream || stream.getAudioTracks().length === 0) {
      throw lastError || new Error('Không thể mở microphone');
    }

    const track = stream.getAudioTracks()[0];
    if (onTrackInfo) {
      onTrackInfo({
        label: track.label,
        muted: track.muted,
        enabled: track.enabled,
        settings: track.getSettings ? track.getSettings() : {}
      });
    }
    if (onMutedChange) {
      onMutedChange(track.muted);
    }
    track.onmute = () => {
      if (onMutedChange) onMutedChange(true);
    };
    track.onunmute = () => {
      if (onMutedChange) onMutedChange(false);
    };

    // 1. Phân tích âm lượng qua AudioContext & AnalyserNode
    analyserSetup = await createAudioAnalyser(stream);
    if (analyserSetup) {
      const analyser = analyserSetup.analyser;
      const freqData = new Uint8Array(analyser.frequencyBinCount);
      const timeData = new Uint8Array(analyser.fftSize);

      const loop = () => {
        analyser.getByteFrequencyData(freqData);
        analyser.getByteTimeDomainData(timeData);
        const vol = calculateVoiceVolume(freqData, timeData);
        if (onVolume) onVolume(vol);
        if (onFrequencyBands) {
          onFrequencyBands(getFrequencyBands(freqData, 20));
        }
        animFrameId = requestAnimationFrame(loop);
      };
      loop();
    }

    // 2. Thu âm thử nghiệm để người dùng có thể nghe lại
    const mimeType = getSupportedAudioMimeType();
    try {
      mediaRecorder = new MediaRecorder(stream, mimeType ? { mimeType } : undefined);
      mediaRecorder.ondataavailable = (e) => {
        if (e.data && e.data.size > 0) audioChunks.push(e.data);
      };
      mediaRecorder.onstop = () => {
        if (audioChunks.length > 0) {
          const blob = new Blob(audioChunks, { type: mimeType });
          const url = URL.createObjectURL(blob);
          if (onRecordedUrl) onRecordedUrl(url, blob);
        }
      };
      mediaRecorder.start(100);
    } catch (recErr) {
      console.warn('Lỗi khởi tạo MediaRecorder khi test mic:', recErr);
    }

    return {
      stream,
      analyserSetup,
      track,
      stop: () => {
        if (animFrameId) cancelAnimationFrame(animFrameId);
        if (mediaRecorder && mediaRecorder.state !== 'inactive') {
          try { mediaRecorder.stop(); } catch {}
        }
        if (analyserSetup?.ctx && analyserSetup.ctx.state !== 'closed') {
          try { analyserSetup.ctx.close(); } catch {}
        }
        if (stream) {
          try { stream.getTracks().forEach((t) => t.stop()); } catch {}
        }
        if (onVolume) onVolume(0);
        if (onFrequencyBands) onFrequencyBands(new Array(20).fill(0));
      }
    };
  } catch (err) {
    if (onError) onError(err);
    if (stream) {
      try { stream.getTracks().forEach((t) => t.stop()); } catch {}
    }
    return { stop: () => {} };
  }
}

// Bắt đầu thu âm chính thức cho trợ lý giọng nói
export async function startAudioCapture({
  deviceId,
  onVolume,
  onFrequencyBands,
  onRecordedBlob,
  onMutedChange,
  onTrackInfo
} = {}) {
  const tryConstraints = [];
  if (deviceId && deviceId !== 'default') {
    tryConstraints.push({
      audio: {
        deviceId: { exact: deviceId },
        autoGainControl: true,
        echoCancellation: true,
        noiseSuppression: true
      }
    });
    tryConstraints.push({
      audio: {
        deviceId: { ideal: deviceId },
        autoGainControl: true,
        echoCancellation: true,
        noiseSuppression: true
      }
    });
  }
  tryConstraints.push({
    audio: {
      autoGainControl: true,
      echoCancellation: true,
      noiseSuppression: true
    }
  });
  tryConstraints.push({ audio: true });

  let stream = null;
  let lastErr = null;
  for (const c of tryConstraints) {
    try {
      stream = await navigator.mediaDevices.getUserMedia(c);
      if (stream && stream.getAudioTracks().length > 0) break;
    } catch (e) {
      lastErr = e;
    }
  }

  if (!stream || stream.getAudioTracks().length === 0) {
    throw lastErr || new Error('Không thể truy cập Microphone');
  }

  const track = stream.getAudioTracks()[0];
  if (onTrackInfo) {
    onTrackInfo({
      label: track.label,
      muted: track.muted,
      enabled: track.enabled
    });
  }
  if (onMutedChange) {
    onMutedChange(track.muted);
    track.onmute = () => onMutedChange(true);
    track.onunmute = () => onMutedChange(false);
  }

  let animFrameId = null;
  const analyserSetup = await createAudioAnalyser(stream);
  if (analyserSetup) {
    const analyser = analyserSetup.analyser;
    const freqData = new Uint8Array(analyser.frequencyBinCount);
    const timeData = new Uint8Array(analyser.fftSize);

    const liveLoop = () => {
      analyser.getByteFrequencyData(freqData);
      analyser.getByteTimeDomainData(timeData);
      const vol = calculateVoiceVolume(freqData, timeData);
      if (onVolume) onVolume(vol);
      if (onFrequencyBands) {
        onFrequencyBands(getFrequencyBands(freqData, 20));
      }
      animFrameId = requestAnimationFrame(liveLoop);
    };
    liveLoop();
  }

  const audioChunks = [];
  const mimeType = getSupportedAudioMimeType();
  const recorder = new MediaRecorder(stream, mimeType ? { mimeType } : undefined);
  recorder.ondataavailable = (e) => {
    if (e.data && e.data.size > 0) audioChunks.push(e.data);
  };
  recorder.start(100);

  return {
    stream,
    analyserSetup,
    track,
    stop: () => {
      return new Promise((resolve) => {
        if (animFrameId) cancelAnimationFrame(animFrameId);
        if (onVolume) onVolume(0);
        if (onFrequencyBands) onFrequencyBands(new Array(20).fill(0));

        recorder.onstop = () => {
          try { stream.getTracks().forEach((t) => t.stop()); } catch {}
          if (analyserSetup?.ctx && analyserSetup.ctx.state !== 'closed') {
            try { analyserSetup.ctx.close(); } catch {}
          }
          const blob = audioChunks.length > 0 
            ? new Blob(audioChunks, { type: mimeType }) 
            : null;
          if (onRecordedBlob) {
            try { onRecordedBlob(blob); } catch (e) { console.warn('Lỗi xử lý onRecordedBlob:', e); }
          }
          resolve(blob);
        };

        if (recorder.state !== 'inactive') {
          try { recorder.stop(); } catch { resolve(null); }
        } else {
          try { stream.getTracks().forEach((t) => t.stop()); } catch {}
          if (analyserSetup?.ctx && analyserSetup.ctx.state !== 'closed') {
            try { analyserSetup.ctx.close(); } catch {}
          }
          resolve(null);
        }
      });
    },
    abort: () => {
      if (animFrameId) cancelAnimationFrame(animFrameId);
      if (onVolume) onVolume(0);
      if (onFrequencyBands) onFrequencyBands(new Array(20).fill(0));
      try {
        recorder.ondataavailable = null;
        recorder.onstop = null;
        if (recorder.state !== 'inactive') recorder.stop();
      } catch {}
      try { stream.getTracks().forEach((t) => t.stop()); } catch {}
      if (analyserSetup?.ctx && analyserSetup.ctx.state !== 'closed') {
        try { analyserSetup.ctx.close(); } catch {}
      }
    }
  };
}

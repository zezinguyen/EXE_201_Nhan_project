import React, { useState, useRef, useEffect } from 'react';
import './App.css';
import { analyzeVoiceWithGemini, transcribeAndAnalyzeAudioWithGemini } from './geminiService';
import { getIotConfig, saveIotConfig, sendIotCommand, testIotConnection } from './iotService';
import { parseVoiceCommand } from './voiceCommandParser';
import {
  getAudioInputDevices,
  pickDefaultMicrophone,
  testMicrophone,
  startAudioCapture
} from './audioCaptureManager';
import {
  getSupabaseConfig,
  saveSupabaseConfig,
  isSupabaseConfigured,
  testSupabaseConnection
} from './supabaseClient';
import {
  fetchDevicesFromSupabase,
  syncDeviceToSupabase,
  recordVoiceCommandLog,
  subscribeDeviceRealtime
} from './supabaseService';
import { SUPABASE_SCHEMA_SQL } from './supabaseSchema';

function App() {
  const [isListening, setIsListening] = useState(false);
  const [transcript, setTranscript] = useState('');
  const [interimText, setInterimText] = useState('');
  const [parsedCommand, setParsedCommand] = useState(null);
  const [actionToast, setActionToast] = useState('');
  const [showSettingsModal, setShowSettingsModal] = useState(false);

  // Danh sách 3 thiết bị trong nhà (Quạt, Điều hòa, TV)
  const [devices, setDevices] = useState([
    { id: 'fan', name: 'Quạt', active: false, statusText: 'Đã tắt' },
    { id: 'ac', name: 'Điều hòa', active: false, statusText: 'Đã tắt' },
    { id: 'tv', name: 'TV', active: false, statusText: 'Đã tắt' }
  ]);

  // Cấu hình Supabase Backend
  const [supabaseUrl, setSupabaseUrl] = useState(() => getSupabaseConfig().url);
  const [supabaseAnonKey, setSupabaseAnonKey] = useState(() => getSupabaseConfig().anonKey);
  const [supabasePingResult, setSupabasePingResult] = useState('');
  const [isTestingSupabase, setIsTestingSupabase] = useState(false);
  const [isSupabaseOnline, setIsSupabaseOnline] = useState(() => isSupabaseConfigured());
  const [sqlCopied, setSqlCopied] = useState(false);

  // Cấu hình IoT ESP32
  const [iotEndpoint, setIotEndpoint] = useState(() => getIotConfig().endpoint);
  const [iotAutoSend, setIotAutoSend] = useState(() => getIotConfig().autoSend);
  const [iotTvBrand, setIotTvBrand] = useState(() => getIotConfig().tvBrand);
  const [iotAcBrand, setIotAcBrand] = useState(() => getIotConfig().acBrand || 'Panasonic');
  const [acTemp, setAcTemp] = useState(26);
  const [iotPingResult, setIotPingResult] = useState('');
  const [isPingingIot, setIsPingingIot] = useState(false);
  const [isIotOnline, setIsIotOnline] = useState(false);

  const iotAutoSendRef = useRef(iotAutoSend);
  const iotEndpointRef = useRef(iotEndpoint);
  const iotAcBrandRef = useRef(iotAcBrand);
  useEffect(() => { iotAutoSendRef.current = iotAutoSend; }, [iotAutoSend]);
  useEffect(() => { iotEndpointRef.current = iotEndpoint; }, [iotEndpoint]);
  useEffect(() => { iotAcBrandRef.current = iotAcBrand; }, [iotAcBrand]);

  const geminiApiKey = import.meta.env.VITE_GEMINI_API_KEY || localStorage.getItem('gemini_api_key') || '';

  // Quản lý thu âm & Sóng âm trực tiếp
  const audioCaptureRef = useRef(null);
  const micTestControllerRef = useRef(null);
  const [liveVolume, setLiveVolume] = useState(0);
  const [frequencyBands, setFrequencyBands] = useState(() => new Array(20).fill(0));

  // Thiết bị Micro
  const [audioDevices, setAudioDevices] = useState([]);
  const [selectedDeviceId, setSelectedDeviceId] = useState('');
  const [isTestingMic, setIsTestingMic] = useState(false);
  const [testRecordedAudioUrl, setTestRecordedAudioUrl] = useState('');
  const [isMicHardwareMuted, setIsMicHardwareMuted] = useState(false);

  const recognitionRef = useRef(null);
  const sessionIdRef = useRef(0);
  const isListeningRef = useRef(false);
  const isStartingRef = useRef(false);
  const commandHandledRef = useRef(false);
  const silenceTimerRef = useRef(null);
  const currentTranscriptRef = useRef('');
  const maxSessionTimerRef = useRef(null);
  const maxSessionVolumeRef = useRef(0);

  // Đọc phản hồi bằng giọng nói tiếng Việt rõ ràng, thân thiện cho người lớn tuổi
  const speakFeedback = (text) => {
    if (!('speechSynthesis' in window) || !text) return;
    try {
      window.speechSynthesis.cancel();
      const utterance = new SpeechSynthesisUtterance(text);
      utterance.lang = 'vi-VN';
      utterance.rate = 1.0;
      utterance.pitch = 1.0;
      window.speechSynthesis.speak(utterance);
    } catch { }
  };

  const showNotification = (msg) => {
    setActionToast(msg);
    setTimeout(() => setActionToast(''), 4000);
  };

  // Cập nhật trạng thái bật/tắt thiết bị & đồng bộ Supabase
  const updateDeviceState = (deviceId, shouldBeActive, customStatus = null, meta = {}) => {
    const statusText = customStatus || (shouldBeActive ? 'Đang bật' : 'Đã tắt');
    setDevices((prev) =>
      prev.map((d) => {
        if (d.id === deviceId) {
          return {
            ...d,
            active: shouldBeActive,
            statusText
          };
        }
        return d;
      })
    );

    // Đồng bộ lên Supabase Database (nếu đã cấu hình)
    syncDeviceToSupabase(deviceId, {
      active: shouldBeActive,
      statusText,
      temp: meta.temp !== undefined ? meta.temp : (deviceId === 'ac' ? acTemp : undefined),
      mode: meta.mode || 'cool',
      brand: meta.brand || (deviceId === 'ac' ? iotAcBrandRef.current : undefined),
      irCode: meta.irCode || '0x00000000'
    }).then((ok) => {
      if (ok) setIsSupabaseOnline(true);
    }).catch(() => {});
  };

  // Bấm trực tiếp vào Card thiết bị để Bật/Tắt
  const toggleDevice = (device) => {
    const nextState = !device.active;
    updateDeviceState(device.id, nextState);

    if (device.id === 'tv') {
      const tvCmd = {
        action: nextState ? 'TV_POWER_ON' : 'TV_POWER_OFF',
        title: nextState ? 'Bật nguồn TV' : 'Tắt nguồn TV',
        irCode: '0x20DF10EF',
        device: 'Smart TV (Hồng ngoại)'
      };
      setParsedCommand(tvCmd);
      speakFeedback(tvCmd.title);
      recordVoiceCommandLog({
        transcript: tvCmd.title,
        action: tvCmd.action,
        title: tvCmd.title,
        device: tvCmd.device,
        irCode: tvCmd.irCode,
        source: 'manual_tap'
      }).catch(() => {});
      sendIotCommand(tvCmd, iotEndpointRef.current).then((res) => {
        if (res.success) {
          setIsIotOnline(true);
          showNotification(`Đã gửi lệnh hồng ngoại: ${tvCmd.title}`);
        }
      });
    } else if (device.id === 'ac') {
      const acCmd = {
        device: 'ac',
        action: nextState ? 'AC_POWER_ON' : 'AC_POWER_OFF',
        title: nextState ? `Bật điều hòa (${acTemp}°C)` : 'Tắt điều hòa',
        temp: acTemp,
        power: nextState,
        mode: 'cool',
        acBrand: iotAcBrandRef.current
      };
      setParsedCommand(acCmd);
      speakFeedback(acCmd.title);
      updateDeviceState('ac', nextState, nextState ? `Đang bật (${acTemp}°C)` : 'Đã tắt', { temp: acTemp });
      recordVoiceCommandLog({
        transcript: acCmd.title,
        action: acCmd.action,
        title: acCmd.title,
        device: 'ac',
        irCode: '0x00000000',
        source: 'manual_tap'
      }).catch(() => {});
      sendIotCommand(acCmd, iotEndpointRef.current).then((res) => {
        if (res.success) {
          setIsIotOnline(true);
          showNotification(`Đã gửi lệnh điều hòa: ${acCmd.title}`);
        }
      });
    } else {
      const actionName = `${nextState ? 'Bật' : 'Tắt'} ${device.name.toLowerCase()}`;
      speakFeedback(`Đã ${actionName}`);
      showNotification(`Đã ${actionName}`);
      recordVoiceCommandLog({
        transcript: actionName,
        action: nextState ? 'FAN_POWER_ON' : 'FAN_POWER_OFF',
        title: actionName,
        device: 'fan',
        irCode: '0x00000000',
        source: 'manual_tap'
      }).catch(() => {});
    }
  };

  // Gửi lệnh sang mạch ESP32
  const handleSendToIoT = async (cmd) => {
    if (!cmd || cmd.action === 'UNKNOWN') return;
    recordVoiceCommandLog({
      transcript: transcript || cmd.title,
      action: cmd.action,
      title: cmd.title,
      device: cmd.device || 'Smart TV',
      irCode: cmd.irCode || '0x00000000',
      source: 'voice'
    }).catch(() => {});
    try {
      const res = await sendIotCommand(cmd, iotEndpointRef.current);
      if (res.success) {
        setIsIotOnline(true);
        showNotification(`Đã phát tín hiệu IR [${cmd.irCode || cmd.title}]`);
      } else {
        showNotification(`ESP32: ${res.message || 'Lỗi gửi lệnh'}`);
      }
    } catch {
      showNotification(`Không kết nối được tới ESP32 (${iotEndpoint})`);
    }
  };

  // Xử lý câu lệnh giọng nói & Phân loại vào 3 thiết bị (Quạt, Điều hòa, TV)
  const processVoiceText = (text) => {
    if (!text || !text.trim()) return;
    const clean = text.trim();
    const lower = clean.toLowerCase();

    // 1. Kiểm tra lệnh Quạt
    if (lower.includes('quạt') || lower.includes('quat') || lower.includes('gió')) {
      const isOn = lower.includes('bật') || lower.includes('mở') || lower.includes('chạy') || lower.includes('bat');
      updateDeviceState('fan', isOn);
      speakFeedback(`Đã ${isOn ? 'bật' : 'tắt'} quạt`);
      showNotification(`Đã ${isOn ? 'bật' : 'tắt'} quạt`);
      return;
    }

    // 2. Kiểm tra lệnh Điều hòa / Máy lạnh
    if (lower.includes('điều hòa') || lower.includes('máy lạnh') || lower.includes('dieu hoa') || lower.includes('may lanh')) {
      const isOff = lower.includes('tắt') || lower.includes('tat') || lower.includes('đóng') || lower.includes('nghỉ') || lower.includes('dẹp');
      const isOn = !isOff;

      let targetTemp = acTemp;
      const tempMatch = clean.match(/(\d{2})\s*(?:độ|do|c)?/i);
      if (tempMatch && tempMatch[1]) {
        const num = parseInt(tempMatch[1], 10);
        if (num >= 16 && num <= 30) {
          targetTemp = num;
          setAcTemp(num);
        }
      } else if (lower.includes('tăng') || lower.includes('nóng')) {
        targetTemp = Math.min(30, acTemp + 1);
        setAcTemp(targetTemp);
      } else if (lower.includes('giảm') || lower.includes('lạnh hơn') || lower.includes('mát hơn')) {
        targetTemp = Math.max(16, acTemp - 1);
        setAcTemp(targetTemp);
      }

      updateDeviceState('ac', isOn, isOn ? `Đang bật (${targetTemp}°C)` : 'Đã tắt');
      const actionTitle = isOn ? `Đã bật điều hòa ${targetTemp} độ` : 'Đã tắt điều hòa';
      speakFeedback(actionTitle);
      showNotification(actionTitle);

      const acCmd = {
        device: 'ac',
        action: isOn ? 'AC_POWER_ON' : 'AC_POWER_OFF',
        title: actionTitle,
        temp: targetTemp,
        power: isOn,
        mode: 'cool',
        acBrand: iotAcBrandRef.current
      };
      setParsedCommand(acCmd);
      sendIotCommand(acCmd, iotEndpointRef.current).then((res) => {
        if (res.success) {
          setIsIotOnline(true);
        }
      });
      return;
    }

    // 3. Các lệnh TV & YouTube qua Fast-Path voiceCommandParser
    const tvCmd = parseVoiceCommand(clean);
    if (tvCmd && tvCmd.action !== 'UNKNOWN') {
      setParsedCommand(tvCmd);
      speakFeedback(tvCmd.title);

      if (tvCmd.action === 'TV_POWER_ON') {
        updateDeviceState('tv', true, 'Đang bật');
      } else if (tvCmd.action === 'TV_POWER_OFF') {
        updateDeviceState('tv', false, 'Đã tắt');
      } else if (tvCmd.action === 'APP_YOUTUBE' || tvCmd.action === 'YOUTUBE_SEARCH') {
        updateDeviceState('tv', true, 'Đang bật');
      }

      handleSendToIoT(tvCmd);
      return;
    }

    // 4. Nếu không khớp câu quen thuộc -> Gọi Gemini AI suy luận
    if (geminiApiKey) {
      analyzeVoiceWithGemini(clean, geminiApiKey)
        .then((aiRes) => {
          if (aiRes && aiRes.action && aiRes.action !== 'UNKNOWN') {
            const cmd = {
              action: aiRes.action,
              title: aiRes.title || 'Lệnh điều khiển TV',
              irCode: aiRes.irCode || '0x20DF10EF',
              channelQuery: aiRes.channelQuery || '',
              youtubeUrl: aiRes.action === 'YOUTUBE_SEARCH' && aiRes.channelQuery
                ? `https://www.youtube.com/results?search_query=${encodeURIComponent(aiRes.channelQuery)}`
                : ''
            };
            setParsedCommand(cmd);
            speakFeedback(cmd.title);
            if (cmd.action === 'TV_POWER_ON') updateDeviceState('tv', true, 'Đang bật');
            if (cmd.action === 'TV_POWER_OFF') updateDeviceState('tv', false, 'Đã tắt');
            handleSendToIoT(cmd);
          } else {
            speakFeedback('Chưa nhận diện được giọng nói');
            showNotification(`Chưa nhận diện được giọng nói: "${clean}"`);
          }
        })
        .catch(() => {
          speakFeedback('Chưa nhận diện được giọng nói');
          showNotification('Chưa nhận diện được giọng nói');
        });
      return;
    }

    speakFeedback('Chưa nhận diện được giọng nói');
    showNotification(`Chưa nhận diện được giọng nói: "${clean}"`);
  };

  const stopMicTest = () => {
    if (micTestControllerRef.current) {
      try { micTestControllerRef.current.stop(); } catch { }
      micTestControllerRef.current = null;
    }
    setIsTestingMic(false);
    setLiveVolume(0);
    setFrequencyBands(new Array(20).fill(0));
  };

  const startMicTest = async (deviceId) => {
    stopMicTest();
    setIsTestingMic(true);
    setTestRecordedAudioUrl('');
    setIsMicHardwareMuted(false);
    const controller = await testMicrophone(deviceId, {
      onVolume: (vol) => setLiveVolume(vol),
      onFrequencyBands: (bands) => setFrequencyBands(bands),
      onRecordedUrl: (url) => setTestRecordedAudioUrl(url),
      onMutedChange: (muted) => setIsMicHardwareMuted(muted),
      onError: (err) => {
        console.warn('Lỗi kiểm tra micro:', err);
        setIsTestingMic(false);
      }
    });
    micTestControllerRef.current = controller;
  };

  // Khởi tạo microphone & thiết bị
  useEffect(() => {
    const savedDeviceId = localStorage.getItem('selected_mic_device_id') || '';
    getAudioInputDevices().then((mics) => {
      setAudioDevices(mics);
      if (savedDeviceId && mics.some((m) => m.deviceId === savedDeviceId)) {
        setSelectedDeviceId(savedDeviceId);
      } else {
        const def = pickDefaultMicrophone(mics);
        if (def) {
          setSelectedDeviceId(def);
          localStorage.setItem('selected_mic_device_id', def);
        }
      }
    });
    return () => {
      stopMicTest();
      if (recognitionRef.current) {
        try { recognitionRef.current.abort(); } catch { }
      }
      if (audioCaptureRef.current) {
        try { audioCaptureRef.current.abort(); } catch { }
      }
    };
  }, []);

  // Khởi tạo & Lắng nghe Realtime từ Supabase Backend
  useEffect(() => {
    if (isSupabaseConfigured()) {
      fetchDevicesFromSupabase().then((serverDevices) => {
        if (serverDevices && serverDevices.length > 0) {
          setIsSupabaseOnline(true);
          setDevices((prev) =>
            prev.map((d) => {
              const remote = serverDevices.find((sd) => sd.id === d.id);
              return remote
                ? { ...d, active: remote.active, statusText: remote.statusText }
                : d;
            })
          );
          const acDevice = serverDevices.find((d) => d.id === 'ac');
          if (acDevice && acDevice.temp) {
            setAcTemp(acDevice.temp);
          }
        }
      });

      const unsubscribe = subscribeDeviceRealtime((updatedDevice) => {
        setIsSupabaseOnline(true);
        setDevices((prev) =>
          prev.map((d) =>
            d.id === updatedDevice.id
              ? { ...d, active: updatedDevice.active, statusText: updatedDevice.statusText }
              : d
          )
        );
        if (updatedDevice.id === 'ac' && updatedDevice.temp) {
          setAcTemp(updatedDevice.temp);
        }
      });

      return () => {
        unsubscribe();
      };
    }
  }, []);

  // Hàm thực thi kết quả phân tích từ Gemini AI
  const executeAiCommand = (aiRes) => {
    const cmd = {
      action: aiRes.action || 'UNKNOWN',
      title: aiRes.title || 'Lệnh điều khiển',
      irCode: aiRes.irCode || '0x20DF10EF',
      channelQuery: aiRes.channelQuery || '',
      youtubeUrl: aiRes.action === 'YOUTUBE_SEARCH' && aiRes.channelQuery
        ? `https://www.youtube.com/results?search_query=${encodeURIComponent(aiRes.channelQuery)}`
        : ''
    };
    setParsedCommand(cmd);
    speakFeedback(cmd.title);
    if (cmd.action === 'TV_POWER_ON') updateDeviceState('tv', true, 'Đang bật');
    if (cmd.action === 'TV_POWER_OFF') updateDeviceState('tv', false, 'Đã tắt');
    handleSendToIoT(cmd);
  };

  // Quản lý thu âm giọng nói với Dual-Pipeline (Web Speech API + Gemini AI Audio Fallback)
  const stopListening = async () => {
    if (!isListeningRef.current && !isStartingRef.current) return;
    isListeningRef.current = false;
    isStartingRef.current = false;
    setIsListening(false);
    setLiveVolume(0);
    setFrequencyBands(new Array(20).fill(0));

    if (silenceTimerRef.current) clearTimeout(silenceTimerRef.current);
    if (maxSessionTimerRef.current) clearTimeout(maxSessionTimerRef.current);

    if (recognitionRef.current) {
      try { recognitionRef.current.stop(); } catch { }
      recognitionRef.current = null;
    }

    // Lấy file âm thanh thu được từ MediaRecorder
    let recordedBlob = null;
    if (audioCaptureRef.current) {
      try {
        recordedBlob = await audioCaptureRef.current.stop();
      } catch (err) {
        console.warn('Lỗi stop audioCapture:', err);
      }
      audioCaptureRef.current = null;
    }

    // 1. FAST-PATH: Nếu Web Speech API đã nhận diện được câu nói của người dùng
    const spokenText = (currentTranscriptRef.current || interimText || transcript).trim();
    if (spokenText && !commandHandledRef.current) {
      commandHandledRef.current = true;
      setTranscript(spokenText);
      setInterimText('');
      processVoiceText(spokenText);
      return;
    }

    // 2. NẾU NGƯỜI DÙNG KHÔNG NÓI GÌ (âm lượng cả phiên < 8% hoặc không có tín hiệu nói)
    if (!spokenText && maxSessionVolumeRef.current < 8) {
      commandHandledRef.current = true;
      setTranscript('');
      setInterimText('');
      speakFeedback('Chưa nhận diện được giọng nói');
      showNotification('Chưa nhận diện được giọng nói');
      return;
    }

    // 3. ROBUST FALLBACK: Nếu có tiếng nói (volume >= 8%) nhưng Web Speech API không bắt được text, gửi sang Gemini AI
    if (!commandHandledRef.current && recordedBlob && recordedBlob.size > 500 && geminiApiKey) {
      showNotification('Đang gửi âm thanh sang AI để nhận diện...');
      try {
        const aiRes = await transcribeAndAnalyzeAudioWithGemini(recordedBlob, geminiApiKey);
        if (aiRes && aiRes.transcript && aiRes.transcript.trim()) {
          const aiText = aiRes.transcript.trim();
          setTranscript(aiText);
          setInterimText('');
          commandHandledRef.current = true;
          showNotification(`Đã nghe: "${aiText}"`);
          processVoiceText(aiText);
          return;
        } else {
          // AI không nhận diện được từ nào hoặc là âm thanh im lặng/không rõ
          commandHandledRef.current = true;
          setTranscript('');
          setInterimText('');
          speakFeedback('Chưa nhận diện được giọng nói');
          showNotification('Chưa nhận diện được giọng nói');
          return;
        }
      } catch (err) {
        console.warn('Lỗi Gemini audio transcribe:', err);
      }
    }

    // 4. Nếu không nhận diện được bất kỳ câu lệnh nào
    if (!commandHandledRef.current) {
      commandHandledRef.current = true;
      setTranscript('');
      setInterimText('');
      speakFeedback('Chưa nhận diện được giọng nói');
      showNotification('Chưa nhận diện được giọng nói');
    }
  };

  const startListening = async () => {
    if (isStartingRef.current || isListeningRef.current) return;
    isStartingRef.current = true;
    sessionIdRef.current += 1;
    const currentSession = sessionIdRef.current;

    setTranscript('');
    setInterimText('');
    currentTranscriptRef.current = '';
    commandHandledRef.current = false;
    maxSessionVolumeRef.current = 0;

    // 1. Khởi động Audio Analyser & MediaRecorder (vừa vẽ sóng âm, vừa ghi âm làm fallback)
    try {
      const capture = await startAudioCapture({
        deviceId: selectedDeviceId,
        onVolume: (vol) => {
          if (sessionIdRef.current === currentSession) {
            setLiveVolume(vol);
            if (vol > maxSessionVolumeRef.current) {
              maxSessionVolumeRef.current = vol;
            }
          }
        },
        onFrequencyBands: (bands) => {
          if (sessionIdRef.current === currentSession) {
            setFrequencyBands(bands);
          }
        },
        onMutedChange: (muted) => {
          if (muted) {
            showNotification('⚠️ Micro đang bị tắt tiếng (Mute) trong Windows hoặc phím Fn+F4!');
          }
        }
      });
      audioCaptureRef.current = capture;
    } catch (err) {
      console.warn('Lỗi mở microphone:', err);
      if (err.name === 'NotAllowedError' || err.name === 'PermissionDeniedError') {
        showNotification('⚠️ Vui lòng cấp quyền Microphone trên trình duyệt để sử dụng!');
        isStartingRef.current = false;
        return;
      }
    }

    isStartingRef.current = false;
    isListeningRef.current = true;
    setIsListening(true);

    // 2. Khởi động Web Speech API ở chế độ liên tục (không bị tự ngắt giữa chừng)
    const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
    if (SpeechRecognition) {
      try {
        const recognition = new SpeechRecognition();
        recognition.lang = 'vi-VN';
        recognition.continuous = true;
        recognition.interimResults = true;
        recognition.maxAlternatives = 1;

        recognition.onresult = (event) => {
          let interim = '';
          let final = '';

          for (let i = event.resultIndex; i < event.results.length; i++) {
            const transcriptText = event.results[i][0].transcript;
            if (event.results[i].isFinal) {
              final += transcriptText;
            } else {
              interim += transcriptText;
            }
          }

          if (interim) {
            setInterimText(interim);
            currentTranscriptRef.current = interim;
          }

          if (final) {
            setTranscript(final);
            setInterimText('');
            currentTranscriptRef.current = final;
          }

          // Khi người dùng nói xong và im lặng 1.4s -> Tự động chốt lệnh
          if (silenceTimerRef.current) clearTimeout(silenceTimerRef.current);
          silenceTimerRef.current = setTimeout(() => {
            if (isListeningRef.current && (currentTranscriptRef.current || final)) {
              stopListening();
            }
          }, 1400);
        };

        recognition.onerror = (event) => {
          console.warn('SpeechRecognition error:', event.error);
          if (event.error === 'not-allowed') {
            showNotification('⚠️ Vui lòng cấp quyền Microphone trên trình duyệt!');
          } else if (event.error === 'audio-capture') {
            showNotification('⚠️ Không thu được tín hiệu Microphone! Kiểm tra lại micro.');
          } else if (event.error === 'network') {
            console.log('Web Speech API lỗi mạng Google, tự động chuyển sang Gemini AI...');
          }
        };

        recognition.onend = () => {
          // Web speech kết thúc
        };

        recognitionRef.current = recognition;
        recognition.start();
      } catch (err) {
        console.warn('Không thể khởi chạy Web Speech API, sử dụng AI nhận diện âm thanh:', err);
      }
    }

    // Tự động dừng sau 6.5 giây nếu người dùng không nói
    maxSessionTimerRef.current = setTimeout(() => {
      if (isListeningRef.current && sessionIdRef.current === currentSession) {
        stopListening();
      }
    }, 6500);
  };

  const handleMicClick = () => {
    if (isListening) {
      stopListening();
    } else {
      startListening();
    }
  };

  // Render Lucide SVG Icons nhất quán cho 3 thiết bị (Quạt, Điều hòa, TV)
  const renderDeviceIcon = (id) => {
    switch (id) {
      case 'fan':
        return (
          <svg className="device-svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
            <path d="M10.8 16.4A6.1 6.1 0 0 1 2.2 9.4L7.6 11A6.1 6.1 0 0 1 14.6 2.4l-1.4 5.4a6.1 6.1 0 0 1 8.6 7l-5.4-1.6a6.1 6.1 0 0 1-7 8.6l1.4-5.4Z" />
            <circle cx="12" cy="12" r="2" />
          </svg>
        );
      case 'ac':
        return (
          <svg className="device-svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
            <rect x="2" y="4" width="20" height="9" rx="2" />
            <line x1="6" y1="9" x2="18" y2="9" />
            <path d="M7 17.5c.5.8 1.5 1.5 2.5 1.5" />
            <path d="M12 17.5v2" />
            <path d="M17 17.5c-.5.8-1.5 1.5-2.5 1.5" />
          </svg>
        );
      case 'tv':
      default:
        return (
          <svg className="device-svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
            <rect x="2" y="7" width="20" height="13" rx="2" />
            <polyline points="17 2 12 7 7 2" />
          </svg>
        );
    }
  };

  return (
    <div className="app-viewport">
      {/* 1. HEADER TINH TẾ & GỌN GÀNG */}
      <header className="app-header">
        <div className="header-brand">
          <div className="brand-icon-box">
            <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
              <path d="m3 9 9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z" />
              <polyline points="9 22 9 12 15 12 15 22" />
            </svg>
          </div>
          <span className="header-title">NHAN</span>
        </div>

        <div className="header-actions">
          <div className={`status-pill ${isSupabaseOnline ? 'online' : ''}`} title="Đồng bộ Cloud Supabase">
            <span className="status-dot" />
            <span>{isSupabaseOnline ? 'cloud sync' : 'local'}</span>
          </div>

          <div className={`status-pill ${isIotOnline ? 'online' : ''}`} title="Trạng thái kết nối ESP32">
            <span className="status-dot" />
            <span>{isIotOnline ? 'esp32' : 'offline'}</span>
          </div>

          <button
            type="button"
            className="header-icon-btn"
            onClick={() => setShowSettingsModal(true)}
            aria-label="Cài đặt hệ thống & Supabase"
          >
            <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
              <circle cx="12" cy="12" r="3" />
              <path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z" />
            </svg>
          </button>
        </div>
      </header>

      {/* THÔNG BÁO HÀNH ĐỘNG GỬI LỆNH (TOAST) */}
      {actionToast && (
        <div className="action-toast-banner" role="status">
          <span className="toast-text">{actionToast}</span>
          {parsedCommand?.youtubeUrl && (
            <a href={parsedCommand.youtubeUrl} target="_blank" rel="noopener noreferrer" className="toast-link">
              Mở Video
            </a>
          )}
        </div>
      )}

      {/* 2. MICROPHONE SECTION: TRỌNG TÂM THỊ GIÁC */}
      <section className="mic-hero-section">
        <div
          className={`mic-button-ring ${isListening ? 'listening' : ''}`}
          style={{
            transform: isListening
              ? `scale(${1 + Math.min(0.18, (liveVolume / 100) * 0.22)})`
              : 'scale(1)',
            boxShadow: isListening
              ? `0 0 ${16 + liveVolume * 0.4}px rgba(16, 185, 129, ${0.25 + (liveVolume / 100) * 0.45})`
              : undefined
          }}
        >
          <button
            type="button"
            className={`mic-primary-btn ${isListening ? 'listening' : ''}`}
            onClick={handleMicClick}
            aria-label={isListening ? 'Dừng lắng nghe' : 'Nhấn để nói'}
          >
            {isListening ? (
              <svg width="34" height="34" viewBox="0 0 24 24" fill="currentColor">
                <rect x="6" y="6" width="12" height="12" rx="2" />
              </svg>
            ) : (
              <svg width="36" height="36" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.2" strokeLinecap="round" strokeLinejoin="round">
                <path d="M12 2a3 3 0 0 0-3 3v7a3 3 0 0 0 6 0V5a3 3 0 0 0-3-3Z" />
                <path d="M19 10v2a7 7 0 0 1-14 0v-2" />
                <line x1="12" y1="19" x2="12" y2="22" />
              </svg>
            )}
          </button>
        </div>

        {/* Trạng thái chữ & Sóng âm giọng nói */}
        <div className="mic-status-container">
          {isListening ? (
            <div className="mic-listening-feedback">
              <h1 className="mic-heading-title listening">Đang nghe...</h1>

              {/* Sóng âm thanh 20 dải tần số Real-time Equalizer */}
              <div className="sound-wave-bars real-time-visualizer">
                {frequencyBands.map((band, idx) => {
                  const h = Math.max(6, Math.min(54, Math.round(6 + (band * 0.48))));
                  return (
                    <span
                      key={idx}
                      className={`wave-bar ${band > 8 ? 'active-voice' : ''}`}
                      style={{
                        height: `${h}px`,
                        opacity: band > 4 ? 1 : 0.65
                      }}
                    />
                  );
                })}
              </div>

              {/* Đồng hồ VU báo âm lượng & tín hiệu Micro tức thì */}
              <div className={`mic-live-vol-pill ${liveVolume > 5 ? 'active-signal' : 'waiting-signal'}`}>
                <span className="live-vol-dot" />
                {liveVolume > 5 ? (
                  <>
                    <span className="vol-text">Đang thu âm: <strong>{liveVolume}%</strong></span>
                    <div className="mini-vu-track">
                      <div className="mini-vu-bar" style={{ width: `${Math.min(100, liveVolume)}%` }} />
                    </div>
                  </>
                ) : (
                  <span className="vol-text-waiting">Đang nghe... Bạn hãy nói vào micro</span>
                )}
              </div>

              {interimText || transcript ? (
                <div className="mic-transcript-quote">“{interimText || transcript}”</div>
              ) : (
                <p className="mic-subtext">Hãy nói lệnh bạn muốn thực hiện</p>
              )}
            </div>
          ) : (
            <div className="mic-idle-feedback">
              <h1 className="mic-heading-title">Nhấn để nói</h1>
              <p className="mic-subtext">Ví dụ: Bật TV, bật quạt, mở YouTube...</p>
            </div>
          )}
        </div>
      </section>

      {/* ĐƯỜNG PHÂN CÁCH NHẸ NHÀNG */}
      <div className="section-divider" />

      {/* 3. LƯỚI 3 THIẾT BỊ TRONG NHÀ (QUẠT, ĐIỀU HÒA, TV) */}
      <section className="devices-section">
        <h2 className="section-heading">Thiết bị trong nhà</h2>

        <div className="devices-grid">
          {devices.map((device) => {
            const isActive = device.active;
            const isTv = device.id === 'tv';
            return (
              <div
                key={device.id}
                className={`device-card-item ${isTv ? 'tv-card-span' : ''} ${isActive ? 'active' : ''}`}
                onClick={() => toggleDevice(device)}
                role="button"
                tabIndex={0}
                aria-pressed={isActive}
              >
                <div className="device-card-content">
                  <div className="device-icon-container">
                    {renderDeviceIcon(device.id)}
                  </div>
                  <div className="device-info-container">
                    <span className="device-title-text">{device.name}</span>
                    <div className="device-status-indicator">
                      <span className="status-indicator-dot" />
                      <span>{device.statusText}</span>
                    </div>
                  </div>
                </div>

                {isTv && (
                  <div className="tv-power-badge">
                    {isActive ? 'Bật nguồn' : 'Tắt nguồn'}
                  </div>
                )}
              </div>
            );
          })}
        </div>
      </section>

      {/* 4. KHẨU LỆNH NHANH (LỆNH NHANH) */}
      <section className="quick-commands-section">
        <h2 className="section-heading">Lệnh nhanh</h2>
        <div className="quick-chips-container">
          {[
            'Bật TV',
            'Tắt TV',
            'Mở YouTube',
            'Bật điều hòa',
            'Điều hòa 26 độ',
            'Điều hòa 24 độ',
            'Tắt điều hòa',
            'Bật quạt',
            'Tắt quạt',
            'Tăng âm lượng'
          ].map((cmd, idx) => (
            <button
              key={idx}
              type="button"
              className="command-chip-btn"
              onClick={() => {
                setTranscript(cmd);
                processVoiceText(cmd);
              }}
            >
              {cmd}
            </button>
          ))}
        </div>
      </section>

      {/* 5. ESP32 SETTINGS MODAL (CẤU HÌNH ESP32 HIỆN ĐẠI, DỄ DÙNG) */}
      {showSettingsModal && (
        <div className="modal-overlay-backdrop" onClick={() => setShowSettingsModal(false)}>
          <div className="modal-dialog-card" onClick={(e) => e.stopPropagation()} role="dialog" aria-modal="true">
            <div className="modal-header-row">
              <div className="modal-title-group">
                <h3 className="modal-main-title">Cài đặt Hệ thống & Supabase</h3>
                <p className="modal-sub-desc">Cơ sở dữ liệu đám mây Supabase & điều khiển thiết bị</p>
              </div>
              <button
                type="button"
                className="modal-close-icon-btn"
                onClick={() => setShowSettingsModal(false)}
                aria-label="Đóng"
              >
                <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.2" strokeLinecap="round" strokeLinejoin="round">
                  <line x1="18" y1="6" x2="6" y2="18" />
                  <line x1="6" y1="6" x2="18" y2="18" />
                </svg>
              </button>
            </div>

            <div className="modal-form-body">
              {/* 1. Cấu hình Supabase Cloud Backend */}
              <div className="form-group-item supabase-config-card">
                <div className="form-item-header-row">
                  <label className="form-item-label">Cơ sở dữ liệu Supabase (Cloud Backend)</label>
                  <span className={`backend-status-tag ${isSupabaseOnline ? 'online' : 'offline'}`}>
                    {isSupabaseOnline ? '🟢 Đã kết nối' : '⚪ Chưa kết nối'}
                  </span>
                </div>

                <div className="form-vertical-inputs">
                  <div className="input-with-label">
                    <span className="input-micro-label">Project URL</span>
                    <input
                      type="text"
                      className="form-text-input"
                      value={supabaseUrl}
                      onChange={(e) => {
                        const val = e.target.value.trim().replace(/\/+$/, '');
                        setSupabaseUrl(val);
                        saveSupabaseConfig({ url: val, anonKey: supabaseAnonKey });
                      }}
                      placeholder="https://xyzcompany.supabase.co"
                    />
                  </div>
                  <div className="input-with-label">
                    <span className="input-micro-label">Anon Public API Key</span>
                    <input
                      type="password"
                      className="form-text-input"
                      value={supabaseAnonKey}
                      onChange={(e) => {
                        const val = e.target.value.trim();
                        setSupabaseAnonKey(val);
                        saveSupabaseConfig({ url: supabaseUrl, anonKey: val });
                      }}
                      placeholder="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
                    />
                  </div>
                </div>

                <div className="form-control-row" style={{ marginTop: '8px' }}>
                  <button
                    type="button"
                    className="form-action-btn full-width"
                    onClick={async () => {
                      const cleanUrl = supabaseUrl.trim().replace(/\/+$/, '');
                      const cleanKey = supabaseAnonKey.trim();
                      setSupabaseUrl(cleanUrl);
                      setSupabaseAnonKey(cleanKey);
                      saveSupabaseConfig({ url: cleanUrl, anonKey: cleanKey });

                      setIsTestingSupabase(true);
                      setSupabasePingResult('Đang kiểm tra kết nối Supabase...');
                      try {
                        const res = await testSupabaseConnection({ url: cleanUrl, anonKey: cleanKey });
                        setSupabasePingResult(res.message);
                        if (res.success && !res.tableMissing) {
                          setIsSupabaseOnline(true);
                          fetchDevicesFromSupabase().then((serverDevices) => {
                            if (serverDevices && serverDevices.length > 0) {
                              setDevices((prev) =>
                                prev.map((d) => {
                                  const remote = serverDevices.find((sd) => sd.id === d.id);
                                  return remote ? { ...d, active: remote.active, statusText: remote.statusText } : d;
                                })
                              );
                            }
                          });
                        } else if (res.tableMissing) {
                          setIsSupabaseOnline(true);
                        } else {
                          setIsSupabaseOnline(false);
                        }
                      } catch (e) {
                        setSupabasePingResult(`Lỗi: ${e.message}`);
                        setIsSupabaseOnline(false);
                      } finally {
                        setIsTestingSupabase(false);
                      }
                    }}
                    disabled={isTestingSupabase}
                  >
                    {isTestingSupabase ? 'Đang kiểm tra...' : '⚡ Kiểm tra kết nối Supabase'}
                  </button>
                </div>

                {supabasePingResult && (
                  <span className={`ping-status-note ${supabasePingResult.includes('thành công') ? 'success' : 'error'}`}>
                    {supabasePingResult}
                  </span>
                )}

                <div className="supabase-guide-box">
                  <span className="guide-title">📌 Cách lấy thông tin Supabase (miễn phí):</span>
                  <ol className="guide-steps">
                    <li>Vào trang <strong>supabase.com</strong> đăng nhập &gt; bấm <strong>New Project</strong>.</li>
                    <li>Vào <strong>Project Settings &gt; API</strong>: copy <strong>Project URL</strong> và <strong>anon public key</strong> dán vào 2 ô trên.</li>
                    <li>Vào mục <strong>SQL Editor</strong> &gt; dán toàn bộ file <code>supabase_schema.sql</code> và bấm <strong>Run</strong>.</li>
                  </ol>
                  <button
                    type="button"
                    className={`copy-sql-btn ${sqlCopied ? 'copied' : ''}`}
                    onClick={() => {
                      if (navigator?.clipboard?.writeText) {
                        navigator.clipboard.writeText(SUPABASE_SCHEMA_SQL);
                      }
                      setSqlCopied(true);
                      setTimeout(() => setSqlCopied(false), 3000);
                    }}
                  >
                    {sqlCopied ? '✅ Đã sao chép SQL! Hãy dán vào SQL Editor trên Supabase' : '📋 Sao chép mã SQL (supabase_schema.sql)'}
                  </button>
                </div>
              </div>

              {/* 2. Địa chỉ IP ESP32 */}
              <div className="form-group-item">
                <label className="form-item-label">Địa chỉ IP ESP32</label>
                <div className="form-control-row">
                  <input
                    type="text"
                    className="form-text-input"
                    value={iotEndpoint}
                    onChange={(e) => {
                      setIotEndpoint(e.target.value);
                      saveIotConfig({ endpoint: e.target.value, autoSend: iotAutoSend, tvBrand: iotTvBrand });
                    }}
                    placeholder="http://192.168.1.50"
                  />
                  <button
                    type="button"
                    className="form-action-btn"
                    onClick={async () => {
                      setIsPingingIot(true);
                      setIotPingResult('Đang kiểm tra...');
                      try {
                        const res = await testIotConnection(iotEndpoint);
                        if (res.success) {
                          setIotPingResult(`Đã kết nối (${res.ip || iotEndpoint})`);
                          setIsIotOnline(true);
                        } else {
                          setIotPingResult(`Không phản hồi: ${res.message}`);
                          setIsIotOnline(false);
                        }
                      } catch (e) {
                        setIotPingResult(`Lỗi: ${e.message}`);
                      } finally {
                        setIsPingingIot(false);
                      }
                    }}
                    disabled={isPingingIot}
                  >
                    {isPingingIot ? 'Kiểm tra...' : 'Kiểm tra'}
                  </button>
                </div>
                {iotPingResult && (
                  <span className={`ping-status-note ${iotPingResult.includes('Đã kết nối') ? 'success' : 'error'}`}>
                    {iotPingResult}
                  </span>
                )}
              </div>

              {/* Microphone Selection & Testing */}
              <div className="form-group-item">
                <label className="form-item-label">Microphone thu âm</label>
                <div className="form-control-row">
                  <select
                    className="form-select-input"
                    value={selectedDeviceId}
                    onChange={(e) => {
                      const newId = e.target.value;
                      setSelectedDeviceId(newId);
                      localStorage.setItem('selected_mic_device_id', newId);
                      if (isTestingMic) startMicTest(newId);
                    }}
                  >
                    {audioDevices.length > 0 ? (
                      audioDevices.map((d, i) => {
                        const lbl = d.label || `Microphone ${i + 1}`;
                        const isVirtual = /deskin|deskln|virtual|stereo mix|camo|droidcam|iriun|obs/i.test(lbl);
                        return (
                          <option key={d.deviceId || i} value={d.deviceId}>
                            {lbl} {isVirtual ? '⚠️ (Thiết bị ảo)' : ''}
                          </option>
                        );
                      })
                    ) : (
                      <option value="">Microphone mặc định hệ thống</option>
                    )}
                  </select>
                  <button
                    type="button"
                    className={`form-action-btn ${isTestingMic ? 'active-test' : ''}`}
                    onClick={() => {
                      if (isTestingMic) stopMicTest();
                      else startMicTest(selectedDeviceId);
                    }}
                  >
                    {isTestingMic ? `${liveVolume}% ⏹ Dừng` : '🎙️ Thử mic'}
                  </button>
                </div>

                {/* Cảnh báo micro bị mute phần cứng */}
                {isMicHardwareMuted && (
                  <div className="mic-muted-warning-alert">
                    ⚠️ <strong>Cảnh báo:</strong> Micro đang bị TẮT TIẾNG (MUTE) trong Windows hoặc trên bàn phím laptop (như phím F4 / Fn+F4). Vui lòng nhấn phím bật lại micro!
                  </div>
                )}

                {/* Visualizer sóng âm khi đang thử mic */}
                {isTestingMic && (
                  <div className="modal-test-visualizer">
                    <div className="sound-wave-bars modal-bars">
                      {frequencyBands.map((band, idx) => (
                        <span
                          key={idx}
                          className={`wave-bar ${band > 8 ? 'active-voice' : ''}`}
                          style={{ height: `${Math.max(4, Math.min(32, Math.round(4 + band * 0.28)))}px` }}
                        />
                      ))}
                    </div>
                    <span className="modal-vol-readout">
                      {liveVolume > 0 ? `Tín hiệu micro: ${liveVolume}% (Đang thu tốt)` : 'Đang thử... Bạn hãy nói thử "Alo 1 2 3" vào micro'}
                    </span>
                  </div>
                )}

                {/* Phát lại âm thanh vừa thử để người dùng kiểm chứng */}
                {testRecordedAudioUrl && !isTestingMic && (
                  <div className="test-audio-playback-card">
                    <div className="test-playback-header">
                      <span className="test-playback-title">🎧 Nghe lại đoạn vừa thử nghiệm:</span>
                      <button
                        type="button"
                        className="test-playback-clear-btn"
                        onClick={() => setTestRecordedAudioUrl('')}
                      >
                        Đóng
                      </button>
                    </div>
                    <audio controls src={testRecordedAudioUrl} className="test-playback-audio-player" />
                    <p className="test-playback-hint">
                      Bấm nút Play ▶️ bên trên. Nếu bạn nghe thấy rõ giọng nói của mình, micro này đã sẵn sàng để điều khiển giọng nói!
                    </p>
                  </div>
                )}

                {/* Hướng dẫn khắc phục nếu không có âm thanh */}
                <div className="mic-troubleshoot-guide">
                  <span className="mic-guide-title">💡 Mẹo khi không thu được tiếng:</span>
                  <ul className="mic-guide-list">
                    <li>Ưu tiên chọn <strong>Microphone Array (Realtek Audio)</strong> hoặc <strong>Default - Microphone</strong> trong danh sách.</li>
                    <li>Kiểm tra phím tắt micro trên bàn phím (Fn + F4 hoặc phím F4 có hình micro).</li>
                    <li>Vào Windows: <em>Cài đặt &gt; System &gt; Sound &gt; Input</em> và kiểm tra thanh âm lượng Micro (kéo lên 80-100%).</li>
                  </ul>
                </div>
              </div>

              {/* Hãng Điều Hòa */}
              <div className="form-group-item">
                <label className="form-item-label">Hãng Điều Hòa (Máy Lạnh)</label>
                <div className="form-control-row">
                  <select
                    className="form-select-input"
                    value={iotAcBrand}
                    onChange={(e) => {
                      setIotAcBrand(e.target.value);
                      saveIotConfig({
                        endpoint: iotEndpoint,
                        autoSend: iotAutoSend,
                        tvBrand: iotTvBrand,
                        acBrand: e.target.value
                      });
                    }}
                  >
                    <option value="Panasonic">Panasonic</option>
                    <option value="Daikin">Daikin</option>
                    <option value="Casper">Casper</option>
                    <option value="LG">LG</option>
                    <option value="Samsung">Samsung</option>
                    <option value="Gree">Gree</option>
                    <option value="Midea">Midea / Funiki</option>
                    <option value="Mitsubishi">Mitsubishi</option>
                    <option value="Toshiba">Toshiba</option>
                  </select>
                </div>
              </div>

              {/* Tự động phát IR khi nói xong */}
              <div className="form-switch-row">
                <div className="switch-text-group">
                  <span className="switch-title">Tự động phát IR khi nói xong</span>
                  <span className="switch-desc">Gửi tín hiệu tới TV ngay sau khi nhận diện giọng nói</span>
                </div>
                <label className="toggle-switch-control">
                  <input
                    type="checkbox"
                    checked={iotAutoSend}
                    onChange={(e) => {
                      setIotAutoSend(e.target.checked);
                      saveIotConfig({ endpoint: iotEndpoint, autoSend: e.target.checked, tvBrand: iotTvBrand });
                    }}
                  />
                  <span className="toggle-slider" />
                </label>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}

export default App;

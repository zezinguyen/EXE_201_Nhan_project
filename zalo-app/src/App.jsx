import React, { useState, useRef, useEffect } from 'react';
import './App.css';
import { analyzeVoiceWithGemini, transcribeAndAnalyzeAudioWithGemini } from './services/geminiService';
import { getIotConfig, saveIotConfig, sendIotCommand } from './services/iotService';
import { parseVoiceCommand } from './utils/voiceCommandParser';
import {
  getAudioInputDevices,
  pickDefaultMicrophone,
  testMicrophone,
  startAudioCapture
} from './utils/audioCaptureManager';
import {
  getSupabaseConfig,
  saveSupabaseConfig,
  isSupabaseConfigured,
  testSupabaseConnection
} from './services/supabaseClient';
import {
  fetchHomeData,
  pairBoardWithCode,
  fetchPendingLearnedCodes,
  assignLearnedCode,
  recordVoiceCommandLog,
  subscribeHomeRealtime,
  createHomeInvite,
  joinHomeWithInvite,
  removeHomeMember
} from './services/supabaseService';
import { SUPABASE_SCHEMA_SQL } from './services/supabaseSchema';


function App() {
  const [isListening, setIsListening] = useState(false);
  const [transcript, setTranscript] = useState('');
  const [interimText, setInterimText] = useState('');
  const [parsedCommand, setParsedCommand] = useState(null);
  const [actionToast, setActionToast] = useState('');
  const [showSettingsModal, setShowSettingsModal] = useState(false);
  const [showPairingModal, setShowPairingModal] = useState(false);
  const [showAssignModal, setShowAssignModal] = useState(false);
  const [showFamilyModal, setShowFamilyModal] = useState(false);

  // Thành viên gia đình cùng dùng chung mạch (Specs 4.4 Multi-user)
  const [homeMembers, setHomeMembers] = useState([
    {
      id: 'mem_owner',
      userId: '00000000-0000-0000-0000-0000000000aa',
      role: 'owner',
      displayName: 'Nguyễn Văn An',
      createdAt: new Date().toISOString()
    }
  ]);
  const [activeInvite, setActiveInvite] = useState({
    inviteCode: 'H84K92',
    role: 'member',
    expiresAt: new Date(Date.now() + 86400000).toISOString(),
    usedCount: 1,
    maxUses: 10
  });
  const [inviteLoading, setInviteLoading] = useState(false);
  const [joinInviteInput, setJoinInviteInput] = useState('');
  const [joinLoading, setJoinLoading] = useState(false);
  const [joinMessage, setJoinMessage] = useState(null);
  const [currentUserId] = useState(() => {
    let id = localStorage.getItem('smart_home_user_id');
    if (!id) {
      id = '00000000-0000-0000-0000-0000000000aa';
      localStorage.setItem('smart_home_user_id', id);
    }
    return id;
  });
  const [currentUserDisplayName, setCurrentUserDisplayName] = useState(() => {
    return localStorage.getItem('smart_home_user_name') || 'Nguyễn Văn An';
  });

  // Danh sách mạch phần cứng ESP32 trong nhà (Specs 4.4, Contracts 3.6)
  const [boards, setBoards] = useState([
    {
      id: 'board_living_01',
      name: 'Mạch IR Phòng Khách',
      room: 'Phòng khách',
      battery: 85,
      charging: false,
      usb: true,
      rssi: -58,
      firmware: '0.1.0',
      isOnline: true
    }
  ]);

  // Danh sách thiết bị trong nhà (Quạt, Điều hòa, TV)
  const [devices, setDevices] = useState([
    { id: 'tv_living', boardId: 'board_living_01', name: 'Tivi LG Phòng Khách', room: 'Phòng khách', type: 'tv', active: false, statusText: 'Đã tắt' },
    { id: 'ac_living', boardId: 'board_living_01', name: 'Điều hòa Panasonic', room: 'Phòng khách', type: 'ac', active: false, temp: 24, mode: 'cool', fan: 'auto', statusText: 'Đã tắt' }
  ]);

  // Remote ảo tự tạo & Mã học từ nút bấm (Specs 7.1)
  const [customRemotes, setCustomRemotes] = useState([]);
  const [pendingCodes, setPendingCodes] = useState([]);
  const [selectedCodeToAssign, setSelectedCodeToAssign] = useState(null);
  const [assignLabel, setAssignLabel] = useState('Chế độ phim');
  const [assignAction, setAssignAction] = useState('MOVIE_MODE');
  const [assignAliases, setAssignAliases] = useState('chế độ phim, bật rạp phim');

  // Ghép đôi mạch mới
  const [pairingInputCode, setPairingInputCode] = useState('');
  const [pairingBoardName, setPairingBoardName] = useState('Mạch IR Phòng Ngủ');
  const [pairingRoom, setPairingRoom] = useState('Phòng ngủ');
  const [pairingLoading, setPairingLoading] = useState(false);

  // Điều hòa State
  const [acTemp, setAcTemp] = useState(24);
  const [acMode, setAcMode] = useState('cool');

  // Cấu hình Supabase Backend
  const [supabaseUrl, setSupabaseUrl] = useState(() => getSupabaseConfig().url);
  const [supabaseAnonKey, setSupabaseAnonKey] = useState(() => getSupabaseConfig().anonKey);
  const [supabasePingResult, setSupabasePingResult] = useState('');
  const [isTestingSupabase, setIsTestingSupabase] = useState(false);
  const [isSupabaseOnline, setIsSupabaseOnline] = useState(() => isSupabaseConfigured());
  const [sqlCopied, setSqlCopied] = useState(false);

  // Cấu hình IoT
  const [iotAutoSend, setIotAutoSend] = useState(() => getIotConfig().autoSend);
  const [iotTvBrand, setIotTvBrand] = useState(() => getIotConfig().tvBrand);
  const [iotAcBrand, setIotAcBrand] = useState(() => getIotConfig().acBrand || 'Panasonic');

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

  const recognitionRef = useRef(null);
  const sessionIdRef = useRef(0);
  const isListeningRef = useRef(false);
  const isStartingRef = useRef(false);
  const commandHandledRef = useRef(false);
  const silenceTimerRef = useRef(null);
  const currentTranscriptRef = useRef('');
  const maxSessionTimerRef = useRef(null);
  const maxSessionVolumeRef = useRef(0);

  // Đọc phản hồi bằng giọng nói tiếng Việt
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

  // Tải dữ liệu ban đầu từ Supabase (Homes, Boards, Devices, Remotes, Pending Codes)
  const refreshHomeData = async () => {
    if (!isSupabaseConfigured()) return;
    try {
      const data = await fetchHomeData();
      if (data) {
        setIsSupabaseOnline(true);
        if (data.boards && data.boards.length > 0) setBoards(data.boards);
        if (data.devices && data.devices.length > 0) {
          setDevices(data.devices);
          const ac = data.devices.find(d => d.type === 'ac');
          if (ac) {
            setAcTemp(ac.temp || 24);
            setAcMode(ac.mode || 'cool');
          }
        }
        if (data.customRemotes) setCustomRemotes(data.customRemotes);
        if (data.homeMembers && data.homeMembers.length > 0) setHomeMembers(data.homeMembers);
        if (data.activeInvite) setActiveInvite(data.activeInvite);
      }
      const pending = await fetchPendingLearnedCodes();
      setPendingCodes(pending || []);
    } catch (err) {
      console.warn('Lỗi refreshHomeData:', err);
    }
  };

  useEffect(() => {
    refreshHomeData();

    // Đăng ký Supabase Realtime lắng nghe cập nhật thiết bị, mạch, mã học và thành viên
    const unsubscribe = subscribeHomeRealtime({
      onDeviceStateUpdated: (st) => {
        setDevices(prev => prev.map(d => {
          if (d.id === st.device_id) {
            return {
              ...d,
              active: Boolean(st.power),
              temp: st.temp || d.temp,
              mode: st.mode || d.mode,
              fan: st.fan || d.fan,
              statusText: st.power ? `Đang bật (${st.temp || 24}°C)` : 'Đã tắt'
            };
          }
          return d;
        }));
        if (st.device_id === 'ac_living') {
          if (st.temp) setAcTemp(st.temp);
          if (st.mode) setAcMode(st.mode);
        }
      },
      onBoardUpdated: (board) => {
        setBoards(prev => {
          const exists = prev.some(b => b.id === board.id);
          if (exists) {
            return prev.map(b => {
              if (b.id === board.id) {
                const isOnline = board.isOnline !== undefined
                  ? board.isOnline
                  : (board.last_seen_at ? (Date.now() - new Date(board.last_seen_at).getTime()) <= (6 * 60 * 1000) : false);
                return { ...b, ...board, isOnline };
              }
              return b;
            });
          }
          return [...prev, { name: 'Mạch IR ESP32', room: 'Phòng khách', ...board, isOnline: true }];
        });
      },
      onLearnEvent: (evt) => {
        if (evt.event === 'learn_progress') {
          showNotification(evt.status === 'first_captured' ? '📡 Đã nhận lần 1! Bấm lại lần 2 để xác nhận.' : '⚠️ Mã không khớp, vui lòng bấm lại.');
        } else if (evt.event === 'learn_result') {
          showNotification(evt.status === 'ok' ? '✅ Đã học mã thành công!' : '❌ Học lệnh thất bại.');
        } else if (evt.event === 'learn_timeout') {
          showNotification('⏳ Hết thời gian chờ học lệnh (15 giây).');
        }
      },
      onLearnedCodeAdded: (code) => {
        setPendingCodes(prev => [code, ...prev]);
        showNotification('⚡ Mạch vừa học được mã IR mới từ remote!');
      },
      onMembersUpdated: () => {
        refreshHomeData();
      }
    });

    return () => {
      if (typeof unsubscribe === 'function') unsubscribe();
    };
  }, []);

  // Xử lý tạo mã mời gia đình mới (Specs 4.4)
  const handleCreateNewInvite = async () => {
    setInviteLoading(true);
    try {
      const res = await createHomeInvite('00000000-0000-0000-0000-000000000001', 'member', currentUserId);
      if (res && res.success) {
        setActiveInvite({
          inviteCode: res.inviteCode,
          role: 'member',
          expiresAt: res.expiresAt,
          usedCount: 0,
          maxUses: 10
        });
        showNotification(`Đã tạo mã mời mới: ${res.inviteCode}`);
      } else {
        showNotification(res.message || 'Lỗi tạo mã mời');
      }
    } finally {
      setInviteLoading(false);
    }
  };

  const handleCopyInviteCode = () => {
    if (!activeInvite?.inviteCode) return;
    navigator.clipboard.writeText(activeInvite.inviteCode);
    showNotification(`Đã sao chép mã mời [${activeInvite.inviteCode}]!`);
  };

  const handleShareZaloInvite = () => {
    if (!activeInvite?.inviteCode) return;
    const shareText = `Mời bạn cùng tham gia điều khiển Nhà thông minh (Tivi & Điều hòa)! Nhập mã mời: ${activeInvite.inviteCode} trong Zalo Mini App.`;
    if (navigator.share) {
      navigator.share({
        title: 'Mời tham gia Nhà Thông Minh',
        text: shareText
      }).catch(() => {});
    } else {
      navigator.clipboard.writeText(shareText);
      showNotification('Đã sao chép nội dung mời Zalo vào bộ nhớ tạm!');
    }
  };

  const handleJoinHomeSubmit = async (e) => {
    e.preventDefault();
    if (!joinInviteInput.trim()) return;
    setJoinLoading(true);
    setJoinMessage(null);
    try {
      const res = await joinHomeWithInvite(joinInviteInput, currentUserId, currentUserDisplayName);
      if (res && res.success) {
        setJoinMessage({ success: true, text: res.message });
        showNotification(res.message);
        setJoinInviteInput('');
        refreshHomeData();
      } else {
        setJoinMessage({ success: false, text: res.message || 'Mã mời không hợp lệ hoặc đã hết hạn' });
      }
    } finally {
      setJoinLoading(false);
    }
  };

  const handleRemoveMemberClick = async (memberUserId, memberName) => {
    if (!window.confirm(`Bạn có chắc chắn muốn gỡ thành viên [${memberName}] khỏi nhà không? Người này sẽ lập tức mất quyền điều khiển mạch.`)) {
      return;
    }
    const res = await removeHomeMember('00000000-0000-0000-0000-000000000001', memberUserId);
    if (res && res.success) {
      showNotification(`Đã gỡ [${memberName}] thành công`);
      refreshHomeData();
    } else {
      showNotification(res.message || 'Lỗi khi gỡ thành viên');
    }
  };

  // Điều khiển TV
  const handleTvCommand = async (action, title) => {
    const cmd = {
      deviceId: 'tv_living',
      action,
      title,
      device: 'Smart TV (Hồng ngoại)'
    };
    setParsedCommand(cmd);
    speakFeedback(title);
    showNotification(`Đang gửi lệnh: ${title}...`);

    const res = await sendIotCommand(cmd);
    if (res.success) {
      showNotification(res.message);
      if (action === 'TV_POWER_ON') {
        setDevices(prev => prev.map(d => d.id === 'tv_living' ? { ...d, active: true, statusText: 'Đang bật' } : d));
      } else if (action === 'TV_POWER_OFF') {
        setDevices(prev => prev.map(d => d.id === 'tv_living' ? { ...d, active: false, statusText: 'Đã tắt' } : d));
      }
    } else {
      showNotification(`Lỗi: ${res.message}`);
    }
  };

  // Điều khiển Điều hòa (AC)
  const handleAcControl = async ({ power, temp, mode, fan }) => {
    const targetPower = power !== undefined ? power : true;
    const targetTemp = temp !== undefined ? temp : acTemp;
    const targetMode = mode !== undefined ? mode : acMode;
    const targetFan = fan || 'auto';

    if (temp !== undefined) setAcTemp(targetTemp);
    if (mode !== undefined) setAcMode(targetMode);

    const action = targetPower ? 'SET' : 'POWER_OFF';
    const title = targetPower ? `Bật điều hòa ${targetTemp}°C` : 'Tắt điều hòa';

    const cmd = {
      deviceId: 'ac_living',
      action,
      title,
      power: targetPower,
      temp: targetTemp,
      mode: targetMode,
      fan: targetFan,
      device: 'Điều hòa (Hồng ngoại)'
    };

    setParsedCommand(cmd);
    speakFeedback(title);
    showNotification(`Đang gửi: ${title}...`);

    const res = await sendIotCommand(cmd);
    if (res.success) {
      showNotification(res.message);
      setDevices(prev => prev.map(d => d.id === 'ac_living' ? {
        ...d,
        active: targetPower,
        temp: targetTemp,
        mode: targetMode,
        statusText: targetPower ? `Đang bật (${targetTemp}°C)` : 'Đã tắt'
      } : d));
    } else {
      showNotification(`Lỗi: ${res.message}`);
    }
  };

  // Bấm nút trên Remote ảo tự tạo
  const handleCustomButtonClick = async (remote, btn) => {
    const cmd = {
      deviceId: remote.deviceId || 'tv_living',
      action: btn.action || 'CUSTOM_ACTION',
      title: btn.label || 'Nút tuỳ chỉnh',
      device: remote.name || 'Remote ảo'
    };
    setParsedCommand(cmd);
    speakFeedback(cmd.title);
    showNotification(`Đang phát: ${cmd.title}...`);

    const res = await sendIotCommand(cmd);
    if (res.success) {
      showNotification(res.message);
    } else {
      showNotification(`Lỗi: ${res.message}`);
    }
  };

  // Ghép đôi mạch bằng mã 6 số
  const handlePairBoardSubmit = async (e) => {
    e.preventDefault();
    if (!pairingInputCode.trim()) {
      showNotification('Vui lòng nhập mã 6 số hiển thị trên OLED mạch!');
      return;
    }
    setPairingLoading(true);
    const res = await pairBoardWithCode(pairingInputCode, pairingBoardName, pairingRoom);
    setPairingLoading(false);
    if (res.success) {
      showNotification('Ghép đôi mạch ESP32 thành công!');
      setShowPairingModal(false);
      setPairingInputCode('');
      refreshHomeData();
    } else {
      showNotification(res.message || 'Mã ghép đôi không đúng hoặc hết hạn!');
    }
  };

  // Gán mã IR vừa học vào thiết bị
  const handleAssignCodeSubmit = async (e) => {
    e.preventDefault();
    if (!selectedCodeToAssign) return;

    const aliases = assignAliases.split(',').map(s => s.trim()).filter(Boolean);
    const res = await assignLearnedCode({
      codeId: selectedCodeToAssign.id,
      deviceId: 'tv_living',
      action: assignAction,
      label: assignLabel,
      voiceAliases: aliases
    });

    if (res.success) {
      showNotification('Đã gán mã IR thành công!');
      setShowAssignModal(false);
      setSelectedCodeToAssign(null);
      refreshHomeData();
    } else {
      showNotification(res.message || 'Lỗi khi gán mã IR');
    }
  };

  // Xử lý khẩu lệnh giọng nói với hỗ trợ Remote ảo và Điều hòa
  const processVoiceText = async (text) => {
    if (!text || !text.trim()) return;
    const clean = text.trim();

    // 1. Phân tích qua parser nội bộ với danh sách customRemotes
    const parsed = parseVoiceCommand(clean, customRemotes);
    if (parsed && parsed.action !== 'UNKNOWN') {
      setParsedCommand(parsed);
      speakFeedback(parsed.title);

      if (parsed.deviceId === 'ac_living' || parsed.action === 'POWER_OFF' || parsed.action === 'SET') {
        await handleAcControl({
          power: parsed.power,
          temp: parsed.temp,
          mode: parsed.mode
        });
      } else {
        await handleTvCommand(parsed.action, parsed.title);
      }
      return;
    }

    // 2. Dự phòng gọi Gemini AI
    if (geminiApiKey) {
      try {
        const aiRes = await analyzeVoiceWithGemini(clean, geminiApiKey);
        if (aiRes && aiRes.action && aiRes.action !== 'UNKNOWN') {
          const cmd = {
            deviceId: 'tv_living',
            action: aiRes.action,
            title: aiRes.title || 'Lệnh điều khiển TV'
          };
          setParsedCommand(cmd);
          speakFeedback(cmd.title);
          await handleTvCommand(cmd.action, cmd.title);
          return;
        }
      } catch { }
    }

    speakFeedback('Chưa nhận diện được giọng nói');
    showNotification(`Chưa nhận diện được: "${clean}"`);
  };

  // Quản lý Microphone & Nhận diện
  const startListening = async () => {
    if (isListeningRef.current || isStartingRef.current) return;
    isStartingRef.current = true;
    sessionIdRef.current += 1;
    const currentSession = sessionIdRef.current;

    setTranscript('');
    setInterimText('');
    setParsedCommand(null);
    commandHandledRef.current = false;
    currentTranscriptRef.current = '';

    try {
      const capture = await startAudioCapture({
        deviceId: selectedDeviceId,
        onVolumeChange: (vol) => {
          if (sessionIdRef.current === currentSession) setLiveVolume(vol);
        },
        onFftChange: (bands) => {
          if (sessionIdRef.current === currentSession) setFrequencyBands(bands);
        }
      });
      audioCaptureRef.current = capture;
    } catch { }

    setIsListening(true);
    isListeningRef.current = true;
    isStartingRef.current = false;

    // Web Speech API
    const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
    if (SpeechRecognition) {
      try {
        const recognition = new SpeechRecognition();
        recognition.lang = 'vi-VN';
        recognition.continuous = true;
        recognition.interimResults = true;

        recognition.onresult = (event) => {
          let interim = '';
          let final = '';
          for (let i = event.resultIndex; i < event.results.length; i++) {
            if (event.results[i].isFinal) final += event.results[i][0].transcript;
            else interim += event.results[i][0].transcript;
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
          if (silenceTimerRef.current) clearTimeout(silenceTimerRef.current);
          silenceTimerRef.current = setTimeout(() => {
            if (isListeningRef.current && (currentTranscriptRef.current || final)) {
              stopListening();
            }
          }, 1400);
        };

        recognition.onerror = () => {};
        recognitionRef.current = recognition;
        recognition.start();
      } catch { }
    }

    maxSessionTimerRef.current = setTimeout(() => {
      if (isListeningRef.current && sessionIdRef.current === currentSession) {
        stopListening();
      }
    }, 6500);
  };

  const stopListening = async () => {
    if (!isListeningRef.current) return;
    isListeningRef.current = false;
    setIsListening(false);

    if (silenceTimerRef.current) clearTimeout(silenceTimerRef.current);
    if (maxSessionTimerRef.current) clearTimeout(maxSessionTimerRef.current);

    if (recognitionRef.current) {
      try { recognitionRef.current.stop(); } catch { }
      recognitionRef.current = null;
    }

    let recordedBlob = null;
    if (audioCaptureRef.current) {
      try {
        const res = await audioCaptureRef.current.stop();
        recordedBlob = res?.blob;
      } catch { }
      audioCaptureRef.current = null;
    }

    setLiveVolume(0);
    setFrequencyBands(new Array(20).fill(0));

    const finalQuery = currentTranscriptRef.current || transcript || interimText;
    if (finalQuery.trim()) {
      processVoiceText(finalQuery);
    } else if (recordedBlob && geminiApiKey) {
      showNotification('Đang nhận diện giọng nói qua Gemini AI...');
      try {
        const aiRes = await transcribeAndAnalyzeAudioWithGemini(recordedBlob, geminiApiKey);
        if (aiRes && aiRes.transcript) {
          setTranscript(aiRes.transcript);
          processVoiceText(aiRes.transcript);
        }
      } catch { }
    }
  };

  const handleMicClick = () => {
    if (isListening) stopListening();
    else startListening();
  };

  const isAnyBoardOnline = boards.some(b => b.isOnline);

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
          <span className="header-title">SMARTHOME</span>
        </div>

        <div className="header-actions">
          <div className={`status-pill ${isSupabaseOnline ? 'online' : ''}`} title="Supabase Cloud">
            <span className="status-dot" />
            <span>{isSupabaseOnline ? 'cloud' : 'local'}</span>
          </div>

          <div className={`status-pill ${isAnyBoardOnline ? 'online' : ''}`} title="Mạch ESP32">
            <span className="status-dot" />
            <span>{isAnyBoardOnline ? 'online' : 'offline'}</span>
          </div>

          <button
            type="button"
            className="family-badge-btn"
            onClick={() => setShowFamilyModal(true)}
            aria-label="Thành viên gia đình"
            title="Quản lý thành viên dùng chung mạch"
          >
            <span style={{ fontSize: '16px' }}>👨‍👩‍👧</span>
            <span className="family-count-badge">{homeMembers.length}</span>
          </button>

          <button
            type="button"
            className="header-icon-btn"
            onClick={() => setShowSettingsModal(true)}
            aria-label="Cài đặt"
          >
            <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
              <circle cx="12" cy="12" r="3" />
              <path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z" />
            </svg>
          </button>
        </div>
      </header>

      {/* TOAST THÔNG BÁO */}
      {actionToast && (
        <div className="action-toast-banner" role="status">
          <span className="toast-text">{actionToast}</span>
        </div>
      )}

      {/* 2. THANH TRẠNG THÁI MẠCH PHẦN CỨNG ESP32 (Specs 4.4 Multi-board) */}
      <section className="boards-section">
        <div className="boards-scroll-container">
          {boards.map((b) => (
            <div key={b.id} className={`board-card-pill ${b.isOnline ? 'online' : 'offline'}`}>
              <div className="board-pill-header">
                <span className="board-pill-name">{b.name}</span>
                <span className={`board-pill-badge ${b.isOnline ? 'online' : 'offline'}`}>
                  {b.isOnline ? 'ONLINE' : 'OFFLINE'}
                </span>
              </div>
              <div className="board-pill-stats">
                <span className="board-pill-stat-item">
                  🔋 {b.battery}%{b.charging ? '⚡' : (b.usb ? '🔌' : '')}
                </span>
                <span className="board-pill-stat-item">
                  📶 {b.rssi}dBm
                </span>
                <span className="board-pill-stat-item">
                  📍 {b.room}
                </span>
              </div>
            </div>
          ))}

          <button
            type="button"
            className="add-board-card-btn"
            onClick={() => setShowPairingModal(true)}
            title="Ghép đôi mạch ESP32 mới bằng mã 6 số"
          >
            <span style={{ fontSize: '18px' }}>+</span>
            <span>Ghép mạch</span>
          </button>
        </div>
      </section>

      {/* 3. VÙNG MICRO TRỢ LÝ GIỌNG NÓI */}
      <section className="mic-hero-section">
        <div className="mic-button-wrapper">
          <button
            type="button"
            className={`mic-primary-btn ${isListening ? 'listening' : ''}`}
            onClick={handleMicClick}
            aria-label={isListening ? 'Đang lắng nghe' : 'Nhấn để ra lệnh'}
          >
            <svg className="mic-svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.4" strokeLinecap="round" strokeLinejoin="round">
              <path d="M12 2a3 3 0 0 0-3 3v7a3 3 0 0 0 6 0V5a3 3 0 0 0-3-3Z" />
              <path d="M19 10v2a7 7 0 0 1-14 0v-2" />
              <line x1="12" y1="19" x2="12" y2="22" />
            </svg>
          </button>
        </div>

        <div className="mic-status-container">
          {isListening ? (
            <div className="mic-active-feedback">
              <div className="audio-wave-visualizer">
                {frequencyBands.slice(0, 14).map((band, i) => {
                  const h = Math.max(4, Math.min(28, (band / 255) * 32));
                  return (
                    <div
                      key={i}
                      className={`wave-bar ${band > 8 ? 'active-voice' : ''}`}
                      style={{ height: `${h}px` }}
                    />
                  );
                })}
              </div>

              <div className="mic-live-vol-pill active-signal">
                <span className="vol-text">Đang nghe: {liveVolume > 5 ? `${liveVolume}%` : 'Bạn hãy nói...'}</span>
              </div>

              {interimText || transcript ? (
                <div className="mic-transcript-quote">“{interimText || transcript}”</div>
              ) : (
                <p className="mic-subtext">Nói: "Bật TV", "Mở YouTube", "Điều hòa 24 độ"...</p>
              )}
            </div>
          ) : (
            <div className="mic-idle-feedback">
              <h1 className="mic-heading-title">Nhấn để nói</h1>
              <p className="mic-subtext">Ví dụ: Bật TV, mở YouTube, điều hòa 24 độ...</p>
            </div>
          )}
        </div>
      </section>

      {/* 4. MÃ HỌC TỪ NÚT BẤM CHỜ GÁN (Specs 7.1 Cách 2) */}
      {pendingCodes.length > 0 && (
        <section className="pending-codes-box">
          <div className="pending-codes-header">
            <span className="pending-codes-title">⚡ Có {pendingCodes.length} mã IR vừa học từ nút bấm</span>
            <button
              type="button"
              className="assign-code-btn"
              onClick={() => {
                setSelectedCodeToAssign(pendingCodes[0]);
                setShowAssignModal(true);
              }}
            >
              Gán vào thiết bị
            </button>
          </div>
          <div className="pending-codes-list">
            {pendingCodes.slice(0, 2).map((pc) => (
              <div key={pc.id} className="pending-code-card">
                <span className="pending-code-info">
                  Protocol: <strong>{pc.ir?.protocol || 'RAW'}</strong> • Code: {pc.ir?.code || `${pc.ir?.raw?.length || 0} xung`}
                </span>
              </div>
            ))}
          </div>
        </section>
      )}

      {/* 5. ĐIỀU KHIỂN TIVI LG */}
      <section className="devices-section">
        <h2 className="section-heading">Tivi Phòng Khách</h2>
        <div className="quick-chips-container" style={{ marginTop: '8px' }}>
          <button type="button" className="command-chip-btn" onClick={() => handleTvCommand('TV_POWER_ON', 'Bật Tivi')}>
            📺 Bật TV
          </button>
          <button type="button" className="command-chip-btn" onClick={() => handleTvCommand('TV_POWER_OFF', 'Tắt Tivi')}>
            🔌 Tắt TV
          </button>
          <button type="button" className="command-chip-btn" onClick={() => handleTvCommand('APP_YOUTUBE', 'Mở YouTube')}>
            ▶️ YouTube
          </button>
          <button type="button" className="command-chip-btn" onClick={() => handleTvCommand('VOL_UP', 'Tăng âm lượng')}>
            🔊 To lên (+)
          </button>
          <button type="button" className="command-chip-btn" onClick={() => handleTvCommand('VOL_DOWN', 'Giảm âm lượng')}>
            🔉 Nhỏ lại (-)
          </button>
          <button type="button" className="command-chip-btn" onClick={() => handleTvCommand('MUTE', 'Tắt tiếng')}>
            🔇 Mute
          </button>
          <button type="button" className="command-chip-btn" onClick={() => handleTvCommand('CH_VTV1', 'Kênh VTV1')}>
            📡 VTV1
          </button>
          <button type="button" className="command-chip-btn" onClick={() => handleTvCommand('CH_VTV3', 'Kênh VTV3')}>
            📡 VTV3
          </button>
        </div>
      </section>

      {/* 6. ĐIỀU KHIỂN ĐIỀU HÒA PANASONIC (AC PANEL) */}
      <section className="ac-panel-card">
        <div className="ac-panel-header">
          <div>
            <h3 style={{ fontSize: '15px', fontWeight: '700' }}>Điều hòa Panasonic</h3>
            <span style={{ fontSize: '12px', color: 'var(--secondary-text)' }}>Chế độ làm lạnh IRac</span>
          </div>
          <button
            type="button"
            className="command-chip-btn"
            style={{ margin: 0, background: devices.find(d => d.id === 'ac_living')?.active ? 'var(--primary-green)' : '#E2E8F0', color: devices.find(d => d.id === 'ac_living')?.active ? '#fff' : '#475569' }}
            onClick={() => {
              const currentActive = devices.find(d => d.id === 'ac_living')?.active;
              handleAcControl({ power: !currentActive });
            }}
          >
            {devices.find(d => d.id === 'ac_living')?.active ? 'Đang Bật' : 'Đã Tắt'}
          </button>
        </div>

        <div className="ac-temp-controller">
          <button
            type="button"
            className="ac-stepper-btn"
            onClick={() => handleAcControl({ temp: Math.max(16, acTemp - 1) })}
          >
            -
          </button>
          <div className="ac-temp-display">
            <span className="ac-temp-number">{acTemp}</span>
            <span className="ac-temp-unit">độ C</span>
          </div>
          <button
            type="button"
            className="ac-stepper-btn"
            onClick={() => handleAcControl({ temp: Math.min(30, acTemp + 1) })}
          >
            +
          </button>
        </div>

        <div className="ac-modes-row">
          {[
            { id: 'cool', label: '❄️ Lạnh' },
            { id: 'dry', label: '💧 Hút ẩm' },
            { id: 'fan', label: '🌀 Quạt' },
            { id: 'auto', label: '🔄 Tự động' }
          ].map((m) => (
            <button
              key={m.id}
              type="button"
              className={`ac-mode-chip ${acMode === m.id ? 'active' : ''}`}
              onClick={() => handleAcControl({ mode: m.id })}
            >
              {m.label}
            </button>
          ))}
        </div>
      </section>

      {/* 7. REMOTE ẢO TỰ TẠO (CUSTOM REMOTES - Specs 7.1) */}
      {customRemotes.length > 0 && (
        <section className="devices-section">
          <h2 className="section-heading">Remote ảo của bạn</h2>
          {customRemotes.map((remote) => (
            <div key={remote.id} style={{ marginTop: '8px' }}>
              <div className="quick-chips-container">
                {remote.buttons.map((btn, idx) => (
                  <button
                    key={idx}
                    type="button"
                    className="command-chip-btn"
                    onClick={() => handleCustomButtonClick(remote, btn)}
                  >
                    ✨ {btn.label}
                  </button>
                ))}
              </div>
            </div>
          ))}
        </section>
      )}

      {/* MODAL: GHÉP ĐÔI MẠCH ESP32 (Specs 10.2, Contracts 7) */}
      {showPairingModal && (
        <div className="modal-overlay-backdrop" onClick={() => setShowPairingModal(false)}>
          <div className="modal-dialog-card" onClick={(e) => e.stopPropagation()}>
            <div className="modal-header-row">
              <div className="modal-title-group">
                <h3 className="modal-main-title">Ghép đôi Mạch ESP32</h3>
                <p className="modal-sub-desc">Nhập mã 6 số hiển thị trên màn hình OLED</p>
              </div>
              <button type="button" className="modal-close-icon-btn" onClick={() => setShowPairingModal(false)}>✕</button>
            </div>

            <form onSubmit={handlePairBoardSubmit} className="modal-form-body">
              <div className="input-with-label">
                <span className="input-micro-label">Mã Ghép Đôi (6 chữ số trên OLED)</span>
                <input
                  type="text"
                  maxLength={6}
                  required
                  className="form-text-input"
                  style={{ fontSize: '24px', letterSpacing: '4px', textAlign: 'center', fontWeight: 'bold' }}
                  value={pairingInputCode}
                  onChange={(e) => setPairingInputCode(e.target.value)}
                  placeholder="123456"
                />
              </div>

              <div className="input-with-label">
                <span className="input-micro-label">Tên gọi mạch</span>
                <input
                  type="text"
                  className="form-text-input"
                  value={pairingBoardName}
                  onChange={(e) => setPairingBoardName(e.target.value)}
                />
              </div>

              <div className="input-with-label">
                <span className="input-micro-label">Vị trí phòng</span>
                <input
                  type="text"
                  className="form-text-input"
                  value={pairingRoom}
                  onChange={(e) => setPairingRoom(e.target.value)}
                />
              </div>

              <button
                type="submit"
                disabled={pairingLoading}
                className="form-action-btn full-width"
                style={{ marginTop: '12px' }}
              >
                {pairingLoading ? 'Đang xác thực...' : 'Hoàn tất ghép đôi'}
              </button>
            </form>
          </div>
        </div>
      )}

      {/* MODAL: GÁN MÃ IR HỌC ĐƯỢC (Specs 7.1) */}
      {showAssignModal && selectedCodeToAssign && (
        <div className="modal-overlay-backdrop" onClick={() => setShowAssignModal(false)}>
          <div className="modal-dialog-card" onClick={(e) => e.stopPropagation()}>
            <div className="modal-header-row">
              <div className="modal-title-group">
                <h3 className="modal-main-title">Gán mã IR vào nút bấm</h3>
                <p className="modal-sub-desc">Mã: {selectedCodeToAssign.ir?.code || 'RAW'}</p>
              </div>
              <button type="button" className="modal-close-icon-btn" onClick={() => setShowAssignModal(false)}>✕</button>
            </div>

            <form onSubmit={handleAssignCodeSubmit} className="modal-form-body">
              <div className="input-with-label">
                <span className="input-micro-label">Tên nút hiển thị</span>
                <input
                  type="text"
                  required
                  className="form-text-input"
                  value={assignLabel}
                  onChange={(e) => setAssignLabel(e.target.value)}
                  placeholder="Ví dụ: Chế độ rạp phim"
                />
              </div>

              <div className="input-with-label">
                <span className="input-micro-label">Mã hành động (Action ID)</span>
                <input
                  type="text"
                  required
                  className="form-text-input"
                  value={assignAction}
                  onChange={(e) => setAssignAction(e.target.value)}
                  placeholder="Ví dụ: MOVIE_MODE"
                />
              </div>

              <div className="input-with-label">
                <span className="input-micro-label">Khẩu lệnh giọng nói (cách nhau bằng dấu phẩy)</span>
                <input
                  type="text"
                  className="form-text-input"
                  value={assignAliases}
                  onChange={(e) => setAssignAliases(e.target.value)}
                  placeholder="chế độ phim, bật rạp phim"
                />
              </div>

              <button type="submit" className="form-action-btn full-width" style={{ marginTop: '12px' }}>
                Lưu vào Remote ảo
              </button>
            </form>
          </div>
        </div>
      )}

      {/* MODAL: CÀI ĐẶT SUPABASE & DATABASE (SETTINGS) */}
      {showSettingsModal && (
        <div className="modal-overlay-backdrop" onClick={() => setShowSettingsModal(false)}>
          <div className="modal-dialog-card" onClick={(e) => e.stopPropagation()} role="dialog">
            <div className="modal-header-row">
              <div className="modal-title-group">
                <h3 className="modal-main-title">Cài đặt Hệ thống & Supabase</h3>
                <p className="modal-sub-desc">Cơ sở dữ liệu đám mây Supabase v3</p>
              </div>
              <button type="button" className="modal-close-icon-btn" onClick={() => setShowSettingsModal(false)}>✕</button>
            </div>

            <div className="modal-form-body">
              <div className="form-group-item supabase-config-card">
                <div className="form-item-header-row">
                  <label className="form-item-label">Cơ sở dữ liệu Supabase</label>
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
                      setIsTestingSupabase(true);
                      setSupabasePingResult('Đang kiểm tra kết nối...');
                      try {
                        const res = await testSupabaseConnection({ url: supabaseUrl, anonKey: supabaseAnonKey });
                        setSupabasePingResult(res.message);
                        if (res.success) {
                          setIsSupabaseOnline(true);
                          refreshHomeData();
                        }
                      } finally {
                        setIsTestingSupabase(false);
                      }
                    }}
                  >
                    {isTestingSupabase ? 'Đang kiểm tra...' : 'Kiểm tra kết nối'}
                  </button>
                </div>

                {supabasePingResult && (
                  <div className="ping-result-box" style={{ marginTop: '8px' }}>
                    {supabasePingResult}
                  </div>
                )}
              </div>

              {/* Hướng dẫn SQL Script */}
              <div className="form-group-item" style={{ marginTop: '12px' }}>
                <label className="form-item-label">Cài đặt CSDL Supabase (SQL Script)</label>
                <p style={{ fontSize: '12px', color: 'var(--secondary-text)', marginTop: '4px' }}>
                  Chạy script này trong Supabase SQL Editor để kích hoạt toàn bộ 12 bảng, RPC functions và Realtime.
                </p>
                <button
                  type="button"
                  className={`copy-sql-btn ${sqlCopied ? 'copied' : ''}`}
                  onClick={() => {
                    navigator.clipboard.writeText(SUPABASE_SCHEMA_SQL);
                    setSqlCopied(true);
                    setTimeout(() => setSqlCopied(false), 2500);
                  }}
                >
                  {sqlCopied ? '✓ Đã sao chép SQL!' : '📋 Sao chép mã SQL Schema'}
                </button>
              </div>
            </div>
          </div>
        </div>
      )}

      {/* MODAL: QUẢN LÝ THÀNH VIÊN GIA ĐÌNH & CHIA SẺ MẠCH (Specs 4.4) */}
      {showFamilyModal && (
        <div className="modal-overlay-backdrop family-members-modal" onClick={() => setShowFamilyModal(false)}>
          <div className="modal-dialog-card" onClick={(e) => e.stopPropagation()} role="dialog">
            <div className="modal-header-row">
              <div className="modal-title-group">
                <h3 className="modal-main-title">Gia Đình & Thành Viên</h3>
                <p className="modal-sub-desc">Cùng điều khiển mạch ESP32 & thiết bị trong nhà</p>
              </div>
              <button type="button" className="modal-close-icon-btn" onClick={() => setShowFamilyModal(false)}>✕</button>
            </div>

            <div className="modal-form-body">
              {/* Thẻ tóm tắt Nhà & Mạch ESP32 đang chia sẻ */}
              <div className="family-meta-card">
                <div className="family-home-header">
                  <span className="family-home-name">🏠 Nhà thông minh của tôi</span>
                  <span className={`backend-status-tag ${isAnyBoardOnline ? 'online' : 'offline'}`}>
                    {boards.length} Mạch ({boards.filter(b => b.isOnline).length} Online)
                  </span>
                </div>
                <p className="family-board-sync-note">
                  Tất cả thành viên trong danh sách dưới đây cùng điều khiển chung các mạch ESP32 trong nhà qua Supabase Cloud Realtime mà không cần cấu hình lại phần cứng.
                </p>
              </div>

              {/* Danh sách thành viên */}
              <div className="family-section-label">
                <span>Danh sách thành viên ({homeMembers.length})</span>
              </div>
              <div className="family-members-list">
                {homeMembers.map((m) => {
                  const isOwner = m.role === 'owner';
                  const isSelf = m.userId === currentUserId || m.displayName?.includes('Bạn');
                  return (
                    <div key={m.id || m.userId} className="family-member-row">
                      <div className="family-member-left">
                        <div className={`member-avatar-circle ${isOwner ? 'owner' : ''}`}>
                          {m.displayName ? m.displayName.charAt(0).toUpperCase() : 'U'}
                        </div>
                        <div className="member-details-col">
                          <div className="member-name-row">
                            <span className="member-display-name">{m.displayName}</span>
                            {isSelf && <span className="member-self-badge">Bạn</span>}
                          </div>
                          <span className={`member-role-badge ${m.role}`}>
                            {isOwner ? '👑 Chủ nhà (Owner)' : '👤 Thành viên (Member)'}
                          </span>
                        </div>
                      </div>

                      <div className="family-member-right">
                        {!isSelf && (
                          <button
                            type="button"
                            className="member-remove-action-btn"
                            onClick={() => handleRemoveMemberClick(m.userId, m.displayName)}
                            title="Gỡ thành viên khỏi nhà"
                          >
                            Gỡ
                          </button>
                        )}
                      </div>
                    </div>
                  );
                })}
              </div>

              {/* Hộp mã mời gia đình (Invite Code) */}
              <div className="invite-card-container">
                <div className="invite-card-title">Mời Người Thân Cùng Dùng Mạch</div>
                <p style={{ fontSize: '12px', color: '#64748B', margin: '0 0 6px 0' }}>
                  Gửi mã này cho người thân (vợ/chồng/con cái). Họ nhập mã trong app là có thể điều khiển ngay lập tức:
                </p>
                {activeInvite ? (
                  <>
                    <div className="invite-code-pill-row">
                      <div className="invite-code-box">{activeInvite.inviteCode}</div>
                    </div>
                    <div style={{ fontSize: '11px', color: '#64748B' }}>
                      Hiệu lực 24h • Đã tham gia: {activeInvite.usedCount || 0}/{activeInvite.maxUses || 10} người
                    </div>
                    <div className="invite-actions-flex">
                      <button
                        type="button"
                        className="invite-action-pill-btn primary"
                        onClick={handleCopyInviteCode}
                      >
                        📋 Sao chép mã
                      </button>
                      <button
                        type="button"
                        className="invite-action-pill-btn"
                        onClick={handleShareZaloInvite}
                      >
                        💬 Gửi qua Zalo
                      </button>
                      <button
                        type="button"
                        className="invite-action-pill-btn"
                        onClick={handleCreateNewInvite}
                        disabled={inviteLoading}
                      >
                        {inviteLoading ? 'Đang tạo...' : '🔄 Đổi mã'}
                      </button>
                    </div>
                  </>
                ) : (
                  <button
                    type="button"
                    className="invite-action-pill-btn primary"
                    onClick={handleCreateNewInvite}
                    disabled={inviteLoading}
                  >
                    {inviteLoading ? 'Đang tạo mã...' : 'Tạo mã mời gia đình'}
                  </button>
                )}
              </div>

              {/* Hộp tham gia nhà khác bằng mã mời */}
              <div className="join-home-card">
                <span className="family-section-label" style={{ marginBottom: '4px' }}>
                  Bạn được mời vào nhà người thân?
                </span>
                <p style={{ fontSize: '12px', color: 'var(--secondary-text)', margin: '0 0 8px 0' }}>
                  Nhập mã mời 6 chữ số/chữ cái nhận được từ chủ nhà để cùng điều khiển mạch:
                </p>
                <form onSubmit={handleJoinHomeSubmit} className="join-home-input-group">
                  <input
                    type="text"
                    maxLength={6}
                    required
                    className="join-home-input"
                    value={joinInviteInput}
                    onChange={(e) => setJoinInviteInput(e.target.value.toUpperCase())}
                    placeholder="VD: H84K92"
                  />
                  <button
                    type="submit"
                    className="join-home-btn"
                    disabled={joinLoading || joinInviteInput.length < 4}
                  >
                    {joinLoading ? 'Đang vào...' : 'Tham Gia'}
                  </button>
                </form>
                {joinMessage && (
                  <div className={`join-feedback-banner ${joinMessage.success ? 'success' : 'error'}`}>
                    {joinMessage.text}
                  </div>
                )}
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}

export default App;

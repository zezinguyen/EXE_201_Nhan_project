import React, { useEffect, useRef, useState } from 'react';
import {
  acSetupErrorText,
  cancelLearn,
  fetchBoardProfile,
  learnIrCode,
  saveLearnedCommand,
  saveTvPreset,
  tryIrCode,
} from '../services/profileSetupService';
import { TV_PRESETS, describeIrProtocol, findPresetByIr } from '../services/tvPresets';
import { TV_KEY_GROUPS, TV_KEYS } from '../services/tvKeys';
import { clearLearnedCodes, getLearnedCodes, recordLearnedCode, toPresetJson } from '../services/learnedCodeLog';

const keyHint = (label, status) => {
  if (status === 'first_captured') return `«${label}»: đã nhận lần 1 — bấm lại đúng phím đó lần nữa.`;
  if (status === 'mismatch') return `«${label}»: hai lần bấm khác nhau — bấm lại từ đầu.`;
  return `Hướng remote Tivi vào mạch rồi bấm phím «${label}».`;
};

// Nhớ hãng đã chọn trên máy này (mạch chỉ lưu mã, không lưu tên hãng)
const PRESET_KEY = 'tvPresetId';
function loadPresetId() {
  try {
    return localStorage.getItem(PRESET_KEY);
  } catch {
    return null;
  }
}
function storePresetId(id) {
  try {
    if (id) localStorage.setItem(PRESET_KEY, id);
    else localStorage.removeItem(PRESET_KEY);
  } catch {
    // Không có localStorage: chỉ mất phần ghi nhớ hãng
  }
}

/**
 * Cài Tivi: tự nhận từ phím Nguồn, chọn hãng có sẵn mã, hoặc học từng phím từ remote thật.
 * Lưu thẳng vào profile trên mạch; mã học được ghi lại trên máy này để xuất JSON.
 * Props: tvDevice { id, name }, onClose(), onNotify(msg)
 */
export default function TvSetupModal({ tvDevice, onClose, onNotify }) {
  const [loading, setLoading] = useState(true);
  const [loadError, setLoadError] = useState(null);
  const [learned, setLearned] = useState([]);
  const [presetId, setPresetId] = useState(loadPresetId);
  const [busy, setBusy] = useState(null); // 'try:<id>' | 'save:<id>' | 'learn:<action>' | 'detect'
  const [hint, setHint] = useState(null);
  const [detectInfo, setDetectInfo] = useState(null);
  const [sequence, setSequence] = useState(null); // Học lần lượt: { index, total }
  const [codes, setCodes] = useState(() => getLearnedCodes(tvDevice.id)); // Mã đã học trên máy này, để xuất JSON
  // Tăng mỗi lần Huỷ: kết quả của lần học đã huỷ (về muộn, VD hết giờ) bị bỏ qua
  const generationRef = useRef(0);
  const learningRef = useRef(false);

  useEffect(() => {
    let alive = true;
    fetchBoardProfile().then((res) => {
      if (!alive) return;
      setLoading(false);
      if (!res.ok) {
        setLoadError(acSetupErrorText(res.error));
        return;
      }
      const tv = res.devices.find((d) => d.id === tvDevice.id);
      setLearned(tv?.commands || []);
    });
    return () => {
      alive = false;
      // Cố ý đọc giá trị mới nhất: bộ đếm Huỷ, không phải DOM node
      // eslint-disable-next-line react-hooks/exhaustive-deps
      generationRef.current++;
      if (learningRef.current) cancelLearn();
    };
  }, [tvDevice.id]);

  const markLearned = (actions) => setLearned((prev) => [...new Set([...prev, ...actions])]);

  const tryPreset = async (preset) => {
    setBusy(`try:${preset.id}`);
    const res = await tryIrCode(preset.commands.POWER);
    setBusy(null);
    onNotify(res.ok ? `📡 Đã phát mã Nguồn ${preset.brand} — Tivi có bật/tắt không?` : `❌ ${acSetupErrorText(res.error)}`);
  };

  const applyPreset = async (preset) => {
    setBusy(`save:${preset.id}`);
    const res = await saveTvPreset({ deviceId: tvDevice.id, deviceName: tvDevice.name, preset });
    setBusy(null);
    if (!res.ok) {
      onNotify(`❌ ${acSetupErrorText(res.error)}`);
      return;
    }
    markLearned(Object.keys(preset.commands));
    setPresetId(preset.id);
    storePresetId(preset.id);
    onNotify(`✅ Đã lưu mã Tivi ${preset.brand}`);
  };

  /**
   * Học một mã trên mạch. Trả về { ok, ir } | { ok: false, error } | null nếu đã bị Huỷ.
   */
  const captureIr = async (label) => {
    const generation = generationRef.current;
    setHint(keyHint(label));
    learningRef.current = true;
    const res = await learnIrCode({ onProgress: (status) => setHint(keyHint(label, status)) });
    if (generation !== generationRef.current) return null;
    learningRef.current = false;
    setHint(null);
    return res;
  };

  // Lưu phím đã học lên mạch + ghi lại để xuất JSON. Trả về true nếu xong.
  const saveKey = async (key, ir) => {
    const saved = await saveLearnedCommand({ deviceId: tvDevice.id, deviceName: tvDevice.name, action: key.action, ir });
    if (!saved.ok) {
      onNotify(`❌ «${key.label}»: ${acSetupErrorText(saved.error)}`);
      return false;
    }
    markLearned([key.action]);
    setCodes(recordLearnedCode(tvDevice.id, key.action, ir));
    // Mã học tay có thể khác bộ mã của hãng đã chọn
    setPresetId(null);
    storePresetId(null);
    return true;
  };

  // Học một phím. Trả về true nếu xong, false nếu lỗi hoặc bị Huỷ.
  const learnKey = async (key) => {
    setBusy(`learn:${key.action}`);
    const res = await captureIr(key.label);
    if (res === null) return false;
    if (!res.ok) {
      setBusy(null);
      onNotify(`❌ «${key.label}»: ${acSetupErrorText(res.error)}`);
      return false;
    }
    const ok = await saveKey(key, res.ir);
    setBusy(null);
    return ok;
  };

  const learnOne = async (key) => {
    if (await learnKey(key)) onNotify(`✅ Đã học phím «${key.label}»`);
  };

  // Học lần lượt các phím chưa có mã; dừng khi lỗi hoặc Huỷ
  const learnAll = async () => {
    const queue = TV_KEYS.filter((k) => !learned.includes(k.action));
    for (let i = 0; i < queue.length; i++) {
      setSequence({ index: i + 1, total: queue.length });
      if (!(await learnKey(queue[i]))) {
        setSequence(null);
        return;
      }
    }
    setSequence(null);
    onNotify(`✅ Đã học xong ${queue.length} phím`);
  };

  // Học phím Nguồn: trùng mã một hãng có sẵn -> lưu cả bộ mã hãng đó; không trùng -> lưu riêng phím Nguồn
  const detect = async () => {
    setBusy('detect');
    const res = await captureIr('Nguồn');
    if (res === null) return;
    if (!res.ok) {
      setBusy(null);
      onNotify(`❌ ${acSetupErrorText(res.error)}`);
      return;
    }
    const preset = findPresetByIr(res.ir);
    if (preset) {
      setBusy(null);
      setDetectInfo(`Nhận ra Tivi ${preset.brand} (giao thức ${preset.protocol}).`);
      await applyPreset(preset);
      return;
    }
    const ok = await saveKey({ action: 'POWER', label: 'Nguồn' }, res.ir);
    setBusy(null);
    if (!ok) return;
    const code = res.ir?.code ? `, mã ${res.ir.code}` : '';
    setDetectInfo(
      `Đã nhận và lưu phím Nguồn: ${describeIrProtocol(res.ir)}${code}. Chưa có bộ mã sẵn cho hãng này — học thêm các phím khác ở Cách 3.`
    );
    onNotify('✅ Đã lưu phím Nguồn');
  };

  const stopLearn = () => {
    generationRef.current++;
    cancelLearn();
    learningRef.current = false;
    setHint(null);
    setBusy(null);
    setSequence(null);
  };

  const codesJson = toPresetJson(codes);

  const copyJson = async () => {
    try {
      await navigator.clipboard.writeText(codesJson);
      onNotify('📋 Đã sao chép JSON bộ mã');
    } catch {
      onNotify('Không sao chép được — bôi đen khung JSON rồi copy tay');
    }
  };

  const clearCodes = () => {
    clearLearnedCodes(tvDevice.id);
    setCodes({});
  };

  const disabled = busy !== null || loading || !!loadError;
  const currentPreset = TV_PRESETS.find((p) => p.id === presetId);
  const learnedCount = TV_KEYS.filter((k) => learned.includes(k.action)).length;
  const missingCount = TV_KEYS.length - learnedCount;

  return (
    <div className="modal-overlay-backdrop" onClick={onClose}>
      <div className="modal-dialog-card" onClick={(e) => e.stopPropagation()}>
        <div className="modal-header-row">
          <div className="modal-title-group">
            <h3 className="modal-main-title">Cài đặt Tivi</h3>
            <p className="modal-sub-desc">
              {tvDevice.name} • {currentPreset ? `Mã ${currentPreset.brand}` : 'Mã tự học'} •{' '}
              {loading ? '...' : `${learnedCount}/${TV_KEYS.length} phím`}
            </p>
          </div>
          <button type="button" className="modal-close-icon-btn" onClick={onClose}>✕</button>
        </div>

        <div className="modal-form-body">
          {loadError && <p className="ac-setup-error">{loadError}</p>}

          <div className="ac-setup-block">
            <span className="input-micro-label">Cách 1 — Tự nhận từ phím Nguồn trên remote</span>
            {busy === 'detect' ? (
              <>
                <p className="ac-setup-hint">{hint}</p>
                <button type="button" className="ac-setup-secondary-btn" onClick={stopLearn}>Huỷ</button>
              </>
            ) : (
              <button type="button" className="form-action-btn full-width" disabled={disabled} onClick={detect}>
                Bắt đầu nhận
              </button>
            )}
            {detectInfo && <p className="ac-setup-hint">{detectInfo}</p>}
          </div>

          <div className="ac-setup-block">
            <span className="input-micro-label">Cách 2 — Chọn hãng (bấm Thử, Tivi bật/tắt thì bấm Dùng)</span>
            <ul className="ac-setup-list">
              {TV_PRESETS.map((preset) => (
                <li key={preset.id} className={`ac-setup-row ${preset.id === presetId ? 'current' : ''}`}>
                  <span className="ac-setup-name">
                    {preset.brand}
                    <span className="ac-setup-status">Giao thức {preset.protocol}</span>
                  </span>
                  <button type="button" className="ac-setup-secondary-btn" disabled={disabled} onClick={() => tryPreset(preset)}>
                    {busy === `try:${preset.id}` ? '...' : 'Thử'}
                  </button>
                  <button
                    type="button"
                    className="ac-setup-primary-btn"
                    disabled={disabled || preset.id === presetId}
                    onClick={() => applyPreset(preset)}
                  >
                    {preset.id === presetId ? 'Đang dùng' : busy === `save:${preset.id}` ? '...' : 'Dùng'}
                  </button>
                </li>
              ))}
            </ul>
          </div>

          <div className="ac-setup-block">
            <span className="input-micro-label">Cách 3 — Học phím từ remote thật (mỗi phím bấm 2 lần)</span>
            {sequence ? (
              <button type="button" className="ac-setup-secondary-btn" onClick={stopLearn}>
                Dừng học lần lượt ({sequence.index}/{sequence.total})
              </button>
            ) : (
              <button
                type="button"
                className="form-action-btn full-width"
                disabled={disabled || missingCount === 0}
                onClick={learnAll}
              >
                {missingCount === 0 ? 'Đã có mã đủ mọi phím' : `Học lần lượt ${missingCount} phím chưa có`}
              </button>
            )}
            {hint && busy !== 'detect' && <p className="ac-setup-hint">{hint}</p>}

            {TV_KEY_GROUPS.map((group) => (
              <div key={group.title} className="ac-setup-block">
                <span className="ac-setup-group-title">{group.title}</span>
                <ul className="ac-setup-list">
                  {group.keys.map((key) => {
                    const done = learned.includes(key.action);
                    return (
                      <li key={key.action} className={`ac-setup-row ${done ? 'current' : ''}`}>
                        <span className="ac-setup-name">
                          {key.label}
                          <span className="ac-setup-status">{done ? 'Đã có mã' : 'Chưa có mã'}</span>
                        </span>
                        {busy === `learn:${key.action}` ? (
                          sequence ? (
                            <span className="ac-setup-status">Đang học...</span>
                          ) : (
                            <button type="button" className="ac-setup-secondary-btn" onClick={stopLearn}>Huỷ</button>
                          )
                        ) : (
                          <button
                            type="button"
                            className={done ? 'ac-setup-secondary-btn' : 'ac-setup-primary-btn'}
                            disabled={disabled}
                            onClick={() => learnOne(key)}
                          >
                            {done ? 'Học lại' : 'Học'}
                          </button>
                        )}
                      </li>
                    );
                  })}
                </ul>
              </div>
            ))}
          </div>

          {Object.keys(codes).length > 0 && (
            <div className="ac-setup-block">
              <span className="input-micro-label">
                Mã đã học trên máy này: {Object.keys(codes).length} phím (JSON — gửi để thêm bộ mã hãng)
              </span>
              <textarea className="ir-json-box" readOnly value={codesJson} rows={10} onFocus={(e) => e.target.select()} />
              <div className="ir-json-actions">
                <button type="button" className="ac-setup-primary-btn" onClick={copyJson}>Sao chép</button>
                <button type="button" className="ac-setup-secondary-btn" onClick={clearCodes}>Xoá</button>
              </div>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}

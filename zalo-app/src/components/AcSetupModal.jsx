import React, { useEffect, useRef, useState } from 'react';
import {
  acSetupErrorText,
  cancelLearn,
  detectAcProtocol,
  fetchBoardProfile,
  saveAcProtocol,
  tryAcProtocol,
} from '../services/profileSetupService';

const DETECT_HINTS = {
  waiting: 'Hướng remote điều hòa vào mạch rồi bấm nút Nguồn (hoặc đổi nhiệt độ).',
  first_captured: 'Đã nhận lần 1 — bấm lại đúng nút đó lần nữa.',
  mismatch: 'Hai lần bấm khác nhau — bấm lại từ đầu.',
};

/**
 * Chọn giao thức điều hòa: tự nhận từ remote thật, hoặc thử lần lượt rồi chọn.
 * Props: acDevice { id, name }, acState { temp, mode, fan }, onClose(), onSaved(protocol), onNotify(msg)
 */
export default function AcSetupModal({ acDevice, acState, onClose, onSaved, onNotify }) {
  const [loading, setLoading] = useState(true);
  const [loadError, setLoadError] = useState(null);
  const [current, setCurrent] = useState(null);
  const [candidates, setCandidates] = useState([]);
  const [busy, setBusy] = useState(null); // 'detect' | 'try:<p>' | 'save:<p>'
  const [detectHint, setDetectHint] = useState(null);
  const [detectResult, setDetectResult] = useState(null);
  const detectingRef = useRef(false);

  useEffect(() => {
    let alive = true;
    fetchBoardProfile().then((res) => {
      if (!alive) return;
      setLoading(false);
      if (!res.ok) {
        setLoadError(acSetupErrorText(res.error));
        return;
      }
      const ac = res.devices.find((d) => d.id === acDevice.id);
      setCurrent(ac?.acProtocol || null);
      setCandidates(res.acProtocols);
    });
    return () => {
      alive = false;
      if (detectingRef.current) cancelLearn();
    };
  }, [acDevice.id]);

  const save = async (protocol) => {
    setBusy(`save:${protocol}`);
    const res = await saveAcProtocol({ deviceId: acDevice.id, protocol, name: acDevice.name });
    setBusy(null);
    if (!res.ok) {
      onNotify(`❌ ${acSetupErrorText(res.error)}`);
      return false;
    }
    setCurrent(protocol);
    onSaved(protocol);
    onNotify(`✅ Đã lưu giao thức ${protocol} cho ${acDevice.name}`);
    return true;
  };

  const tryProtocol = async (protocol) => {
    setBusy(`try:${protocol}`);
    const res = await tryAcProtocol({ protocol, power: true, ...acState });
    setBusy(null);
    onNotify(res.ok ? `📡 Đã phát thử ${protocol} — điều hòa có phản ứng không?` : `❌ ${acSetupErrorText(res.error)}`);
  };

  const detect = async () => {
    setBusy('detect');
    setDetectResult(null);
    setDetectHint(DETECT_HINTS.waiting);
    detectingRef.current = true;
    const res = await detectAcProtocol({ onProgress: (status) => setDetectHint(DETECT_HINTS[status] || null) });
    detectingRef.current = false;
    setDetectHint(null);
    setBusy(null);
    if (!res.ok) {
      setDetectResult({ error: acSetupErrorText(res.error) });
      return;
    }
    setDetectResult(res);
    if (res.acSupported) await save(res.detected);
  };

  const stopDetect = () => {
    cancelLearn();
    detectingRef.current = false;
    setDetectHint(null);
    setBusy(null);
  };

  const isBusy = busy !== null;

  return (
    <div className="modal-overlay-backdrop" onClick={onClose}>
      <div className="modal-dialog-card" onClick={(e) => e.stopPropagation()}>
        <div className="modal-header-row">
          <div className="modal-title-group">
            <h3 className="modal-main-title">Giao thức điều hòa</h3>
            <p className="modal-sub-desc">
              {acDevice.name} • Đang dùng: <strong>{loading ? '...' : current || 'chưa đặt'}</strong>
            </p>
          </div>
          <button type="button" className="modal-close-icon-btn" onClick={onClose}>✕</button>
        </div>

        <div className="modal-form-body">
          {loadError && <p className="ac-setup-error">{loadError}</p>}

          <div className="ac-setup-block">
            <span className="input-micro-label">Cách 1 — Tự nhận từ remote thật</span>
            {busy === 'detect' ? (
              <>
                <p className="ac-setup-hint">{detectHint}</p>
                <button type="button" className="ac-setup-secondary-btn" onClick={stopDetect}>Huỷ</button>
              </>
            ) : (
              <button type="button" className="form-action-btn full-width" disabled={isBusy || !!loadError} onClick={detect}>
                Bắt đầu nhận giao thức
              </button>
            )}
            {detectResult?.error && <p className="ac-setup-error">{detectResult.error}</p>}
            {detectResult?.ok && !detectResult.acSupported && (
              <p className="ac-setup-hint">
                Mạch nhận ra <strong>{detectResult.detected}</strong> nhưng không điều khiển điều hòa bằng giao thức này
                được. Hãy thử Cách 2.
              </p>
            )}
          </div>

          <div className="ac-setup-block">
            <span className="input-micro-label">Cách 2 — Thử lần lượt, thấy điều hòa phản ứng thì chọn</span>
            <ul className="ac-setup-list">
              {candidates.map((protocol) => (
                <li key={protocol} className={`ac-setup-row ${protocol === current ? 'current' : ''}`}>
                  <span className="ac-setup-name">{protocol}</span>
                  <button
                    type="button"
                    className="ac-setup-secondary-btn"
                    disabled={isBusy}
                    onClick={() => tryProtocol(protocol)}
                  >
                    {busy === `try:${protocol}` ? '...' : 'Thử'}
                  </button>
                  <button
                    type="button"
                    className="ac-setup-primary-btn"
                    disabled={isBusy || protocol === current}
                    onClick={() => save(protocol)}
                  >
                    {protocol === current ? 'Đang dùng' : busy === `save:${protocol}` ? '...' : 'Dùng'}
                  </button>
                </li>
              ))}
            </ul>
          </div>
        </div>
      </div>
    </div>
  );
}

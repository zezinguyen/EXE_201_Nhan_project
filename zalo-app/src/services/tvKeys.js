/**
 * Phím remote Tivi: action là tên phím lưu trên mạch (profile `commands`) và gửi trong lệnh `command`.
 * Nguồn là một phím bật/tắt chung (POWER).
 */
export const TV_KEY_GROUPS = [
  {
    title: 'Cơ bản',
    keys: [
      { action: 'POWER', label: 'Nguồn' },
      { action: 'MUTE', label: 'Tắt tiếng' },
      { action: 'VOL_UP', label: 'Tăng âm lượng' },
      { action: 'VOL_DOWN', label: 'Giảm âm lượng' },
      { action: 'CH_UP', label: 'Kênh tiếp' },
      { action: 'CH_DOWN', label: 'Kênh trước' },
      { action: 'INPUT', label: 'Nguồn vào (Input)' },
    ],
  },
  {
    title: 'Điều hướng',
    keys: [
      { action: 'HOME', label: 'Home' },
      { action: 'MENU', label: 'Menu' },
      { action: 'LIST', label: 'List' },
      { action: 'BACK', label: 'Quay lại (Back)' },
      { action: 'UP', label: 'Lên' },
      { action: 'DOWN', label: 'Xuống' },
      { action: 'LEFT', label: 'Trái' },
      { action: 'RIGHT', label: 'Phải' },
      { action: 'OK', label: 'OK' },
    ],
  },
  {
    title: 'Phím số',
    keys: Array.from({ length: 10 }, (_, n) => ({ action: `NUM_${n}`, label: `Số ${n}` })),
  },
  {
    title: 'Ứng dụng',
    keys: [{ action: 'APP_YOUTUBE', label: 'YouTube' }],
  },
];

export const TV_KEYS = TV_KEY_GROUPS.flatMap((group) => group.keys);

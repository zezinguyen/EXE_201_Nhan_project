export const SUPABASE_SCHEMA_SQL = `-- ====================================================================
-- SMART HOME DATABASE SCHEMA FOR SUPABASE
-- Hướng dẫn: Mở Supabase Console -> SQL Editor -> Dán toàn bộ nội dung này và bấm RUN
-- ====================================================================

-- 1. BẢNG QUẢN LÝ THIẾT BỊ TRONG NHÀ (DEVICES)
CREATE TABLE IF NOT EXISTS public.devices (
    id TEXT PRIMARY KEY,                       -- 'fan', 'ac', 'tv'
    name TEXT NOT NULL,                        -- 'Quạt', 'Điều hòa', 'TV'
    active BOOLEAN NOT NULL DEFAULT FALSE,     -- true: Đang bật, false: Đã tắt
    status_text TEXT DEFAULT 'Đã tắt',         -- Mô tả trạng thái hiển thị
    temp INTEGER DEFAULT 26,                   -- Nhiệt độ điều hòa (16 - 30 độ C)
    mode TEXT DEFAULT 'cool',                  -- Chế độ điều hòa: 'cool', 'dry', 'fan'
    brand TEXT DEFAULT 'Panasonic',            -- Hãng thiết bị (Panasonic, LG, Daikin...)
    ir_code TEXT DEFAULT '0x00000000',         -- Mã hồng ngoại gần nhất
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT timezone('utc'::text, now()) NOT NULL
);

-- Khởi tạo sẵn 3 thiết bị mặc định
INSERT INTO public.devices (id, name, active, status_text, temp, mode, brand)
VALUES 
    ('fan', 'Quạt', FALSE, 'Đã tắt', NULL, NULL, 'Chung'),
    ('ac', 'Điều hòa', FALSE, 'Đã tắt', 26, 'cool', 'Panasonic'),
    ('tv', 'TV', FALSE, 'Đã tắt', NULL, NULL, 'LG')
ON CONFLICT (id) DO NOTHING;

-- 2. BẢNG LỊCH SỬ KHẨU LỆNH & ĐIỀU KHIỂN (VOICE_COMMAND_LOGS)
CREATE TABLE IF NOT EXISTS public.voice_command_logs (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    transcript TEXT,                           -- Câu người dùng nói
    action TEXT NOT NULL,                      -- Tên hành động: 'TV_POWER_ON', 'AC_TEMP_26', 'FAN_ON'...
    title TEXT,                                -- Tên mô tả ngắn gọn
    device TEXT,                               -- Thiết bị điều khiển
    ir_code TEXT,                              -- Mã IR đã phát
    source TEXT DEFAULT 'voice',               -- 'voice' hoặc 'manual_tap'
    created_at TIMESTAMP WITH TIME ZONE DEFAULT timezone('utc'::text, now()) NOT NULL
);

-- 3. BẢNG CẤU HÌNH NHÀ THÔNG MINH (HOME_SETTINGS)
CREATE TABLE IF NOT EXISTS public.home_settings (
    key TEXT PRIMARY KEY,                      -- 'esp32_endpoint', 'iot_auto_send', 'ac_brand'...
    value TEXT NOT NULL,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT timezone('utc'::text, now()) NOT NULL
);

INSERT INTO public.home_settings (key, value)
VALUES 
    ('esp32_endpoint', 'http://192.168.1.50'),
    ('iot_auto_send', 'true'),
    ('ac_brand', 'Panasonic'),
    ('tv_brand', 'LG')
ON CONFLICT (key) DO NOTHING;

-- ====================================================================
-- PHÂN QUYỀN TRUY CẬP (ROW LEVEL SECURITY - RLS)
-- Cho phép Zalo Mini App và ESP32 truy xuất qua Anon Key
-- ====================================================================
ALTER TABLE public.devices ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.voice_command_logs ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.home_settings ENABLE ROW LEVEL SECURITY;

-- Tạo chính sách cho phép đọc và ghi công khai qua anon key
DROP POLICY IF EXISTS "Public access devices" ON public.devices;
CREATE POLICY "Public access devices" ON public.devices FOR ALL USING (true) WITH CHECK (true);

DROP POLICY IF EXISTS "Public access logs" ON public.voice_command_logs;
CREATE POLICY "Public access logs" ON public.voice_command_logs FOR ALL USING (true) WITH CHECK (true);

DROP POLICY IF EXISTS "Public access settings" ON public.home_settings;
CREATE POLICY "Public access settings" ON public.home_settings FOR ALL USING (true) WITH CHECK (true);

-- ====================================================================
-- KÍCH HOẠT SUPABASE REALTIME (TỰ ĐỘNG ĐỒNG BỘ TỨC THÌ ĐA THIẾT BỊ)
-- ====================================================================
DO $$
BEGIN
    IF NOT EXISTS (
        SELECT 1 FROM pg_publication_tables 
        WHERE pubname = 'supabase_realtime' AND tablename = 'devices'
    ) THEN
        ALTER PUBLICATION supabase_realtime ADD TABLE public.devices;
    END IF;

    IF NOT EXISTS (
        SELECT 1 FROM pg_publication_tables 
        WHERE pubname = 'supabase_realtime' AND tablename = 'voice_command_logs'
    ) THEN
        ALTER PUBLICATION supabase_realtime ADD TABLE public.voice_command_logs;
    END IF;
END $$;
`;

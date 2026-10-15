#pragma once
// Mẫu bí mật. Copy thành include/secrets.h (đã .gitignore) rồi điền giá trị thật.
// KHÔNG BAO GIỜ đặt service_role key ở đây – firmware chỉ dùng anon key + token riêng của mạch.

#define SUPABASE_HOST "your-project-ref.supabase.co"
#define SUPABASE_ANON_KEY "your-anon-key"

// CA gốc của *.supabase.co (PEM). Không dùng setInsecure().
#define SUPABASE_ROOT_CA \
    "-----BEGIN CERTIFICATE-----\n" \
    "...\n" \
    "-----END CERTIFICATE-----\n"

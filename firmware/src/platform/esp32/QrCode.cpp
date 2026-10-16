#include "modules/QrCode.h"

#include <qrcode.h>  // esp_qrcode của ESP-IDF (có sẵn trong SDK, không cần thư viện ngoài)

namespace {
// esp_qrcode_generate gọi display_func không kèm ngữ cảnh -> giữ tạm ở đây (chỉ gọi từ loop chính)
QrPlotFn currentPlot = nullptr;
void* currentCtx = nullptr;
int currentSize = 0;

void plotModules(esp_qrcode_handle_t qrcode) {
    currentSize = esp_qrcode_get_size(qrcode);
    for (int y = 0; y < currentSize; y++) {
        for (int x = 0; x < currentSize; x++) {
            if (esp_qrcode_get_module(qrcode, x, y)) {
                currentPlot(currentCtx, currentSize, x, y);
            }
        }
    }
}
}  // namespace

int renderQrCode(const char* text, int maxVersion, QrPlotFn plot, void* ctx) {
    if (text == nullptr || plot == nullptr) {
        return 0;
    }
    currentPlot = plot;
    currentCtx = ctx;
    currentSize = 0;
    esp_qrcode_config_t config = ESP_QRCODE_CONFIG_DEFAULT();
    config.display_func = plotModules;
    config.max_qrcode_version = maxVersion;
    config.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;  // Màn hình nét, không bẩn: ưu tiên ô to (version nhỏ)
    const int size = esp_qrcode_generate(&config, text) == ESP_OK ? currentSize : 0;
    currentPlot = nullptr;
    currentCtx = nullptr;
    return size;
}

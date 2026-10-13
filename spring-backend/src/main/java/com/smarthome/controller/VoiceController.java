package com.smarthome.controller;

import org.springframework.web.bind.annotation.*;
import org.springframework.web.multipart.MultipartFile;
import org.springframework.http.ResponseEntity;
import org.springframework.http.HttpHeaders;
import org.springframework.http.MediaType;
import org.springframework.http.HttpEntity;
import org.springframework.util.MultiValueMap;
import org.springframework.util.LinkedMultiValueMap;
import org.springframework.web.client.RestTemplate;
import java.util.Map;
import java.util.HashMap;

@RestController
@RequestMapping("/api/voice")
@CrossOrigin(origins = "*") // Cho phép Zalo Mini App gọi API (CORS)
public class VoiceController {

    @PostMapping("/recognize")
    public ResponseEntity<Map<String, String>> recognizeAudio(@RequestParam("audio") MultipartFile audioFile) {
        System.out.println("Nhận được file âm thanh từ Zalo App: " + audioFile.getOriginalFilename());
        System.out.println("Kích thước file: " + audioFile.getSize() + " bytes");

        String recognizedText = "Không nhận diện được giọng nói";
        
        try {
            // TODO: BẠN CẦN THAY API KEY CỦA BẠN VÀO ĐÂY (Lấy từ console.fpt.ai)
            String FPT_API_KEY = "YOUR_FPT_AI_API_KEY"; 
            
            // Gọi API FPT.AI (Hoặc bạn có thể test chạy giả lập bằng cách cmt đoạn code này lại)
            /* 
            RestTemplate restTemplate = new RestTemplate();
            HttpHeaders headers = new HttpHeaders();
            headers.setContentType(MediaType.MULTIPART_FORM_DATA);
            headers.set("api-key", FPT_API_KEY);

            MultiValueMap<String, Object> body = new LinkedMultiValueMap<>();
            body.add("file", audioFile.getResource());

            HttpEntity<MultiValueMap<String, Object>> requestEntity = new HttpEntity<>(body, headers);
            
            ResponseEntity<String> response = restTemplate.postForEntity(
                "https://api.fpt.ai/hmi/asr/general", 
                requestEntity, 
                String.class
            );

            // Parse kết quả JSON trả về từ FPT (tuỳ thuộc format FPT)
            // Tạm thời in ra màn hình để bạn xem kết quả thô
            System.out.println("FPT AI Trả về: " + response.getBody());
            recognizedText = "Đã gọi FPT AI thành công (Xem log)";
            */
            
            // Nếu chưa cấu hình API Key:
            recognizedText = "Không nhận diện được giọng nói";

        } catch (Exception e) {
            e.printStackTrace();
            recognizedText = "Lỗi khi gọi AI";
        }
        
        System.out.println("AI nhận diện được: " + recognizedText);

        // Trả kết quả về cho React
        Map<String, String> response = new HashMap<>();
        response.put("text", recognizedText);
        
        return ResponseEntity.ok(response);
    }
}

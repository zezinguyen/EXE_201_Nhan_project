package com.smarthome.controller;

import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.multipart.MultipartFile;

import java.util.HashMap;
import java.util.Map;

@RestController
@RequestMapping("/api/voice")
@CrossOrigin(origins = "*") // Cho phép Zalo Mini App gọi API (CORS)
public class VoiceController {

    @GetMapping("/status")
    public ResponseEntity<Map<String, String>> getStatus() {
        Map<String, String> response = new HashMap<>();
        response.put("status", "online");
        response.put("message", "Smart Home Voice API Service is active");
        return ResponseEntity.ok(response);
    }

    @PostMapping("/recognize")
    public ResponseEntity<Map<String, String>> recognizeAudio(@RequestParam("audio") MultipartFile audioFile) {
        System.out.println("Nhận được file âm thanh từ Client: " + audioFile.getOriginalFilename());
        System.out.println("Kích thước file: " + audioFile.getSize() + " bytes");

        // Hệ thống hiện tại nhận diện và xử lý qua Gemini AI trực tiếp từ Zalo Mini App.
        // Endpoint này đóng vai trò endpoint dự phòng / health check.
        Map<String, String> response = new HashMap<>();
        response.put("status", "success");
        response.put("message", "Audio received successfully");
        response.put("filename", audioFile.getOriginalFilename());

        return ResponseEntity.ok(response);
    }
}

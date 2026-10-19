#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdint>
#include <map>
#include <algorithm>

extern "C" {
#include "jpeglib.h"
}

struct ImageMetadata {
    int width = 0;
    int height = 0;
    int components = 0;
    int precision = 0;
    std::string color_space = "Unknown";
    bool progressive = false;
    std::string comment = "";
    std::map<std::string, int> markers;
};

void get_jpeg_info(const std::string& filepath, ImageMetadata& meta) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE* infile = fopen(filepath.c_str(), "rb");
    if (!infile) throw std::runtime_error("Cannot open input file");

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);
    jpeg_read_header(&cinfo, TRUE);
    
    meta.width = cinfo.image_width;
    meta.height = cinfo.image_height;
    meta.components = cinfo.num_components;
    meta.precision = cinfo.data_precision;
    meta.progressive = cinfo.progressive_mode;

    switch(cinfo.jpeg_color_space) {
        case JCS_GRAYSCALE: meta.color_space = "Grayscale"; break;
        case JCS_RGB: meta.color_space = "RGB"; break;
        case JCS_YCbCr: meta.color_space = "YCbCr"; break;
        case JCS_CMYK: meta.color_space = "CMYK"; break;
        case JCS_YCCK: meta.color_space = "YCCK"; break;
        default: meta.color_space = "Unknown"; break;
    }

    jpeg_destroy_decompress(&cinfo);
    fclose(infile);
}

void scan_markers(const std::string& filepath, ImageMetadata& meta) {
    std::ifstream file(filepath, std::ios::binary);
    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    for (size_t i = 0; i < buffer.size() - 1; ++i) {
        if (buffer[i] == 0xFF && buffer[i+1] != 0x00 && buffer[i+1] != 0xFF) {
            uint8_t marker = buffer[i+1];
            std::string name = "";
            switch(marker) {
                case 0xD8: name = "SOI"; break; case 0xD9: name = "EOI"; break;
                case 0xC0: name = "SOF0"; break; case 0xC2: name = "SOF2"; break;
                case 0xC4: name = "DHT"; break; case 0xDA: name = "SOS"; break;
                case 0xDB: name = "DQT"; break; case 0xE0: name = "APP0"; break;
                case 0xE1: name = "APP1"; break; case 0xE2: name = "APP2"; break;
                case 0xFE: name = "COM"; break;
            }
            if (!name.empty()) meta.markers[name]++;
            i++; 
        }
    }
}

// ИСПРАВЛЕНИЕ 1: Безопасная работа с памятью
std::string extract_comment_manual(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    for (size_t i = 0; i < buffer.size() - 1; ++i) {
        if (buffer[i] == 0xFF && buffer[i+1] == 0xFE) {
            if (i + 4 > buffer.size()) break;
            
            uint16_t seg_len = (buffer[i+2] << 8) | buffer[i+3];
            uint16_t comment_len = seg_len - 2;

            // ПРОВЕРКА ГРАНИЦ: убеждаемся, что comment_len не превышает реальные данные
            // И ограничиваем разумным максимумом, чтобы избежать DoS
            if (comment_len > 0 && comment_len <= 4096 && (i + 4 + comment_len <= buffer.size())) {
                // Безопасное копирование: выделяем ровно столько, сколько нужно
                std::string result(reinterpret_cast<const char*>(&buffer[i+4]), comment_len);
                return result;
            }
        }
    }
    return "";
}

// ИСПРАВЛЕНИЕ 2: Защита от Path Traversal
void save_report(const std::string& user_filename, const ImageMetadata& meta) {
    // Санитизация: удаляем любые символы '/', '\' и последовательности '..'
    std::string safe_filename = user_filename;
    size_t pos;
    while ((pos = safe_filename.find("..")) != std::string::npos) {
        safe_filename.erase(pos, 2);
    }
    while ((pos = safe_filename.find("/")) != std::string::npos) {
        safe_filename.erase(pos, 1);
    }
    while ((pos = safe_filename.find("\\")) != std::string::npos) {
        safe_filename.erase(pos, 1);
    }

    if (safe_filename.empty()) {
        std::cerr << "Invalid filename after sanitization.\n";
        return;
    }

    std::string out_path = "./output/" + safe_filename;
    std::ofstream out_file(out_path);
    if (!out_file) {
        std::cerr << "Failed to open output file: " << out_path << "\n";
        return;
    }
    
    out_file << "Image Metadata Report\n=====================\n";
    out_file << "Width: " << meta.width << " px\nHeight: " << meta.height << " px\n";
    out_file << "Color Space: " << meta.color_space << "\nProgressive: " << (meta.progressive ? "Yes" : "No") << "\n\n";
    out_file << "Comment: " << (meta.comment.empty() ? "Not found" : meta.comment) << "\n";
    out_file.close();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <image.jpg> [-o <output_report.txt>]\n";
        return 1;
    }

    std::string input_file = argv[1];
    std::string output_file = "";
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "-o" && i + 1 < argc) {
            output_file = argv[i+1];
            ++i;
        }
    }

    try {
        ImageMetadata meta;
        get_jpeg_info(input_file, meta);
        scan_markers(input_file, meta);
        meta.comment = extract_comment_manual(input_file);

        std::cout << "Image dimensions: " << meta.width << "x" << meta.height << "\n";
        if (!output_file.empty()) {
            save_report(output_file, meta);
            std::cout << "Report saved to " << output_file << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
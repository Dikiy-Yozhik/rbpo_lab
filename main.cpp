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

// Структура для хранения всех метаданных
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

// Получаем базовые и расширенные метаданные через libjpeg
void get_jpeg_info(const std::string& filepath, ImageMetadata& meta) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE* infile;

    if ((infile = fopen(filepath.c_str(), "rb")) == NULL) {
        throw std::runtime_error("Cannot open input file");
    }

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

// Сканируем файл на наличие маркеров JPEG (чтобы показать структуру)
void scan_markers(const std::string& filepath, ImageMetadata& meta) {
    std::ifstream file(filepath, std::ios::binary);
    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    for (size_t i = 0; i < buffer.size() - 1; ++i) {
        if (buffer[i] == 0xFF && buffer[i+1] != 0x00 && buffer[i+1] != 0xFF) {
            uint8_t marker = buffer[i+1];
            std::string name = "";
            
            switch(marker) {
                case 0xD8: name = "SOI (Start of Image)"; break;
                case 0xD9: name = "EOI (End of Image)"; break;
                case 0xC0: name = "SOF0 (Baseline DCT)"; break;
                case 0xC2: name = "SOF2 (Progressive DCT)"; break;
                case 0xC4: name = "DHT (Huffman Table)"; break;
                case 0xDA: name = "SOS (Start of Scan)"; break;
                case 0xDB: name = "DQT (Quantization Table)"; break;
                case 0xE0: name = "APP0 (JFIF)"; break;
                case 0xE1: name = "APP1 (EXIF/XMP)"; break;
                case 0xE2: name = "APP2 (ICC Profile)"; break;
                case 0xED: name = "APP13 (IPTC/Photoshop)"; break;
                case 0xEE: name = "APP14 (Adobe)"; break;
                case 0xFE: name = "COM (Comment)"; break;
            }
            
            if (!name.empty()) {
                meta.markers[name]++;
            }
            i++; // перешагиваем через байт маркера
        }
    }
}

// Ручной парсер для поиска маркера COM (Намеренная ошибка 1: Память)
std::string extract_comment_manual(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    for (size_t i = 0; i < buffer.size() - 1; ++i) {
        if (buffer[i] == 0xFF && buffer[i+1] == 0xFE) {
            if (i + 4 > buffer.size()) break;
            
            uint16_t seg_len = (buffer[i+2] << 8) | buffer[i+3];
            uint16_t comment_len = seg_len - 2;

            // ОШИБКА: Выделяем фиксированный буфер на 256 байт, но копируем comment_len байт без проверки!
            char* unsafe_buf = new char[256]; 
            if (i + 4 + comment_len <= buffer.size()) {
                memcpy(unsafe_buf, &buffer[i+4], comment_len); 
                std::string result(unsafe_buf, comment_len);
                delete[] unsafe_buf;
                return result;
            }
            delete[] unsafe_buf;
        }
    }
    return "";
}

// Функция сохранения отчета (Намеренная ошибка 2: Path Traversal)
void save_report(const std::string& user_filename, const ImageMetadata& meta) {
    // ОШИБКА: Нет проверки на "../" в user_filename
    std::string out_path = "./output/" + user_filename;
    
    std::ofstream out_file(out_path);
    if (!out_file) {
        std::cerr << "Failed to open output file: " << out_path << "\n";
        return;
    }
    
    out_file << "Image Metadata Report\n";
    out_file << "=====================\n";
    out_file << "Width: " << meta.width << " px\n";
    out_file << "Height: " << meta.height << " px\n";
    out_file << "Color Components: " << meta.components << "\n";
    out_file << "Color Space: " << meta.color_space << "\n";
    out_file << "Precision: " << meta.precision << " bits\n";
    out_file << "Progressive Mode: " << (meta.progressive ? "Yes" : "No") << "\n\n";
    
    out_file << "JPEG Structure (Markers):\n";
    for (const auto& pair : meta.markers) {
        out_file << "  " << pair.first << ": " << pair.second << "\n";
    }
    out_file << "\n";
    
    if (!meta.comment.empty()) {
        out_file << "Comment: " << meta.comment << "\n";
    } else {
        out_file << "Comment: Not found\n";
    }
    
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
        
        // 1. Читаем через libjpeg
        get_jpeg_info(input_file, meta);
        
        // 2. Сканируем структуру маркеров
        scan_markers(input_file, meta);
        
        // 3. Ищем комментарий (с уязвимостью)
        meta.comment = extract_comment_manual(input_file);

        // Вывод в консоль
        std::cout << "Image dimensions: " << meta.width << "x" << meta.height << "\n";
        std::cout << "Color Space: " << meta.color_space << "\n";
        std::cout << "Markers found: " << meta.markers.size() << " unique types\n";

        // Сохраняем отчет
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
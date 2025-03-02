#pragma once

#include <memory>
#include "FileSaveManager.h"
#include "Logger.h"
#include <QImage>
#include <QBuffer>
#include <QDebug>

// 数据转换器接口
class IDataConverter {
public:
    virtual ~IDataConverter() = default;

    // 转换数据格式
    virtual QByteArray convert(const DataPacket& packet, const SaveParameters& params) = 0;

    // 获取输出文件扩展名
    virtual QString getFileExtension() const = 0;
};

// 基础图像转换器，提取共享逻辑
class BaseImageConverter : public IDataConverter {
public:
    BaseImageConverter() = default;
    virtual ~BaseImageConverter() override = default;

protected:
    // 将RAW8数据转换为图像
    QImage convertRaw8ToImage(const uint8_t* data, size_t size, uint16_t width, uint16_t height) {
        // 检查数据大小是否合理
        if (size < width * height) {
            throw std::runtime_error("Insufficient data for RAW8 conversion");
        }

        // 创建灰度图像
        QImage image(width, height, QImage::Format_Grayscale8);

        // 填充图像数据
        for (int y = 0; y < height; ++y) {
            uint8_t* scanLine = image.scanLine(y);
            memcpy(scanLine, data + y * width, width);
        }

        return image;
    }

    // 将RAW10数据转换为图像
    QImage convertRaw10ToImage(const uint8_t* data, size_t size, uint16_t width, uint16_t height) {
        // RAW10格式每4个像素占用5个字节
        size_t expectedSize = (width * height * 5) / 4;
        if (size < expectedSize) {
            throw std::runtime_error("Insufficient data for RAW10 conversion");
        }

        // 创建8位灰度图像
        QImage image(width, height, QImage::Format_Grayscale8);

        // 解析RAW10数据并填充图像
        for (int y = 0; y < height; ++y) {
            uint8_t* scanLine = image.scanLine(y);

            for (int x = 0; x < width; x += 4) {
                // 计算源数据偏移
                size_t srcOffset = (y * width + x) * 5 / 4;

                // 提取4个10位像素值
                if (srcOffset + 4 < size) {
                    // 提取4个完整的10位像素，并将其缩放到8位
                    for (int i = 0; i < 4 && x + i < width; ++i) {
                        uint16_t pixelValue = 0;

                        // 根据位打包模式提取10位值
                        switch (i) {
                        case 0:
                            pixelValue = (data[srcOffset] << 2) | ((data[srcOffset + 4] >> 0) & 0x03);
                            break;
                        case 1:
                            pixelValue = (data[srcOffset + 1] << 2) | ((data[srcOffset + 4] >> 2) & 0x03);
                            break;
                        case 2:
                            pixelValue = (data[srcOffset + 2] << 2) | ((data[srcOffset + 4] >> 4) & 0x03);
                            break;
                        case 3:
                            pixelValue = (data[srcOffset + 3] << 2) | ((data[srcOffset + 4] >> 6) & 0x03);
                            break;
                        }

                        // 缩放10位值到8位 (0-1023 -> 0-255)
                        scanLine[x + i] = static_cast<uint8_t>(pixelValue >> 2);
                    }
                }
            }
        }

        return image;
    }

    // 将RAW12数据转换为图像
    QImage convertRaw12ToImage(const uint8_t* data, size_t size, uint16_t width, uint16_t height) {
        // RAW12格式每2个像素占用3个字节
        size_t expectedSize = (width * height * 3) / 2;
        if (size < expectedSize) {
            throw std::runtime_error("Insufficient data for RAW12 conversion");
        }

        // 创建8位灰度图像
        QImage image(width, height, QImage::Format_Grayscale8);

        // 解析RAW12数据并填充图像
        for (int y = 0; y < height; ++y) {
            uint8_t* scanLine = image.scanLine(y);

            for (int x = 0; x < width; x += 2) {
                // 计算源数据偏移
                size_t srcOffset = (y * width + x) * 3 / 2;

                // 提取2个12位像素值
                if (srcOffset + 2 < size) {
                    // 第一个像素：使用第一个字节和第三个字节的高4位
                    uint16_t pixel1 = (data[srcOffset] << 4) | ((data[srcOffset + 2] >> 4) & 0x0F);

                    // 第二个像素：使用第二个字节和第三个字节的低4位
                    uint16_t pixel2 = (data[srcOffset + 1] << 4) | (data[srcOffset + 2] & 0x0F);

                    // 缩放12位值到8位 (0-4095 -> 0-255)
                    scanLine[x] = static_cast<uint8_t>(pixel1 >> 4);

                    if (x + 1 < width) {
                        scanLine[x + 1] = static_cast<uint8_t>(pixel2 >> 4);
                    }
                }
            }
        }

        return image;
    }

    // 根据RAW格式转换为图像的通用方法
    QImage convertRawToImage(const DataPacket& packet, const SaveParameters& params) {
        uint16_t width = params.options.value("width", 1920).toUInt();
        uint16_t height = params.options.value("height", 1080).toUInt();
        uint8_t format = params.options.value("format", 0x39).toUInt();

        try {
            // 根据格式类型转换数据为图像
            switch (format) {
            case 0x38: // RAW8
                return convertRaw8ToImage(packet.data.data(), packet.size, width, height);
            case 0x39: // RAW10
                return convertRaw10ToImage(packet.data.data(), packet.size, width, height);
            case 0x3A: // RAW12
                return convertRaw12ToImage(packet.data.data(), packet.size, width, height);
            default:
                LOG_ERROR(fromLocal8Bit("不支持的图像格式: 0x%1")
                    .arg(format, 2, 16, QChar('0')));
                throw std::runtime_error("Unsupported image format");
            }
        }
        catch (const std::exception& e) {
            LOG_ERROR(fromLocal8Bit("图像转换错误: %1").arg(e.what()));
            throw;
        }
    }

    // 将图像保存为特定格式
    QByteArray saveImageToFormat(const QImage& image, const QString& format, int quality = -1) {
        if (image.isNull()) {
            LOG_ERROR(fromLocal8Bit("尝试保存空图像为 %1").arg(format));
            return QByteArray();
        }

        // 将图像保存为指定格式
        QByteArray imageData;
        QBuffer buffer(&imageData);
        buffer.open(QIODevice::WriteOnly);

        if (!image.save(&buffer, format.toUtf8().constData(), quality)) {
            LOG_ERROR(fromLocal8Bit("保存%1图像失败").arg(format));
            return QByteArray();
        }

        buffer.close();
        return imageData;
    }
};

// 原始数据转换器
class RawDataConverter : public IDataConverter {
public:
    RawDataConverter() = default;
    ~RawDataConverter() override = default;

    QByteArray convert(const DataPacket& packet, const SaveParameters& params) override {
        // 简单直接返回原始数据
        return QByteArray(reinterpret_cast<const char*>(packet.data.data()),
            static_cast<int>(packet.size));
    }

    QString getFileExtension() const override {
        return "raw";
    }
};

// BMP图像转换器
class BmpConverter : public BaseImageConverter {
public:
    BmpConverter() = default;
    ~BmpConverter() override = default;

    QByteArray convert(const DataPacket& packet, const SaveParameters& params) override {
        try {
            // 先转换为QImage
            QImage image = convertRawToImage(packet, params);

            // 保存为BMP格式
            return saveImageToFormat(image, "BMP");
        }
        catch (const std::exception& e) {
            LOG_ERROR(fromLocal8Bit("BMP转换错误: %1").arg(e.what()));
            return QByteArray();
        }
    }

    QString getFileExtension() const override {
        return "bmp";
    }
};

// TIFF图像转换器
class TiffConverter : public BaseImageConverter {
public:
    TiffConverter() = default;
    ~TiffConverter() override = default;

    QByteArray convert(const DataPacket& packet, const SaveParameters& params) override {
        try {
            // 先转换为QImage
            QImage image = convertRawToImage(packet, params);

            // 设置TIFF压缩选项
            int compressionLevel = params.compressionLevel;

            // 保存为TIFF格式
            return saveImageToFormat(image, "TIFF", compressionLevel);
        }
        catch (const std::exception& e) {
            LOG_ERROR(fromLocal8Bit("TIFF转换错误: %1").arg(e.what()));
            return QByteArray();
        }
    }

    QString getFileExtension() const override {
        return "tiff";
    }
};

// PNG图像转换器
class PngConverter : public BaseImageConverter {
public:
    PngConverter() = default;
    ~PngConverter() override = default;

    QByteArray convert(const DataPacket& packet, const SaveParameters& params) override {
        try {
            // 先转换为QImage
            QImage image = convertRawToImage(packet, params);

            // 设置PNG压缩级别 (0-9)
            int compressionLevel = params.compressionLevel;
            if (compressionLevel < 0) compressionLevel = 0;
            if (compressionLevel > 9) compressionLevel = 9;

            // 保存为PNG格式
            return saveImageToFormat(image, "PNG", compressionLevel);
        }
        catch (const std::exception& e) {
            LOG_ERROR(fromLocal8Bit("PNG转换错误: %1").arg(e.what()));
            return QByteArray();
        }
    }

    QString getFileExtension() const override {
        return "png";
    }
};

// CSV元数据转换器
class CsvMetadataConverter : public IDataConverter {
public:
    CsvMetadataConverter() = default;
    ~CsvMetadataConverter() override = default;

    QByteArray convert(const DataPacket& packet, const SaveParameters& params) override {
        QByteArray csvData;
        QTextStream stream(&csvData);

        // 写入CSV头
        stream << "Timestamp,Size,Width,Height,Format,CaptureTime\n";

        // 获取参数
        uint16_t width = params.options.value("width", 1920).toUInt();
        uint16_t height = params.options.value("height", 1080).toUInt();
        uint8_t format = params.options.value("format", 0x39).toUInt();

        // 格式化时间戳
        QDateTime timestamp;
        timestamp.setMSecsSinceEpoch(packet.timestamp / 1000000); // 从纳秒转换为毫秒

        // 写入数据行
        stream << timestamp.toString(Qt::ISODate) << ",";
        stream << packet.size << ",";
        stream << width << ",";
        stream << height << ",";
        stream << "0x" << QString::number(format, 16) << ",";
        stream << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";

        return csvData;
    }

    QString getFileExtension() const override {
        return "csv";
    }
};

// 数据转换器工厂
class DataConverterFactory {
public:
    static std::shared_ptr<IDataConverter> createConverter(FileFormat format) {
        switch (format) {
        case FileFormat::RAW:
            return std::make_shared<RawDataConverter>();
        case FileFormat::BMP:
            return std::make_shared<BmpConverter>();
        case FileFormat::TIFF:
            return std::make_shared<TiffConverter>();
        case FileFormat::PNG:
            return std::make_shared<PngConverter>();
        case FileFormat::CSV:
            return std::make_shared<CsvMetadataConverter>();
        default:
            LOG_WARN(fromLocal8Bit("未知的文件格式: %1, 使用RAW转换器").arg(static_cast<int>(format)));
            return std::make_shared<RawDataConverter>();
        }
    }
};
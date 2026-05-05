#include "ShaderLoom/Image.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace ShaderLoom {
namespace {

std::string lowerExtension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return extension;
}

std::vector<std::uint8_t> readFileBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to open image: " + path.string());
    }

    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    input.seekg(0, std::ios::beg);
    if (size <= 0) {
        throw std::runtime_error("Image file is empty: " + path.string());
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!input) {
        throw std::runtime_error("Failed to read image: " + path.string());
    }
    return bytes;
}

std::string quotePath(const std::filesystem::path& path) {
    return "\"" + path.string() + "\"";
}

std::optional<std::filesystem::path> findFfmpegPath() {
#ifdef _WIN32
    char buffer[MAX_PATH] = {};
    const DWORD found = SearchPathA(nullptr, "ffmpeg.exe", nullptr, MAX_PATH, buffer, nullptr);
    if (found > 0 && found < MAX_PATH) {
        return std::filesystem::path(buffer);
    }

    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (localAppData != nullptr) {
        const std::filesystem::path wingetRoot = std::filesystem::path(localAppData) / "Microsoft" / "WinGet" / "Packages";
        std::error_code ignored;
        if (std::filesystem::exists(wingetRoot, ignored)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(
                     wingetRoot,
                     std::filesystem::directory_options::skip_permission_denied,
                     ignored
                 )) {
                if (!entry.is_regular_file(ignored)) {
                    continue;
                }
                if (entry.path().filename() == "ffmpeg.exe") {
                    return entry.path();
                }
            }
        }
    }

    const std::array<std::filesystem::path, 3> fallbacks = {
        std::filesystem::path("C:/ffmpeg/bin/ffmpeg.exe"),
        std::filesystem::path("C:/Program Files/ffmpeg/bin/ffmpeg.exe"),
        std::filesystem::path("C:/Program Files/Derivative/TouchDesigner/bin/ffmpeg.exe")
    };
    for (const std::filesystem::path& path : fallbacks) {
        std::error_code ignored;
        if (std::filesystem::exists(path, ignored)) {
            return path;
        }
    }
#else
    return std::filesystem::path("ffmpeg");
#endif
    return std::nullopt;
}

std::string readTextFile(const std::filesystem::path& path, std::size_t maxBytes = 1800) {
    std::ifstream input(path);
    if (!input) {
        return {};
    }

    std::ostringstream text;
    text << input.rdbuf();
    std::string value = text.str();
    if (value.size() > maxBytes) {
        value = value.substr(value.size() - maxBytes);
    }
    return value;
}

int runCommand(const std::filesystem::path& application, const std::string& arguments, const std::filesystem::path& logPath) {
    const std::string command = quotePath(application) + " " + arguments;
#ifdef _WIN32
    SECURITY_ATTRIBUTES securityAttributes{};
    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.bInheritHandle = TRUE;
    HANDLE logFile = CreateFileA(
        logPath.string().c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        &securityAttributes,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (logFile == INVALID_HANDLE_VALUE) {
        return -static_cast<int>(GetLastError());
    }
    HANDLE nullInput = CreateFileA(
        "NUL",
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        &securityAttributes,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    STARTUPINFOA startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = nullInput == INVALID_HANDLE_VALUE ? nullptr : nullInput;
    startupInfo.hStdOutput = logFile;
    startupInfo.hStdError = logFile;
    PROCESS_INFORMATION processInfo{};
    std::vector<char> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back('\0');

    const BOOL started = CreateProcessA(
        application.string().c_str(),
        mutableCommand.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startupInfo,
        &processInfo
    );
    if (!started) {
        const int errorCode = static_cast<int>(GetLastError());
        if (nullInput != INVALID_HANDLE_VALUE) {
            CloseHandle(nullInput);
        }
        CloseHandle(logFile);
        return -errorCode;
    }

    WaitForSingleObject(processInfo.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(processInfo.hProcess, &exitCode);
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    if (nullInput != INVALID_HANDLE_VALUE) {
        CloseHandle(nullInput);
    }
    CloseHandle(logFile);
    return static_cast<int>(exitCode);
#else
    const std::string redirected = command + " > " + quotePath(logPath) + " 2>&1";
    return std::system(redirected.c_str());
#endif
}

std::filesystem::path uniqueDecodeDirectory() {
    const std::filesystem::path parent = std::filesystem::temp_directory_path();
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    for (int suffix = 0; suffix < 1000; ++suffix) {
        std::filesystem::path candidate = parent / ("ShaderLoom_webp_" + std::to_string(tick) + "_" + std::to_string(suffix));
        if (!std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    throw std::runtime_error("Could not create a temporary WebP decode directory.");
}

Image loadStillImage(const std::filesystem::path& path) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* loaded = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
    if (!loaded) {
        throw std::runtime_error("Failed to load image: " + path.string());
    }

    std::vector<std::uint8_t> rgba(
        loaded,
        loaded + static_cast<std::size_t>(width * height * 4)
    );
    stbi_image_free(loaded);
    return Image(width, height, std::move(rgba));
}

std::vector<ImageFrame> loadWebPFrames(const std::filesystem::path& path) {
    const std::optional<std::filesystem::path> ffmpegPath = findFfmpegPath();
    if (!ffmpegPath) {
        throw std::runtime_error("WebP import requires FFmpeg. Install FFmpeg or add it to PATH.");
    }

    const std::filesystem::path decodeDirectory = uniqueDecodeDirectory();
    std::filesystem::create_directories(decodeDirectory);

    try {
        const std::filesystem::path framePattern = decodeDirectory / "frame_%05d.png";
        const std::filesystem::path logPath = decodeDirectory / "ffmpeg.log";
        std::ostringstream arguments;
        arguments << "-y -hide_banner -loglevel error"
                  << " -i " << quotePath(path)
                  << " -an -vsync 0 "
                  << quotePath(framePattern);

        const int result = runCommand(*ffmpegPath, arguments.str(), logPath);
        if (result != 0) {
            std::string log = readTextFile(logPath);
            std::replace(log.begin(), log.end(), '\r', ' ');
            std::replace(log.begin(), log.end(), '\n', ' ');
            const std::string detail = log.empty() ? "exit code " + std::to_string(result) : log;
            throw std::runtime_error("Failed to decode WebP with FFmpeg: " + path.string() + " (" + detail + ")");
        }

        std::vector<std::filesystem::path> framePaths;
        for (const auto& entry : std::filesystem::directory_iterator(decodeDirectory)) {
            if (entry.is_regular_file() && lowerExtension(entry.path()) == ".png") {
                framePaths.push_back(entry.path());
            }
        }
        std::sort(framePaths.begin(), framePaths.end());
        if (framePaths.empty()) {
            throw std::runtime_error("WebP contained no decodable frames: " + path.string());
        }

        std::vector<ImageFrame> frames;
        frames.reserve(framePaths.size());
        for (const std::filesystem::path& framePath : framePaths) {
            frames.push_back(ImageFrame{loadStillImage(framePath), 100});
        }

        std::error_code ignored;
        std::filesystem::remove_all(decodeDirectory, ignored);
        return frames;
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(decodeDirectory, ignored);
        throw;
    }
}

} // namespace

Image::Image(int width, int height)
    : width_(width), height_(height), rgba_(static_cast<std::size_t>(width * height * 4), 0) {
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            setPixel(x, y, Pixel{0, 0, 0, 255});
        }
    }
}

Image::Image(int width, int height, std::vector<std::uint8_t> rgba)
    : width_(width), height_(height), rgba_(std::move(rgba)) {
    if (width_ <= 0 || height_ <= 0) {
        throw std::runtime_error("Image dimensions must be positive.");
    }
    if (rgba_.size() != static_cast<std::size_t>(width_ * height_ * 4)) {
        throw std::runtime_error("Image pixel buffer must contain RGBA data.");
    }
}

Image Image::load(const std::filesystem::path& path) {
    std::vector<ImageFrame> frames = loadImageFrames(path);
    return std::move(frames.front().image);
}

std::vector<ImageFrame> loadImageFrames(const std::filesystem::path& path) {
    const std::string extension = lowerExtension(path);
    if (extension == ".webp") {
        return loadWebPFrames(path);
    }

    if (extension == ".gif") {
        const std::vector<std::uint8_t> bytes = readFileBytes(path);
        int width = 0;
        int height = 0;
        int frameCount = 0;
        int channels = 0;
        int* delays = nullptr;
        stbi_uc* loaded = stbi_load_gif_from_memory(
            bytes.data(),
            static_cast<int>(bytes.size()),
            &delays,
            &width,
            &height,
            &frameCount,
            &channels,
            4
        );
        if (!loaded) {
            throw std::runtime_error("Failed to load GIF: " + path.string());
        }

        std::vector<ImageFrame> frames;
        frames.reserve(static_cast<std::size_t>(std::max(frameCount, 1)));
        const auto frameBytes = static_cast<std::size_t>(width * height * 4);
        for (int frame = 0; frame < frameCount; ++frame) {
            const stbi_uc* begin = loaded + (static_cast<std::size_t>(frame) * frameBytes);
            std::vector<std::uint8_t> rgba(begin, begin + frameBytes);
            const int delay = delays != nullptr && delays[frame] > 0 ? delays[frame] : 100;
            frames.push_back(ImageFrame{Image(width, height, std::move(rgba)), delay});
        }

        stbi_image_free(loaded);
        if (delays != nullptr) {
            stbi_image_free(delays);
        }
        if (frames.empty()) {
            throw std::runtime_error("GIF contained no frames: " + path.string());
        }
        return frames;
    }

    return {ImageFrame{loadStillImage(path), 100}};
}

bool Image::empty() const noexcept {
    return width_ <= 0 || height_ <= 0 || rgba_.empty();
}

int Image::width() const noexcept {
    return width_;
}

int Image::height() const noexcept {
    return height_;
}

const std::vector<std::uint8_t>& Image::pixels() const noexcept {
    return rgba_;
}

std::vector<std::uint8_t>& Image::pixels() noexcept {
    return rgba_;
}

Pixel Image::pixel(int x, int y) const {
    x = std::clamp(x, 0, width_ - 1);
    y = std::clamp(y, 0, height_ - 1);
    const auto index = static_cast<std::size_t>((y * width_ + x) * 4);
    return Pixel{rgba_[index], rgba_[index + 1], rgba_[index + 2], rgba_[index + 3]};
}

void Image::setPixel(int x, int y, Pixel pixel) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return;
    }
    const auto index = static_cast<std::size_t>((y * width_ + x) * 4);
    rgba_[index] = pixel.r;
    rgba_[index + 1] = pixel.g;
    rgba_[index + 2] = pixel.b;
    rgba_[index + 3] = pixel.a;
}

void Image::writePng(const std::filesystem::path& path) const {
    const int stride = width_ * 4;
    stbi_write_png_compression_level = 1;
    if (!stbi_write_png(path.string().c_str(), width_, height_, 4, rgba_.data(), stride)) {
        throw std::runtime_error("Failed to write PNG: " + path.string());
    }
}

void Image::writeJpeg(const std::filesystem::path& path, int quality) const {
    quality = std::clamp(quality, 1, 100);
    if (!stbi_write_jpg(path.string().c_str(), width_, height_, 4, rgba_.data(), quality)) {
        throw std::runtime_error("Failed to write JPEG: " + path.string());
    }
}

} // namespace ShaderLoom

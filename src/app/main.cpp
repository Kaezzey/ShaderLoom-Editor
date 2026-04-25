#include "ShaderLoom/Image.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#endif

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <utility>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace {

constexpr float LeftRailWidth = 232.0F;
constexpr float RightRailWidth = 300.0F;
constexpr float FooterHeight = 28.0F;

struct LoadedImageState {
    ShaderLoom::Image image;
    std::filesystem::path path;
    GLuint texture = 0;
    float zoom = 1.0F;
    ImVec2 pan = ImVec2(0.0F, 0.0F);
    std::string error;

    [[nodiscard]] bool hasImage() const noexcept {
        return texture != 0 && !image.empty();
    }
};

void glfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW error " << error << ": " << description << '\n';
}

std::string truncateMiddle(const std::string& value, std::size_t maxLength) {
    if (value.size() <= maxLength) {
        return value;
    }
    if (maxLength < 8) {
        return value.substr(0, maxLength);
    }

    const std::size_t left = (maxLength - 3) / 2;
    const std::size_t right = maxLength - 3 - left;
    return value.substr(0, left) + "..." + value.substr(value.size() - right);
}

void destroyTexture(LoadedImageState& state) {
    if (state.texture != 0) {
        glDeleteTextures(1, &state.texture);
        state.texture = 0;
    }
}

bool loadImage(LoadedImageState& state, const std::filesystem::path& path) {
    try {
        ShaderLoom::Image loaded = ShaderLoom::Image::load(path);

        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            loaded.width(),
            loaded.height(),
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            loaded.pixels().data()
        );
        glBindTexture(GL_TEXTURE_2D, 0);

        destroyTexture(state);
        state.image = std::move(loaded);
        state.path = path;
        state.texture = texture;
        state.zoom = 1.0F;
        state.pan = ImVec2(0.0F, 0.0F);
        state.error.clear();
        return true;
    } catch (const std::exception& error) {
        state.error = error.what();
        return false;
    }
}

void clearImage(LoadedImageState& state) {
    destroyTexture(state);
    state.image = ShaderLoom::Image();
    state.path.clear();
    state.zoom = 1.0F;
    state.pan = ImVec2(0.0F, 0.0F);
    state.error.clear();
}

#ifdef _WIN32
std::optional<std::filesystem::path> browseForImage() {
    char filename[MAX_PATH] = {};
    OPENFILENAMEA dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = nullptr;
    dialog.lpstrFilter = "Images\0*.png;*.jpg;*.jpeg\0PNG\0*.png\0JPEG\0*.jpg;*.jpeg\0All Files\0*.*\0";
    dialog.lpstrFile = filename;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    dialog.lpstrDefExt = "png";

    if (GetOpenFileNameA(&dialog) == TRUE) {
        return std::filesystem::path(filename);
    }
    return std::nullopt;
}
#else
std::optional<std::filesystem::path> browseForImage() {
    return std::nullopt;
}
#endif

void dropCallback(GLFWwindow* window, int count, const char** paths) {
    auto* state = static_cast<LoadedImageState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr || count <= 0 || paths == nullptr) {
        return;
    }
    loadImage(*state, std::filesystem::path(paths[0]));
}

void applyShaderLoomStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0F;
    style.ChildRounding = 0.0F;
    style.FrameRounding = 0.0F;
    style.PopupRounding = 0.0F;
    style.ScrollbarRounding = 0.0F;
    style.GrabRounding = 0.0F;
    style.WindowBorderSize = 1.0F;
    style.ChildBorderSize = 1.0F;
    style.FrameBorderSize = 1.0F;
    style.ItemSpacing = ImVec2(8.0F, 8.0F);
    style.FramePadding = ImVec2(6.0F, 4.0F);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.82F, 0.84F, 0.86F, 1.0F);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.32F, 0.34F, 0.36F, 1.0F);
    colors[ImGuiCol_WindowBg] = ImVec4(0.045F, 0.047F, 0.047F, 1.0F);
    colors[ImGuiCol_ChildBg] = ImVec4(0.055F, 0.057F, 0.057F, 1.0F);
    colors[ImGuiCol_Border] = ImVec4(0.13F, 0.14F, 0.14F, 1.0F);
    colors[ImGuiCol_FrameBg] = ImVec4(0.035F, 0.037F, 0.037F, 1.0F);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.10F, 0.11F, 0.11F, 1.0F);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.13F, 0.14F, 0.14F, 1.0F);
    colors[ImGuiCol_Button] = ImVec4(0.055F, 0.057F, 0.057F, 1.0F);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.12F, 0.13F, 0.13F, 1.0F);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.18F, 0.19F, 0.19F, 1.0F);
    colors[ImGuiCol_CheckMark] = ImVec4(0.76F, 0.78F, 0.80F, 1.0F);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.70F, 0.72F, 0.74F, 1.0F);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.88F, 0.90F, 0.92F, 1.0F);
    colors[ImGuiCol_Header] = ImVec4(0.08F, 0.09F, 0.09F, 1.0F);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.11F, 0.12F, 0.12F, 1.0F);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.14F, 0.15F, 0.15F, 1.0F);
}

void sectionTitle(const char* title) {
    ImGui::Spacing();
    ImGui::TextUnformatted(title);
    ImGui::Separator();
}

void valueSlider(const char* label, float* value, float min, float max, const char* format = "%.1f") {
    ImGui::PushID(label);
    std::string visibleLabel = label;
    const std::size_t idSeparator = visibleLabel.find("##");
    if (idSeparator != std::string::npos) {
        visibleLabel.resize(idSeparator);
    }
    ImGui::TextDisabled("%s", visibleLabel.c_str());
    ImGui::SameLine(92.0F);
    ImGui::Text(format, *value);
    ImGui::SameLine(138.0F);
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::SliderFloat("##slider", value, min, max, "");
    ImGui::PopID();
}

bool formatTile(const char* name, const char* extension, bool selected, const ImVec2& size) {
    ImGui::PushID(name);
    ImGui::PushStyleColor(ImGuiCol_Button, selected ? ImVec4(0.08F, 0.08F, 0.08F, 1.0F) : ImVec4(0.045F, 0.047F, 0.047F, 1.0F));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.10F, 0.11F, 0.11F, 1.0F));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12F, 0.13F, 0.13F, 1.0F));
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.80F, 0.80F, 0.80F, 1.0F));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.12F, 0.13F, 0.13F, 1.0F));
    }

    const bool clicked = ImGui::Button("##format-tile", size);
    const ImVec2 min = ImGui::GetItemRectMin();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddText(ImVec2(min.x + 10.0F, min.y + 10.0F), IM_COL32(205, 216, 230, 255), name);
    drawList->AddText(ImVec2(min.x + 10.0F, min.y + 28.0F), IM_COL32(92, 96, 100, 255), extension);

    ImGui::PopStyleColor(4);
    ImGui::PopID();
    return clicked;
}

void drawLeftRail(int& selectedEffect, LoadedImageState& imageState) {
    ImGui::BeginChild("left-rail", ImVec2(LeftRailWidth, 0.0F), true);
    ImGui::TextUnformatted("ShaderLoom");
    ImGui::Separator();

    sectionTitle("- Input");
    ImGui::TextDisabled("%s", imageState.hasImage() ? "Image loaded" : "No image");
    if (imageState.hasImage()) {
        ImGui::SameLine(174.0F);
        if (ImGui::SmallButton("Clear")) {
            clearImage(imageState);
        }
    }
    ImGui::TextDisabled("Resolution");
    ImGui::SameLine(124.0F);
    if (imageState.hasImage()) {
        ImGui::TextDisabled("%d x %d", imageState.image.width(), imageState.image.height());
    } else {
        ImGui::TextDisabled("-");
    }
    ImGui::TextDisabled("File");
    ImGui::SameLine(124.0F);
    if (imageState.hasImage()) {
        ImGui::TextDisabled("%s", truncateMiddle(imageState.path.filename().string(), 18).c_str());
    } else {
        ImGui::TextDisabled("-");
    }

    if (ImGui::Button("Drop file or click to browse\nPNG, JPG", ImVec2(-1.0F, 56.0F))) {
        if (const std::optional<std::filesystem::path> selected = browseForImage()) {
            loadImage(imageState, *selected);
        }
    }

    if (!imageState.error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95F, 0.45F, 0.45F, 1.0F));
        ImGui::TextWrapped("%s", imageState.error.c_str());
        ImGui::PopStyleColor();
    }

    sectionTitle("- Effects");
    const char* effects[] = {
        "ASCII",
        "Dithering",
        "Halftone",
        "Matrix Rain",
        "Dots",
        "Contour",
        "Pixel Sort",
        "Blockify",
        "Threshold",
        "Edge Detection",
        "Crosshatch",
        "Wave Lines",
        "Noise Field",
        "Voronoi",
        "VHS"
    };

    for (int i = 0; i < static_cast<int>(sizeof(effects) / sizeof(effects[0])); ++i) {
        const bool active = selectedEffect == i;
        ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(0.82F, 0.90F, 1.0F, 1.0F) : ImVec4(0.35F, 0.37F, 0.38F, 1.0F));
        const std::string label = std::string(active ? "*  " : "o  ") + effects[i];
        if (ImGui::Selectable(label.c_str(), active, 0, ImVec2(0.0F, 18.0F))) {
            selectedEffect = i;
        }
        ImGui::PopStyleColor();
    }

    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - FooterHeight - 42.0F);
    sectionTitle("+ Presets");
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - FooterHeight);
    ImGui::TextDisabled("Follow   About   Changelog");
    ImGui::EndChild();
}

void drawPreview(const char* effectName, LoadedImageState& imageState, float width) {
    ImGui::BeginChild("preview", ImVec2(width, 0.0F), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const ImVec2 previewMin = ImGui::GetWindowPos();
    const ImVec2 previewSize = ImGui::GetWindowSize();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(previewMin, ImVec2(previewMin.x + previewSize.x, previewMin.y + previewSize.y), IM_COL32(4, 5, 5, 255));

    const std::string title = std::string(effectName) + " [CPU]";
    const ImVec2 titleSize = ImGui::CalcTextSize(title.c_str());
    ImGui::SetCursorPos(ImVec2((previewSize.x - titleSize.x) * 0.5F, 16.0F));
    ImGui::TextDisabled("%s", title.c_str());

    ImGui::SetCursorPos(ImVec2(previewSize.x - 86.0F, 14.0F));
    ImGui::SmallButton("[]");
    ImGui::SameLine();
    ImGui::SmallButton("<>");
    ImGui::SameLine();
    ImGui::SmallButton("::");

    const ImVec2 contentMin(previewMin.x + 18.0F, previewMin.y + 54.0F);
    const ImVec2 contentMax(previewMin.x + previewSize.x - 18.0F, previewMin.y + previewSize.y - FooterHeight - 12.0F);
    drawList->PushClipRect(contentMin, contentMax, true);

    if (imageState.hasImage()) {
        const float availableWidth = std::max(1.0F, contentMax.x - contentMin.x);
        const float availableHeight = std::max(1.0F, contentMax.y - contentMin.y);
        const float fit = std::min(
            availableWidth / static_cast<float>(imageState.image.width()),
            availableHeight / static_cast<float>(imageState.image.height())
        );

        if (ImGui::IsWindowHovered()) {
            const ImGuiIO& io = ImGui::GetIO();
            if (io.KeyCtrl && io.MouseWheel != 0.0F) {
                imageState.zoom = std::clamp(imageState.zoom + (io.MouseWheel * 0.08F), 0.1F, 8.0F);
            }
            if (io.KeyAlt && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                imageState.pan.x += io.MouseDelta.x;
                imageState.pan.y += io.MouseDelta.y;
            }
        }

        const float scale = fit * imageState.zoom;
        const float imageWidth = static_cast<float>(imageState.image.width()) * scale;
        const float imageHeight = static_cast<float>(imageState.image.height()) * scale;
        const ImVec2 imageMin(
            contentMin.x + ((availableWidth - imageWidth) * 0.5F) + imageState.pan.x,
            contentMin.y + ((availableHeight - imageHeight) * 0.5F) + imageState.pan.y
        );
        const ImVec2 imageMax(imageMin.x + imageWidth, imageMin.y + imageHeight);

        drawList->AddRectFilled(imageMin, imageMax, IM_COL32(10, 11, 11, 255));
        drawList->AddImage(
            static_cast<ImTextureID>(imageState.texture),
            imageMin,
            imageMax,
            ImVec2(0.0F, 0.0F),
            ImVec2(1.0F, 1.0F)
        );
        drawList->AddRect(imageMin, imageMax, IM_COL32(34, 36, 36, 255));
    } else {
        const ImVec2 emptyMin(contentMin.x + 36.0F, contentMin.y + 72.0F);
        const ImVec2 emptyMax(contentMax.x - 36.0F, contentMax.y - 72.0F);
        drawList->AddRect(emptyMin, emptyMax, IM_COL32(28, 30, 30, 255));
        const char* emptyText = "Drop a PNG or JPG here";
        const ImVec2 emptyTextSize = ImGui::CalcTextSize(emptyText);
        drawList->AddText(
            ImVec2((emptyMin.x + emptyMax.x - emptyTextSize.x) * 0.5F, (emptyMin.y + emptyMax.y - emptyTextSize.y) * 0.5F),
            IM_COL32(92, 96, 100, 255),
            emptyText
        );
    }
    drawList->PopClipRect();

    if (imageState.hasImage()) {
        ImGui::SetCursorPos(ImVec2(previewSize.x - 220.0F, previewSize.y - FooterHeight));
        if (ImGui::SmallButton("-")) {
            imageState.zoom = std::max(0.1F, imageState.zoom - 0.1F);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%d%%", static_cast<int>(std::round(imageState.zoom * 100.0F)));
        ImGui::SameLine();
        if (ImGui::SmallButton("+")) {
            imageState.zoom = std::min(8.0F, imageState.zoom + 0.1F);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset")) {
            imageState.zoom = 1.0F;
            imageState.pan = ImVec2(0.0F, 0.0F);
        }
    }

    ImGui::SetCursorPos(ImVec2(18.0F, previewSize.y - FooterHeight));
    ImGui::TextDisabled("Scroll to pan  Ctrl+Scroll to zoom  Alt+Drag to pan");
    ImGui::EndChild();
}

void drawExportSection() {
    sectionTitle("- Export");
    ImGui::TextDisabled("Format");

    const char* names[] = {"PNG", "JPEG", "GIF", "Video", "SVG", "Text", "Three.js"};
    const char* extensions[] = {".png", ".jpg", ".gif", ".mp4", ".svg", ".txt", ".html"};
    static int selectedFormat = 0;

    const float gap = 6.0F;
    const float tileWidth = (ImGui::GetContentRegionAvail().x - gap) * 0.5F;
    const ImVec2 tileSize(tileWidth, 52.0F);

    for (int i = 0; i < 7; ++i) {
        if (formatTile(names[i], extensions[i], selectedFormat == i, tileSize)) {
            selectedFormat = i;
        }
        if ((i % 2) == 0 && i != 6) {
            ImGui::SameLine(0.0F, gap);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("High quality image");
    ImGui::Button("Export PNG", ImVec2(-1.0F, 30.0F));
}

void drawSettingsRail(const char* effectName) {
    ImGui::BeginChild("settings-rail", ImVec2(RightRailWidth, 0.0F), true);
    ImGui::TextUnformatted("- Settings");
    ImGui::SameLine(RightRailWidth - 56.0F);
    ImGui::TextDisabled("Reset");

    sectionTitle(effectName);
    static float scale = 1.0F;
    static float spacing = 0.0F;
    static float outputWidth = 0.0F;
    valueSlider("Scale", &scale, 0.0F, 4.0F, "%.0f");
    valueSlider("Spacing", &spacing, 0.0F, 2.0F);
    valueSlider("Output Width", &outputWidth, 0.0F, 4096.0F, "%.0f");
    const char* characterSets[] = {"DETAILED", "BLOCKS", "MINIMAL"};
    static int characterSet = 0;
    ImGui::TextDisabled("Character Set");
    ImGui::SameLine(92.0F);
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::Combo("##character-set", &characterSet, characterSets, 3);

    sectionTitle("Adjustments");
    static float brightness = 1.0F;
    static float contrast = 0.0F;
    static float saturation = 0.0F;
    static float hueRotation = 0.0F;
    static float sharpness = 0.0F;
    static float gamma = 1.0F;
    valueSlider("Brightness", &brightness, -100.0F, 100.0F, "%.0f");
    valueSlider("Contrast", &contrast, -100.0F, 100.0F, "%.0f");
    valueSlider("Saturation", &saturation, -100.0F, 100.0F, "%.0f");
    valueSlider("Hue Rotation", &hueRotation, -180.0F, 180.0F, "%.0f deg");
    valueSlider("Sharpness", &sharpness, 0.0F, 5.0F, "%.0f");
    valueSlider("Gamma", &gamma, 0.1F, 4.0F);

    sectionTitle("Color");
    const char* modes[] = {"Original", "Monochrome", "Duotone"};
    static int mode = 0;
    ImGui::TextDisabled("Mode");
    ImGui::SameLine(92.0F);
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::Combo("##mode", &mode, modes, 3);
    static char background[] = "#000000";
    ImGui::TextDisabled("Background");
    ImGui::SameLine(92.0F);
    ImGui::SetNextItemWidth(-1.0F);
    ImGui::InputText("##background", background, sizeof(background));
    static float intensity = 1.1F;
    valueSlider("Intensity", &intensity, 0.0F, 2.0F);

    sectionTitle("+ Processing");

    sectionTitle("- Post-Processing");
    static bool bloom = true;
    static bool grain = true;
    static bool chromatic = false;
    static bool scanlines = false;
    static bool vignette = false;
    static bool crtCurve = false;
    static bool phosphor = false;
    ImGui::Checkbox("Bloom", &bloom);
    static float threshold = 0.1F;
    static float softThreshold = 1.0F;
    static float bloomIntensity = 0.7F;
    static float radius = 7.0F;
    valueSlider("Threshold", &threshold, 0.0F, 1.0F);
    valueSlider("Soft Threshold", &softThreshold, 0.0F, 2.0F);
    valueSlider("Intensity", &bloomIntensity, 0.0F, 2.0F);
    valueSlider("Radius", &radius, 0.0F, 32.0F, "%.0f");
    ImGui::Checkbox("Grain", &grain);
    static float grainIntensity = 35.0F;
    static float grainSize = 2.0F;
    static float grainSpeed = 50.0F;
    valueSlider("Intensity##grain", &grainIntensity, 0.0F, 100.0F, "%.0f");
    valueSlider("Size", &grainSize, 0.0F, 8.0F, "%.0f");
    valueSlider("Speed", &grainSpeed, 0.0F, 100.0F, "%.0f");
    ImGui::Checkbox("Chromatic", &chromatic);
    ImGui::Checkbox("Scanlines", &scanlines);
    ImGui::Checkbox("Vignette", &vignette);
    ImGui::Checkbox("CRT Curve", &crtCurve);
    ImGui::Checkbox("Phosphor", &phosphor);

    drawExportSection();
    ImGui::EndChild();
}

} // namespace

int main(int argc, char** argv) {
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) {
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1440, 900, "ShaderLoom Offline Editor", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    LoadedImageState imageState;
    glfwSetWindowUserPointer(window, &imageState);
    glfwSetDropCallback(window, dropCallback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    applyShaderLoomStyle();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    static int selectedEffect = 0;
    const char* effects[] = {
        "ASCII",
        "Dithering",
        "Halftone",
        "Matrix Rain",
        "Dots",
        "Contour",
        "Pixel Sort",
        "Blockify",
        "Threshold",
        "Edge Detection",
        "Crosshatch",
        "Wave Lines",
        "Noise Field",
        "Voronoi",
        "VHS"
    };

    if (argc > 1) {
        loadImage(imageState, std::filesystem::path(argv[1]));
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("ShaderLoomRoot", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);

        const float centerWidth = std::max(320.0F, viewport->WorkSize.x - LeftRailWidth - RightRailWidth);
        drawLeftRail(selectedEffect, imageState);
        ImGui::SameLine(0.0F, 0.0F);
        drawPreview(effects[selectedEffect], imageState, centerWidth);
        ImGui::SameLine(0.0F, 0.0F);
        drawSettingsRail(effects[selectedEffect]);

        ImGui::End();

        ImGui::Render();
        int displayWidth = 0;
        int displayHeight = 0;
        glfwGetFramebufferSize(window, &displayWidth, &displayHeight);
        glViewport(0, 0, displayWidth, displayHeight);
        glClearColor(0.08F, 0.08F, 0.09F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    destroyTexture(imageState);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

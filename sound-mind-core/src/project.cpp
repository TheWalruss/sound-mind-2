#include "sound_mind/core/project.h"

#include <algorithm>
#include <fstream>
#include <ios>
#include <string>

#include "sound_mind/codec/pool_file.h"
#include "sound_mind/codec/stream_file.h"

namespace sound_mind::core {

namespace {
// The Background layer is always the first layer a new project has.
constexpr LayerId kBackgroundLayerId = 1;

// The Equalizer layer is always the second - immediately after
// Background, and (per addLayer()'s own docs) the last one addLayer()
// will ever let anything else be appended above.
constexpr LayerId kEqualizerLayerId = 2;

/// @brief The directory a project's media/sources/pool/mindshots folders
/// live under, per docs/sound-mind-architecture.md's Project File & Folder
/// layout: a same-named folder next to the .smproj file.
[[nodiscard]] std::filesystem::path projectFolder(const std::filesystem::path& projectFilePath) {
    return projectFilePath.parent_path() / projectFilePath.stem();
}

/// @brief The path a given layer's cached Stream render would live at,
/// within a project's folder.
[[nodiscard]] std::filesystem::path mediaPathFor(const std::filesystem::path& projectFilePath, LayerId layerId) {
    return projectFolder(projectFilePath) / "media" / ("layer_" + std::to_string(layerId) + ".smstream");
}

/// @brief The path a given layer's cached Pool render would live at,
/// within a project's folder.
[[nodiscard]] std::filesystem::path poolPathFor(const std::filesystem::path& projectFilePath, LayerId layerId) {
    return projectFolder(projectFilePath) / "pool" / ("layer_" + std::to_string(layerId) + ".smpool");
}
}  // namespace

Project Project::createNew(ProjectSettings settings) {
    Project project;
    project.settings_ = settings;
    project.layers_.emplace_back(kBackgroundLayerId, "Background", LayerType::Background);

    // The Equalizer's own default "Cut" gradient: intensity pinned to
    // the silence floor on both stops/channels - see createNew()'s own
    // docs - opacity left at FilterConfiguration's own default (0, no
    // cut applied yet).
    Layer equalizer(kEqualizerLayerId, "Equalizer", LayerType::Equalizer);
    FilterConfiguration equalizerConfig;
    Gradient& cutGradient = equalizerConfig.frequencyGradient();
    GradientStop startStop = cutGradient.stops().front();
    startStop.leftIntensity = -96.0f;
    startStop.rightIntensity = -96.0f;
    cutGradient.setStopValues(0, startStop);
    GradientStop endStop = cutGradient.stops().back();
    endStop.leftIntensity = -96.0f;
    endStop.rightIntensity = -96.0f;
    cutGradient.setStopValues(1, endStop);
    equalizer.setFilterConfiguration(equalizerConfig);
    project.layers_.push_back(std::move(equalizer));

    return project;
}

Project::Project(const Project& other)
    : settings_(other.settings_),
      layers_(other.layers_),
      mindWaves_(other.mindWaves_),
      mindShots_(other.mindShots_),
      mindGrains_(other.mindGrains_),
      convolutionKernels_(other.convolutionKernels_) {
    // operationLog_ deliberately left default-constructed (empty) - see
    // this constructor's own docs.
}

Project& Project::operator=(const Project& other) {
    if (this != &other) {
        settings_ = other.settings_;
        layers_ = other.layers_;
        mindWaves_ = other.mindWaves_;
        mindShots_ = other.mindShots_;
        mindGrains_ = other.mindGrains_;
        convolutionKernels_ = other.convolutionKernels_;
        operationLog_ = OperationLog{};
    }
    return *this;
}

Project Project::load(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::ios_base::failure("Could not open project file for reading: " + path.string());
    }
    nlohmann::json json;
    file >> json;
    Project project = json.get<Project>();

    for (Layer& layer : project.layers_) {
        const std::filesystem::path mediaPath = mediaPathFor(path, layer.id());
        if (std::filesystem::exists(mediaPath)) {
            layer.setContent(sound_mind::codec::readStreamFile(mediaPath));
        }
        const std::filesystem::path poolPath = poolPathFor(path, layer.id());
        if (std::filesystem::exists(poolPath)) {
            layer.setPoolContent(sound_mind::codec::readPoolFile(poolPath));
        }
    }

    return project;
}

void Project::save(const std::filesystem::path& path) const {
    std::ofstream file(path);
    if (!file) {
        throw std::ios_base::failure("Could not open project file for writing: " + path.string());
    }
    const nlohmann::json json = *this;
    file << json.dump(2);

    for (const Layer& layer : layers_) {
        if (layer.content().has_value()) {
            const std::filesystem::path mediaPath = mediaPathFor(path, layer.id());
            std::filesystem::create_directories(mediaPath.parent_path());
            sound_mind::codec::writeStreamFile(mediaPath, *layer.content());
        }
        if (layer.poolContent().has_value()) {
            const std::filesystem::path poolPath = poolPathFor(path, layer.id());
            std::filesystem::create_directories(poolPath.parent_path());
            sound_mind::codec::writePoolFile(poolPath, *layer.poolContent());
        }
    }
}

std::string Project::uniqueLayerName(const std::string& desiredName, std::optional<LayerId> excludingId) const {
    auto collides = [&](const std::string& candidate) {
        for (const Layer& layer : layers_) {
            if (excludingId.has_value() && layer.id() == *excludingId) {
                continue;
            }
            if (layer.name() == candidate) {
                return true;
            }
        }
        return false;
    };

    if (!collides(desiredName)) {
        return desiredName;
    }
    for (int suffix = 2;; ++suffix) {
        std::string candidate = desiredName + " (" + std::to_string(suffix) + ")";
        if (!collides(candidate)) {
            return candidate;
        }
    }
}

LayerId Project::addLayer(Layer layer) {
    const auto maxId = std::max_element(
        layers_.begin(), layers_.end(), [](const Layer& a, const Layer& b) { return a.id() < b.id(); });
    const LayerId newId = (maxId == layers_.end() ? kBackgroundLayerId : maxId->id()) + 1;

    layer.setId(newId);
    layer.setName(uniqueLayerName(layer.name()));
    // See this method's own docs - inserted just below an existing
    // Equalizer layer rather than unconditionally appended to the top.
    if (!layers_.empty() && layers_.back().type() == LayerType::Equalizer) {
        layers_.insert(layers_.end() - 1, std::move(layer));
    } else {
        layers_.push_back(std::move(layer));
    }
    return newId;
}

const Layer* Project::layerById(LayerId id) const noexcept {
    for (const Layer& layer : layers_) {
        if (layer.id() == id) {
            return &layer;
        }
    }
    return nullptr;
}

Layer* Project::layerById(LayerId id) noexcept {
    for (Layer& layer : layers_) {
        if (layer.id() == id) {
            return &layer;
        }
    }
    return nullptr;
}

MindWaveId Project::addMindWave(std::string name, MindWave wave) {
    const auto maxId = std::max_element(mindWaves_.begin(), mindWaves_.end(),
                                          [](const NamedMindWave& a, const NamedMindWave& b) { return a.id < b.id; });
    const MindWaveId newId = (maxId == mindWaves_.end() ? MindWaveId{0} : maxId->id) + 1;
    mindWaves_.push_back(NamedMindWave{newId, std::move(name), std::move(wave)});
    return newId;
}

bool Project::removeMindWave(MindWaveId id) {
    const auto it =
        std::find_if(mindWaves_.begin(), mindWaves_.end(), [id](const NamedMindWave& named) { return named.id == id; });
    if (it == mindWaves_.end()) {
        return false;
    }
    mindWaves_.erase(it);
    return true;
}

const NamedMindWave* Project::mindWaveById(MindWaveId id) const noexcept {
    for (const NamedMindWave& named : mindWaves_) {
        if (named.id == id) {
            return &named;
        }
    }
    return nullptr;
}

NamedMindWave* Project::mindWaveById(MindWaveId id) noexcept {
    for (NamedMindWave& named : mindWaves_) {
        if (named.id == id) {
            return &named;
        }
    }
    return nullptr;
}

MindShotId Project::addMindShot(std::string name, Clip clip) {
    const auto maxId = std::max_element(mindShots_.begin(), mindShots_.end(),
                                          [](const NamedMindShot& a, const NamedMindShot& b) { return a.id < b.id; });
    const MindShotId newId = (maxId == mindShots_.end() ? MindShotId{0} : maxId->id) + 1;
    mindShots_.push_back(NamedMindShot{newId, std::move(name), std::move(clip)});
    return newId;
}

bool Project::removeMindShot(MindShotId id) {
    const auto it =
        std::find_if(mindShots_.begin(), mindShots_.end(), [id](const NamedMindShot& named) { return named.id == id; });
    if (it == mindShots_.end()) {
        return false;
    }
    mindShots_.erase(it);
    return true;
}

const NamedMindShot* Project::mindShotById(MindShotId id) const noexcept {
    for (const NamedMindShot& named : mindShots_) {
        if (named.id == id) {
            return &named;
        }
    }
    return nullptr;
}

NamedMindShot* Project::mindShotById(MindShotId id) noexcept {
    for (NamedMindShot& named : mindShots_) {
        if (named.id == id) {
            return &named;
        }
    }
    return nullptr;
}

MindGrainId Project::addMindGrain(std::string name, LayerId sourceLayerId, TimeFrequencyRect bounds) {
    const auto maxId = std::max_element(
        mindGrains_.begin(), mindGrains_.end(),
        [](const NamedMindGrain& a, const NamedMindGrain& b) { return a.id < b.id; });
    const MindGrainId newId = (maxId == mindGrains_.end() ? MindGrainId{0} : maxId->id) + 1;
    mindGrains_.push_back(NamedMindGrain{newId, std::move(name), sourceLayerId, bounds});
    return newId;
}

bool Project::removeMindGrain(MindGrainId id) {
    const auto it = std::find_if(mindGrains_.begin(), mindGrains_.end(),
                                   [id](const NamedMindGrain& named) { return named.id == id; });
    if (it == mindGrains_.end()) {
        return false;
    }
    mindGrains_.erase(it);
    return true;
}

const NamedMindGrain* Project::mindGrainById(MindGrainId id) const noexcept {
    for (const NamedMindGrain& named : mindGrains_) {
        if (named.id == id) {
            return &named;
        }
    }
    return nullptr;
}

NamedMindGrain* Project::mindGrainById(MindGrainId id) noexcept {
    for (NamedMindGrain& named : mindGrains_) {
        if (named.id == id) {
            return &named;
        }
    }
    return nullptr;
}

ConvolutionKernelId Project::addConvolutionKernel(std::string name, int size, std::vector<float> coefficients,
                                                   bool normalize) {
    const auto maxId = std::max_element(
        convolutionKernels_.begin(), convolutionKernels_.end(),
        [](const NamedConvolutionKernel& a, const NamedConvolutionKernel& b) { return a.id < b.id; });
    const ConvolutionKernelId newId = (maxId == convolutionKernels_.end() ? ConvolutionKernelId{0} : maxId->id) + 1;
    convolutionKernels_.push_back(
        NamedConvolutionKernel{newId, std::move(name), size, std::move(coefficients), normalize});
    return newId;
}

bool Project::removeConvolutionKernel(ConvolutionKernelId id) {
    const auto it = std::find_if(convolutionKernels_.begin(), convolutionKernels_.end(),
                                   [id](const NamedConvolutionKernel& named) { return named.id == id; });
    if (it == convolutionKernels_.end()) {
        return false;
    }
    convolutionKernels_.erase(it);
    return true;
}

const NamedConvolutionKernel* Project::convolutionKernelById(ConvolutionKernelId id) const noexcept {
    for (const NamedConvolutionKernel& named : convolutionKernels_) {
        if (named.id == id) {
            return &named;
        }
    }
    return nullptr;
}

NamedConvolutionKernel* Project::convolutionKernelById(ConvolutionKernelId id) noexcept {
    for (NamedConvolutionKernel& named : convolutionKernels_) {
        if (named.id == id) {
            return &named;
        }
    }
    return nullptr;
}

bool Project::removeLayer(LayerId id) {
    const auto it = std::find_if(layers_.begin(), layers_.end(), [id](const Layer& layer) { return layer.id() == id; });
    if (it == layers_.end()) {
        return false;
    }
    layers_.erase(it);
    return true;
}

bool Project::reorderLayers(const std::vector<LayerId>& newOrderBottomToTop) {
    if (newOrderBottomToTop.size() != layers_.size()) {
        return false;
    }

    std::vector<Layer> reordered;
    reordered.reserve(layers_.size());
    // Tracks which current layers newOrderBottomToTop has already
    // consumed - without this, a duplicated id would pass the size check
    // above by silently standing in for a missing one, corrupting the
    // stack (losing a real layer) instead of being rejected.
    std::vector<bool> used(layers_.size(), false);
    for (const LayerId id : newOrderBottomToTop) {
        const auto it = std::find_if(layers_.begin(), layers_.end(), [id](const Layer& layer) { return layer.id() == id; });
        if (it == layers_.end()) {
            return false;  // Unknown id.
        }
        const auto index = static_cast<std::size_t>(std::distance(layers_.begin(), it));
        if (used[index]) {
            return false;  // Duplicate id.
        }
        used[index] = true;
        reordered.push_back(*it);
    }

    layers_ = std::move(reordered);
    return true;
}

void to_json(nlohmann::json& json, const Project& project) {
    json = nlohmann::json{
        {"settings", project.settings_},
        {"layers", project.layers_},
        {"operationLog", project.operationLog_},
        {"mindWaves", project.mindWaves_},
        {"mindShots", project.mindShots_},
        {"mindGrains", project.mindGrains_},
        {"convolutionKernels", project.convolutionKernels_},
    };
}

void from_json(const nlohmann::json& json, Project& project) {
    json.at("settings").get_to(project.settings_);
    json.at("operationLog").get_to(project.operationLog_);

    project.layers_.clear();
    for (const auto& layerJson : json.at("layers")) {
        project.layers_.push_back(layerJson.get<Layer>());
    }

    // Lenient (defaults to empty if absent) - didn't exist before
    // v0.Y.31.1 Installment C1; a project saved before it had no
    // MindWaves to lose.
    project.mindWaves_.clear();
    if (json.contains("mindWaves")) {
        for (const auto& namedJson : json.at("mindWaves")) {
            project.mindWaves_.push_back(namedJson.get<NamedMindWave>());
        }
    }

    // Lenient, same reasoning - didn't exist before v0.Y.33.1 Installment
    // A; a project saved before it had no Mind Shots to lose.
    project.mindShots_.clear();
    if (json.contains("mindShots")) {
        for (const auto& namedJson : json.at("mindShots")) {
            project.mindShots_.push_back(namedJson.get<NamedMindShot>());
        }
    }

    // Lenient, same reasoning - didn't exist before v0.Y.33.1 Installment
    // B; a project saved before it had no Mind Grains to lose.
    project.mindGrains_.clear();
    if (json.contains("mindGrains")) {
        for (const auto& namedJson : json.at("mindGrains")) {
            project.mindGrains_.push_back(namedJson.get<NamedMindGrain>());
        }
    }

    // Lenient, same reasoning - didn't exist before v0.Y.36.1 Installment
    // B; a project saved before it had no saved convolution kernels to lose.
    project.convolutionKernels_.clear();
    if (json.contains("convolutionKernels")) {
        for (const auto& namedJson : json.at("convolutionKernels")) {
            project.convolutionKernels_.push_back(namedJson.get<NamedConvolutionKernel>());
        }
    }
}

}  // namespace sound_mind::core

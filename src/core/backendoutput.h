/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2025 Xaver Hugl <xaver.hugl@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include "output.h"

namespace KWin
{

class RenderLoop;
class OutputConfiguration;
class ColorTransformation;
class IccProfile;
class OutputChangeSet;
class BrightnessDevice;
class OutputFrame;
class OutputLayer;

class KWIN_EXPORT BackendOutput : public QObject
{
    Q_OBJECT
public:
    enum class DpmsMode {
        On,
        Off,
    };
    Q_ENUM(DpmsMode)

    enum class Capability : uint {
        Dpms = 1,
        Overscan = 1 << 1,
        Vrr = 1 << 2,
        RgbRange = 1 << 3,
        HighDynamicRange = 1 << 4,
        WideColorGamut = 1 << 5,
        AutoRotation = 1 << 6,
        IccProfile = 1 << 7,
        Tearing = 1 << 8,
        BrightnessControl = 1 << 9,
        BuiltInColorProfile = 1 << 10,
        DdcCi = 1 << 11,
        MaxBitsPerColor = 1 << 12,
        Edr = 1 << 13,
        SharpnessControl = 1 << 14,
        CustomModes = 1 << 15,
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    enum class SubPixel {
        Unknown,
        None,
        Horizontal_RGB,
        Horizontal_BGR,
        Vertical_RGB,
        Vertical_BGR,
    };
    Q_ENUM(SubPixel)

    enum class RgbRange {
        Automatic = 0,
        Full = 1,
        Limited = 2,
    };
    Q_ENUM(RgbRange)

    enum class AutoRotationPolicy {
        Never = 0,
        InTabletMode,
        Always
    };
    Q_ENUM(AutoRotationPolicy);
    enum class ColorProfileSource {
        sRGB = 0,
        ICC,
        EDID,
    };
    enum class ColorPowerTradeoff {
        PreferEfficiency = 0,
        PreferAccuracy,
    };
    Q_ENUM(ColorPowerTradeoff);

    enum class EdrPolicy {
        Never = 0,
        Always,
    };
    Q_ENUM(EdrPolicy);

    explicit BackendOutput();
    ~BackendOutput() override;

    void ref();
    void unref();

    /**
     * Returns a short identifiable name of this output.
     */
    QString name() const;

    /**
     * Returns the identifying uuid of this output.
     * NOTE that this is set by the output configuration store, and
     * can potentially change on hotplug events, because displays are terrible
     */
    QString uuid() const;

    /**
     * Returns @c true if the output is enabled; otherwise returns @c false.
     */
    bool isEnabled() const;

    /**
     * Returns the position setting of this output
     */
    QPoint position() const;

    /**
     * Returns the approximate vertical refresh rate of this output, in mHz.
     */
    uint32_t refreshRate() const;

    /**
     * Returns whether this output is connected through an internal connector,
     * e.g. LVDS, or eDP.
     */
    bool isInternal() const;

    /**
     * Returns the ratio between physical pixels and logical pixels.
     */
    qreal scale() const;

    /**
     * Returns the non-rotated physical size of this output, in millimeters.
     */
    QSize physicalSize() const;

    /** Returns the resolution of the output.  */
    QSize pixelSize() const;
    QSize modeSize() const;

    QString eisaId() const;

    /**
     * Returns the manufacturer of the screen.
     */
    QString manufacturer() const;
    /**
     * Returns the model of the screen.
     */
    QString model() const;
    /**
     * Returns the serial number of the screen.
     */
    QString serialNumber() const;

    /**
     * Returns the RenderLoop for this output. If the platform does not support per screen
     * rendering, all outputs will share the same render loop.
     * FIXME: remove this and decouple RenderLoop from Output
     */
    virtual RenderLoop *renderLoop() const = 0;

    OutputTransform transform() const;
    /**
     * The transform that the user has configured, and which doesn't get changed
     * by automatic rotation
     */
    OutputTransform manualTransform() const;
    QSize orientateSize(const QSize &size) const;

    virtual void applyChanges(const OutputConfiguration &config);

    SubPixel subPixel() const;
    QString description() const;
    Capabilities capabilities() const;
    const Edid &edid() const;
    QList<std::shared_ptr<OutputMode>> modes() const;
    std::shared_ptr<OutputMode> currentMode() const;
    QSize desiredModeSize() const;
    uint32_t desiredModeRefreshRate() const;
    DpmsMode dpmsMode() const;

    uint32_t overscan() const;

    VrrPolicy vrrPolicy() const;
    RgbRange rgbRange() const;

    bool isPlaceholder() const;
    bool isNonDesktop() const;
    OutputTransform panelOrientation() const;
    bool wideColorGamut() const;
    bool highDynamicRange() const;
    uint32_t referenceLuminance() const;
    AutoRotationPolicy autoRotationPolicy() const;
    std::shared_ptr<IccProfile> iccProfile() const;
    QString iccProfilePath() const;
    /**
     * @returns the mst path of this output. Is empty if invalid
     */
    QByteArray mstPath() const;

    virtual bool setChannelFactors(const QVector3D &rgb);

    std::optional<double> maxPeakBrightness() const;
    std::optional<double> maxAverageBrightness() const;
    double minBrightness() const;
    std::optional<double> maxPeakBrightnessOverride() const;
    std::optional<double> maxAverageBrightnessOverride() const;
    std::optional<double> minBrightnessOverride() const;

    double sdrGamutWideness() const;
    ColorProfileSource colorProfileSource() const;

    double brightnessSetting() const;
    std::optional<double> currentBrightness() const;
    double artificialHdrHeadroom() const;
    double dimming() const;

    bool detectedDdcCi() const;
    bool allowDdcCi() const;
    bool isDdcCiKnownBroken() const;

    BrightnessDevice *brightnessDevice() const;
    virtual void unsetBrightnessDevice();
    bool allowSdrSoftwareBrightness() const;

    ColorPowerTradeoff colorPowerTradeoff() const;
    QString replicationSource() const;
    uint32_t maxBitsPerColor() const;
    struct BpcRange
    {
        uint32_t min = 0;
        uint32_t max = 0;
        auto operator<=>(const BpcRange &) const = default;
    };
    BpcRange bitsPerColorRange() const;
    std::optional<uint32_t> automaticMaxBitsPerColorLimit() const;
    EdrPolicy edrPolicy() const;
    std::optional<uint32_t> minVrrRefreshRateHz() const;
    double sharpnessSetting() const;

    virtual void setAutoRotateAvailable(bool isAvailable);

    virtual bool presentAsync(OutputLayer *layer, std::optional<std::chrono::nanoseconds> allowedVrrDelay);
    virtual bool testPresentation(const std::shared_ptr<OutputFrame> &frame) = 0;
    virtual bool present(const QList<OutputLayer *> &layersToUpdate, const std::shared_ptr<OutputFrame> &frame) = 0;
    virtual void repairPresentation();

    /**
     * Can be used by the backend to suggest the compositor not to
     * use overlay planes, to avoid driver issues
     */
    virtual bool overlayLayersLikelyBroken() const;

    /**
     * The color space in which the scene is blended
     */
    const std::shared_ptr<ColorDescription> &blendingColor() const;
    /**
     * The color space in which output layers are blended.
     * Note that this may be different from blendingColor.
     */
    const std::shared_ptr<ColorDescription> &layerBlendingColor() const;
    /**
     * The color space that is sent to the output, after blending
     * has happened. May be different from layerBlendingColor.
     */
    const std::shared_ptr<ColorDescription> &colorDescription() const;

    uint32_t priority() const;

    /**
     * The setting for the scale factor, which may differ from scale
     * if the screen is mirrored
     */
    double scaleSetting() const;

    /**
     * The offset at which the screen contents should be rendered,
     * used for creating black bars while mirroring.
     */
    QPoint deviceOffset() const;

Q_SIGNALS:
    /**
     * This signal is emitted when the position of this output has changed.
     */
    void positionChanged();
    /**
     * This signal is emitted when the output has been enabled or disabled.
     */
    void enabledChanged();
    /**
     * This signal is emitted when the device pixel ratio of the output has changed.
     */
    void scaleChanged();
    /**
     * This signal is emitted when the scale setting of the output has changed
     */
    void scaleSettingChanged();
    void deviceOffsetChanged();

    /**
     * Notifies that the output is about to change configuration based on a
     * user interaction.
     *
     * Be it because it gets a transformation or moved around.
     *
     * Only to be used for effects
     */
    void aboutToChange(OutputChangeSet *changeSet);

    /**
     * Notifies that the output changed based on a user interaction.
     *
     * Be it because it gets a transformation or moved around.
     *
     * Only to be used for effects
     */
    void changed();

    void outputLayersChanged();

    void currentModeChanged();
    void modesChanged();
    void transformChanged();
    void dpmsModeChanged();
    void capabilitiesChanged();
    void overscanChanged();
    void vrrPolicyChanged();
    void rgbRangeChanged();
    void wideColorGamutChanged();
    void referenceLuminanceChanged();
    void highDynamicRangeChanged();
    void autoRotationPolicyChanged();
    void iccProfileChanged();
    void iccProfilePathChanged();
    void brightnessMetadataChanged();
    void sdrGamutWidenessChanged();
    void colorDescriptionChanged();
    void blendingColorChanged();
    void colorProfileSourceChanged();
    void brightnessChanged();
    void colorPowerTradeoffChanged();
    void dimmingChanged();
    void uuidChanged();
    void replicationSourceChanged();
    void allowDdcCiChanged();
    void maxBitsPerColorChanged();
    void edrPolicyChanged();
    void sharpnessChanged();
    void priorityChanged();

protected:
    struct Information
    {
        QString name;
        QString manufacturer;
        QString model;
        QString serialNumber;
        QString eisaId;
        QSize physicalSize;
        Edid edid;
        SubPixel subPixel = SubPixel::Unknown;
        Capabilities capabilities;
        OutputTransform panelOrientation = OutputTransform::Normal;
        bool internal = false;
        bool placeholder = false;
        bool nonDesktop = false;
        QByteArray mstPath;
        std::optional<double> maxPeakBrightness;
        std::optional<double> maxAverageBrightness;
        double minBrightness = 0;
        BpcRange bitsPerColorRange;
        std::optional<uint32_t> minVrrRefreshRateHz;
    };

    struct State
    {
        QPoint position;
        qreal scale = 1;
        OutputTransform transform = OutputTransform::Normal;
        OutputTransform manualTransform = OutputTransform::Normal;
        QList<std::shared_ptr<OutputMode>> modes;
        std::shared_ptr<OutputMode> currentMode;
        QSize desiredModeSize;
        uint32_t desiredModeRefreshRate = 0;
        DpmsMode dpmsMode = DpmsMode::On;
        SubPixel subPixel = SubPixel::Unknown;
        bool enabled = false;
        uint32_t overscan = 0;
        RgbRange rgbRange = RgbRange::Automatic;
        bool wideColorGamut = false;
        bool highDynamicRange = false;
        uint32_t referenceLuminance = 200;
        AutoRotationPolicy autoRotatePolicy = AutoRotationPolicy::InTabletMode;
        QString iccProfilePath;
        std::shared_ptr<IccProfile> iccProfile;
        ColorProfileSource colorProfileSource = ColorProfileSource::sRGB;
        // color description without night light applied
        std::shared_ptr<ColorDescription> originalColorDescription = ColorDescription::sRGB;
        std::shared_ptr<ColorDescription> colorDescription = ColorDescription::sRGB;
        std::shared_ptr<ColorDescription> blendingColor = ColorDescription::sRGB;
        std::shared_ptr<ColorDescription> layerBlendingColor = ColorDescription::sRGB;
        std::optional<double> maxPeakBrightnessOverride;
        std::optional<double> maxAverageBrightnessOverride;
        std::optional<double> minBrightnessOverride;
        double sdrGamutWideness = 0;
        VrrPolicy vrrPolicy = VrrPolicy::Automatic;
        /// the desired brightness level as set by the user
        double brightnessSetting = 1.0;
        /// the actually applied brightness level
        std::optional<double> currentBrightness;
        bool allowSdrSoftwareBrightness = true;
        /// how much HDR headroom is created by increasing the backlight beyond the user setting
        double artificialHdrHeadroom = 1.0;
        ColorPowerTradeoff colorPowerTradeoff = ColorPowerTradeoff::PreferEfficiency;
        double dimming = 1.0;
        BrightnessDevice *brightnessDevice = nullptr;
        QString uuid;
        QString replicationSource;
        bool detectedDdcCi = false;
        bool allowDdcCi = true;
        uint32_t maxBitsPerColor = 0;
        std::optional<uint32_t> automaticMaxBitsPerColorLimit;
        EdrPolicy edrPolicy = EdrPolicy::Always;
        double sharpnessSetting = 0;
        uint32_t priority = 0;
        QList<CustomModeDefinition> customModes;
        double scaleSetting = 1;
        QPoint deviceOffset;
    };

    void setInformation(const Information &information);
    void setState(const State &state);

    State m_state;
    Information m_information;
    int m_refCount = 1;
};

}

Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::BackendOutput::Capabilities)

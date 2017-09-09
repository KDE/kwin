/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright © 2017-2018 Fredrik Höglund <fredrik@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

#ifndef KWINVULKANUTILS_H
#define KWINVULKANUTILS_H

// kwin
#include <kwinvulkanutils_export.h>
#include <kwinvulkanutils_funcs.h>

// Qt
#include <QtCore/QObject>
#include <QtGui/QMatrix4x4>
#include <QtGui/QRegion>
#include <QtGui/QVector4D>

// STL
#include <memory>
#include <functional>
#include <unordered_map>

// xcb
#include <xcb/xcb.h>

#include <assert.h>


namespace KWin {

/**
 * Helper function that invokes a vulkan entry point with the signature
 * vkEntryPoint(..., uint32_t *count, T *data), and returns data as an
 * std::vector<T>(count)
 */
template <typename T, typename Func, typename... Args>
std::vector<T> resultAsVector(Func function, Args&&... params)
{
    uint32_t count = 0;
    function(params..., &count, nullptr);

    std::vector<T> v(count);

    if (count > 0) {
        function(params..., &count, v.data());

        if (count < v.size())
            v.resize(count);
    }

    return v;
}



// -----------------------------------------------------------------------



template <typename T>
class VulkanArrayProxy
{
public:
    VulkanArrayProxy(std::nullptr_t) noexcept
        : m_count(0), m_data(nullptr)
    {}

    VulkanArrayProxy(const T &value) noexcept
        : m_count(1), m_data(&value)
    {}

    VulkanArrayProxy(const std::initializer_list<T> &list) noexcept
        : m_count(list.size()), m_data(list.begin())
    {}

    VulkanArrayProxy(const std::vector<T> &vector) noexcept
        : m_count(vector.size()), m_data(vector.data())
    {}

    VulkanArrayProxy(const QVector<T> &vector) noexcept
        : m_count(vector.count()), m_data(vector.constData())
    {}

    VulkanArrayProxy(const QVarLengthArray<T> &array) noexcept
        : m_count(array.count()), m_data(array.constData())
    {}

    template <size_t N>
    VulkanArrayProxy(const std::array<T, N> &array) noexcept
        : m_count(array.size()), m_data(array.data())
    {}

    template <size_t N>
    VulkanArrayProxy(const T (&array)[N]) noexcept
        : m_count(N), m_data(array)
    {}

    const T &front() const noexcept { return m_data[0]; }
    const T &back() const noexcept { return m_data[m_count - 1]; }

    const T *data() const noexcept { return m_data; }
    const T *constData() const noexcept { return m_data; }

    bool empty() const noexcept { return m_count == 0; }
    bool isEmpty() const noexcept { return m_count == 0; }

    const T *begin() const noexcept { return m_data; }
    const T *cbegin() const noexcept { return m_data; }

    const T *end() const noexcept { return m_data + m_count; }
    const T *cend() const noexcept { return m_data + m_count; }

    uint32_t size() const noexcept { return m_count; }
    uint32_t count() const noexcept { return m_count; }

private:
    const uint32_t m_count;
    const T * const m_data;
};




// -----------------------------------------------------------------------



class VulkanExtensionVector : public std::vector<VkExtensionProperties>
{
public:
    VulkanExtensionVector() : std::vector<VkExtensionProperties>() {}
    VulkanExtensionVector(std::vector<VkExtensionProperties> &&other)
        : std::vector<VkExtensionProperties>(std::move(other)) {}

    bool contains(const char *name) const {
        return std::any_of(cbegin(), cend(),
                           [=](const VkExtensionProperties &properties) {
                               return strcmp(properties.extensionName, name) == 0;
                           });
    }

    bool containsAll(const VulkanArrayProxy<const char *> &extensions) const {
        for (auto extension : extensions) {
            if (!contains(extension))
                return false;
        }
        return true;
    }
};



class VulkanLayerVector : public std::vector<VkLayerProperties>
{
public:
    VulkanLayerVector() : std::vector<VkLayerProperties>() {}
    VulkanLayerVector(std::vector<VkLayerProperties> &&other)
        : std::vector<VkLayerProperties>(std::move(other)) {}

    bool contains(const char *name) const {
        return std::any_of(cbegin(), cend(),
                           [=](const VkLayerProperties &properties) {
                               return strcmp(properties.layerName, name) == 0;
                           });
    }

    bool containsAll(const VulkanArrayProxy<const char *> &layers) const {
        for (auto layer : layers) {
            if (!contains(layer))
                return false;
        }
        return true;
    }
};



// -----------------------------------------------------------------------



/**
 * The base class for all vulkan objects.
 */
class KWINVULKANUTILS_EXPORT VulkanObject
{
public:
    virtual ~VulkanObject() {}

protected:
    VulkanObject() = default;
};



// -----------------------------------------------------------------------



class KWINVULKANUTILS_EXPORT VulkanPhysicalDevice
{
public:
    typedef VkPhysicalDevice NativeHandleType;

    VulkanPhysicalDevice() : m_physicalDevice(VK_NULL_HANDLE) {}
    VulkanPhysicalDevice(VkPhysicalDevice device) : m_physicalDevice(device) {}
    VulkanPhysicalDevice(const VulkanPhysicalDevice &device) : m_physicalDevice(device.m_physicalDevice) {}

    VulkanPhysicalDevice &operator = (const VulkanPhysicalDevice &other) {
        m_physicalDevice = other.m_physicalDevice;
        return *this;
    }

    VulkanPhysicalDevice &operator = (VkPhysicalDevice device) {
        m_physicalDevice = device;
        return *this;
    }

    operator VkPhysicalDevice () const { return m_physicalDevice; }

    VkPhysicalDevice handle() const { return m_physicalDevice; }

    bool isValid() const { return m_physicalDevice != VK_NULL_HANDLE; }

    void getFeatures(VkPhysicalDeviceFeatures *pFeatures) {
        vkGetPhysicalDeviceFeatures(m_physicalDevice, pFeatures);
    }

    void getFormatProperties(VkFormat format, VkFormatProperties *pFormatProperties) {
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, pFormatProperties);
    }

    VkResult getImageFormatProperties(VkFormat format, VkImageType type, VkImageTiling tiling,
                                      VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties *pImageFormatProperties) {
        return vkGetPhysicalDeviceImageFormatProperties(m_physicalDevice, format, type, tiling, usage, flags, pImageFormatProperties);
    }

    void getProperties(VkPhysicalDeviceProperties *pProperties) {
        vkGetPhysicalDeviceProperties(m_physicalDevice, pProperties);
    }

    void getQueueFamilyProperties(uint32_t *pQueueFamilyPropertyCount, VkQueueFamilyProperties *pQueueFamilyProperties) {
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
    }

    std::vector<VkQueueFamilyProperties> getQueueFamilyProperties() {
        return resultAsVector<VkQueueFamilyProperties>(vkGetPhysicalDeviceQueueFamilyProperties, m_physicalDevice);
    }

    void getMemoryProperties(VkPhysicalDeviceMemoryProperties *pMemoryProperties) {
        vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, pMemoryProperties);
    }

    VkResult createDevice(const VkDeviceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDevice *pDevice) {
        return vkCreateDevice(m_physicalDevice, pCreateInfo, pAllocator, pDevice);
    }

    VkResult enumerateDeviceExtensionProperties(const char *pLayerName, uint32_t *pPropertyCount, VkExtensionProperties *pProperties) {
        return vkEnumerateDeviceExtensionProperties(m_physicalDevice, pLayerName, pPropertyCount, pProperties);
    }

    VulkanExtensionVector enumerateDeviceExtensionProperties(const char *pLayerName = nullptr) {
        return resultAsVector<VkExtensionProperties>(vkEnumerateDeviceExtensionProperties, m_physicalDevice, pLayerName);
    }

    // Deprecated
    VkResult enumerateDeviceLayerProperties(uint32_t *pPropertyCount, VkLayerProperties *pProperties) {
        return vkEnumerateDeviceLayerProperties(m_physicalDevice, pPropertyCount, pProperties);
    }

    void getSparseImageFormatProperties(VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage,
                                        VkImageTiling tiling, uint32_t *pPropertyCount, VkSparseImageFormatProperties *pProperties) {
        vkGetPhysicalDeviceSparseImageFormatProperties(m_physicalDevice, format, type, samples, usage, tiling, pPropertyCount, pProperties);
    }

    std::vector<VkSparseImageFormatProperties> getSparseImageFormatProperties(VkFormat format, VkImageType type, VkSampleCountFlagBits samples,
                                                                              VkImageUsageFlags usage, VkImageTiling tiling) {
        return resultAsVector<VkSparseImageFormatProperties>(vkGetPhysicalDeviceSparseImageFormatProperties, m_physicalDevice,
                                                             format, type, samples, usage, tiling);
    }

    VkResult getSurfaceSupportKHR(uint32_t queueFamilyIndex, VkSurfaceKHR surface, VkBool32 *pSupported) {
        return pfnGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, queueFamilyIndex, surface, pSupported);
    }

    VkResult getSurfaceCapabilitiesKHR(VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR *pSurfaceCapabilities) {
        return pfnGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, surface, pSurfaceCapabilities);
    }

    VkResult getSurfaceFormatsKHR(VkSurfaceKHR surface, uint32_t *pSurfaceFormatCount, VkSurfaceFormatKHR *pSurfaceFormats) {
        return pfnGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, surface, pSurfaceFormatCount, pSurfaceFormats);
    }

    std::vector<VkSurfaceFormatKHR> getSurfaceFormatsKHR(VkSurfaceKHR surface) {
        return resultAsVector<VkSurfaceFormatKHR>(pfnGetPhysicalDeviceSurfaceFormatsKHR, m_physicalDevice, surface);
    }

    VkResult getSurfacePresentModesKHR(VkSurfaceKHR surface, uint32_t *pPresentModeCount, VkPresentModeKHR *pPresentModes) {
        return pfnGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, surface, pPresentModeCount, pPresentModes);
    }

    std::vector<VkPresentModeKHR> getSurfacePresentModesKHR(VkSurfaceKHR surface) {
        return resultAsVector<VkPresentModeKHR>(pfnGetPhysicalDeviceSurfacePresentModesKHR, m_physicalDevice, surface);
    }

#ifdef VK_USE_PLATFORM_XCB_KHR
    VkBool32 getXcbPresentationSupportKHR(uint32_t queueFamilyIndex, xcb_connection_t *connection, xcb_visualid_t visual_id) {
        return pfnGetPhysicalDeviceXcbPresentationSupportKHR(m_physicalDevice, queueFamilyIndex, connection, visual_id);
    }
#endif

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    VkBool32 getWaylandPresentationSupportKHR(uint32_t queueFamilyIndex, struct wl_display *display) {
        return pfnGetPhysicalDeviceWaylandPresentationSupportKHR(m_physicalDevice, queueFamilyIndex, display);
    }
#endif

    // VK_KHR_get_physical_device_properties2
    void getFeatures2KHR(VkPhysicalDeviceFeatures2KHR *pFeatures) {
        pfnGetPhysicalDeviceFeatures2KHR(m_physicalDevice, pFeatures);
    }

    void getProperties2KHR(VkPhysicalDeviceProperties2KHR *pProperties) {
        pfnGetPhysicalDeviceProperties2KHR(m_physicalDevice, pProperties);
    }

    void getFormatProperties2KHR(VkFormat format, VkFormatProperties2KHR *pFormatProperties) {
        pfnGetPhysicalDeviceFormatProperties2KHR(m_physicalDevice, format, pFormatProperties);
    }

    VkResult getImageFormatProperties2KHR(const VkPhysicalDeviceImageFormatInfo2KHR *pImageFormatInfo, VkImageFormatProperties2KHR *pImageFormatProperties) {
        return pfnGetPhysicalDeviceImageFormatProperties2KHR(m_physicalDevice, pImageFormatInfo, pImageFormatProperties);
    }

    void getQueueFamilyProperties2KHR(uint32_t *pQueueFamilyPropertyCount, VkQueueFamilyProperties2KHR *pQueueFamilyProperties) {
        pfnGetPhysicalDeviceQueueFamilyProperties2KHR(m_physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
    }

    void getMemoryProperties2KHR(VkPhysicalDeviceMemoryProperties2KHR* pMemoryProperties) {
        pfnGetPhysicalDeviceMemoryProperties2KHR(m_physicalDevice, pMemoryProperties);
    }

    void getSparseImageFormatProperties2KHR(const VkPhysicalDeviceSparseImageFormatInfo2KHR *pFormatInfo,
                                            uint32_t *pPropertyCount, VkSparseImageFormatProperties2KHR *pProperties) {
        pfnGetPhysicalDeviceSparseImageFormatProperties2KHR(m_physicalDevice, pFormatInfo, pPropertyCount, pProperties);
    }

    std::vector<VkSparseImageFormatProperties2KHR> getSparseImageFormatProperties2KHR(const VkPhysicalDeviceSparseImageFormatInfo2KHR *pFormatInfo) {
        return resultAsVector<VkSparseImageFormatProperties2KHR>(pfnGetPhysicalDeviceSparseImageFormatProperties2KHR, m_physicalDevice, pFormatInfo);
    }

    void getExternalBufferPropertiesKHR(const VkPhysicalDeviceExternalBufferInfoKHR *pExternalBufferInfo, VkExternalBufferPropertiesKHR *pExternalBufferProperties) {
        pfnGetPhysicalDeviceExternalBufferPropertiesKHR(m_physicalDevice, pExternalBufferInfo, pExternalBufferProperties);
    }

    void getExternalSemaphorePropertiesKHR(const VkPhysicalDeviceExternalSemaphoreInfoKHR *pExternalSemaphoreInfo,
                                           VkExternalSemaphorePropertiesKHR *pExternalSemaphoreProperties) {
        pfnGetPhysicalDeviceExternalSemaphorePropertiesKHR(m_physicalDevice, pExternalSemaphoreInfo, pExternalSemaphoreProperties);
    }

private:
    VkPhysicalDevice m_physicalDevice;
};



// -----------------------------------------------------------------------



class KWINVULKANUTILS_EXPORT VulkanInstance
{
public:
    typedef VkInstance NativeHandleType;

    VulkanInstance() : m_instance(VK_NULL_HANDLE) {}

    VulkanInstance(const VkApplicationInfo &applicationInfo,
                   const VulkanArrayProxy<const char *> &enabledLayers = {},
                   const VulkanArrayProxy<const char *> &enabledExtensions = {})
        : m_instance(VK_NULL_HANDLE)
    {
        const VkInstanceCreateInfo createInfo {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .pApplicationInfo = &applicationInfo,
            .enabledLayerCount = enabledLayers.count(),
            .ppEnabledLayerNames = enabledLayers.data(),
            .enabledExtensionCount = enabledExtensions.size(),
            .ppEnabledExtensionNames = enabledExtensions.data(),
        };

        vkCreateInstance(&createInfo, nullptr, &m_instance);
    }

    VulkanInstance(const VulkanInstance &other) = delete;

    VulkanInstance(VulkanInstance &&other)
        : m_instance(other.m_instance)
    {
        other.m_instance = VK_NULL_HANDLE;
    }

    ~VulkanInstance() {
        if (m_instance)
            vkDestroyInstance(m_instance, nullptr);
    }

    static VulkanExtensionVector supportedExtensions() {
        return resultAsVector<VkExtensionProperties>(vkEnumerateInstanceExtensionProperties, nullptr);
    }

    static VulkanLayerVector supportedLayers() {
        return resultAsVector<VkLayerProperties>(vkEnumerateInstanceLayerProperties);
    }

    VulkanInstance &operator = (const VulkanInstance &other) = delete;

    VulkanInstance &operator = (VulkanInstance &&other) {
        if (m_instance)
            vkDestroyInstance(m_instance, nullptr);

        m_instance = other.m_instance;
        other.m_instance = VK_NULL_HANDLE;
        return *this;
    }

    operator VkInstance () const { return m_instance; }
    VkInstance handle() const { return m_instance; }

    bool isValid() const { return m_instance != VK_NULL_HANDLE; }

    std::vector<VulkanPhysicalDevice> enumeratePhysicalDevices() const {
        static_assert(sizeof(VulkanPhysicalDevice) == sizeof(VkPhysicalDevice));

        uint32_t count = 0;
        vkEnumeratePhysicalDevices(m_instance, &count, nullptr);

        std::vector<VulkanPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(m_instance, &count, reinterpret_cast<VkPhysicalDevice *>(devices.data()));

        return devices;
    }

    PFN_vkVoidFunction getProcAddress(const char *name) const {
        return vkGetInstanceProcAddr(m_instance, name);
    }

    void destroySurfaceKHR(VkSurfaceKHR surface, const VkAllocationCallbacks *pAllocator = nullptr) {
        pfnDestroySurfaceKHR(m_instance, surface, pAllocator);
    }

#ifdef VK_USE_PLATFORM_XCB_KHR
    VkResult createXcbSurfaceKHR(const VkXcbSurfaceCreateInfoKHR *pCreateInfo,
                                 const VkAllocationCallbacks *pAllocator,
                                 VkSurfaceKHR *pSurface) {
        return pfnCreateXcbSurfaceKHR(m_instance, pCreateInfo, pAllocator, pSurface);
    }

    VkResult createXcbSurfaceKHR(const VkXcbSurfaceCreateInfoKHR &createInfo,
                                 const VkAllocationCallbacks *pAllocator,
                                 VkSurfaceKHR *pSurface) {
        return pfnCreateXcbSurfaceKHR(m_instance, &createInfo, pAllocator, pSurface);
    }
#endif

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    VkResult createWaylandSurfaceKHR(const VkWaylandSurfaceCreateInfoKHR *pCreateInfo,
                                     const VkAllocationCallbacks *pAllocator,
                                     VkSurfaceKHR *pSurface) {
        return pfnCreateWaylandSurfaceKHR(m_instance, pCreateInfo, pAllocator, pSurface);
    }

    VkResult createWaylandSurfaceKHR(const VkWaylandSurfaceCreateInfoKHR &createInfo,
                                     const VkAllocationCallbacks *pAllocator,
                                     VkSurfaceKHR *pSurface) {
        return pfnCreateWaylandSurfaceKHR(m_instance, &createInfo, pAllocator, pSurface);
    }
#endif

#ifdef VK_USE_PLATFORM_ANDROID_KHR
    VkResult createAndroidSurfaceKHR(const VkAndroidSurfaceCreateInfoKHR *pCreateInfo,
                                     const VkAllocationCallbacks *pAllocator,
                                     VkSurfaceKHR* pSurface) {
        return pfnCreateAndroidSurfaceKHR(m_instance, pCreateInfo, pAllocator, pSurface);
    }

    VkResult createAndroidSurfaceKHR(const VkAndroidSurfaceCreateInfoKHR &createInfo,
                                     const VkAllocationCallbacks *pAllocator,
                                     VkSurfaceKHR* pSurface) {
        return pfnCreateAndroidSurfaceKHR(m_instance, &createInfo, pAllocator, pSurface);
    }
#endif

    VkResult createDebugReportCallbackEXT(const VkDebugReportCallbackCreateInfoEXT *pCreateInfo,
                                          const VkAllocationCallbacks *pAllocator,
                                          VkDebugReportCallbackEXT *pCallback) {
        return pfnCreateDebugReportCallbackEXT(m_instance, pCreateInfo, pAllocator, pCallback);
    }

    VkResult createDebugReportCallbackEXT(const VkDebugReportCallbackCreateInfoEXT &createInfo,
                                          const VkAllocationCallbacks *pAllocator,
                                          VkDebugReportCallbackEXT *pCallback) {
        return pfnCreateDebugReportCallbackEXT(m_instance, &createInfo, pAllocator, pCallback);
    }

    void destroyDebugReportCallbackEXT(VkDebugReportCallbackEXT callback, const VkAllocationCallbacks *pAllocator) {
        pfnDestroyDebugReportCallbackEXT(m_instance, callback, pAllocator);
    }

    void debugReportMessageEXT(VkDebugReportFlagsEXT flags,
                               VkDebugReportObjectTypeEXT objectType,
                               uint64_t object,
                               size_t location,
                               int32_t messageCode,
                               const char *pLayerPrefix,
                               const char *pMessage) {
        pfnDebugReportMessageEXT(m_instance, flags, objectType, object, location, messageCode, pLayerPrefix, pMessage);
    }

private:
    VkInstance m_instance;
};



// -----------------------------------------------------------------------



class VulkanDebugReportCallback
{
public:
    typedef VkDebugReportCallbackEXT NativeHandleType;

    VulkanDebugReportCallback()
        : m_instance(VK_NULL_HANDLE),
          m_callback(VK_NULL_HANDLE)
    {
    }

    VulkanDebugReportCallback(VulkanInstance &instance, VkDebugReportFlagsEXT flags, PFN_vkDebugReportCallbackEXT pfnCallback, void *pUserData = nullptr)
        : m_instance(instance),
          m_callback(VK_NULL_HANDLE)
    {
        const VkDebugReportCallbackCreateInfoEXT createInfo = {
            .sType       = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT,
            .pNext       = nullptr,
            .flags       = flags,
            .pfnCallback = pfnCallback,
            .pUserData   = pUserData
        };

        instance.createDebugReportCallbackEXT(&createInfo, nullptr, &m_callback);
    }

    VulkanDebugReportCallback(VulkanInstance &instance, const VkDebugReportCallbackCreateInfoEXT *pCreateInfo)
        : m_instance(instance),
          m_callback(VK_NULL_HANDLE)
    {
        instance.createDebugReportCallbackEXT(pCreateInfo, nullptr, &m_callback);
    }

    VulkanDebugReportCallback(VulkanInstance &instance, const VkDebugReportCallbackCreateInfoEXT &createInfo)
        : m_instance(instance),
          m_callback(VK_NULL_HANDLE)
    {
        instance.createDebugReportCallbackEXT(&createInfo, nullptr, &m_callback);
    }

    VulkanDebugReportCallback(const VulkanDebugReportCallback &other) = delete;

    VulkanDebugReportCallback(VulkanDebugReportCallback &&other)
        : m_instance(other.m_instance),
          m_callback(other.m_callback)
    {
        other.m_instance = VK_NULL_HANDLE;
        other.m_callback = VK_NULL_HANDLE;
    }

    ~VulkanDebugReportCallback() {
        if (m_callback)
            pfnDestroyDebugReportCallbackEXT(m_instance, m_callback, nullptr);
    }

    VkDebugReportCallbackEXT handle() const { return m_callback; }

    operator VkDebugReportCallbackEXT () const { return m_callback; }

    VulkanDebugReportCallback &operator = (const VulkanDebugReportCallback &other) = delete;

    VulkanDebugReportCallback &operator = (VulkanDebugReportCallback &&other)
    {
        if (m_callback)
            pfnDestroyDebugReportCallbackEXT(m_instance, m_callback, nullptr);

        m_instance = other.m_instance;
        m_callback = other.m_callback;

        other.m_instance = VK_NULL_HANDLE;
        other.m_callback = VK_NULL_HANDLE;
        return *this;
    }

private:
    VkInstance m_instance;
    VkDebugReportCallbackEXT m_callback;
};



// -----------------------------------------------------------------------



class VulkanQueue;


/**
 * Represents a logical device.
 */
class KWINVULKANUTILS_EXPORT VulkanDevice
{
public:
    typedef VkDevice NativeHandleType;

    VulkanDevice()
        : m_physicalDevice(VK_NULL_HANDLE),
          m_device(VK_NULL_HANDLE)
    {
    }

    VulkanDevice(VulkanPhysicalDevice physicalDevice, const VkDeviceCreateInfo &createInfo, const VkAllocationCallbacks *pAllocator = nullptr)
        : m_physicalDevice(physicalDevice),
          m_device(VK_NULL_HANDLE)
    {
        vkCreateDevice(physicalDevice, &createInfo, pAllocator, &m_device);
    }

    VulkanDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator = nullptr)
        : m_physicalDevice(physicalDevice),
          m_device(VK_NULL_HANDLE)
    {
        vkCreateDevice(physicalDevice, pCreateInfo, pAllocator, &m_device);
    }

    VulkanDevice(const VulkanDevice &other) = delete;

    VulkanDevice(VulkanDevice &&other)
        : m_physicalDevice(other.m_physicalDevice),
          m_device(other.m_device)
    {
        other.m_physicalDevice = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;
    }

    ~VulkanDevice() {
        if (m_device)
            vkDestroyDevice(m_device, nullptr);
    }

    operator VkDevice () const { return m_device; }

    VulkanDevice &operator = (const VulkanDevice &other) = delete;

    VulkanDevice &operator = (VulkanDevice &&other) {
        if (m_device)
            vkDestroyDevice(m_device, nullptr);

        m_physicalDevice = other.m_physicalDevice;
        m_device = other.m_device;

        other.m_physicalDevice = VK_NULL_HANDLE;
        other.m_device = VK_NULL_HANDLE;

        return *this;
    }

    bool isValid() const { return m_device != VK_NULL_HANDLE; }

    VulkanPhysicalDevice physicalDevice() const { return m_physicalDevice; }
    VkDevice handle() const { return m_device; }

    PFN_vkVoidFunction getProcAddress(const char *pName) {
        return vkGetDeviceProcAddr(m_device, pName);
    }

    void getQueue(uint32_t queueFamilyIndex, uint32_t queueIndex, VkQueue *pQueue) {
        vkGetDeviceQueue(m_device, queueFamilyIndex, queueIndex, pQueue);
    }

    VulkanQueue getQueue(uint32_t queueFamilyIndex, uint32_t queueIndex);

    void waitIdle() {
        vkDeviceWaitIdle(m_device);
    }

    VkResult allocateMemory(const VkMemoryAllocateInfo *pAllocateInfo, const VkAllocationCallbacks *pAllocator, VkDeviceMemory *pMemory) {
        return vkAllocateMemory(m_device, pAllocateInfo, pAllocator, pMemory);
    }

    void freeMemory(VkDeviceMemory memory, const VkAllocationCallbacks *pAllocator = nullptr) {
        vkFreeMemory(m_device, memory, pAllocator);
    }

    VkResult mapMemory(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void **ppData) {
        return vkMapMemory(m_device, memory, offset, size, flags, ppData);
    }

    void unmapMemory(VkDeviceMemory memory) {
        vkUnmapMemory(m_device, memory);
    }

    VkResult flushMappedMemoryRanges(uint32_t memoryRangeCount, const VkMappedMemoryRange *pMemoryRanges) {
        return vkFlushMappedMemoryRanges(m_device, memoryRangeCount, pMemoryRanges);
    }

    VkResult flushMappedMemoryRanges(const VulkanArrayProxy<VkMappedMemoryRange> &memoryRanges) {
        return vkFlushMappedMemoryRanges(m_device, memoryRanges.count(), memoryRanges.data());
    }

    VkResult invalidateMappedMemoryRanges(uint32_t memoryRangeCount, const VkMappedMemoryRange *pMemoryRanges) {
        return vkInvalidateMappedMemoryRanges(m_device, memoryRangeCount, pMemoryRanges);
    }

    VkResult invalidateMappedMemoryRanges(const VulkanArrayProxy<VkMappedMemoryRange> &memoryRanges) {
        return vkInvalidateMappedMemoryRanges(m_device, memoryRanges.count(), memoryRanges.data());
    }

    void getMemoryCommitment(VkDeviceMemory memory, VkDeviceSize *pCommittedMemoryInBytes) {
        vkGetDeviceMemoryCommitment(m_device, memory, pCommittedMemoryInBytes);
    }

    VkResult bindBufferMemory(VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
        return vkBindBufferMemory(m_device, buffer, memory, memoryOffset);
    }

    VkResult bindImageMemory(VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
        return vkBindImageMemory(m_device, image, memory, memoryOffset);
    }

    // VK_KHR_bind_memory2
    void bindBufferMemory2KHR(uint32_t bindInfoCount, const VkBindBufferMemoryInfoKHR* pBindInfos) {
        pfnBindBufferMemory2KHR(m_device, bindInfoCount, pBindInfos);
    }

    void bindBufferMemory2KHR(const VulkanArrayProxy<VkBindBufferMemoryInfoKHR> &bindInfos) {
        pfnBindBufferMemory2KHR(m_device, bindInfos.count(), bindInfos.data());
    }

    void bindImageMemory2KHR(uint32_t bindInfoCount, const VkBindImageMemoryInfoKHR* pBindInfos) {
        pfnBindImageMemory2KHR(m_device, bindInfoCount, pBindInfos);
    }

    void bindImageMemory2KHR(const VulkanArrayProxy<VkBindImageMemoryInfoKHR> &bindInfos) {
        pfnBindImageMemory2KHR(m_device, bindInfos.count(), bindInfos.data());
    }

    void getBufferMemoryRequirements(VkBuffer buffer, VkMemoryRequirements *pMemoryRequirements) {
        vkGetBufferMemoryRequirements(m_device, buffer, pMemoryRequirements);
    }

    void getImageMemoryRequirements(VkImage image, VkMemoryRequirements *pMemoryRequirements) {
        vkGetImageMemoryRequirements(m_device, image, pMemoryRequirements);
    }

    void getImageSparseMemoryRequirements(VkImage image, uint32_t *pSparseMemoryRequirementCount,
                                          VkSparseImageMemoryRequirements *pSparseMemoryRequirements) {
        vkGetImageSparseMemoryRequirements(m_device, image, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
    }

    // VK_KHR_get_memory_requirements2
    void getImageMemoryRequirements2KHR(const VkImageMemoryRequirementsInfo2KHR *pInfo, VkMemoryRequirements2KHR *pMemoryRequirements) {
        pfnGetImageMemoryRequirements2KHR(m_device, pInfo, pMemoryRequirements);
    }

    void getImageMemoryRequirements2KHR(const VkImageMemoryRequirementsInfo2KHR &info, VkMemoryRequirements2KHR *pMemoryRequirements) {
        pfnGetImageMemoryRequirements2KHR(m_device, &info, pMemoryRequirements);
    }

    void getBufferMemoryRequirements2KHR(const VkBufferMemoryRequirementsInfo2KHR *pInfo, VkMemoryRequirements2KHR *pMemoryRequirements) {
        pfnGetBufferMemoryRequirements2KHR(m_device, pInfo, pMemoryRequirements);
    }

    void getBufferMemoryRequirements2KHR(const VkBufferMemoryRequirementsInfo2KHR &info, VkMemoryRequirements2KHR *pMemoryRequirements) {
        pfnGetBufferMemoryRequirements2KHR(m_device, &info, pMemoryRequirements);
    }

    void getImageSparseMemoryRequirements2KHR(const VkImageSparseMemoryRequirementsInfo2KHR *pInfo,
                                             uint32_t *pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2KHR *pSparseMemoryRequirements) {
        pfnGetImageSparseMemoryRequirements2KHR(m_device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
    }

    void getImageSparseMemoryRequirements2KHR(const VkImageSparseMemoryRequirementsInfo2KHR &info, uint32_t *pSparseMemoryRequirementCount,
                                              VkSparseImageMemoryRequirements2KHR *pSparseMemoryRequirements) {
        pfnGetImageSparseMemoryRequirements2KHR(m_device, &info, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
    }

    VkResult createFence(const VkFenceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkFence *pFence) {
        return vkCreateFence(m_device, pCreateInfo, pAllocator, pFence);
    }

    void destroyFence(VkFence fence, const VkAllocationCallbacks *pAllocator = nullptr) {
        vkDestroyFence(m_device, fence, pAllocator);
    }

    VkResult resetFences(uint32_t fenceCount, const VkFence *pFences) {
        return vkResetFences(m_device, fenceCount, pFences);
    }

    VkResult getFenceStatus(VkFence fence) {
        return vkGetFenceStatus(m_device, fence);
    }

    VkResult waitForFences(uint32_t fenceCount, const VkFence *pFences, VkBool32 waitAll = VK_TRUE, uint64_t timeout = UINT64_MAX) {
        return vkWaitForFences(m_device, fenceCount, pFences, waitAll, timeout);
    }

    VkResult createSemaphore(const VkSemaphoreCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSemaphore *pSemaphore) {
        return vkCreateSemaphore(m_device, pCreateInfo, pAllocator, pSemaphore);
    }

    void destroySemaphore(VkSemaphore semaphore, const VkAllocationCallbacks *pAllocator = nullptr) {
        vkDestroySemaphore(m_device, semaphore, pAllocator);
    }

    VkResult createEvent(const VkEventCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkEvent *pEvent) {
        return vkCreateEvent(m_device, pCreateInfo, pAllocator, pEvent);
    }

    void destroyEvent(VkEvent event, const VkAllocationCallbacks *pAllocator = nullptr) {
        vkDestroyEvent(m_device, event, pAllocator);
    }

    VkResult getEventStatus(VkEvent event) {
        return vkGetEventStatus(m_device, event);
    }

    VkResult setEvent(VkEvent event) {
        return vkSetEvent(m_device, event);
    }

    VkResult resetEvent(VkEvent event) {
        return vkResetEvent(m_device, event);
    }

    VkResult createQueryPool(const VkQueryPoolCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkQueryPool *pQueryPool) {
        return vkCreateQueryPool(m_device, pCreateInfo, pAllocator, pQueryPool);
    }

    void destroyQueryPool(VkQueryPool queryPool, const VkAllocationCallbacks *pAllocator = nullptr) {
        vkDestroyQueryPool(m_device, queryPool, pAllocator);
    }

    VkResult getQueryPoolResults(VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount,
                                 size_t dataSize, void *pData, VkDeviceSize stride, VkQueryResultFlags flags) {
        return vkGetQueryPoolResults(m_device, queryPool, firstQuery, queryCount, dataSize, pData, stride, flags);
    }

    VkResult createBuffer(const VkBufferCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkBuffer *pBuffer) {
        return vkCreateBuffer(m_device, pCreateInfo, pAllocator, pBuffer);
    }

    void destroyBuffer(VkBuffer buffer, const VkAllocationCallbacks *pAllocator = nullptr) {
        vkDestroyBuffer(m_device, buffer, pAllocator);
    }

    VkResult createBufferView(const VkBufferViewCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkBufferView *pView) {
        return vkCreateBufferView(m_device, pCreateInfo, pAllocator, pView);
    }

    void destroyBufferView(VkBufferView bufferView, const VkAllocationCallbacks *pAllocator = nullptr) {
        vkDestroyBufferView(m_device, bufferView, pAllocator);
    }

    VkResult createImage(const VkImageCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkImage *pImage) {
        return vkCreateImage(m_device, pCreateInfo, pAllocator, pImage);
    }

    void destroyImage(VkImage image, const VkAllocationCallbacks *pAllocator = nullptr) {
        vkDestroyImage(m_device, image, pAllocator);
    }

    void getImageSubresourceLayout(VkImage image, const VkImageSubresource *pSubresource, VkSubresourceLayout *pLayout) {
        vkGetImageSubresourceLayout(m_device, image, pSubresource, pLayout);
    }

    VkResult createImageView(const VkImageViewCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkImageView *pView) {
        return vkCreateImageView(m_device, pCreateInfo, pAllocator, pView);
    }

    void destroyImageView(VkImageView imageView, const VkAllocationCallbacks *pAllocator = nullptr) {
        vkDestroyImageView(m_device, imageView, pAllocator);
    }

    VkResult createShaderModule(const VkShaderModuleCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkShaderModule *pShaderModule) {
        return vkCreateShaderModule(m_device, pCreateInfo, pAllocator, pShaderModule);
    }

    void destroyShaderModule(VkShaderModule shaderModule, const VkAllocationCallbacks *pAllocator = nullptr) {
        vkDestroyShaderModule(m_device, shaderModule, pAllocator);
    }

    VkResult createPipelineCache(const VkPipelineCacheCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkPipelineCache *pPipelineCache) {
        return vkCreatePipelineCache(m_device, pCreateInfo, pAllocator, pPipelineCache);
    }

    VkResult createPipelineCache(const VkPipelineCacheCreateInfo &createInfo, const VkAllocationCallbacks *pAllocator, VkPipelineCache *pPipelineCache) {
        return vkCreatePipelineCache(m_device, &createInfo, pAllocator, pPipelineCache);
    }

    void destroyPipelineCache(VkPipelineCache pipelineCache, const VkAllocationCallbacks *pAllocator = nullptr) {
        vkDestroyPipelineCache(m_device, pipelineCache, pAllocator);
    }

    VkResult getPipelineCacheData(VkPipelineCache pipelineCache, size_t *pDataSize, void *pData) {
        return vkGetPipelineCacheData(m_device, pipelineCache, pDataSize, pData);
    }

    VkResult mergePipelineCaches(VkPipelineCache dstCache, uint32_t srcCacheCount, const VkPipelineCache *pSrcCaches) {
        return vkMergePipelineCaches(m_device, dstCache, srcCacheCount, pSrcCaches);
    }

    VkResult createGraphicsPipelines(VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo *pCreateInfos,
                                     const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines) {
        return vkCreateGraphicsPipelines(m_device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
    }

    VkResult createComputePipelines(VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo *pCreateInfos,
                                    const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines) {
        return vkCreateComputePipelines(m_device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
    }

    void destroyPipeline(VkPipeline pipeline, const VkAllocationCallbacks *pAllocator = nullptr) {
        vkDestroyPipeline(m_device, pipeline, pAllocator);
    }

    VkResult createPipelineLayout(const VkPipelineLayoutCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator,
                                  VkPipelineLayout *pPipelineLayout) {
        return vkCreatePipelineLayout(m_device, pCreateInfo, pAllocator, pPipelineLayout);
    }

    VkResult createPipelineLayout(const VkPipelineLayoutCreateInfo &createInfo, const VkAllocationCallbacks *pAllocator,
                                  VkPipelineLayout *pPipelineLayout) {
        return vkCreatePipelineLayout(m_device, &createInfo, pAllocator, pPipelineLayout);
    }

    void destroyPipelineLayout(VkPipelineLayout pipelineLayout, const VkAllocationCallbacks *pAllocator = nullptr) {
        vkDestroyPipelineLayout(m_device, pipelineLayout, pAllocator);
    }

    VkResult createSampler(const VkSamplerCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSampler *pSampler) {
        return vkCreateSampler(m_device, pCreateInfo, pAllocator, pSampler);
    }

    void destroySampler(VkSampler sampler, const VkAllocationCallbacks *pAllocator = nullptr) {
        vkDestroySampler(m_device, sampler, pAllocator);
    }

    VkResult createDescriptorSetLayout(const VkDescriptorSetLayoutCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator,
                                       VkDescriptorSetLayout *pSetLayout) {
        return vkCreateDescriptorSetLayout(m_device, pCreateInfo, pAllocator, pSetLayout);
    }

    VkResult createDescriptorSetLayout(const VkDescriptorSetLayoutCreateInfo &createInfo, const VkAllocationCallbacks *pAllocator,
                                       VkDescriptorSetLayout *pSetLayout) {
        return vkCreateDescriptorSetLayout(m_device, &createInfo, pAllocator, pSetLayout);
    }

    void destroyDescriptorSetLayout(VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks *pAllocator = nullptr) {
        vkDestroyDescriptorSetLayout(m_device, descriptorSetLayout, pAllocator);
    }

    VkResult createDescriptorPool(const VkDescriptorPoolCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator,
                                  VkDescriptorPool *pDescriptorPool) {
        return vkCreateDescriptorPool(m_device, pCreateInfo, pAllocator, pDescriptorPool);
    }

    void destroyDescriptorPool(VkDescriptorPool descriptorPool, const VkAllocationCallbacks *pAllocator = nullptr) {
        vkDestroyDescriptorPool(m_device, descriptorPool, pAllocator);
    }

    VkResult resetDescriptorPool(VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags) {
        return vkResetDescriptorPool(m_device, descriptorPool, flags);
    }

    VkResult allocateDescriptorSets(const VkDescriptorSetAllocateInfo *pAllocateInfo, VkDescriptorSet *pDescriptorSets) {
        return vkAllocateDescriptorSets(m_device, pAllocateInfo, pDescriptorSets);
    }

    VkResult freeDescriptorSets(VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet *pDescriptorSets) {
        return vkFreeDescriptorSets(m_device, descriptorPool, descriptorSetCount, pDescriptorSets);
    }

    void updateDescriptorSets(uint32_t descriptorWriteCount, const VkWriteDescriptorSet *pDescriptorWrites,
                              uint32_t descriptorCopyCount, const VkCopyDescriptorSet *pDescriptorCopies) {
        vkUpdateDescriptorSets(m_device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
    }

    VkResult createFramebuffer(const VkFramebufferCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkFramebuffer *pFramebuffer) {
        return vkCreateFramebuffer(m_device, pCreateInfo, pAllocator, pFramebuffer);
    }

    void destroyFramebuffer(VkFramebuffer framebuffer, const VkAllocationCallbacks *pAllocator = nullptr) {
        vkDestroyFramebuffer(m_device, framebuffer, pAllocator);
    }

    VkResult createRenderPass(const VkRenderPassCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkRenderPass *pRenderPass) {
        return vkCreateRenderPass(m_device, pCreateInfo, pAllocator, pRenderPass);
    }

    void destroyRenderPass(VkRenderPass renderPass, const VkAllocationCallbacks *pAllocator = nullptr) {
        vkDestroyRenderPass(m_device, renderPass, pAllocator);
    }

    void getRenderAreaGranularity(VkRenderPass renderPass, VkExtent2D *pGranularity) {
        vkGetRenderAreaGranularity(m_device, renderPass, pGranularity);
    }

    VkResult createCommandPool(const VkCommandPoolCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkCommandPool *pCommandPool) {
        return vkCreateCommandPool(m_device, pCreateInfo, pAllocator, pCommandPool);
    }

    void destroyCommandPool(VkCommandPool commandPool, const VkAllocationCallbacks *pAllocator = nullptr) {
        vkDestroyCommandPool(m_device, commandPool, pAllocator);
    }

    VkResult resetCommandPool(VkCommandPool commandPool, VkCommandPoolResetFlags flags) {
        return vkResetCommandPool(m_device, commandPool, flags);
    }

    VkResult allocateCommandBuffers(const VkCommandBufferAllocateInfo *pAllocateInfo, VkCommandBuffer *pCommandBuffers) {
        return vkAllocateCommandBuffers(m_device, pAllocateInfo, pCommandBuffers);
    }

    void freeCommandBuffers(VkCommandPool commandPool, uint32_t commandBufferCount, const VkCommandBuffer *pCommandBuffers) {
        vkFreeCommandBuffers(m_device, commandPool, commandBufferCount, pCommandBuffers);
    }

    // VK_KHR_swapchain
    VkResult createSwapchainKHR(const VkSwapchainCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSwapchainKHR *pSwapchain) {
        return pfnCreateSwapchainKHR(m_device, pCreateInfo, pAllocator, pSwapchain);
    }

    void destroySwapchainKHR(VkSwapchainKHR swapchain, const VkAllocationCallbacks *pAllocator = nullptr) {
        pfnDestroySwapchainKHR(m_device, swapchain, pAllocator);
    }

    VkResult getSwapchainImagesKHR(VkSwapchainKHR swapchain, uint32_t *pSwapchainImageCount, VkImage *pSwapchainImages) {
        return pfnGetSwapchainImagesKHR(m_device, swapchain, pSwapchainImageCount, pSwapchainImages);
    }

    VkResult acquireNextImageKHR(VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t *pImageIndex) {
        return pfnAcquireNextImageKHR(m_device, swapchain, timeout, semaphore, fence, pImageIndex);
    }

    // VK_KHR_maintenance1
    void trimCommandPoolKHR(VkCommandPool commandPool, VkCommandPoolTrimFlagsKHR flags) {
        pfnTrimCommandPoolKHR(m_device, commandPool, flags);
    }

    // VK_KHR_descriptor_update_template
    VkResult createDescriptorUpdateTemplateKHR(const VkDescriptorUpdateTemplateCreateInfoKHR *pCreateInfo,
                                               const VkAllocationCallbacks *pAllocator, VkDescriptorUpdateTemplateKHR *pDescriptorUpdateTemplate) {
        return pfnCreateDescriptorUpdateTemplateKHR(m_device, pCreateInfo, pAllocator, pDescriptorUpdateTemplate);
    }

    void destroyDescriptorUpdateTemplateKHR(VkDescriptorUpdateTemplateKHR descriptorUpdateTemplate, const VkAllocationCallbacks *pAllocator = nullptr) {
        pfnDestroyDescriptorUpdateTemplateKHR(m_device, descriptorUpdateTemplate, pAllocator);
    }

    void updateDescriptorSetWithTemplateKHR(VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplateKHR descriptorUpdateTemplate, const void *pData) {
        pfnUpdateDescriptorSetWithTemplateKHR(m_device, descriptorSet, descriptorUpdateTemplate, pData);
    }

    // VK_KHR_external_memory_fd
    VkResult getMemoryFdKHR(const VkMemoryGetFdInfoKHR *pGetFdInfo, int *pFd) {
        return pfnGetMemoryFdKHR(m_device, pGetFdInfo, pFd);
    }

    VkResult getMemoryFdKHR(const VkMemoryGetFdInfoKHR &getFdInfo, int *pFd) {
        return pfnGetMemoryFdKHR(m_device, &getFdInfo, pFd);
    }

    VkResult getMemoryFdPropertiesKHR(VkExternalMemoryHandleTypeFlagBitsKHR handleType, int fd, VkMemoryFdPropertiesKHR *pMemoryFdProperties) {
        return pfnGetMemoryFdPropertiesKHR(m_device, handleType, fd, pMemoryFdProperties);
    }

    // VK_KHR_external_semaphore_fd
    VkResult importSemaphoreFdKHR(const VkImportSemaphoreFdInfoKHR *pImportSemaphoreFdInfo) {
        return pfnImportSemaphoreFdKHR(m_device, pImportSemaphoreFdInfo);
    }

    VkResult getSemaphoreFdKHR(const VkSemaphoreGetFdInfoKHR *pGetFdInfo, int *pFd) {
        return pfnGetSemaphoreFdKHR(m_device, pGetFdInfo, pFd);
    }

    VkResult getSemaphoreFdKHR(const VkSemaphoreGetFdInfoKHR &getFdInfo, int *pFd) {
        return pfnGetSemaphoreFdKHR(m_device, &getFdInfo, pFd);
    }

private:
    VulkanPhysicalDevice m_physicalDevice;
    VkDevice m_device;
};



// -----------------------------------------------------------------------



/**
 * Represents a queue in a logical device.
 *
 * Note that this class does not inherit from VulkanObject, and does not have
 * a destructor. Queues are owned by the device, and calling vkGetDeviceQueue
 * with the same parameters multiple times returns the same queue. That means
 * that copying VulkanQueue objects is also safe.
 */
class KWINVULKANUTILS_EXPORT VulkanQueue
{
public:
    typedef VkQueue NativeHandleType;

    VulkanQueue()
        : m_device(nullptr),
          m_queue(VK_NULL_HANDLE),
          m_familyIndex(0)
    {
    }

    VulkanQueue(VulkanDevice *device, uint32_t queueFamilyIndex, uint32_t queueIndex)
        : m_device(device),
          m_queue(VK_NULL_HANDLE),
          m_familyIndex(queueFamilyIndex),
          m_index(queueIndex)
    {
        device->getQueue(queueFamilyIndex, queueIndex, &m_queue);
    }

    VulkanDevice *device() const { return m_device; }
    uint32_t familyIndex() const { return m_familyIndex; }
    uint32_t index() const { return m_index; }
    bool isValid() const { return m_queue != VK_NULL_HANDLE; }

    VkResult submit(uint32_t submitCount, const VkSubmitInfo *pSubmits, VkFence fence = VK_NULL_HANDLE) {
        return vkQueueSubmit(m_queue, submitCount, pSubmits, fence);
    }

    VkResult submit(const VulkanArrayProxy<VkSubmitInfo> &submitInfo, VkFence fence = VK_NULL_HANDLE) {
        return vkQueueSubmit(m_queue, submitInfo.count(), submitInfo.data(), fence);
    }

    VkResult waitIdle() {
        return vkQueueWaitIdle(m_queue);
    }

    VkResult bindSparse(uint32_t bindInfoCount, const VkBindSparseInfo *pBindInfo, VkFence fence) {
        return vkQueueBindSparse(m_queue, bindInfoCount, pBindInfo, fence);
    }

    VkResult presentKHR(const VkPresentInfoKHR *pPresentInfo) {
        return pfnQueuePresentKHR(m_queue, pPresentInfo);
    }

    VkResult presentKHR(const VkPresentInfoKHR &presentInfo) {
        return pfnQueuePresentKHR(m_queue, &presentInfo);
    }

private:
    VulkanDevice *m_device;
    VkQueue m_queue;
    uint32_t m_familyIndex;
    uint32_t m_index;
};

inline VulkanQueue VulkanDevice::getQueue(uint32_t queueFamilyIndex, uint32_t queueIndex) {
    return VulkanQueue(this, queueFamilyIndex, queueIndex);
}



// -----------------------------------------------------------------------



class KWINVULKANUTILS_EXPORT VulkanSurface : public VulkanObject
{
public:
    typedef VkSurfaceKHR NativeHandleType;

#ifdef VK_USE_PLATFORM_XCB_KHR
    VulkanSurface(VulkanInstance *instance, xcb_connection_t *connection, xcb_window_t window)
        : m_instance(instance), m_surface(VK_NULL_HANDLE)
    {
        const VkXcbSurfaceCreateInfoKHR createInfo = {
            .sType      = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
            .pNext      = nullptr,
            .flags      = 0,
            .connection = connection,
            .window     = window
        };
        instance->createXcbSurfaceKHR(&createInfo, nullptr, &m_surface);
    }

    VulkanSurface(VulkanInstance *instance, const VkXcbSurfaceCreateInfoKHR &createInfo)
        : m_instance(instance), m_surface(VK_NULL_HANDLE)
    {
        instance->createXcbSurfaceKHR(&createInfo, nullptr, &m_surface);
    }
#endif

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    VulkanSurface(VulkanInstance *instance, wl_display *display, wl_surface *surface)
        : m_instance(instance), m_surface(VK_NULL_HANDLE)
    {
        const VkWaylandSurfaceCreateInfoKHR createInfo = {
            .sType      = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
            .pNext      = nullptr,
            .flags      = 0,
            .display    = display,
            .surface    = surface
        };

        instance->createWaylandSurfaceKHR(&createInfo, nullptr, &m_surface);
    }

    VulkanSurface(VulkanInstance *instance, const VkWaylandSurfaceCreateInfoKHR &createInfo)
        : m_instance(instance), m_surface(VK_NULL_HANDLE)
    {
        instance->createWaylandSurfaceKHR(&createInfo, nullptr, &m_surface);
    }
#endif

#ifdef VK_USE_PLATFORM_ANDROID_KHR
    VulkanSurface(VulkanInstance *instance, ANativeWindow *window)
        : m_instance(instance), m_surface(VK_NULL_HANDLE)
    {
        const VkAndroidSurfaceCreateInfoKHR createInfo = {
            .sType      = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
            .pNext      = nullptr,
            .flags      = 0,
            .window     = window
        };

        instance->createAndroidSurfaceKHR(&createInfo, nullptr, &m_surface);
    }

    VulkanSurface(VulkanInstance *instance, const VkAndroidSurfaceCreateInfoKHR &createInfo)
        : m_instance(instance), m_surface(VK_NULL_HANDLE)
    {
        instance->createAndroidSurfaceKHR(&createInfo, nullptr, &m_surface);
    }
#endif

    VulkanSurface(const VulkanSurface &) = delete;

    VulkanSurface(VulkanSurface &&other)
    {
        m_instance = other.m_instance;
        m_surface = other.m_surface;

        other.m_instance = nullptr;
        other.m_surface = VK_NULL_HANDLE;
    }

    ~VulkanSurface() override
    {
        if (m_surface)
            m_instance->destroySurfaceKHR(m_surface, nullptr);
    }

    bool isValid() const { return m_surface != VK_NULL_HANDLE; }

    VkSurfaceKHR handle() const { return m_surface; }

    operator VkSurfaceKHR() const { return m_surface; }

    VulkanSurface &operator = (const VulkanSurface &other) = delete;

    VulkanSurface &operator = (VulkanSurface &&other)
    {
        m_instance = other.m_instance;
        m_surface = other.m_surface;

        other.m_instance = nullptr;
        other.m_surface = VK_NULL_HANDLE;
        return *this;
    }

private:
    VulkanInstance *m_instance;
    VkSurfaceKHR m_surface;
};



// -----------------------------------------------------------------------



class KWINVULKANUTILS_EXPORT VulkanImage : public VulkanObject
{
public:
    typedef VkImage NativeHandleType;

    VulkanImage(VulkanDevice *device, const VkImageCreateInfo &createInfo)
        : VulkanObject(),
          m_device(device),
          m_image(VK_NULL_HANDLE),
          m_type(createInfo.imageType),
          m_format(createInfo.format),
          m_usage(createInfo.usage),
          m_extent(createInfo.extent),
          m_mipLevels(createInfo.mipLevels)
    {
        device->createImage(&createInfo, nullptr, &m_image);
    }

    VulkanImage(const VulkanImage &) = delete;

    VulkanImage(VulkanImage &&other)
        : VulkanObject(),
          m_device(other.m_device),
          m_image(other.m_image),
          m_type(other.m_type),
          m_format(other.m_format),
          m_usage(other.m_usage),
          m_extent(other.m_extent),
          m_mipLevels(other.m_mipLevels)
    {
        other.m_device    = nullptr;
        other.m_image     = VK_NULL_HANDLE;
        other.m_type      = VK_IMAGE_TYPE_1D;
        other.m_format    = VK_FORMAT_UNDEFINED;
        other.m_usage     = 0;
        other.m_extent    = { 0, 0, 0 };
        other.m_mipLevels = 0;
    }

    ~VulkanImage() override {
        if (m_image)
            m_device->destroyImage(m_image);
    }

    VulkanImage &operator = (const VulkanImage &) = delete;

    VulkanImage &operator = (VulkanImage &&other) {
        if (m_image)
            m_device->destroyImage(m_image);

        m_device    = other.m_device;
        m_image     = other.m_image;
        m_type      = other.m_type;
        m_format    = other.m_format;
        m_usage     = other.m_usage;
        m_extent    = other.m_extent;
        m_mipLevels = other.m_mipLevels;

        other.m_device    = nullptr;
        other.m_image     = VK_NULL_HANDLE;
        other.m_type      = VK_IMAGE_TYPE_1D;
        other.m_format    = VK_FORMAT_UNDEFINED;
        other.m_usage     = 0;
        other.m_extent    = { 0, 0, 0 };
        other.m_mipLevels = 0;

        return *this;
    }

    operator VkImage () const { return m_image; }

    VkImage handle() const { return m_image; }

    VulkanDevice *device() const { return m_device; }
    VkImageType type() const { return m_type; }
    VkFormat format() const { return m_format; }
    VkImageUsageFlags usage() const { return m_usage; }
    uint32_t width(int level = 0) const { return m_extent.width >> level; }
    uint32_t height(int level = 0) const { return m_extent.height >> level; }
    uint32_t depth(int level = 0) const { return m_extent.depth >> level; }
    uint32_t mipLevelCount() const { return m_mipLevels; }
    const VkExtent3D &extent() const { return m_extent; }
    const QSize size(int level = 0) const { return QSize(width(level), height(level)); }
    const QRect rect(int level = 0) const { return QRect(0, 0, width(level), height(level)); }

    VkSubresourceLayout getSubresourceLayout(const VkImageSubresource &subresource) {
        VkSubresourceLayout layout;
        m_device->getImageSubresourceLayout(m_image, &subresource, &layout);
        return layout;
    }

    bool isValid() const { return m_image != VK_NULL_HANDLE; }

private:
    VulkanDevice *m_device;
    VkImage m_image;
    VkImageType m_type;
    VkFormat m_format;
    VkImageUsageFlags m_usage;
    VkExtent3D m_extent;
    uint32_t m_mipLevels;
};



// -----------------------------------------------------------------------



/**
 * VulkanStagingImage extents VulkanImage with a setData() and a data() method,
 * allowing a pointer to the image data to be stored in the image object.
 */
class VulkanStagingImage : public VulkanImage
{
public:
    typedef VkImage NativeHandleType;

    VulkanStagingImage(VulkanDevice *device, const VkImageCreateInfo &createInfo)
        : VulkanImage(device, createInfo)
    {
    }

    VulkanStagingImage(const VulkanStagingImage &other) = delete;

    VulkanStagingImage(VulkanStagingImage &&other)
        : VulkanImage(std::forward<VulkanStagingImage>(other)),
          m_data(other.m_data)
    {
        other.m_data = nullptr;
    }

    VulkanStagingImage &operator = (const VulkanStagingImage &other) = delete;

    VulkanStagingImage &operator = (VulkanStagingImage &&other) {
        VulkanImage::operator = (std::forward<VulkanStagingImage>(other));
        m_data = other.m_data;
        other.m_data = nullptr;
        return *this;
    }

    void setData(uint8_t *data) { m_data = data; }
    uint8_t *data() const { return m_data; }

private:
    uint8_t *m_data = nullptr;
};



// -----------------------------------------------------------------------



class KWINVULKANUTILS_EXPORT VulkanImageView : public VulkanObject
{
public:
    typedef VkImageView NativeHandleType;

    VulkanImageView(VulkanDevice *device, const VkImageViewCreateInfo &createInfo)
        : VulkanObject(),
          m_device(device),
          m_imageView(VK_NULL_HANDLE),
          m_type(createInfo.viewType),
          m_format(createInfo.format)
    {
        device->createImageView(&createInfo, nullptr, &m_imageView);
    }

    VulkanImageView(const VulkanImageView &) = delete;

    VulkanImageView(VulkanImageView &&other)
        : VulkanObject(),
          m_device(other.m_device),
          m_imageView(other.m_imageView),
          m_type(other.m_type),
          m_format(other.m_format)
    {
        other.m_device    = nullptr;
        other.m_imageView = VK_NULL_HANDLE;
        other.m_type      = VK_IMAGE_VIEW_TYPE_1D;
        other.m_format    = VK_FORMAT_UNDEFINED;
    }

    ~VulkanImageView() override {
        if (m_imageView)
            m_device->destroyImageView(m_imageView);
    }

    VulkanImageView &operator = (const VulkanImageView &) = delete;

    VulkanImageView &operator = (VulkanImageView &&other) {
        if (m_imageView)
            m_device->destroyImageView(m_imageView);

        m_device    = other.m_device;
        m_imageView = other.m_imageView;
        m_type      = other.m_type;
        m_format    = other.m_format;

        other.m_device    = nullptr;
        other.m_imageView = VK_NULL_HANDLE;
        other.m_type      = VK_IMAGE_VIEW_TYPE_1D;
        other.m_format    = VK_FORMAT_UNDEFINED;

        return *this;
    }

    operator VkImageView () const { return m_imageView; }

    VkImageView handle() const { return m_imageView; }

    VulkanDevice *device() const { return m_device; }
    VkImageViewType type() const { return m_type; }
    VkFormat format() const { return m_format; }

    bool isValid() const { return m_imageView != VK_NULL_HANDLE; }

private:
    VulkanDevice *m_device;
    VkImageView m_imageView;
    VkImageViewType m_type;
    VkFormat m_format;
};



// -----------------------------------------------------------------------



class KWINVULKANUTILS_EXPORT VulkanFramebuffer : public VulkanObject
{
public:
    typedef VkFramebuffer NativeHandleType;

    VulkanFramebuffer()
        : VulkanObject(),
          m_device(nullptr),
          m_framebuffer(VK_NULL_HANDLE),
          m_width(0),
          m_height(0),
          m_layers(0)
    {
    }

    VulkanFramebuffer(VulkanDevice *device, const VkFramebufferCreateInfo &createInfo)
        : VulkanObject(),
          m_device(device),
          m_framebuffer(VK_NULL_HANDLE),
          m_width(createInfo.width),
          m_height(createInfo.height),
          m_layers(createInfo.layers)
    {
        device->createFramebuffer(&createInfo, nullptr, &m_framebuffer);
    }

    VulkanFramebuffer(const VulkanFramebuffer &) = delete;

    VulkanFramebuffer(VulkanFramebuffer &&other)
        : VulkanObject(),
          m_device(other.m_device),
          m_framebuffer(other.m_framebuffer),
          m_width(other.m_width),
          m_height(other.m_height),
          m_layers(other.m_layers)
    {
        other.m_device      = nullptr;
        other.m_framebuffer = VK_NULL_HANDLE;
        other.m_width       = 0;
        other.m_height      = 0;
        other.m_layers      = 0;
    }

    ~VulkanFramebuffer() override {
        if (m_framebuffer)
            m_device->destroyFramebuffer(m_framebuffer);
    }

    VulkanFramebuffer &operator = (const VulkanFramebuffer &) = delete;

    VulkanFramebuffer &operator = (VulkanFramebuffer &&other) {
        if (m_framebuffer)
            m_device->destroyFramebuffer(m_framebuffer);

        m_device      = other.m_device;
        m_framebuffer = other.m_framebuffer;
        m_width       = other.m_width;
        m_height      = other.m_height;
        m_layers      = other.m_layers;

        other.m_device      = nullptr;
        other.m_framebuffer = VK_NULL_HANDLE;
        other.m_width       = 0;
        other.m_height      = 0;
        other.m_layers      = 0;

        return *this;
    }

    operator VkFramebuffer () const { return m_framebuffer; }

    VkFramebuffer handle() const { return m_framebuffer; }

    bool isValid() const { return m_framebuffer != VK_NULL_HANDLE; }

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    uint32_t layers() const { return m_layers; }

private:
    VulkanDevice *m_device;
    VkFramebuffer m_framebuffer;
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_layers;
};



// -----------------------------------------------------------------------



class KWINVULKANUTILS_EXPORT VulkanBuffer : public VulkanObject
{
public:
    typedef VkBuffer NativeHandleType;

    VulkanBuffer(VulkanDevice *device, const VkBufferCreateInfo &createInfo)
        : VulkanObject(),
          m_device(device),
          m_buffer(VK_NULL_HANDLE),
          m_size(createInfo.size),
          m_usage(createInfo.usage)
    {
        device->createBuffer(&createInfo, nullptr, &m_buffer);
    }

    VulkanBuffer(const VulkanBuffer &) = delete;

    VulkanBuffer(VulkanBuffer &&other)
        : VulkanObject(),
          m_device(other.m_device),
          m_buffer(other.m_buffer),
          m_size(other.m_size),
          m_usage(other.m_usage)
    {
        other.m_device = nullptr;
        other.m_buffer = VK_NULL_HANDLE;
        other.m_size   = 0;
        other.m_usage  = 0;
    }

    ~VulkanBuffer() override {
        if (m_buffer)
            m_device->destroyBuffer(m_buffer);
    }

    VulkanBuffer &operator = (const VulkanBuffer &) = delete;

    VulkanBuffer &operator = (VulkanBuffer &&other) {
        if (m_buffer)
            m_device->destroyBuffer(m_buffer);

        m_device = other.m_device;
        m_buffer = other.m_buffer;
        m_size   = other.m_size;
        m_usage  = other.m_usage;

        other.m_device = nullptr;
        other.m_buffer = VK_NULL_HANDLE;
        other.m_size   = 0;
        other.m_usage  = 0;

        return *this;
    }

    operator VkBuffer () const { return m_buffer; }

    VkBuffer handle() const { return m_buffer; }

    bool isValid() const { return m_buffer != VK_NULL_HANDLE; }
    VkDeviceSize size() const { return m_size; }
    VkBufferUsageFlags usage() const { return m_usage; }

private:
    VulkanDevice *m_device;
    VkBuffer m_buffer;
    VkDeviceSize m_size;
    VkBufferUsageFlags m_usage;
};



// -----------------------------------------------------------------------



class KWINVULKANUTILS_EXPORT VulkanBufferView : public VulkanObject
{
public:
    typedef VkBufferView NativeHandleType;

    VulkanBufferView()
        : m_device(nullptr),
          m_bufferView(VK_NULL_HANDLE),
          m_format(VK_FORMAT_UNDEFINED),
          m_offset(0),
          m_range(0)
    {
    }

    VulkanBufferView(VulkanDevice *device, const VkBufferViewCreateInfo &createInfo)
        : m_device(device),
          m_bufferView(VK_NULL_HANDLE),
          m_format(createInfo.format),
          m_offset(createInfo.offset),
          m_range(createInfo.range)
    {
        device->createBufferView(&createInfo, nullptr, &m_bufferView);
    }

    VulkanBufferView(VulkanBufferView &&other)
        : m_device(other.m_device),
          m_bufferView(other.m_bufferView),
          m_format(other.m_format),
          m_offset(other.m_offset),
          m_range(other.m_range)
    {
        other.m_device     = nullptr;
        other.m_bufferView = VK_NULL_HANDLE;
        other.m_format     = VK_FORMAT_UNDEFINED;
        other.m_offset     = 0;
        other.m_range      = 0;
    }

    VulkanBufferView(const VulkanBufferView &other) = delete;

    ~VulkanBufferView() {
        if (m_bufferView)
            m_device->destroyBufferView(m_bufferView);
    }

    bool isValid() const { return m_bufferView != VK_NULL_HANDLE; }

    VkBufferView handle() const { return m_bufferView; }
    operator VkBufferView () const { return m_bufferView; }

    VkFormat format() const { return m_format; }
    VkDeviceSize offset() const { return m_offset; }
    VkDeviceSize range() const { return m_range; }

    VulkanBufferView &operator = (const VulkanBufferView &other) = delete;

    VulkanBufferView &operator = (VulkanBufferView &&other) {
        if (m_bufferView)
            m_device->destroyBufferView(m_bufferView);

        m_device     = other.m_device;
        m_bufferView = other.m_bufferView;
        m_format     = other.m_format;
        m_offset     = other.m_offset;
        m_range      = other.m_range;

        other.m_device     = nullptr;
        other.m_bufferView = VK_NULL_HANDLE;
        other.m_format     = VK_FORMAT_UNDEFINED;
        other.m_offset     = 0;
        other.m_range      = 0;
        return *this;
    }

private:
    VulkanDevice *m_device;
    VkBufferView m_bufferView;
    VkFormat m_format;
    VkDeviceSize m_offset;
    VkDeviceSize m_range;
};



// -----------------------------------------------------------------------



class KWINVULKANUTILS_EXPORT VulkanSampler : public VulkanObject
{
public:
    typedef VkSampler NativeHandleType;

    VulkanSampler()
        : m_device(nullptr),
          m_sampler(VK_NULL_HANDLE)
    {
    }

    VulkanSampler(VulkanDevice *device, const VkSamplerCreateInfo &createInfo)
        : m_device(device),
          m_sampler(VK_NULL_HANDLE)
    {
        device->createSampler(&createInfo, nullptr, &m_sampler);
    }

    VulkanSampler(VulkanSampler &&other)
        : m_device(other.m_device),
          m_sampler(other.m_sampler)
    {
        other.m_device  = nullptr;
        other.m_sampler = VK_NULL_HANDLE;
    }

    VulkanSampler(const VulkanSampler &other) = delete;

    ~VulkanSampler() {
        if (m_sampler)
            m_device->destroySampler(m_sampler);
    }

    bool isValid() const { return m_sampler != VK_NULL_HANDLE; }

    VkSampler handle() const { return m_sampler; }
    operator VkSampler () const { return m_sampler; }

    VulkanSampler &operator = (const VulkanSampler &other) = delete;

    VulkanSampler &operator = (VulkanSampler &&other) {
        if (m_sampler)
            m_device->destroySampler(m_sampler);

        m_device  = other.m_device;
        m_sampler = other.m_sampler;

        other.m_device  = nullptr;
        other.m_sampler = VK_NULL_HANDLE;
        return *this;
    }

private:
    VulkanDevice *m_device;
    VkSampler m_sampler;
};



// -----------------------------------------------------------------------



class KWINVULKANUTILS_EXPORT VulkanRenderPass : public VulkanObject
{
public:
    typedef VkRenderPass NativeHandleType;

    VulkanRenderPass()
        : m_device(nullptr),
          m_renderPass(VK_NULL_HANDLE)
    {
    }

    VulkanRenderPass(VulkanDevice *device, const VkRenderPassCreateInfo &createInfo)
        : m_device(device),
          m_renderPass(VK_NULL_HANDLE)
    {
        device->createRenderPass(&createInfo, nullptr, &m_renderPass);
    }

    VulkanRenderPass(VulkanDevice *device,
                     const VulkanArrayProxy<VkAttachmentDescription> attachments,
                     const VulkanArrayProxy<VkSubpassDescription> subpasses,
                     const VulkanArrayProxy<VkSubpassDependency> dependencies = {})
        : m_device(device),
          m_renderPass(VK_NULL_HANDLE)
    {
        const VkRenderPassCreateInfo createInfo = {
            .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext           = nullptr,
            .flags           = 0,
            .attachmentCount = attachments.count(),
            .pAttachments    = attachments.data(),
            .subpassCount    = subpasses.count(),
            .pSubpasses      = subpasses.data(),
            .dependencyCount = dependencies.count(),
            .pDependencies   = dependencies.data(),
        };

        device->createRenderPass(&createInfo, nullptr, &m_renderPass);
    }

    VulkanRenderPass(VulkanRenderPass &&other)
        : m_device(other.m_device),
          m_renderPass(other.m_renderPass)
    {
        other.m_device = VK_NULL_HANDLE;
        other.m_renderPass = VK_NULL_HANDLE;
    }

    VulkanRenderPass(const VulkanRenderPass &other) = delete;

    ~VulkanRenderPass() {
        if (m_renderPass)
            m_device->destroyRenderPass(m_renderPass);
    }

    bool isValid() const { return m_renderPass != VK_NULL_HANDLE; }

    VkRenderPass handle() const { return m_renderPass; }
    operator VkRenderPass () const { return m_renderPass; }

    VulkanRenderPass &operator = (const VulkanRenderPass &other) = delete;

    VulkanRenderPass &operator = (VulkanRenderPass &&other) {
        if (m_renderPass)
            m_device->destroyRenderPass(m_renderPass);

        m_device = other.m_device;
        m_renderPass = other.m_renderPass;

        other.m_device = VK_NULL_HANDLE;
        other.m_renderPass = VK_NULL_HANDLE;
        return *this;
    }

private:
    VulkanDevice *m_device;
    VkRenderPass m_renderPass;
};



// -----------------------------------------------------------------------



class KWINVULKANUTILS_EXPORT VulkanCommandPool : public VulkanObject
{
public:
    typedef VkCommandPool NativeHandleType;

    VulkanCommandPool()
        : m_device(nullptr),
          m_commandPool(VK_NULL_HANDLE),
          m_createFlags(0),
          m_queueFamilyIndex(0)
    {
    }

    VulkanCommandPool(VulkanDevice *device, const VkCommandPoolCreateInfo &createInfo)
        : m_device(device),
          m_commandPool(VK_NULL_HANDLE),
          m_createFlags(createInfo.flags),
          m_queueFamilyIndex(createInfo.queueFamilyIndex)
    {
        device->createCommandPool(&createInfo, nullptr, &m_commandPool);
    }

    VulkanCommandPool(VulkanDevice *device, VkCommandPoolCreateFlags flags, uint32_t queueFamilyIndex)
        : m_device(device),
          m_commandPool(VK_NULL_HANDLE),
          m_createFlags(flags),
          m_queueFamilyIndex(queueFamilyIndex)
    {
        const VkCommandPoolCreateInfo createInfo = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = flags,
            .queueFamilyIndex = queueFamilyIndex
        };

        device->createCommandPool(&createInfo, nullptr, &m_commandPool);
    }

    VulkanCommandPool(VulkanCommandPool &&other)
        : m_device(other.m_device),
          m_commandPool(other.m_commandPool),
          m_queueFamilyIndex(other.m_queueFamilyIndex)
    {
        other.m_device  = nullptr;
        other.m_commandPool = VK_NULL_HANDLE;
        other.m_queueFamilyIndex = 0;
    }

    VulkanCommandPool(const VulkanCommandPool &other) = delete;

    ~VulkanCommandPool() {
        if (m_commandPool)
            m_device->destroyCommandPool(m_commandPool);
    }

    /**
     * Recycles all of the resources from all of the command buffers allocated
     * from the command pool back to the command pool. All command buffers that
     * have been allocated from the command pool are put in the initial state.
     */
    void reset(VkCommandPoolResetFlags flags = 0) {
        m_device->resetCommandPool(m_commandPool, flags);
    }

    /**
     * Recycles unused memory back to the system.
     *
     * This function requires VK_KHR_maintenance1.
     */
    void trim(VkCommandPoolTrimFlagsKHR flags = 0) {
        m_device->trimCommandPoolKHR(m_commandPool, flags);
    }

    bool isValid() const { return m_commandPool != VK_NULL_HANDLE; }
    bool commandBuffersResettable() const { return m_createFlags & VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; }
    uint32_t queueFamilyIndex() const { return m_queueFamilyIndex; }

    VkCommandPool handle() const { return m_commandPool; }
    operator VkCommandPool () const { return m_commandPool; }

    VulkanCommandPool &operator = (const VulkanCommandPool &other) = delete;

    VulkanCommandPool &operator = (VulkanCommandPool &&other) {
        if (m_commandPool)
            m_device->destroyCommandPool(m_commandPool);

        m_device  = other.m_device;
        m_commandPool = other.m_commandPool;
        m_queueFamilyIndex = other.m_queueFamilyIndex;

        other.m_device  = nullptr;
        other.m_commandPool = VK_NULL_HANDLE;
        other.m_queueFamilyIndex = 0;
        return *this;
    }

private:
    VulkanDevice *m_device;
    VkCommandPool m_commandPool;
    VkCommandPoolCreateFlags m_createFlags;
    uint32_t m_queueFamilyIndex;
};



// -----------------------------------------------------------------------


                                                   
class KWINVULKANUTILS_EXPORT VulkanCommandBuffer : public VulkanObject
{
public:
    typedef VkCommandBuffer NativeHandleType;

    VulkanCommandBuffer()
        : m_device(nullptr),
          m_commandPool(VK_NULL_HANDLE),
          m_commandBuffer(VK_NULL_HANDLE),
          m_autoDelete(false),
          m_active(false),
          m_renderPassActive(false)
    {
    }

    VulkanCommandBuffer(VulkanDevice *device, VkCommandPool commandPool, VkCommandBufferLevel level)
        : m_device(device),
          m_commandPool(commandPool),
          m_commandBuffer(VK_NULL_HANDLE),
          m_autoDelete(true),
          m_active(false),
          m_renderPassActive(false)
    {
        const VkCommandBufferAllocateInfo allocateInfo = {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext              = nullptr,
            .commandPool        = commandPool,
            .level              = level,
            .commandBufferCount = 1
        };

        device->allocateCommandBuffers(&allocateInfo, &m_commandBuffer);
    }

    VulkanCommandBuffer(const VulkanCommandBuffer &) = delete;

    VulkanCommandBuffer(VulkanCommandBuffer &&other)
        : m_device(other.m_device),
          m_commandPool(other.m_commandPool),
          m_commandBuffer(other.m_commandBuffer),
          m_autoDelete(other.m_autoDelete),
          m_active(other.m_active),
          m_renderPassActive(other.m_renderPassActive)
    {
        other.m_device           = nullptr;
        other.m_commandPool      = VK_NULL_HANDLE;
        other.m_commandBuffer    = VK_NULL_HANDLE;
        other.m_autoDelete       = false;
        other.m_active           = false;
        other.m_renderPassActive = false;
    }

    ~VulkanCommandBuffer() {
        if (m_autoDelete && m_commandBuffer)
            free();
    }

    VulkanCommandBuffer &operator = (VulkanCommandBuffer &&other) {
        if (m_autoDelete && m_commandBuffer)
            free();

        m_device           = other.m_device;
        m_commandPool      = other.m_commandPool;
        m_commandBuffer    = other.m_commandBuffer;
        m_autoDelete       = other.m_autoDelete;
        m_active           = other.m_active;
        m_renderPassActive = other.m_renderPassActive;

        other.m_device           = nullptr;
        other.m_commandPool      = VK_NULL_HANDLE;
        other.m_commandBuffer    = VK_NULL_HANDLE;
        other.m_autoDelete       = false;
        other.m_active           = false;
        other.m_renderPassActive = false;

        return *this;
    }

    /**
     * Returns true if the command buffer is valid; and false otherwise.
     */
    bool isValid() const { return m_commandBuffer != VK_NULL_HANDLE; }

    /**
     * Returns true if the command buffer is in the recording state; and false otherwise.
     */
    bool isActive() const { return m_active; }

    /**
     * Returns true if a render pass instance is being recorded; and false otherwise.
     */
    bool isRenderPassActive() const { return m_renderPassActive; }

    /**
     * Sets whether the command buffer is automatically freed when the
     * VulkanCommandBuffer object is destroyed.
     *
     * Setting this property to false allows all command buffers allocated
     * from a command pool to be freed at the same time.
     *
     * The default value is true.
     */
    void setAutoDelete(bool enable) { m_autoDelete = enable; }

    /**
     * Returns true if the command buffer will be freed when the destructor is invoked; and false otherwise.
     */
    bool autoDelete() const { return m_autoDelete; }

    /**
     * Resets the command buffer, allowing it be recorded again.
     */
    void reset(VkCommandBufferResetFlags flags = 0) {
        vkResetCommandBuffer(m_commandBuffer, flags);
    }

    /**
     * Releases the command buffer back to the command pool.
     */
    void free() {
        m_device->freeCommandBuffers(m_commandPool, 1, &m_commandBuffer);

        m_device = nullptr;
        m_commandBuffer = VK_NULL_HANDLE;
        m_commandPool = VK_NULL_HANDLE;
    }

    /**
     * Returns the command buffer handle.
     */
    VkCommandBuffer handle() const { return m_commandBuffer; }

    /**
     * Returns the command buffer handle.
     */
    operator VkCommandBuffer () const { return m_commandBuffer; }

    /**
     * Begins recording the command buffer.
     *
     * This function also resets the command buffer.
     */
    void begin(VkCommandBufferUsageFlags flags, const VkCommandBufferInheritanceInfo *pInheritanceInfo = nullptr) {
        const VkCommandBufferBeginInfo beginInfo = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = nullptr,
            .flags            = flags,
            .pInheritanceInfo = pInheritanceInfo
        };

        vkBeginCommandBuffer(m_commandBuffer, &beginInfo);
        m_active = true;
    }

    void end() {
        vkEndCommandBuffer(m_commandBuffer);
        m_active = false;
    }

    void bindPipeline(VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) {
        vkCmdBindPipeline(m_commandBuffer, pipelineBindPoint, pipeline);
    }

    void setViewport(uint32_t firstViewport, uint32_t viewportCount, const VkViewport *pViewports) {
        vkCmdSetViewport(m_commandBuffer, firstViewport, viewportCount, pViewports);
    }

    void setViewport(uint32_t firstViewport, const VulkanArrayProxy<VkViewport> &viewports) {
        vkCmdSetViewport(m_commandBuffer, firstViewport, viewports.count(), viewports.data());
    }

    void setScissor(uint32_t firstScissor, uint32_t scissorCount, const VkRect2D *pScissors) {
        vkCmdSetScissor(m_commandBuffer, firstScissor, scissorCount, pScissors);
    }

    void setScissor(uint32_t firstScissor, const VulkanArrayProxy<VkRect2D> &scissors) {
        vkCmdSetScissor(m_commandBuffer, firstScissor, scissors.count(), scissors.data());
    }

    void setLineWidth(float lineWidth) {
        vkCmdSetLineWidth(m_commandBuffer, lineWidth);
    }

    void setDepthBias(float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor) {
        vkCmdSetDepthBias(m_commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
    }

    void setBlendConstants(const float blendConstants[4]) {
        vkCmdSetBlendConstants(m_commandBuffer, blendConstants);
    }

    void setDepthBounds(float minDepthBounds, float maxDepthBounds) {
        vkCmdSetDepthBounds(m_commandBuffer, minDepthBounds, maxDepthBounds);
    }

    void setStencilCompareMask(VkStencilFaceFlags faceMask, uint32_t compareMask) {
        vkCmdSetStencilCompareMask(m_commandBuffer, faceMask, compareMask);
    }

    void setStencilWriteMask(VkStencilFaceFlags faceMask, uint32_t writeMask) {
        vkCmdSetStencilWriteMask(m_commandBuffer, faceMask, writeMask);
    }

    void setStencilReference(VkStencilFaceFlags faceMask, uint32_t reference) {
        vkCmdSetStencilReference(m_commandBuffer, faceMask, reference);
    }

    void bindDescriptorSets(VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout,
                            uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet *pDescriptorSets,
                            uint32_t dynamicOffsetCount, const uint32_t *pDynamicOffsets) {
        vkCmdBindDescriptorSets(m_commandBuffer, pipelineBindPoint, layout, firstSet,
                                descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
    }

    void bindDescriptorSets(VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout,
                            uint32_t firstSet, const VulkanArrayProxy<VkDescriptorSet> &descriptorSets,
                            const VulkanArrayProxy<uint32_t> &dynamicOffsets = {}) {
        vkCmdBindDescriptorSets(m_commandBuffer, pipelineBindPoint, layout, firstSet,
                                descriptorSets.count(), descriptorSets.data(),
                                dynamicOffsets.count(), dynamicOffsets.data());
    }

    void bindIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) {
        vkCmdBindIndexBuffer(m_commandBuffer, buffer, offset, indexType);
    }

    void bindVertexBuffers(uint32_t firstBinding, uint32_t bindingCount, const VkBuffer *pBuffers, const VkDeviceSize *pOffsets) {
        vkCmdBindVertexBuffers(m_commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
    }

    void bindVertexBuffers(uint32_t firstBinding, const VulkanArrayProxy<VkBuffer> &buffers, const VulkanArrayProxy<VkDeviceSize> &offsets) {
        assert(buffers.count() == offsets.count());
        vkCmdBindVertexBuffers(m_commandBuffer, firstBinding, buffers.count(), buffers.data(), offsets.data());
    }

    void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
        vkCmdDraw(m_commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
        vkCmdDrawIndexed(m_commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    void drawIndirect(VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
        vkCmdDrawIndirect(m_commandBuffer, buffer, offset, drawCount, stride);
    }

    void drawIndexedIndirect(VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
        vkCmdDrawIndexedIndirect(m_commandBuffer, buffer, offset, drawCount, stride);
    }

    void dispatch(uint32_t x, uint32_t y, uint32_t z) {
        vkCmdDispatch(m_commandBuffer, x, y, z);
    }

    void dispatchIndirect(VkBuffer buffer, VkDeviceSize offset) {
        vkCmdDispatchIndirect(m_commandBuffer, buffer, offset);
    }

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy *pRegions) {
        vkCmdCopyBuffer(m_commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
    }

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, const VulkanArrayProxy<VkBufferCopy> &regions) {
        vkCmdCopyBuffer(m_commandBuffer, srcBuffer, dstBuffer, regions.count(), regions.data());
    }

    void copyImage(VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout,
                   uint32_t regionCount, const VkImageCopy *pRegions) {
        vkCmdCopyImage(m_commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
    }

    void blitImage(VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout,
                   uint32_t regionCount, const VkImageBlit *pRegions, VkFilter filter) {
        vkCmdBlitImage(m_commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter);
    }

    void copyBufferToImage(VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout,
                           uint32_t regionCount, const VkBufferImageCopy *pRegions) {
        vkCmdCopyBufferToImage(m_commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
    }

    void copyImageToBuffer(VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer,
                           uint32_t regionCount, const VkBufferImageCopy *pRegions) {
        vkCmdCopyImageToBuffer(m_commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions);
    }

    void updateBuffer(VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void *pData) {
        vkCmdUpdateBuffer(m_commandBuffer, dstBuffer, dstOffset, dataSize, pData);
    }

    void fillBuffer(VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data) {
        vkCmdFillBuffer(m_commandBuffer, dstBuffer, dstOffset, size, data);
    }

    void clearColorImage(VkImage image, VkImageLayout imageLayout, const VkClearColorValue *pColor,
                         uint32_t rangeCount, const VkImageSubresourceRange *pRanges) {
        vkCmdClearColorImage(m_commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
    }

    void clearDepthStencilImage(VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue *pDepthStencil,
                                uint32_t rangeCount, const VkImageSubresourceRange *pRanges) {
        vkCmdClearDepthStencilImage(m_commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
    }

    void clearAttachments(uint32_t attachmentCount, const VkClearAttachment *pAttachments, uint32_t rectCount, const VkClearRect *pRects) {
        vkCmdClearAttachments(m_commandBuffer, attachmentCount, pAttachments, rectCount, pRects);
    }

    void resolveImage(VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout,
                      uint32_t regionCount, const VkImageResolve *pRegions) {
        vkCmdResolveImage(m_commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
    }

    void setEvent(VkEvent event, VkPipelineStageFlags stageMask) {
        vkCmdSetEvent(m_commandBuffer, event, stageMask);
    }

    void resetEvent(VkEvent event, VkPipelineStageFlags stageMask) {
        vkCmdResetEvent(m_commandBuffer, event, stageMask);
    }

    void waitEvents(uint32_t eventCount, const VkEvent *pEvents,
                    VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
                    uint32_t memoryBarrierCount, const VkMemoryBarrier *pMemoryBarriers,
                    uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier *pBufferMemoryBarriers,
                    uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier *pImageMemoryBarriers) {
        vkCmdWaitEvents(m_commandBuffer, eventCount, pEvents,
                        srcStageMask, dstStageMask,
                        memoryBarrierCount, pMemoryBarriers,
                        bufferMemoryBarrierCount, pBufferMemoryBarriers,
                        imageMemoryBarrierCount, pImageMemoryBarriers);
    }

    void pipelineBarrier(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags,
                         uint32_t memoryBarrierCount, const VkMemoryBarrier *pMemoryBarriers,
                         uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier *pBufferMemoryBarriers,
                         uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier *pImageMemoryBarriers) {
        vkCmdPipelineBarrier(m_commandBuffer, srcStageMask, dstStageMask, dependencyFlags,
                             memoryBarrierCount, pMemoryBarriers,
                             bufferMemoryBarrierCount, pBufferMemoryBarriers,
                             imageMemoryBarrierCount, pImageMemoryBarriers);
    }

    void pipelineBarrier(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags,
                         const VulkanArrayProxy<VkMemoryBarrier> &memoryBarriers,
                         const VulkanArrayProxy<VkBufferMemoryBarrier> &bufferMemoryBarriers,
                         const VulkanArrayProxy<VkImageMemoryBarrier> &imageMemoryBarriers) {
        vkCmdPipelineBarrier(m_commandBuffer, srcStageMask, dstStageMask, dependencyFlags,
                             memoryBarriers.count(), memoryBarriers.data(),
                             bufferMemoryBarriers.count(), bufferMemoryBarriers.data(),
                             imageMemoryBarriers.count(), imageMemoryBarriers.data());
    }

    void beginQuery(VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags) {
        vkCmdBeginQuery(m_commandBuffer, queryPool, query, flags);
    }

    void endQuery(VkQueryPool queryPool, uint32_t query) {
        vkCmdEndQuery(m_commandBuffer, queryPool, query);
    }

    void resetQueryPool(VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) {
        vkCmdResetQueryPool(m_commandBuffer, queryPool, firstQuery, queryCount);
    }

    void writeTimestamp(VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query) {
        vkCmdWriteTimestamp(m_commandBuffer, pipelineStage, queryPool, query);
    }

    void copyQueryPoolResults(VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount,
                                   VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags) {
        vkCmdCopyQueryPoolResults(m_commandBuffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset, stride, flags);
    }

    void pushConstants(VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void *pValues) {
        vkCmdPushConstants(m_commandBuffer, layout, stageFlags, offset, size, pValues);
    }

    void pushDescriptorSetKHR(VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set,
                              uint32_t descriptorWriteCount, const VkWriteDescriptorSet *pDescriptorWrites) {
        pfnCmdPushDescriptorSetKHR(m_commandBuffer, pipelineBindPoint, layout, set,
                                   descriptorWriteCount, pDescriptorWrites);
    }

    void pushDescriptorSetKHR(VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set,
                              const VulkanArrayProxy<VkWriteDescriptorSet> &descriptorWrites) {
        pfnCmdPushDescriptorSetKHR(m_commandBuffer, pipelineBindPoint, layout, set,
                                   descriptorWrites.count(), descriptorWrites.data());
    }

    void pushDescriptorSetWithTemplateKHR(VkDescriptorUpdateTemplateKHR descriptorUpdateTemplate, VkPipelineLayout layout, uint32_t set, const void *pData) {
        pfnCmdPushDescriptorSetWithTemplateKHR(m_commandBuffer, descriptorUpdateTemplate, layout, set, pData);
    }

    void beginRenderPass(const VkRenderPassBeginInfo *pRenderPassBegin, VkSubpassContents contents) {
        vkCmdBeginRenderPass(m_commandBuffer, pRenderPassBegin, contents);
        m_renderPassActive = true;
    }

    void beginRenderPass(const VkRenderPassBeginInfo &renderPassBegin, VkSubpassContents contents) {
        vkCmdBeginRenderPass(m_commandBuffer, &renderPassBegin, contents);
        m_renderPassActive = true;
    }

    void nextSubpass(VkSubpassContents contents) {
        vkCmdNextSubpass(m_commandBuffer, contents);
    }

    void endRenderPass() {
        vkCmdEndRenderPass(m_commandBuffer);
        m_renderPassActive = false;
    }

    void executeCommands(uint32_t commandBufferCount, const VkCommandBuffer *pCommandBuffers) {
        vkCmdExecuteCommands(m_commandBuffer, commandBufferCount, pCommandBuffers);
    }

    // VK_EXT_discard_rectangles
    void setDiscardRectangleEXT(uint32_t firstDiscardRectangle, uint32_t discardRectangleCount, const VkRect2D *pDiscardRectangles) {
        vkCmdSetDiscardRectangleEXT(m_commandBuffer, firstDiscardRectangle, discardRectangleCount, pDiscardRectangles);
    }

    void setDiscardRectangleEXT(uint32_t firstDiscardRectangle, const VulkanArrayProxy<VkRect2D> &discardRectangles) {
        vkCmdSetDiscardRectangleEXT(m_commandBuffer, firstDiscardRectangle, discardRectangles.count(), discardRectangles.data());
    }

private:
    VulkanDevice *m_device;
    VkCommandPool m_commandPool;
    VkCommandBuffer m_commandBuffer;
    bool m_autoDelete:1;
    bool m_active:1;
    bool m_renderPassActive:1;
};



// -----------------------------------------------------------------------



class KWINVULKANUTILS_EXPORT VulkanDeviceMemory : public VulkanObject
{
public:
    typedef VkDeviceMemory NativeHandleType;

    struct CreateInfo {
        VulkanDevice *device;
        VkDeviceMemory memory;
        VkDeviceSize size;
        VkMemoryPropertyFlags flags;
        VkExternalMemoryHandleTypeFlagsKHR exportableHandleTypes;
        bool dedicated;
        bool imported;
    };

    VulkanDeviceMemory(const CreateInfo &info)
        : VulkanObject(),
          m_device(info.device),
          m_memory(info.memory),
          m_size(info.size),
          m_flags(info.flags),
          m_exportableHandleTypes(info.exportableHandleTypes),
          m_dedicated(info.dedicated),
          m_imported(info.imported),
          m_mapped(false)
    {
    }

    VulkanDeviceMemory(const VulkanDeviceMemory &) = delete;

    VulkanDeviceMemory(VulkanDeviceMemory &&other)
        : VulkanObject(),
          m_device(other.m_device),
          m_memory(other.m_memory),
          m_size(other.m_size),
          m_flags(other.m_flags),
          m_exportableHandleTypes(other.m_exportableHandleTypes),
          m_dedicated(other.m_dedicated),
          m_imported(other.m_imported),
          m_mapped(other.m_mapped)

    {
        other.m_device = nullptr;
        other.m_memory = VK_NULL_HANDLE;
        other.m_size = 0;
        other.m_flags = 0;
        other.m_exportableHandleTypes = 0;
        other.m_dedicated = false;
        other.m_imported = false;
        other.m_mapped = false;
    }

    ~VulkanDeviceMemory() override {
        if (m_memory)
            m_device->freeMemory(m_memory, nullptr);
    }

    VulkanDeviceMemory &operator = (const VulkanDeviceMemory &) = delete;

    VulkanDeviceMemory &operator = (VulkanDeviceMemory &&other) {
        if (m_memory)
            m_device->freeMemory(m_memory, nullptr);

        m_device = other.m_device;
        m_memory = other.m_memory;
        m_size = other.m_size;
        m_flags = other.m_flags;
        m_exportableHandleTypes = other.m_exportableHandleTypes;
        m_dedicated = other.m_dedicated;
        m_imported = other.m_imported;
        m_mapped = other.m_mapped;

        other.m_device = nullptr;
        other.m_memory = VK_NULL_HANDLE;
        other.m_size = 0;
        other.m_flags = 0;
        other.m_exportableHandleTypes = 0;
        other.m_dedicated = false;
        other.m_imported = false;
        other.m_mapped = false;

        return *this;
    }

    VkDeviceMemory handle() const { return m_memory; }
    operator VkDeviceMemory() const { return m_memory; }

    bool isValid() const { return m_memory != VK_NULL_HANDLE; }

    VkDeviceSize size() const { return m_size; }

    VkMemoryPropertyFlags propertyFlags() const { return m_flags; }
    VkExternalMemoryHandleTypeFlagsKHR exportableHandleTypes() const { return m_exportableHandleTypes; }

    bool isDeviceLocal() const { return m_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT; }
    bool isHostCoherent() const { return m_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT; }
    bool isHostVisible() const { return m_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT; }
    bool isHostCached() const { return m_flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT; }
    bool isLazilyAllocated() const { return m_flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT; }
    bool isDedicated() const { return m_dedicated; }
    bool isImported() const { return m_imported; }
    bool isExportable() const { return m_exportableHandleTypes != 0; }
    bool isMapped() const { return m_mapped; }

    VkResult getFd(VkExternalMemoryHandleTypeFlagBitsKHR handleType, int *pFd) {
        const VkMemoryGetFdInfoKHR getFdInfo {
            .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
            .pNext = nullptr,
            .memory = m_memory,
            .handleType = handleType
        };
        assert((m_exportableHandleTypes & handleType) != 0);
        return m_device->getMemoryFdKHR(&getFdInfo, pFd);
    }

    VkResult map(VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void **ppData) {
        VkResult result = m_device->mapMemory(m_memory, offset, size, flags, ppData);
        if (result == VK_SUCCESS)
            m_mapped = true;
        return result;
    }

    VkResult map(VkMemoryMapFlags flags, void **ppData) {
        return map(0, m_size, flags, ppData);
    }

    void unmap() {
        m_device->unmapMemory(m_memory);
        m_mapped = false;
    }

private:
    VulkanDevice *m_device;
    VkDeviceMemory m_memory;
    VkDeviceSize m_size;
    VkMemoryPropertyFlags m_flags;
    VkExternalMemoryHandleTypeFlagsKHR m_exportableHandleTypes;
    bool m_dedicated:1;
    bool m_imported:1;
    bool m_mapped:1;
};



// -----------------------------------------------------------------------



class KWINVULKANUTILS_EXPORT VulkanSemaphore : public VulkanObject
{
public:
    typedef VkSemaphore NativeHandleType;

    VulkanSemaphore()
        : m_device(nullptr),
          m_semaphore(VK_NULL_HANDLE)
    {
    }

    VulkanSemaphore(VulkanDevice *device)
        : m_device(device),
          m_semaphore(VK_NULL_HANDLE)
    {
        const VkSemaphoreCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
        };

        m_device->createSemaphore(&createInfo, nullptr, &m_semaphore);
    }

    VulkanSemaphore(VulkanDevice *device, const VkSemaphoreCreateInfo &createInfo)
        : m_device(device),
          m_semaphore(VK_NULL_HANDLE)
    {
        m_device->createSemaphore(&createInfo, nullptr, &m_semaphore);
    }

    VulkanSemaphore(const VulkanSemaphore &other) = delete;

    VulkanSemaphore(VulkanSemaphore &&other)
        : m_device(other.m_device),
          m_semaphore(other.m_semaphore)
    {
        other.m_device    = nullptr;
        other.m_semaphore = VK_NULL_HANDLE;
    }

    ~VulkanSemaphore() {
        if (m_semaphore)
            m_device->destroySemaphore(m_semaphore);
    }

    VulkanSemaphore &operator = (const VulkanSemaphore &other) = delete;

    VulkanSemaphore &operator = (VulkanSemaphore &&other) {
        if (m_semaphore)
            m_device->destroySemaphore(m_semaphore);

        m_device    = other.m_device;
        m_semaphore = other.m_semaphore;

        other.m_device    = nullptr;
        other.m_semaphore = VK_NULL_HANDLE;
        return *this;
    }

    operator VkSemaphore() const { return m_semaphore; }

    VkSemaphore handle() const { return m_semaphore; }

private:
   VulkanDevice *m_device;
   VkSemaphore m_semaphore;
};



// -----------------------------------------------------------------------



/**
 * A VulkanFence is a synchronization primitive that can be used
 * to insert a dependency from a queue to the host.
 */
class KWINVULKANUTILS_EXPORT VulkanFence : public VulkanObject
{
public:
    typedef VkFence NativeHandleType;

    VulkanFence()
        : m_device(nullptr),
          m_fence(VK_NULL_HANDLE)
    {
    }

    VulkanFence(VulkanDevice *device, VkFenceCreateFlags flags = 0)
        : m_device(device)
    {
        const VkFenceCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = flags
        };

        m_device->createFence(&createInfo, nullptr, &m_fence);
    }

    VulkanFence(VulkanDevice *device, const VkFenceCreateInfo &createInfo)
        : m_device(device)
    {
        m_device->createFence(&createInfo, nullptr, &m_fence);
    }

    VulkanFence(const VulkanFence &) = delete;

    VulkanFence(VulkanFence &&other)
        : m_device(other.m_device),
          m_fence(other.m_fence)
    {
        other.m_device = nullptr;
        other.m_fence  = VK_NULL_HANDLE;
    }

    ~VulkanFence() override {
        if (m_fence)
            m_device->destroyFence(m_fence);
    }

    VulkanFence &operator = (const VulkanFence &) = delete;

    VulkanFence &operator = (VulkanFence &&other) {
        if (m_fence)
            m_device->destroyFence(m_fence);

        m_device = other.m_device;
        m_fence  = other.m_fence;

        other.m_device = nullptr;
        other.m_fence  = VK_NULL_HANDLE;
        return *this;
    }

    operator VkFence () const { return m_fence; }

    VkResult status() const {
        return m_device->getFenceStatus(m_fence);
    }

    bool isSignalled() const {
        return status() == VK_SUCCESS;
    }

    void reset() {
        m_device->resetFences(1, &m_fence);
    }

    void wait(uint64_t timeout = UINT64_MAX) {
        m_device->waitForFences(1, &m_fence, VK_TRUE, timeout);
    }

private:
    VulkanDevice *m_device;
    VkFence m_fence;
};



// -----------------------------------------------------------------------



/**
 * A VulkanShaderModule contains code for a shader.
 */
class KWINVULKANUTILS_EXPORT VulkanShaderModule : public VulkanObject
{
public:
    typedef VkShaderModule NativeHandleType;

    VulkanShaderModule()
        : VulkanObject(),
          m_device(nullptr),
          m_shaderModule(VK_NULL_HANDLE)
    {
    }

    VulkanShaderModule(VulkanDevice *device, const QString &fileName);

    VulkanShaderModule(const VulkanShaderModule &) = delete;

    VulkanShaderModule(VulkanShaderModule &&other)
        : VulkanObject(),
          m_device(other.m_device),
          m_shaderModule(other.m_shaderModule)
    {
        other.m_device = nullptr;
        other.m_shaderModule = VK_NULL_HANDLE;
    }

    ~VulkanShaderModule() override {
        if (m_shaderModule)
            m_device->destroyShaderModule(m_shaderModule, nullptr);
    }

    VulkanShaderModule &operator = (const VulkanShaderModule &) = delete;

    VulkanShaderModule &operator = (VulkanShaderModule &&other) {
        if (m_shaderModule)
            m_device->destroyShaderModule(m_shaderModule, nullptr);

        m_device = other.m_device;
        m_shaderModule = other.m_shaderModule;

        other.m_device = nullptr;
        other.m_shaderModule = VK_NULL_HANDLE;

        return *this;
    }

    bool isValid() const { return m_shaderModule != VK_NULL_HANDLE; }

    VkShaderModule handle() const { return m_shaderModule; }
    operator VkShaderModule () const { return m_shaderModule; }

private:
    VulkanDevice *m_device;
    VkShaderModule m_shaderModule;
};



// -----------------------------------------------------------------------



/**
 * VulkanClippedDrawHelper is used to invoked a draw command in a command buffer once
 * for each scissor rect in a region.
 */
class VulkanClippedDrawHelper
{
public:
    VulkanClippedDrawHelper(VulkanCommandBuffer *cmd, const QRegion &region)
        : cmd(cmd),
          region(region)
    {
    }

    template <typename Func>
    void forEachRect(Func f) {
        for (const QRect &r : region) {
            const VkRect2D scissor = {
                .offset = { (int32_t)  r.x(),     (int32_t)  r.y()      },
                .extent = { (uint32_t) r.width(), (uint32_t) r.height() }
            };

            cmd->setScissor(0, 1, &scissor);
            f();
        }
    }

    void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
        forEachRect(std::bind(&VulkanCommandBuffer::draw, cmd, vertexCount, instanceCount, firstVertex, firstInstance));
    }

    void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
        forEachRect(std::bind(&VulkanCommandBuffer::drawIndexed, cmd, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance));
    }

    void drawIndirect(VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
        forEachRect(std::bind(&VulkanCommandBuffer::drawIndirect, cmd, buffer, offset, drawCount, stride));
    }

    void drawIndexedIndirect(VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
        forEachRect(std::bind(&VulkanCommandBuffer::drawIndexedIndirect, cmd, buffer, offset, drawCount, stride));
    }

private:
    VulkanCommandBuffer *cmd;
    const QRegion &region;
};



// -----------------------------------------------------------------------



class VulkanBufferRange
{
public:
    VulkanBufferRange()
        : m_buffer(nullptr),
          m_offset(0),
          m_range(0),
          m_data(nullptr)
    {
    }

    VulkanBufferRange(std::shared_ptr<VulkanBuffer> buffer, VkDeviceSize offset, VkDeviceSize range, void *data = nullptr)
        : m_buffer(buffer),
          m_offset(offset),
          m_range(range),
          m_data(data)
    {
    }

    std::shared_ptr<VulkanBuffer> buffer() const { return m_buffer; }
    VkBuffer handle() const { return m_buffer->handle(); }
    VkDeviceSize offset() const { return m_offset; }
    VkDeviceSize range() const { return m_range; }
    void *data() const { return m_data; }

private:
    std::shared_ptr<VulkanBuffer> m_buffer;
    VkDeviceSize m_offset;
    VkDeviceSize m_range;
    void *m_data;
};



// -----------------------------------------------------------------------



class KWINVULKANUTILS_EXPORT VulkanDeviceMemoryAllocator
{
public:
    VulkanDeviceMemoryAllocator(VulkanDevice *device, const std::vector<const char *> &enabledDeviceExtensions);
    virtual ~VulkanDeviceMemoryAllocator();

    VulkanDevice *device() const { return m_device; }

    /**
     * Returns the index of the first memory type in memoryTypeBits that has all the property flags in mask set.
     */
    int findMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags mask) const;

    /**
     * Allocates size bytes of device memory.
     *
     * The memory will be allocated from the first memory type in memoryTypeBits that has
     * all the flags in optimal set, or if that fails, the first memory type that has
     * all the flags in required set.
     *
     * The memory will be allocated as non-dedicated and non-exportable.
     */
    std::shared_ptr<VulkanDeviceMemory> allocateMemory(VkDeviceSize size,
                                                       uint32_t memoryTypeBits,
                                                       VkMemoryPropertyFlags optimal,
                                                       VkMemoryPropertyFlags required);

    /**
     * Allocates memory for the given buffer, and binds the buffer to the memory object.
     *
     * The memory will be allocated from the first supported memory type that has
     * all the flags in optimal set, or if that fails, the first memory type that has
     * all the flags in required set.
     *
     * The memory will be allocated as dedicated if the implementation indicates that
     * dedicated memory is preferred or required for the given buffer.
     *
     * The memory will be allocated as non-exportable.
     */
    std::shared_ptr<VulkanDeviceMemory> allocateMemory(std::shared_ptr<VulkanBuffer> &buffer,
                                                       VkMemoryPropertyFlags optimal,
                                                       VkMemoryPropertyFlags required);

    /**
     * Allocates memory for the given image, and binds the image to the memory object.
     *
     * The memory will be allocated from the first supported memory type that has
     * all the flags in optimal set, or if that fails, the first memory type that has
     * all the flags in required set.
     *
     * The memory will be allocated as dedicated if the implementation indicates that
     * dedicated memory is preferred or required for the given image.
     *
     * The memory will be allocated as non-exportable.
     */
    std::shared_ptr<VulkanDeviceMemory> allocateMemory(std::shared_ptr<VulkanImage> &image,
                                                       VkMemoryPropertyFlags optimal,
                                                       VkMemoryPropertyFlags required);

    /**
     * Returns the memory properties for the physical device.
     */
    const VkPhysicalDeviceMemoryProperties &memoryProperties() const { return m_memoryProperties; }

protected:
    /**
     * @internal
     */
    std::shared_ptr<VulkanDeviceMemory> allocateMemoryInternal(VkDeviceSize size, uint32_t memoryTypeBits,
                                                               VkMemoryPropertyFlags optimal,
                                                               VkMemoryPropertyFlags required,
                                                               const void *pAllocateInfoNext);

private:
    VulkanDevice *m_device;
    VkPhysicalDeviceMemoryProperties m_memoryProperties;
    bool m_haveExternalMemory = false;
    bool m_haveExternalMemoryFd = false;
    bool m_haveDedicatedAllocation = false;
    bool m_haveGetMemoryRequirements2 = false;
    bool m_haveBindMemory2 = false;
    bool m_haveExternalMemoryDmaBuf = false;
};



// -----------------------------------------------------------------------



class KWINVULKANUTILS_EXPORT VulkanCircularAllocatorBase
{
public:
    VulkanCircularAllocatorBase(const VulkanCircularAllocatorBase &) = delete;

    virtual ~VulkanCircularAllocatorBase();

    /**
     * Inserts any non-coherent mapped memory ranges that need to be flushed into the vector pointed to by \a it.
     *
     * The memory ranges returned by this method must be flushed prior to submitting
     * command buffers that access data written to those ranges.
     */
    void getNonCoherentAllocatedRanges(std::back_insert_iterator<std::vector<VkMappedMemoryRange>> it);

    /**
     * Creates and returns a frame boundary object.
     *
     * This object should be associated with the fence passed to vkQueueSubmit() when submitting
     * command buffers that access memory allocated from the ring buffer.
     *
     * The FrameBoundary object holds a strong reference to the buffer, preventing it from being
     * deleted while it is busy, along with the busy offset within the ring buffer.
     * When the FrameBoundary object is destroyed, its destructor moves the tail pointer in the
     * internal buffer, allowing the allocated memory ranges to be reused.
     */
    std::shared_ptr<VulkanObject> createFrameBoundary();

    VulkanCircularAllocatorBase &operator = (const VulkanCircularAllocatorBase &) = delete;

protected:
    class CircularBuffer;

    VulkanCircularAllocatorBase(VulkanDevice *device, uint32_t nonCoherentAtomSize);

protected:
    VulkanDevice *m_device;
    std::shared_ptr<CircularBuffer> m_circularBuffer;
    std::vector<std::shared_ptr<CircularBuffer>> m_orphanedBuffers;
    VkDeviceSize m_nonCoherentAtomSize = 1;
};



// -----------------------------------------------------------------------



/**
 * VulkanUploadManager is used for streaming vertices, uniforms and image data.
 */
class KWINVULKANUTILS_EXPORT VulkanUploadManager : public VulkanCircularAllocatorBase
{
public:
    /**
     * initialSize must be a power of two.
     */
    VulkanUploadManager(VulkanDevice *device,
                        VulkanDeviceMemoryAllocator *allocator,
                        size_t initialSize,
                        VkBufferUsageFlags bufferUsage,
                        VkMemoryPropertyFlags optimalFlags,
                        const VkPhysicalDeviceLimits &limits);

    VulkanUploadManager(const VulkanUploadManager &) = delete;

    /**
     * Destroys the upload manager.
     */
    ~VulkanUploadManager() override;

    /**
     * Returns the buffer usage flags.
     */
    VkBufferUsageFlags usage() const { return m_bufferUsage; }

    /**
     * Returns the minimum texel buffer offset alignment.
     */
    VkDeviceSize minTexelBufferOffsetAlignment() const { return m_minTexelBufferOffsetAlignment; }

    /**
     * Returns the minimum uniform buffer offset alignment.
     */
    VkDeviceSize minUniformBufferOffsetAlignment() const { return m_minUniformBufferOffsetAlignment; }

    /**
     * Returns the minimum storage buffer offset alignment.
     */
    VkDeviceSize minStorageBufferOffsetAlignment() const { return m_minStorageBufferOffsetAlignment; }

    /**
     * Allocates size bytes of data from the upload buffer.
     */
    VulkanBufferRange allocate(size_t size, uint32_t alignment = 4);

    /**
     * Uploads size bytes of data into the upload buffer.
     */
    VulkanBufferRange upload(const void *data, size_t size, uint32_t alignment = 4) {
        auto range = allocate(size, alignment);
        memcpy(range.data(), data, size);
        return range;
    }

    /**
     * Uploads size bytes of data into the upload buffer, aligning the allocation
     * to the minimum texel buffer offset alignment.
     */
    VulkanBufferRange uploadTexelData(const void *data, size_t size) {
        return upload(data, size, m_minTexelBufferOffsetAlignment);
    }

    /**
     * Uploads size bytes of data into the upload buffer, aligning the allocation
     * to the minimum uniform buffer offset alignment.
     */
    VulkanBufferRange uploadUniformData(const void *data, size_t size) {
        return upload(data, size, m_minUniformBufferOffsetAlignment);
    }

    /**
     * Uploads size bytes of data into the upload buffer, aligning the allocation
     * to the minimum storage buffer offset alignment.
     */
    VulkanBufferRange uploadStorageData(const void *data, size_t size) {
        return upload(data, size, m_minStorageBufferOffsetAlignment);
    }

    /**
     * Emplaces an object of type T in the upload buffer.
     */
    template <typename T, typename... Args>
    VulkanBufferRange emplace(Args&&... args) {
        auto range = allocate(sizeof(T), 4);
        new(range.data()) T(std::forward<Args>(args)...);
        return range;
    }

    /**
     * Emplaces an object of type T in the upload buffer, aligning the allocation
     * to the minimum uniform buffer offset alignment.
     */
    template <typename T, typename... Args>
    VulkanBufferRange emplaceUniform(Args&&... args) {
        auto range = allocate(sizeof(T), m_minUniformBufferOffsetAlignment);
        new(range.data()) T(std::forward<Args>(args)...);
        return range;
    }

    VulkanUploadManager &operator = (const VulkanUploadManager &) = delete;

private:
    void reallocate(size_t minimumSize);

private:
    VulkanDeviceMemoryAllocator *m_allocator;
    size_t m_initialSize;
    VkBufferUsageFlags m_bufferUsage;
    VkMemoryPropertyFlags m_optimalMemoryFlags;
    VkDeviceSize m_minTexelBufferOffsetAlignment = 4;
    VkDeviceSize m_minUniformBufferOffsetAlignment = 4;
    VkDeviceSize m_minStorageBufferOffsetAlignment = 4;
};



// -----------------------------------------------------------------------



/**
 * VulkanStagingImageAllocator allocates and binds images to memory allocated from a ring buffer.
 */
class KWINVULKANUTILS_EXPORT VulkanStagingImageAllocator : public VulkanCircularAllocatorBase
{
public:
    VulkanStagingImageAllocator(VulkanDevice *device,
                                VulkanDeviceMemoryAllocator *allocator,
                                size_t initialSize,
                                VkMemoryPropertyFlags optimalMemoryFlags,
                                const VkPhysicalDeviceLimits &limits);

    ~VulkanStagingImageAllocator() override;

    /**
     * Creates an image, and binds to memory allocated from the internal ring buffer.
     *
     * All images created by this method must use the same tiling mode,
     * and must not be sparse or exportable.
     */
    std::shared_ptr<VulkanStagingImage> createImage(const VkImageCreateInfo &createInfo);

private:
    void reallocate(size_t minimumSize, uint32_t memoryPropertyMask);

private:
    VulkanDeviceMemoryAllocator *m_allocator;
    size_t m_initialSize;
    VkMemoryPropertyFlags m_optimalMemoryFlags;
};



// -----------------------------------------------------------------------



class KWINVULKANUTILS_EXPORT VulkanPipelineCache
{
public:
    typedef VkPipelineCache NativeHandleType;

    VulkanPipelineCache() : m_device(VK_NULL_HANDLE), m_cache(VK_NULL_HANDLE) {}

    VulkanPipelineCache(VulkanDevice *device, size_t initialDataSize = 0, const void *pInitialData = nullptr)
        : m_device(device),
          m_cache(VK_NULL_HANDLE)
    {
        device->createPipelineCache({
                                         .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
                                         .pNext = nullptr,
                                         .flags = 0,
                                         .initialDataSize = initialDataSize,
                                         .pInitialData = pInitialData
                                     }, nullptr, &m_cache);
    }

    VulkanPipelineCache(VulkanPipelineCache &&other)
        : m_device(other.m_device),
          m_cache(other.m_cache)
    {
        other.m_device = VK_NULL_HANDLE;
        other.m_cache = VK_NULL_HANDLE;
    }

    VulkanPipelineCache(const VulkanPipelineCache &other) = delete;

    ~VulkanPipelineCache() {
        if (m_cache)
            m_device->destroyPipelineCache(m_cache);
    }

    VkPipelineCache handle() const { return m_cache; }
    operator VkPipelineCache () const { return m_cache; }

    bool isValid() const { return m_cache != VK_NULL_HANDLE; }

    std::vector<uint8_t> data() {
        size_t size = 0;
        m_device->getPipelineCacheData(m_cache, &size, nullptr);

        std::vector<uint8_t> data(size);
        m_device->getPipelineCacheData(m_cache, &size, data.data());
        return data;
    }

    VulkanPipelineCache &operator = (const VulkanPipelineCache &other) = delete;

    VulkanPipelineCache &operator = (VulkanPipelineCache &&other) {
        if (m_cache)
            m_device->destroyPipelineCache(m_cache);

        m_device = other.m_device;
        m_cache  = other.m_cache;

        other.m_device = nullptr;
        other.m_cache  = VK_NULL_HANDLE;
        return *this;
    }

private:
    VulkanDevice *m_device;
    VkPipelineCache m_cache;
};



// -----------------------------------------------------------------------



class KWINVULKANUTILS_EXPORT VulkanPipelineManager
{
public:
    /**
     * The material rendered by the pipeline.
     */
    enum Material {
        FlatColor               = 0,
        Texture                 = 1,
        TwoTextures             = 2,
        DecorationStagingImages = 3,
        MaterialCount           = 4,
    };

    /**
     * The traits of the pipeline, such as filters applied to the material,
     * how fragments are rasterized, etc.
     */
    enum Trait {
        NoTraits                 = 0,
        PreMultipliedAlphaBlend  = (1 << 0),
        Modulate                 = (1 << 1),
        Desaturate               = (1 << 2),
        CrossFade                = (1 << 3),
    };

    Q_DECLARE_FLAGS(Traits, Trait);

    /**
     * The type of descriptors that will be used with the pipeline.
     */
    enum DescriptorType {
        DescriptorSet   = 0,
        PushDescriptors = 1
    };

    /**
     * The types of render passes the pipeline will be compatible with.
     *
     * All render passes have one subpass, and one framebuffer attachment.
     * They differ only in the format of the attachment.
     */
    enum RenderPassType {
        SwapchainRenderPass = 0, /// Render pass that targets images with the swap chain format
        OffscreenRenderPass,     /// Render pass that targets VK_FORMAT_R8G8B8A8_UNORM images
        RenderPassTypeCount
    };

    /**
     * The topology of the primitives rendered by the pipeline.
     */
    enum Topology {
        PointList     = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
        LineList      = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
        LineStrip     = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
        TriangleList  = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        TriangleStrip = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        TriangleFan   = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN
    };

    class ColorUniformData {
    public:
        ColorUniformData(const QMatrix4x4 &matrix, const QVector4D &color) {
            memcpy(m_matrix, matrix.constData(), 64);
            memcpy(m_color, &color, 16);
        }

    private:
        float m_matrix[16];
        float m_color[4];
    };

    class TextureUniformData {
    public:
        TextureUniformData(const QMatrix4x4 &matrix, float opacity, float brightness, float saturation, float crossFadeProgress) {
            const float rgb = brightness * opacity;
            const float a = opacity;
            memcpy(m_matrix, matrix.constData(), 64);
            m_modulation[0] = rgb;
            m_modulation[1] = rgb;
            m_modulation[2] = rgb;
            m_modulation[3] = a;
            m_saturation = saturation;
            m_crossFadeProgress = crossFadeProgress;
            m_pad[0] = 0.0f;
            m_pad[1] = 0.0f;
        }

    private:
        float m_matrix[16];
        float m_modulation[4];
        float m_saturation;
        float m_crossFadeProgress;
        float m_pad[2];
    };

    /**
     * Creates a new VulkanPipelineManager.
     *
     * @param device               The logical device
     * @param swapchainRenderPass  Render pass for swap chain compatible pipelines
     * @param offscreenRenderPass  Render pass for offscreen compatible pipelines
     * @param nearestSampler       Nearest-neighbor sampler to be used as an immutable sampler
     * @param linearSampler        Linear sampler to be used as an immutable sampler
     * @param havePushDescriptors  Indicates whether VK_KHR_push_descriptor is supported by the device
     */
    VulkanPipelineManager(VulkanDevice *device, VulkanPipelineCache *cache,
                          VkSampler nearestSampler, VkSampler linearSampler,
                          VkRenderPass swapchainRenderPass, VkRenderPass offscreenRenderPass,
                          bool havePushDescriptors);

    /**
     * Destroys the VulkanPipelineManager.
     */
    ~VulkanPipelineManager();

    /**
     * Returns true if the VulkanPipelineManager is valid, and false otherwise.
     */
    bool isValid() const { return m_valid; }

    /**
     * Returns a pipeline and a pipeline layout matching the given material, triats, descriptor type and topology.
     */
    std::tuple<VkPipeline, VkPipelineLayout> pipeline(Material material,
                                                      Traits traits,
                                                      DescriptorType descriptorType,
                                                      Topology topology,
                                                      RenderPassType renderPassType);

    VkDescriptorSetLayout descriptorSetLayout(Material material) const { return m_descriptorSetLayout[material]; }
    VkDescriptorSetLayout pushDescriptorSetLayout(Material material) const { return m_pushDescriptorSetLayout[material]; }

private:
    bool createDescriptorSetLayouts();
    bool createPipelineLayouts();
    bool createDescriptorUpdateTemplates();
    VkPipeline createPipeline(Material material, Traits traits, DescriptorType descriptorType, Topology topology, VkRenderPass renderPass);

private:
    VulkanDevice *m_device;
    VulkanPipelineCache *m_pipelineCache;
    VkDescriptorSetLayout m_descriptorSetLayout[MaterialCount];
    VkDescriptorSetLayout m_pushDescriptorSetLayout[MaterialCount];
    VkPipelineLayout m_pipelineLayout[MaterialCount];
    VkPipelineLayout m_pushDescriptorPipelineLayout[MaterialCount];
    std::unordered_map<uint32_t, VkPipeline> m_pipelines;
    VkRenderPass m_renderPasses[RenderPassTypeCount];
    VulkanShaderModule m_colorVertexShader;
    VulkanShaderModule m_textureVertexShader;
    VulkanShaderModule m_crossFadeVertexShader;
    VulkanShaderModule m_updateDecorationVertexShader;
    VulkanShaderModule m_colorFragmentShader;
    VulkanShaderModule m_textureFragmentShader;
    VulkanShaderModule m_modulateFragmentShader;
    VulkanShaderModule m_desaturateFragmentShader;
    VulkanShaderModule m_crossFadeFragmentShader;
    VulkanShaderModule m_updateDecorationFragmentShader;
    VkSampler m_nearestSampler = VK_NULL_HANDLE;
    VkSampler m_linearSampler = VK_NULL_HANDLE;
    uint32_t m_maxDiscardRectangles = 0;
    bool m_havePushDescriptors = false;
    bool m_valid = false;
};



// -----------------------------------------------------------------------



KWINVULKANUTILS_EXPORT QByteArray enumToString(VkFormat format);
KWINVULKANUTILS_EXPORT QByteArray enumToString(VkResult result);
KWINVULKANUTILS_EXPORT QByteArray enumToString(VkPresentModeKHR mode);
KWINVULKANUTILS_EXPORT QByteArray enumToString(VkPhysicalDeviceType physicalDeviceType);

KWINVULKANUTILS_EXPORT QByteArray vendorName(uint32_t vendorID);
KWINVULKANUTILS_EXPORT QByteArray driverVersionString(uint32_t vendorID, uint32_t driverVersion);

} // namespace KWin

#endif // KWINVULKANUTILS_H

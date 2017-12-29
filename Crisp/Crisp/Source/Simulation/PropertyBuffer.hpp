#pragma once

#include <vector>

//#include <cuda.h>
//#include <cuda_runtime.h>

namespace crisp
{
    template <typename T>
    class PropertyBuffer
    {
    public:
        PropertyBuffer(size_t size);
        ~PropertyBuffer();

        size_t getSize() const { return m_hostBuffer.size(); }

        void resize(size_t size, bool keepOldGpuData = false);

        void setHostBuffer(const std::vector<T>& buffer);
        void setHostBuffer(std::vector<T>&& buffer);
        void copyToGpu();
        void copyToCpu();

        T* getDeviceBuffer() const { return m_deviceBuffer; }
        const std::vector<T>& getHostBuffer() const { return m_hostBuffer; }

    private:
        std::vector<T> m_hostBuffer;
        T*             m_deviceBuffer;
    };

    template <typename T>
    PropertyBuffer<T>::PropertyBuffer(size_t size)
        : m_hostBuffer(size)
    {
        cudaMalloc(&m_deviceBuffer, size * sizeof(T));
    }

    template <typename T>
    PropertyBuffer<T>::~PropertyBuffer()
    {
        cudaFree(m_deviceBuffer);
    }

    template <typename T>
    void PropertyBuffer<T>::resize(size_t size, bool keepOldGpuData)
    {
        size_t oldSize = m_hostBuffer.size();
        m_hostBuffer.resize(size);

        T* deviceBuffer;
        cudaMalloc(&deviceBuffer, size * sizeof(T));

        if (keepOldGpuData)
            cudaMemcpy(deviceBuffer, m_deviceBuffer, oldSize * sizeof(T), cudaMemcpyDeviceToDevice);

        cudaFree(m_deviceBuffer);
        m_deviceBuffer = deviceBuffer;
    }

    template <typename T>
    void PropertyBuffer<T>::setHostBuffer(const std::vector<T>& buffer)
    {
        m_hostBuffer = buffer;
    }

    template <typename T>
    void PropertyBuffer<T>::setHostBuffer(std::vector<T>&& buffer)
    {
        m_hostBuffer = std::move(buffer);
    }

    template <typename T>
    void PropertyBuffer<T>::copyToGpu()
    {
        cudaMemcpy(m_deviceBuffer, m_hostBuffer.data(), sizeof(T) * m_hostBuffer.size(), cudaMemcpyHostToDevice);
    }

    template <typename T>
    void PropertyBuffer<T>::copyToCpu()
    {
        cudaMemcpy(m_hostBuffer.data(), m_deviceBuffer, sizeof(T) * m_hostBuffer.size(), cudaMemcpyDeviceToHost);
    }
}
#pragma once

/**
 * @author Julian Gaal
 * @author Marcel Flottmann
 */

#include <hw/opencl.h>
#include <memory>
#include <stdexcept>

#include <hw/types.h>
#include <hw/fpga_manager.h>

// because xilinx missed this: Host -> Device, CL_MIGRATE_MEM_OBJECT_HOST defined in cl.h
#define CL_MIGRATE_MEM_OBJECT_DEVICE                  (0 << 0)

namespace fastsense::buffer
{

/**
 * @brief Buffer class: a safe wrapper around cl::Buffer that allocates memory,
 * maps this to a virtual address, and deallocates memory in destructor
 *
 * @tparam T type of single element in buffer
 */
template <typename T>
class Buffer
{

    using size_type = size_t;

private:
    /**
     * @brief unmap buffer: deattach virtual address from buffer
     */
    void unmap_memory()
    {
        if (virtual_address_)
        {
            cl::Event event;
            queue_->enqueueUnmapMemObject(buffer_, virtual_address_, nullptr, &event);
            event.wait();
            virtual_address_ = nullptr;
        }
    }

    /**
     * @brief map buffer: attach virtual address from buffer
     */
    void map_memory()
    {
        virtual_address_ = static_cast<T*>(queue_->enqueueMapBuffer(buffer_, CL_TRUE, map_flag_, 0, size_in_bytes_));
    }

protected:
    /// Xilinx command queue
    CommandQueuePtr queue_;

    /// number of elements in buffer/ size
    size_t num_elements_;

    /// size of buffer, in bytes
    size_t size_in_bytes_;

    /// underlying cl::Buffer
    cl::Buffer buffer_;

    /// mem flag: read or write buffer
    cl_mem_flags mem_flag_;

    /// map flag: read or write virtual address buffer
    cl_map_flags map_flag_;

    /// virtual address, to which buffer is mapped
    T* virtual_address_;

    /**
     * @brief Construct an empty Buffer object
     *
     */
    Buffer()
        : queue_{},
          num_elements_{0},
          size_in_bytes_{0},
          buffer_{},
          mem_flag_{0},
          map_flag_{0},
          virtual_address_{nullptr}
    {
    }

    /**
     * @brief Construct a new Buffer object
     *
     * @param queue program command queue
     * @param num_elements number of elements in buffer
     * @param mem_flag sets type of buffer: read or write
     * @param map_flag sets type of virtual address buffer: read or write
     */
    Buffer(const CommandQueuePtr& queue,
           size_t num_elements,
           cl_mem_flags mem_flag,
           cl_map_flags map_flag)
        : queue_{queue},
          num_elements_{num_elements},
          size_in_bytes_{sizeof(T) * num_elements},
          buffer_{fastsense::hw::FPGAManager::get_context(), mem_flag, size_in_bytes_},
          mem_flag_{mem_flag},
          map_flag_{map_flag},
          virtual_address_{nullptr}
    {
        map_memory();
    }

public:

    using const_iterator = const T*;
    using iterator = T*;

    /**
     * @brief Move construct a new Buffer object
     *
     * @param rhs moved buffer
     */
    Buffer(Buffer&& rhs)
        : queue_{std::move(rhs.queue_)},
          num_elements_{rhs.num_elements_},
          size_in_bytes_{rhs.size_in_bytes_},
          buffer_{std::move(rhs.buffer_)},
          mem_flag_{rhs.mem_flag_},
          map_flag_{rhs.map_flag_},
          virtual_address_{rhs.virtual_address_}
    {
        rhs.num_elements_ = 0;
        rhs.size_in_bytes_ = 0;
        rhs.mem_flag_ = 0;
        rhs.map_flag_ = 0;
        rhs.virtual_address_ = nullptr;
    }

    /**
     * @brief Destroy the Buffer object and unmap memory
     */
    ~Buffer()
    {
        unmap_memory();
    }

    /**
     * @brief delete assignment operator because of pointer member variable
     *
     * @return Buffer& other Buffer
     */
    Buffer& operator=(Buffer&) = delete;

    /**
     * @brief move assign this buffer
     *
     * @param rhs moved other buffer
     * @return Buffer& reference to *this
     */
    Buffer& operator=(Buffer&& rhs)
    {
        //Cleanup
        unmap_memory();

        //Assign new values
        queue_ = std::move(rhs.queue_);
        num_elements_ = rhs.num_elements_;
        size_in_bytes_ = rhs.size_in_bytes_;
        buffer_ = std::move(rhs.buffer_);
        mem_flag_ = rhs.mem_flag_;
        map_flag_ = rhs.map_flag_;
        virtual_address_ = rhs.virtual_address_;

        //Cleanup rhs
        rhs.num_elements_ = 0;
        rhs.size_in_bytes_ = 0;
        rhs.mem_flag_ = 0;
        rhs.map_flag_ = 0;
        rhs.virtual_address_ = nullptr;

        return *this;
    }

    /**
     * @brief delete copy constructor because of pointer member variable
     * @param Buffer& other buffer
     */
    Buffer(Buffer&) = delete;

    /**
     * @brief Get the virtual address that buffer was mapped to
     *
     * @return T* address to virtual address
     */
    T* getVirtualAddress()
    {
        return virtual_address_;
    }

    /**
     * @brief Get the Buffer object
     *
     * @return cl::Buffer&
     */
    const cl::Buffer& getBuffer() const
    {
        return buffer_;
    }

    /**
     * @brief Return the size of the buffer in bytes
     *
     * @return size_t size in bytes
     */
    size_t sizeInBytes() const
    {
        return num_elements_ * sizeof(T);
    }

    /**
     * @brief Return number of elements in buffer
     *
     * @return size_t number or elements in buffer
     */
    size_type size() const
    {
        return num_elements_;
    }

    T& operator[](size_type index)
    {
        if(index >= num_elements_)
        {
            throw std::out_of_range(std::string("Can't access buffer at index: ") + std::to_string(index));
        }
        return virtual_address_[index];
    }

    const T& operator[](size_type index) const
    {
        if(index >= num_elements_)
        {
            throw std::out_of_range(std::string("Can't access buffer at index: ") + std::to_string(index));
        }
        return virtual_address_[index];
    }

    iterator begin()
    {
        return iterator(virtual_address_);
    }

    iterator end()
    {
        return iterator(virtual_address_ + num_elements_);
    }

    const_iterator cbegin() const
    {
        return const_iterator(virtual_address_);
    }

    const_iterator cend() const
    {
        return const_iterator(virtual_address_ + num_elements_);
    }
};

/**
 * @brief Read only buffer
 *
 * @tparam T type of single element in buffer
 */
template <typename T>
class InputBuffer : public Buffer<T>
{
public:
    /**
     * @brief Construct an empty Input Buffer object
     *
     */
    InputBuffer() : Buffer<T>()
    {}

    /**
     * @brief Construct a new Input Buffer object
     *
     * @param queue
     * @param num_elements
     */
    InputBuffer(const CommandQueuePtr& queue, size_t num_elements)
        :   Buffer<T>(queue, num_elements, CL_MEM_READ_ONLY, CL_MAP_WRITE)
    {}

    ~InputBuffer() = default;

    InputBuffer& operator=(InputBuffer&) = delete;
    InputBuffer(InputBuffer&) = delete;
};

/**
 * @brief Write only buffer
 *
 * @tparam T type of single element in buffer
 */
template <typename T>
class OutputBuffer : public Buffer<T>
{
public:
    /**
     * @brief Construct an empty Output Buffer object
     *
     */
    OutputBuffer() : Buffer<T>()
    {}

    /**
     * @brief Construct a new Output Buffer object
     *
     * @param queue
     * @param num_elements
     */
    OutputBuffer(const CommandQueuePtr& queue, size_t num_elements)
        :   Buffer<T>(queue, num_elements, CL_MEM_WRITE_ONLY, CL_MAP_READ)
    {}

    ~OutputBuffer() = default;

    OutputBuffer& operator=(OutputBuffer&) = delete;
    OutputBuffer(OutputBuffer&) = delete;
};

/**
 * @brief Read/Write buffer
 *
 * @tparam T type of single element in buffer
 */
template <typename T>
class InputOutputBuffer : public Buffer<T>
{
public:
    /**
     * @brief Construct an empty Input Output Buffer object
     *
     */
    InputOutputBuffer() : Buffer<T>()
    {}

    /**
     * @brief Construct a new Input Output Buffer object
     *
     * @param queue
     * @param num_elements
     */
    InputOutputBuffer(const CommandQueuePtr& queue, size_t num_elements)
        :   Buffer<T>(queue, num_elements, CL_MEM_READ_WRITE, CL_MAP_READ | CL_MAP_WRITE)
    {}

    ~InputOutputBuffer() = default;

    InputOutputBuffer& operator=(InputOutputBuffer&) = delete;
    InputOutputBuffer(InputOutputBuffer&) = delete;
};

} // namespace fastsense::buffer
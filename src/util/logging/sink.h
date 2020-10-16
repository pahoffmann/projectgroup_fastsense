#pragma once

/**
 * @file sink.h
 * @author Marcel Flottmann
 */

#include <string>
#include <fstream>

namespace fastsense::util::logging::sink
{

class Sink
{
public:
    Sink() = default;
    Sink(Sink&) = delete;
    Sink& operator=(Sink&) = delete;
    virtual ~Sink() = default;

    virtual void write(const std::string& msg) = 0;
};

class CoutSink : public Sink
{
public:
    ~CoutSink() = default;

    void write(const std::string& msg) override;
};

class FileSink : public Sink
{
private:
    std::ofstream file;
public:
    explicit FileSink(const std::string& filename, std::ios_base::openmode mode = std::ios_base::app);
    ~FileSink() = default;

    void write(const std::string& msg) override;
};

} // namespace fastsense::util::logging::sink

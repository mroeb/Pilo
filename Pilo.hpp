//
// pilo.h: A C++23 GPIO library for Linux (gpiodev), suitable for Raspberry Pi 5.
//

#ifndef PILOTEST_PILO_H
#define PILOTEST_PILO_H

#include <algorithm>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <vector>
#include <format>
#include <optional>
#include <type_traits>
#include <utility>

// Required Linux kernel headers for GPIO
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/gpio.h>

namespace Pilo
{
    // --- Enums and Utility Operators ---

    /**
     * @brief Defines the direction of a GPIO line.
     */
    enum class Direction : uint8_t
    {
        None = 0,
        Input = 1 << 0,
        Output = 1 << 1,
        Both = Input | Output
    };

    /**
     * @brief Defines the initial value for an Output line.
     */
    enum class InitialValue : uint8_t
    {
        Low = 0,
        High = 1,
        Default = Low // Default to Low for convenience
    };

    /**
     * @brief Bitwise OR operator for Direction enum.
     */
    constexpr Direction operator|(Direction lhs, Direction rhs)
    {
        using T = std::underlying_type_t<Direction>;
        return static_cast<Direction>(static_cast<T>(lhs) | static_cast<T>(rhs));
    }

    /**
     * @brief Bitwise AND operator for Direction enum.
     */
    constexpr Direction operator&(Direction lhs, Direction rhs)
    {
        using T = std::underlying_type_t<Direction>;
        return static_cast<Direction>(static_cast<T>(lhs) & static_cast<T>(rhs));
    }

    /**
     * @brief Bitwise OR-assignment operator for Direction enum.
     */
    constexpr Direction &operator |=(Direction &lhs, const Direction rhs)
    {
        lhs = lhs | rhs;
        return lhs;
    }

    // --- GPIO Class and Helper Structs ---

    /**
     * @brief Manages the file descriptors (handles) for requested GPIO lines.
     */
    struct LineHandles
    {
        using HandleMap = std::unordered_map<unsigned int, int>;
        HandleMap InputHandles{};
        HandleMap OutputHandles{};

        /**
         * @brief Checks if a line is registered for a specific direction.
         */
        [[nodiscard]] bool contains(unsigned int line, Direction dir) const noexcept
        {
            if ((dir & Direction::Input) == Direction::Input)
            {
                if (InputHandles.contains(line)) return true;
            }
            if ((dir & Direction::Output) == Direction::Output)
            {
                if (OutputHandles.contains(line)) return true;
            }
            return false;
        }
    };

    /**
     * @brief Main class for managing a GPIO chip and its lines.
     */
    class GPIO
    {
    public:
        /**
         * @brief Constructs a GPIO object and opens the chip device.
         *
         * @param chip_name The path to the GPIO chip device, e.g., "/dev/gpiochip0".
         * @param consumer_label A descriptive name for the consumer of the lines.
         * @throws std::runtime_error if the chip cannot be opened.
         */
        GPIO(std::string chip_name, std::string consumer_label)
            : chip_name_(std::move(chip_name)), consumer_label_(std::move(consumer_label))
        {
            // O_RDWR is generally required for requesting line handles
            chip_fd_ = open(chip_name_.c_str(), O_RDWR);
            if (chip_fd_ < 0)
            {
                throw std::runtime_error(std::format("Pilo::GPIO: Failed to open the chip: {}", chip_name_));
            }
        }

        /**
         * @brief Destructor: Closes all line handles and the chip file descriptor.
         */
        ~GPIO()
        {
            // Close all registered line handles
            close_handles(line_handles_.InputHandles);
            close_handles(line_handles_.OutputHandles);

            // Close the chip file descriptor
            if (chip_fd_ >= 0)
            {
                close(chip_fd_);
                chip_fd_ = -1;
            }
        }

        // Disable copy and move for resource management (file descriptors)
        GPIO(const GPIO&) = delete;
        GPIO& operator=(const GPIO&) = delete;
        GPIO(GPIO&&) = delete;
        GPIO& operator=(GPIO&&) = delete;

        // --- Line Management ---

        /**
         * @brief Requests and registers handles for one or more GPIO lines.
         *
         * @param dir The desired Direction (Input, Output, or Both).
         * @param lines A vector of line numbers (offsets) to request.
         * @param initial_value The initial value for output lines (High/Low).
         * @throws std::runtime_error on failure to request any line.
         */
        void add_lines(Direction dir, const std::vector<unsigned int>& lines, InitialValue initial_value = InitialValue::Low)
        {
            for (unsigned int line : lines)
            {
                add_handle(line, dir, initial_value);
            }
        }

        /**
         * @brief Removes the handle(s) for a specific line, if registered.
         *
         * @param line The line number to release.
         * @param dir The direction(s) to release (Input, Output, or Both).
         */
        void remove_line(unsigned int line, Direction dir)
        {
            if ((dir & Direction::Input) == Direction::Input)
            {
                remove_handle(line, line_handles_.InputHandles);
            }
            if ((dir & Direction::Output) == Direction::Output)
            {
                remove_handle(line, line_handles_.OutputHandles);
            }
        }

        // --- I/O Operations ---

        /**
         * @brief Writes a value to a registered Output line.
         *
         * @param line The line number to write to.
         * @param value The value to write (true for High, false for Low).
         * @throws std::runtime_error if the line is not registered as Output.
         */
        void write(unsigned int line, const bool value) const
        {
            if (chip_fd_ < 0) return; // Chip is closed, silently exit

            auto it = line_handles_.OutputHandles.find(line);
            if (it == line_handles_.OutputHandles.end())
            {
                throw std::runtime_error(std::format("Pilo::GPIO: Failed to write to Line: {}. Not registered as Output.", line));
            }

            gpiohandle_data data{};
            data.values[0] = value ? 1 : 0;

            if (ioctl(it->second, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) < 0)
            {
                 throw std::runtime_error(std::format("Pilo::GPIO: Failed to set value for Line {}", line));
            }
        }

        /**
         * @brief Reads the current value from a registered Input line.
         *
         * @param line The line number to read from.
         * @return The read value (true for High, false for Low).
         * @throws std::runtime_error if the line is not registered as Input.
         */
        [[nodiscard]] bool read(unsigned int line) const
        {
            if (chip_fd_ < 0) return false; // Chip is closed

            auto it = line_handles_.InputHandles.find(line);
            if (it == line_handles_.InputHandles.end())
            {
                throw std::runtime_error(std::format("Pilo::GPIO: Failed to read Line: {}. Not registered as Input.", line));
            }

            gpiohandle_data data{};
            if (ioctl(it->second, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0)
            {
                 throw std::runtime_error(std::format("Pilo::GPIO: Failed to get value for Line {}", line));
            }
            return data.values[0];
        }

        // --- Information Accessors ---

        /**
         * @brief Gets the path of the underlying GPIO chip device.
         */
        [[nodiscard]] const std::string& chip_name() const noexcept { return chip_name_; }

        /**
         * @brief Gets the consumer label used when requesting lines.
         */
        [[nodiscard]] const std::string& consumer_label() const noexcept { return consumer_label_; }

        /**
         * @brief Checks if a line is currently registered for I/O.
         */
        [[nodiscard]] bool is_registered(unsigned int line, Direction dir) const noexcept
        {
            return line_handles_.contains(line, dir);
        }

    private:
        int chip_fd_{-1};
        const std::string chip_name_;
        const std::string consumer_label_;
        LineHandles line_handles_{};

        // --- Private Helpers ---

        /**
         * @brief Requests a single handle for a line in a specific direction.
         */
        void add_handle(unsigned int line, Direction dir, InitialValue initial_value)
        {
            if ((dir & Direction::Input) == Direction::Input)
            {
                line_handles_.InputHandles[line] = request_handle(
                    line,
                    GPIOHANDLE_REQUEST_INPUT,
                    std::nullopt // No initial value for input
                );
            }

            if ((dir & Direction::Output) == Direction::Output)
            {
                line_handles_.OutputHandles[line] = request_handle(
                    line,
                    GPIOHANDLE_REQUEST_OUTPUT,
                    (initial_value == InitialValue::High) ? std::optional<uint8_t>(1) : std::optional<uint8_t>(0)
                );
            }
        }

        /**
         * @brief Releases the file descriptor for a single line from a map.
         */
        void remove_handle(unsigned int line, LineHandles::HandleMap& map)
        {
            auto it = map.find(line);
            if (it != map.end())
            {
                close(it->second);
                map.erase(it);
            }
        }

        /**
         * @brief Closes all file descriptors in a map.
         */
        void close_handles(LineHandles::HandleMap& map)
        {
            for (const auto& [line, fd] : map)
            {
                close(fd);
            }
            map.clear();
        }

        /**
         * @brief Performs the actual ioctl call to request a line handle.
         */
        [[nodiscard]] int request_handle(
            unsigned int line,
            uint32_t flag,
            std::optional<uint8_t> initial_value
        )
        {
            gpiohandle_request req{};
            req.lineoffsets[0] = line;
            req.flags = flag;
            req.lines = 1;

            if (initial_value.has_value() && (flag & GPIOHANDLE_REQUEST_OUTPUT))
            {
                req.default_values[0] = initial_value.value();
            }

            // The consumer label must be copied safely
            std::strncpy(req.consumer_label, consumer_label_.c_str(), sizeof(req.consumer_label) - 1);
            req.consumer_label[sizeof(req.consumer_label) - 1] = '\0';

            if (ioctl(chip_fd_, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0)
            {
                throw std::runtime_error(std::format("Pilo::GPIO: Failed to request Line {}: ioctl error.", line));
            }
            return req.fd;
        }
    };
}

#endif // PILOTEST_PILO_H

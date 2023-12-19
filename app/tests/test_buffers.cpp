#if 0
#include "catch_amalgamated.hpp"
#include <tcp/buffers.hpp>
#include <util/defines.hpp>

#include <cstring>
#include <vector>
#include <span>
#include <algorithm>
#include <future>
#include <thread>

using namespace tns;

// A helper function to fill a vector with incrementing values, to help verify the data integrity after wrapping
template<typename T>
void fill_with_increasing_values(std::vector<T>& vec, T start) {
    std::generate(vec.begin(), vec.end(), [&start] {
        auto val = static_cast<std::size_t>(start);
        start = static_cast<T>(val + 1);
        return static_cast<T>(val);
    });
}

/* SendBuffer */
TEST_CASE("SendBuffer writes and reads data correctly") {
    constexpr std::size_t bufferSize = 1024;
    tns::tcp::SendBuffer<bufferSize> sendBuffer;

    std::vector<std::byte> dataToSend(bufferSize / 2, std::byte{0xAB});
    PayloadView dataSpan {dataToSend};

    SECTION("Writing to SendBuffer should return the number of bytes written") {
        std::size_t expected = dataToSend.size();
        auto actual = sendBuffer.write(dataSpan);
        REQUIRE(actual.has_value());
        REQUIRE(*actual == expected);
    }

    SECTION("Reading from SendBuffer should return the same data as written") {
        sendBuffer.write(dataSpan);
        std::vector<std::byte> bufferToRead(bufferSize / 2);
        std::span<std::byte> bufferSpan{bufferToRead};
        auto bytesRead = sendBuffer.sendNBytes(bufferSpan, bufferToRead.size());
        REQUIRE(bytesRead == bufferToRead.size());
        REQUIRE(std::memcmp(dataToSend.data(), bufferToRead.data(), bytesRead) == 0);
    }
}

TEST_CASE("SendBuffer handles acknowledgments correctly") {
    constexpr std::size_t bufferSize = 1024;
    tns::tcp::SendBuffer<bufferSize> sendBuffer;

    std::vector<std::byte> dataToSend(bufferSize / 2, std::byte{0xAB});
    PayloadView dataSpan {dataToSend};
    sendBuffer.write(dataSpan);

    SECTION("Acknowledging sent data should free buffer space") {
        auto previousFreeSpace = sendBuffer.sizeFree();
        sendBuffer.onAck(bufferSize / 4); // Acknowledge beyond sent data doesn't affect the unacked size
        REQUIRE(sendBuffer.sizeFree() == previousFreeSpace);
        REQUIRE(sendBuffer.sizeUnacked() == 0);
        REQUIRE(sendBuffer.sizeNotSent() == bufferSize / 2);

        std::vector<std::byte> bufferToRead(bufferSize);
        std::span<std::byte> bufferSpan{bufferToRead};
        sendBuffer.sendNBytes(bufferSpan, bufferSize / 4); // Send 1/4 of the buffer
        REQUIRE(sendBuffer.sizeFree() == previousFreeSpace);
        REQUIRE(sendBuffer.sizeUnacked() == bufferSize / 4);
        REQUIRE(sendBuffer.sizeNotSent() == bufferSize / 4);

        sendBuffer.onAck(bufferSize / 4); // Getting ack should free up space
        REQUIRE(sendBuffer.sizeFree() == previousFreeSpace + bufferSize / 4);
        REQUIRE(sendBuffer.sizeUnacked() == 0);
        REQUIRE(sendBuffer.sizeNotSent() == bufferSize / 4);
    }

    SECTION("Acknowledging beyond sent data should not affect the unacknowledged size") {
        auto previousUnackedSize = sendBuffer.sizeUnacked();
        sendBuffer.onAck(bufferSize * 2); // Acknowledge beyond sent data
        REQUIRE(sendBuffer.sizeUnacked() == previousUnackedSize);
    }
}

TEST_CASE("SendBuffer sendNBytes works correctly") {
    constexpr std::size_t bufferSize = 1024;
    tns::tcp::SendBuffer<bufferSize> sendBuffer;

    std::vector<std::byte> dataToSend(bufferSize, std::byte{0xCD});
    PayloadView dataSpan {dataToSend};
    sendBuffer.write(dataSpan);

    std::vector<std::byte> bufferToRead(bufferSize);
    std::span<std::byte> bufferSpan{bufferToRead};

    SECTION("Sending exact buffer size should work and return correct size") {
        REQUIRE(sendBuffer.sendNBytes(bufferSpan, bufferSize) == bufferSize);
        REQUIRE(sendBuffer.sizeUnacked() == bufferSize);
        REQUIRE(sendBuffer.sizeNotSent() == 0);
        REQUIRE(sendBuffer.sizeFree() == 0);

        sendBuffer.onAck(static_cast<std::uint32_t>(bufferSize / 2));
        REQUIRE(sendBuffer.sizeUnacked() == bufferSize / 2);
        REQUIRE(sendBuffer.sizeNotSent() == 0);
        REQUIRE(sendBuffer.sizeFree() == bufferSize / 2);
    }
}

TEST_CASE("SendBuffer wrapping behavior is correct") {
    constexpr std::size_t bufferSize = 10;
    tns::tcp::SendBuffer<bufferSize> sendBuffer;

    SECTION("Writing data that wraps around the buffer boundary") {
        std::vector<std::byte> dataToSend(bufferSize - 1, std::byte{0xAB});
        auto dataSpan = std::as_bytes(std::span(dataToSend));

        // Buffer should contain the new data, wrapping around to the beginning
        std::vector<std::byte> readBuffer(bufferSize);
        auto readSpan = std::as_writable_bytes(std::span(readBuffer));

        // Write initial data to almost fill the buffer
        sendBuffer.write(dataSpan);

        // Acknowledge some data to make space, simulating data being sent and acknowledged.
        sendBuffer.sendNBytes(readSpan, 3);
        sendBuffer.onAck(3);
        REQUIRE(sendBuffer.sizeUnacked() == 0);
        REQUIRE(sendBuffer.sizeNotSent() == bufferSize - 1 - 3);
        REQUIRE(sendBuffer.sizeFree() == 4);

        // Now write data that should wrap around the buffer
        std::vector<std::byte> wrapAroundData(4, std::byte{0xCD});
        auto wrapSpan = std::as_bytes(std::span(wrapAroundData));
        auto n = sendBuffer.write(wrapSpan);
        REQUIRE(n.has_value());
        REQUIRE(*n == wrapAroundData.size());
        REQUIRE(sendBuffer.sizeUnacked() == 0);
        REQUIRE(sendBuffer.sizeNotSent() == bufferSize);
        REQUIRE(sendBuffer.sizeFree() == 0);

        REQUIRE(sendBuffer.sendNBytes(readSpan, bufferSize) == bufferSize);

        // Check the contents of the buffer after wrapping
        // Frist 6 elements should be 0xAB
        REQUIRE(std::equal(readBuffer.begin(), readBuffer.begin() + 6, dataToSend.begin()));
        
        // Last 4 elements should be 0xCD
        REQUIRE(std::equal(readBuffer.begin() + 6, readBuffer.end(), wrapAroundData.begin()));
    }

    SECTION("Reading and writing data concurrently") {
        std::vector<std::byte> dataToSend(bufferSize * 2); // Double the buffer size for wrap-around
        fill_with_increasing_values(dataToSend, std::byte{0});

        std::thread writer([&sendBuffer, &dataToSend] {
            auto dataSpan = std::as_bytes(std::span(dataToSend));
            REQUIRE(sendBuffer.write(dataSpan) == dataToSend.size());
        });

        std::thread reader([&sendBuffer, &dataToSend] {
            std::vector<std::byte> readBuffer(bufferSize);
            auto readSpan = std::as_writable_bytes(std::span(readBuffer));

            std::size_t totalBytesRead = 0;
            while (totalBytesRead < dataToSend.size()) {
                if (sendBuffer.sizeNotSent() != bufferSize) continue;
                REQUIRE(sendBuffer.sendNBytes(readSpan, bufferSize) == bufferSize);
                REQUIRE(std::equal(readBuffer.begin(), readBuffer.end(), dataToSend.begin() + totalBytesRead));
                sendBuffer.onAck(bufferSize);
                totalBytesRead += bufferSize;
            }
        });

        writer.join();
        reader.join();
    }

    SECTION("Reading and writing data concurrently") {
        std::vector<std::byte> dataToSend(bufferSize * 2); // Double the buffer size for wrap-around
        fill_with_increasing_values(dataToSend, std::byte{0});

        std::thread writer([&sendBuffer, &dataToSend] {
            auto dataSpan = std::as_bytes(std::span(dataToSend));
            REQUIRE(sendBuffer.write(dataSpan) == dataToSend.size());
        });

        std::thread reader([&sendBuffer, &dataToSend] {
            std::vector<std::byte> readBuffer(bufferSize);
            auto readSpan = std::as_writable_bytes(std::span(readBuffer));

            std::size_t totalBytesRead = 0;
            std::size_t available;
            while (totalBytesRead < dataToSend.size()) {
                if ((available = sendBuffer.sizeNotSent()) == 0) continue;
                auto n = std::max(1UL, available / 2);
                REQUIRE(sendBuffer.sendNBytes(readSpan, n) == n);
                REQUIRE(std::equal(readBuffer.begin(), readBuffer.begin() + n, dataToSend.begin() + totalBytesRead));
                totalBytesRead += n;
                sendBuffer.onAck(static_cast<std::uint32_t>(totalBytesRead));
            }
        });

        writer.join();
        reader.join();
    }

}

/* RecvBuffer */
#endif
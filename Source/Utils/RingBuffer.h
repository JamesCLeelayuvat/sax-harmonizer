#pragma once
#include <vector>
#include <algorithm>
//==============================================================================
/*
    Fixed-size circular buffer of floats (holds recent input for pitch analysis).
*/

class RingBuffer
{
public:
    RingBuffer() = delete;

    explicit RingBuffer(std::size_t a)
    {
        buffer = std::vector<float>(a + 1);
        size = a + 1;
        start = 0;
        end = a;
        std::fill(buffer.begin(), buffer.end(), 0);
    }

    void clear()
    {
        buffer.clear();
        buffer.resize(size);
        start = 0;
        end = size;
    }

    void push(float data)
    {

        end++;
        end = end % size;
        buffer[end] = data;
        start++;
        start = start % size;
    }

    std::size_t readAll(std::vector<float> &dest) const
    {
        std::size_t n = std::min<std::size_t>(getSize(), dest.size());
        for (std::size_t i = start + 1, j = 0; j < n; i++, j++)
        {
            dest[j] = buffer[i % size];
        }

        return n;
    }

private:
    std::size_t size;
    std::vector<float> buffer;
    int start;
    int end;

    std::size_t getSize() const
    {
        if (end >= start)
        {
            return end - start;
        }
        else
        {
            return size - (start - end);
        }
    }
};

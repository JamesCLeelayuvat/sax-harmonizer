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
        end = 0;
    }

    void clear()
    {
        buffer.clear();
        buffer.resize(size);
        start = 0;
        end = 0;
    }

    void push(float data)
    {

        if (full())
        {
            end++;
            end = end % size;
            buffer[end] = data;
            start++;
            start = start % size;
        }
        else
        {
            end++;
            buffer[end] = data;
        }
    }

    std::size_t readAll(std::vector<float>& dest) const
    {
        std::size_t n = std::min<std::size_t>(getSize(), dest.size());
        for(int i = start + 1, j = 0; j < n; i++, j++){
            dest[j] = buffer[i % size];
        }

        return n;
    }

    
    // TODO: setSize(), clear(), push(), read/copy out

private:
    std::size_t size;
    std::vector<float> buffer;
    int start;
    int end;

    bool full() const
    {
        return (end + 1) % size == start;
    }

    bool empty() const
    {
        return start == end;
    }

    std::size_t getSize() const{
        if(end >= start){
            return end - start;
        }
        else{
            return size - (start - end);
        }
    }
};

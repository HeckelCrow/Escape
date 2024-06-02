#pragma once
#include "alias.hpp"
#include "connection.hpp"
#include <type_traits>

enum class ClientId : u8
{
    Invalid,
    Server,

    DoorLock,
    Rings,
    Targets,
    Timer,
    RingDispenser,

    IdMax // Last
};
extern ClientId this_client_id;

enum class MessageType : u8
{
    Invalid,

    Multicast = 1,
    Reset,
    Log,

    // message_door_lock.hpp
    DoorLockCommand = 20,
    DoorLockStatus,

    // message_targets.hpp
    TargetsCommand = 30,
    TargetsStatus,
    TargetsGraph,

    // message_timer.hpp
    TimerCommand = 40,
    TimerStatus,

    // message_ring_dispenser.hpp
    RingDispenserCommand = 50,
    RingDispenserStatus,

    MessageTypeMax // Last
};

struct BufferPtr
{
    BufferPtr() {}
    BufferPtr(u8* start_in, u8* end_in) : start(start_in), end(end_in) {}
    BufferPtr(u8* start_in, u32 size) : start(start_in), end(start_in + size) {}

    u32
    size()
    {
        return (u32)(end - start);
    }

    u8* start = nullptr;
    u8* end   = nullptr;
};

enum class SerializerMode
{
    Serialize,
    Deserialize,
};

struct Serializer;
void WriteToBuffer(BufferPtr source, Serializer& s);
void ReadFromBuffer(BufferPtr dest, Serializer& s);

struct Serializer
{
    Serializer() {}
    Serializer(SerializerMode mode_in, BufferPtr buffer_in) :
        mode(mode_in), full_buffer(buffer_in), buffer(buffer_in)
    {
        if (mode == SerializerMode::Serialize)
        {
            write = WriteToBuffer;
        }
        else
        {
            write = ReadFromBuffer;
        }
    }

    SerializerMode mode = SerializerMode::Serialize;
    BufferPtr      full_buffer;
    BufferPtr      buffer;

    void (*write)(BufferPtr buffer, Serializer& s) = WriteToBuffer;
};

inline void
WriteToBuffer(BufferPtr source, Serializer& s)
{
    auto size = source.size();
    if (size <= s.buffer.size())
    {
        memcpy(s.buffer.start, source.start, size);
        s.buffer.start += size;
    }
    else
    {
        // PrintError("WriteToBuffer: Buffer is too small!\n");
    }
}

inline void
ReadFromBuffer(BufferPtr dest, Serializer& s)
{
    auto size = dest.size();
    if (size <= s.buffer.size())
    {
        memcpy(dest.start, s.buffer.start, size);
        s.buffer.start += size;
    }
    else
    {
        // PrintError("ReadFromBuffer: Buffer is too small!\n");
    }
}

template<typename T>
struct is_fundamental_or_enum
    : std::integral_constant<bool, std::is_fundamental<T>::value
                                       || std::is_enum<T>::value>
{
};

template<typename T, typename std::enable_if<is_fundamental_or_enum<T>::value,
                                             int>::type = 0>
void
Serialize(T& val, Serializer& s)
{
    auto* ptr = (u8*)&val;
    s.write(BufferPtr(ptr, sizeof(val)), s);
}

inline void
Serialize(BufferPtr& str, Serializer& s)
{
    if (s.mode == SerializerMode::Serialize)
    {
        u32 str_length = str.size();
        Serialize(str_length, s);
        s.write(str, s);
    }
    else
    {
        u32 str_length = 0;
        Serialize(str_length, s);
        if (s.buffer.start + str_length <= s.buffer.end)
        {
            str.start = s.buffer.start;
            str.end   = str.start + str_length;
            s.buffer.start += str_length;
        }
        else
        {
            // PrintError("Serialize BufferPtr&: Buffer is too small!\n");
        }
    }
}

struct MessageHeader
{
    MessageHeader() {}
    MessageHeader(MessageType type_in) :
        client_id(this_client_id), type(type_in)
    {}

    void
    serialize(Serializer& s)
    {
        Serialize(client_id, s);
        Serialize(type, s);

        if (s.mode == SerializerMode::Deserialize
            && (u8)client_id >= (u8)ClientId::IdMax)
        {
            client_id = ClientId::Invalid;
        }
    }

    ClientId    client_id = ClientId::Invalid;
    MessageType type      = MessageType::Invalid;
};

struct Multicast
{
    Multicast() {}

    MessageHeader
    getHeader()
    {
        return MessageHeader{MessageType::Multicast};
    }
    void
    serialize(Serializer& s)
    {
        Serialize(str, s);
    }

    BufferPtr str;
};

struct Message
{
    Connection    from;
    MessageHeader header;
    Serializer    deserializer;
};

struct Reset
{
    Reset() {}

    MessageHeader
    getHeader()
    {
        return MessageHeader{MessageType::Reset};
    }
    void
    serialize(Serializer& s)
    {}
};

enum class LogSeverity : u8
{
    Info,
    Warning,
    Error,
    Success
};

struct LogMessage
{
    LogMessage() {}

    MessageHeader
    getHeader()
    {
        return MessageHeader{MessageType::Log};
    }

    void
    serialize(Serializer& s)
    {
        Serialize(severity, s);
        Serialize(string, s);
    }

    LogSeverity severity = LogSeverity::Info;
    BufferPtr   string;
};

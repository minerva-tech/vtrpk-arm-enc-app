#pragma once

namespace Auxiliary {
    static const int Port = 3;

    enum AuxiliaryType {
        InvalidType = 0,
        TimestampType,
        RegisterValType,
        CameraRegisterValType,
        VideoSensorResolutionType,
        VideoSensorSettingsType
    };

    template <typename T>
    struct Pkt {
        uint16_t	type;
        uint16_t	size;
        T			data;
    };

    struct TimestampData {
        uint32_t sec;   // ok, it's actually crap, but i know that timestamps are long, and long on target platform (ARM v5t) with my compiler (GCC 4.7.2) is 32-bit.
        uint32_t usec;  // so, use it, instead of "long int" which could be 64 bit on other platforms, where this code will be used for data receiving.
    };

    struct RegisterValData {
        uint32_t addr;
        uint32_t val;
    };

    struct CameraRegisterValData {
        uint8_t val[8];
    };

/*	struct VideoSensorResolutionsData {
        uint32_t src; // top 16 bits are width, bottom 16 bits are height.
        uint32_t dst;
    };*/

    struct VideoSensorResolutionData {
        int16_t src_w;
        int16_t src_h;
        int16_t dst_w;
        int16_t dst_h;
    };

    struct VideoSensorSettingsData {
        uint8_t binning;
        uint8_t ten_bit_compression;
        uint8_t pixel_correction;
        uint8_t fps_divider;
    };

    static_assert(sizeof(Pkt<TimestampData>) <= Comm::mss, "Size of single auxiliary data packet shouldnt exceed Comm::mss");
    static_assert(sizeof(Pkt<RegisterValData>) <= Comm::mss, "Size of single auxiliary data packet shouldnt exceed Comm::mss");

    inline void SendTimestamp(long sec, long usec) {
        Pkt<TimestampData> pkt;
        pkt.type = TimestampType;
        pkt.data.sec  = sec;
        pkt.data.usec = usec;
        Comm::instance().transmit(Port, sizeof(pkt), (uint8_t*)&pkt);
    }

    inline void SendRegisterVal(uint8_t addr, uint16_t val) {
        Pkt<RegisterValData> pkt;
        pkt.type = RegisterValType;
        pkt.data.addr = addr;
        pkt.data.val = val;
        Comm::instance().transmit(Port, sizeof(pkt), (uint8_t*)&pkt);
    }

    inline void SendCameraRegisterVal(uint8_t* p, size_t size) {
        while (size>0) {
            Pkt<CameraRegisterValData> pkt;
            pkt.type = CameraRegisterValType;

            const size_t to_send = std::min(size, sizeof(pkt.data.val));
            memcpy(pkt.data.val, p, to_send);
            pkt.size = to_send;

            Comm::instance().transmit(Port, sizeof(pkt), (uint8_t*)&pkt);

            size -= to_send;
            p += to_send;
        }
    }

    inline void SendVideoSensorResolution(const VideoSensorResolutionData& res) {
        Pkt<VideoSensorResolutionData> pkt;
        pkt.type = VideoSensorResolutionType;
        pkt.data = res;
        Comm::instance().transmit(Port, sizeof(pkt), (uint8_t*)&pkt);
    }

    inline void SendVideoSensorSettings(const VideoSensorSettingsData& set) {
        Pkt<VideoSensorSettingsData> pkt;
        pkt.type = VideoSensorSettingsType;
        pkt.data = set;
        Comm::instance().transmit(Port, sizeof(pkt), (uint8_t*)&pkt);
    }

    inline AuxiliaryType Type(const uint8_t* data) {
        uint16_t type;
        const uint8_t* p = (uint8_t*)&((Pkt<uint32_t>*)data)->type;
        memcpy(&type, p, sizeof(type)); // unaligned
//		memcpy(&type, (uint8_t*)&((Pkt<uint32_t>*)data)->type, sizeof(type)); // doesn't work with gcc 4.7.2
        return (AuxiliaryType)type;
    }

    inline uint16_t Size(const uint8_t* data) {
        uint16_t size;
        const uint8_t* p = (uint8_t*)&((Pkt<uint32_t>*)data)->size;
        memcpy(&size, p, sizeof(size)); // unaligned
//		memcpy(&type, (uint8_t*)&((Pkt<uint32_t>*)data)->type, sizeof(type)); // doesn't work with gcc 4.7.2
        return size;
    }

    template <class T>
    inline T GetVal(const uint8_t* buf) {
        T data;
        const uint8_t* p = (uint8_t*)&((Pkt<T>*)buf)->data;
        memcpy(&data, p, sizeof(data)); // unaligned
        return data;
    }

    inline TimestampData Timestamp(const uint8_t* buf) {
        assert(Type(buf) == TimestampType);
        return GetVal<TimestampData>(buf);
    }

    inline RegisterValData RegisterVal(const uint8_t* buf) {
        assert(Type(buf) == RegisterValType);
        return GetVal<RegisterValData>(buf);
    }

    inline CameraRegisterValData CameraRegisterVal(const uint8_t* buf) {
        assert(Type(buf) == CameraRegisterValType);
        return GetVal<CameraRegisterValData>(buf);
    }

    inline VideoSensorResolutionData VideoSensorResolution(const uint8_t* buf) {
        assert(Type(buf) == VideoSensorResolutionType);
        return GetVal<VideoSensorResolutionData>(buf);
    }

    inline VideoSensorSettingsData VideoSensorSettings(const uint8_t* buf) {
        assert(Type(buf) == VideoSensorSettingsType);
        return GetVal<VideoSensorSettingsData>(buf);
    }
};


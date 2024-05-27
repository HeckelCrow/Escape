#include "msg.hpp"
#include "wifi_config.hpp"
#include <Wifi.h>

WiFiUDP udp;

Connection multicast_connection = {IPAddress(239, 255, 0, 1), 55872};
Connection server_connection;

u32           time_last_message_received = 0;
constexpr u32 timeout_period             = 3000;

u8 packet_buffer[udp_packet_size];

WifiState wifi_state = WifiState::WifiOff;

bool create_access_point = false;

void
StartWifi(bool access_point)
{
    create_access_point = access_point;
    // WiFi.useStaticBuffers(true);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    if (access_point)
    {
        Serial.println("Wifi mode: Access Point (AP)");
        s32 channel_strengths[12] = {0}; // 1-13
        for (auto& stren : channel_strengths)
        {
            stren = -1000;
        }

        int n = WiFi.scanNetworks();
        Serial.println("Scan done");
        if (n == 0)
        {
            Serial.println("no networks found");
        }
        else
        {
            Serial.print(n);
            Serial.println(" networks found\n");
            Serial.println("Nr | SSID                             | RSSI | CH "
                           "| Encryption");
            for (int i = 0; i < n; ++i)
            {
                String   ssid;
                uint8_t  encryption_type;
                int32_t  rssi;
                uint8_t* bssid;
                int32_t  ch;
                WiFi.getNetworkInfo(i, ssid, encryption_type, rssi, bssid, ch);
                Serial.printf("%2d", i + 1);
                Serial.print(" | ");
                Serial.printf("%-32.32s", ssid);
                Serial.print(" | ");
                Serial.printf("%4ld", rssi);
                Serial.print(" | ");
                Serial.printf("%2ld", ch);
                Serial.print(" | ");
                switch (encryption_type)
                {
                case WIFI_AUTH_OPEN: Serial.print("open"); break;
                case WIFI_AUTH_WEP: Serial.print("WEP"); break;
                case WIFI_AUTH_WPA_PSK: Serial.print("WPA"); break;
                case WIFI_AUTH_WPA2_PSK: Serial.print("WPA2"); break;
                case WIFI_AUTH_WPA_WPA2_PSK: Serial.print("WPA+WPA2"); break;
                case WIFI_AUTH_WPA2_ENTERPRISE: Serial.print("WPA2-EAP"); break;
                case WIFI_AUTH_WPA3_PSK: Serial.print("WPA3"); break;
                case WIFI_AUTH_WPA2_WPA3_PSK: Serial.print("WPA2+WPA3"); break;
                case WIFI_AUTH_WAPI_PSK: Serial.print("WAPI"); break;
                default: Serial.print("unknown");
                }
                Serial.println();
                if (rssi > channel_strengths[ch - 1])
                {
                    channel_strengths[ch - 1] = rssi;
                }
            }
        }
        Serial.println("");
        WiFi.scanDelete();

        WiFi.mode(WIFI_AP);

        s32 channel = 1;

        // channel 1, 6, and 11 are non-overlapping and preferred
        if (channel_strengths[1 - 1] <= channel_strengths[6 - 1]
            && channel_strengths[1 - 1] <= channel_strengths[11 - 1])
        {
            channel = 1;
        }
        else if (channel_strengths[6 - 1] <= channel_strengths[11 - 1])
        {
            channel = 6;
        }
        else
        {
            channel = 11;
        }
        Serial.printf("Channel %2ld\n", channel);

        constexpr s32 max_connection_count = 6;
        WiFi.softAP(wifi_ssid, wifi_password, channel, 0, max_connection_count);
        wifi_state = WifiState::StartMulticast;

        Serial.print(F("IP address: "));
        Serial.println(WiFi.softAPIP());
    }
    else
    {
        Serial.println("Wifi mode: Station (STA)");
        WiFi.begin(wifi_ssid, wifi_password);
        wifi_state = WifiState::WaitingForWifi;
    }
}

Message
ReceiveMessage()
{
    Message message;

    switch (wifi_state)
    {
    case WifiState::WifiOff: StartWifi(create_access_point); break;
    case WifiState::WaitingForWifi:
    {
        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.print(F("IP address: "));
            Serial.println(WiFi.localIP());
            WiFi.setAutoReconnect(true);
            wifi_state = WifiState::StartMulticast;

            // TODO: Reset after waiting too much?
        }
        // else
        // {
        //     Serial.println(F("Waiting for connection"));
        // }
    }
    break;
    case WifiState::StartMulticast:
    {
        if (!udp.beginMulticast(multicast_connection.address,
                                multicast_connection.port))
        {
            Serial.print(F("udp.beginMulticast error\n"));
        }
        wifi_state = WifiState::WaitingForMulticast;
    }
    break;
    case WifiState::WaitingForMulticast:
    {
        server_connection = Connection{};

        auto packet_size = udp.parsePacket();
        if (packet_size > 0)
        {
            if (packet_size > udp_packet_size)
            {
                Serial.println(F("Packet too big!"));
                break;
            }
            server_connection.address = udp.remoteIP();
            server_connection.port    = udp.remotePort();

            udp.read(packet_buffer, packet_size);

            Serial.print(F("Received packet from : "));
            Serial.print(udp.remoteIP());
            Serial.print(F(", port "));
            Serial.println(udp.remotePort());
            // Serial.printf("Data : %s\n", packet_buffer);
            time_last_message_received = millis();

            auto          ds = Serializer(SerializerMode::Deserialize,
                                 {packet_buffer, (u32)packet_size});
            MessageHeader header;
            header.serialize(ds);
            if (header.client_id == ClientId::Server
                && header.type == MessageType::Multicast)
            {
                Multicast multi;
                multi.serialize(ds);
                Serial.printf("Data : %.*s\n", multi.str.end - multi.str.start,
                              (char*)multi.str.start);
                const char* multicast_text     = "Hey it's me, the server.";
                auto        multicast_text_len = strlen(multicast_text);
                if ((multi.str.end - multi.str.start) == multicast_text_len
                    && memcmp(multi.str.start, multicast_text,
                              multicast_text_len)
                           == 0)
                {
                    udp.stop();
                    udp.begin(0);

                    // TODO: resend?
                    auto        ser = Serializer(SerializerMode::Serialize,
                                          {packet_buffer, (u32)packet_size});
                    const char* txt = "Hello";
                    LogMessage  log = {};
                    log.severity    = LogSeverity::Info;
                    log.string      = {(u8*)txt, strlen(txt)};
                    log.getHeader().serialize(ser);
                    log.serialize(ser);

                    udp.beginPacket(server_connection.address,
                                    server_connection.port);
                    BufferPtr buffer = {ser.full_buffer.start,
                                        ser.buffer.start};
                    udp.write(buffer.start, buffer.end - buffer.start);
                    udp.endPacket();
                    wifi_state = WifiState::Connected;
                }
                else
                {
                    Serial.println(F("Multicast message is wrong!"));
                }
            }
            else
            {
                Serial.println(F("Multicast header is wrong!"));
            }
        }

        if (!create_access_point && WiFi.status() != WL_CONNECTED)
        {
            wifi_state = WifiState::WaitingForWifi;
        }
    }
    break;
    case WifiState::Connected:
    {
        auto t           = millis();
        auto packet_size = udp.parsePacket();
        if (packet_size >= 2)
        {
            time_last_message_received = t;
            if (packet_size > udp_packet_size)
            {
                Serial.println(F("Packet too big!"));
                break;
            }
            udp.read(packet_buffer, packet_size);
            message.deserializer = Serializer(
                SerializerMode::Deserialize, {packet_buffer, (u32)packet_size});
            message.header.serialize(message.deserializer);
            message.from = {udp.remoteIP(), udp.remotePort()};
        }

        if (t - time_last_message_received > timeout_period)
        {
            udp.stop();
            wifi_state = WifiState::StartMulticast;
            Serial.println(F("Server timed out!"));
        }
    }
    break;
    }
    return message;
}
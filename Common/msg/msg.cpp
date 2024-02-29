#include "msg.hpp"
#include <Wifi.h>

WiFiUDP     udp;
const char* wifi_ssid     = "Julien";
const char* wifi_password = "01234567";

Connection multicast_connection = {IPAddress(239, 255, 0, 1), 55872};
Connection server_connection;

u8 packet_buffer[udp_packet_size];

enum class WifiState
{
    WifiOff,
    WaitingForWifi,
    StartMulticast,
    WaitingForMulticast,
    Connected,
};
WifiState wifi_state = WifiState::WifiOff;

void
StartWifi(bool access_point)
{
    WiFi.useStaticBuffers(true);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    if (access_point)
    {
        WiFi.mode(WIFI_AP);
        constexpr s32 max_connection_count = 4;
        WiFi.softAP(wifi_ssid, wifi_password, 1, 0, max_connection_count);
        wifi_state = WifiState::StartMulticast;

        Serial.print(F("IP address: "));
        Serial.println(WiFi.softAPIP());
    }
    else
    {
        WiFi.begin(wifi_ssid, wifi_password);
        wifi_state = WifiState::WaitingForWifi;
    }

    // int n = WiFi.scanNetworks();
    // Serial.println("Scan done");
    // if (n == 0)
    // {
    //     Serial.println("no networks found");
    // }
    // else
    // {
    //     Serial.print(n);
    //     Serial.println(" networks found");
    //     Serial.println(
    //         "Nr | SSID                             | RSSI | CH |
    //         Encryption");
    //     for (int i = 0; i < n; ++i)
    //     {
    //         // Print SSID and RSSI for each network found
    //         Serial.printf("%2d", i + 1);
    //         Serial.print(" | ");
    //         Serial.printf("%-32.32s", WiFi.SSID(i).c_str());
    //         Serial.print(" | ");
    //         Serial.printf("%4ld", WiFi.RSSI(i));
    //         Serial.print(" | ");
    //         Serial.printf("%2ld", WiFi.channel(i));
    //         Serial.print(" | ");
    //         switch (WiFi.encryptionType(i))
    //         {
    //         case WIFI_AUTH_OPEN: Serial.print("open"); break;
    //         case WIFI_AUTH_WEP: Serial.print("WEP"); break;
    //         case WIFI_AUTH_WPA_PSK: Serial.print("WPA"); break;
    //         case WIFI_AUTH_WPA2_PSK: Serial.print("WPA2"); break;
    //         case WIFI_AUTH_WPA_WPA2_PSK: Serial.print("WPA+WPA2"); break;
    //         case WIFI_AUTH_WPA2_ENTERPRISE: Serial.print("WPA2-EAP"); break;
    //         case WIFI_AUTH_WPA3_PSK: Serial.print("WPA3"); break;
    //         case WIFI_AUTH_WPA2_WPA3_PSK: Serial.print("WPA2+WPA3"); break;
    //         case WIFI_AUTH_WAPI_PSK: Serial.print("WAPI"); break;
    //         default: Serial.print("unknown");
    //         }
    //         Serial.println();
    //         delay(10);
    //     }
    // }
    // Serial.println("");
    // WiFi.scanDelete();
}

Message
ReceiveMessage()
{
    Message message;

    switch (wifi_state)
    {
    case WifiState::WifiOff: StartWifi(); break;
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
                    log.getHeader(this_client_id).serialize(ser);
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
    }
    break;
    case WifiState::Connected:
    {
        auto packet_size = udp.parsePacket();
        if (packet_size >= 2)
        {
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
    }
    break;
    }
    return message;
}
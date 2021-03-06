#pragma once
#include <Networking/BaseSocket.h>
#include <vector>

#include <Utils/DebugHandler.h>

enum BuildType
{
    Internal,
    Alpha,
    Beta,
    Release
};

// Align data perfectly
#pragma pack(push, 1)
struct ClientLogonChallenge
{
    u8 majorVersion;
    u8 patchVersion;
    u8 minorVersion;
    u8 buildType; // 0 Internal, 1 Alpha, 2 Beta, 3 Release
    u16 gameBuild;
    std::string gameName;
    std::string username;

    std::string BuildTypeString()
    {
        std::string ret;

        switch (buildType)
        {
        case BuildType::Internal:
            ret = "Internal";
            break;
        case BuildType::Alpha:
            ret = "Alpha";
            break;
        case BuildType::Beta:
            ret = "Beta";
            break;
        case BuildType::Release:
            ret = "Release";
            break;
        }

        return ret;
    }

    u16 Serialize(std::shared_ptr<ByteBuffer> buffer)
    {
        u16 size = static_cast<u16>(buffer->WrittenData);

        buffer->PutU8(majorVersion);
        buffer->PutU8(patchVersion);
        buffer->PutU8(minorVersion);
        buffer->PutU8(buildType);
        buffer->PutU16(gameBuild);
        buffer->PutString(gameName);
        buffer->PutString(username);

        return static_cast<u16>(buffer->WrittenData) - size;
    }
};

struct ServerLogonChallenge
{
    u8 status;
    u8 B[128];
    u8 s[8];

    void Deserialize(std::shared_ptr<ByteBuffer> buffer)
    {
        buffer->GetU8(status);
        if (status == 0)
        {
            buffer->GetBytes(B, 128);
            buffer->GetBytes(s, 8);
        }
    }
};

struct ClientLogonResponse
{
    u8 M1[32];

    u16 Serialize(std::shared_ptr<ByteBuffer> buffer)
    {
        u16 size = static_cast<u16>(buffer->WrittenData);

        buffer->PutBytes(M1, 32);

        return static_cast<u16>(buffer->WrittenData) - size;
    }
};
#pragma pack(pop)

class NetworkClient
{
public:
    using tcp = asio::ip::tcp;
    NetworkClient(tcp::socket* socket)
    {
        _baseSocket = new BaseSocket(socket, std::bind(&NetworkClient::HandleRead, this), std::bind(&NetworkClient::HandleDisconnect, this));
    }

    void Listen();
    bool Connect(tcp::endpoint endpoint);
    bool Connect(std::string address, u16 port);
    void HandleConnect();
    void HandleDisconnect();
    void HandleRead();
    void Send(ByteBuffer* buffer) { _baseSocket->Send(buffer); }
    void Close(asio::error_code code) { _baseSocket->Close(code); }
    bool IsClosed() { return _baseSocket->IsClosed(); }

    BaseSocket* GetBaseSocket() { return _baseSocket; }

    std::string username = "test";
    std::string password = "test";
private:
    BaseSocket* _baseSocket;
};